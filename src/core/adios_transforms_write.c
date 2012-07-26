
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "adios_bp_v1.h"
#include "common_adios.h"
#include "adios_logger.h"
#include "adios_internals.h"
#include "public/adios_types.h"

#include "adios_transforms_hooks_write.h"
#include "adios_transforms_common.h"
#include "adios_transforms_write.h"

typedef char bool;

////////////////////////////////////////
// adios_group_size support
////////////////////////////////////////
uint64_t adios_transform_worst_case_transformed_group_size(uint64_t group_size, struct adios_file_struct *fd)
{
    // New group size is always at least the original group size
    uint64_t max_transformed_group_size = group_size;
    int non_scalar_var_count = 0;

    struct adios_var_struct *cur_var;
    uint64_t transformed_group_size;
    int transform_type;

    // Table of what transform types have been seen so far.
    // Allocated on stack; no dynamic memory to clean up.
    bool transform_type_seen[num_adios_transform_types];
    memset(transform_type_seen, 0, num_adios_transform_types * sizeof(bool));

    // Identify all transform methods used, and count the number of non-scalar
    // variables
    for (cur_var = fd->group->vars; cur_var; cur_var = cur_var->next)
    {
        // Skip unknown/none transform types, transform types that
        // have already been processed, and scalar variables
        if (cur_var->transform_type == adios_transform_none ||
            transform_type_seen[cur_var->transform_type] ||
            !cur_var->dimensions)
        {
            continue;
        }

        transform_type_seen[cur_var->transform_type] = 1;
        non_scalar_var_count++;
    }

    // For each transform type, get a worst-case group size estimate, and
    // record the worst of the worst cases
    for (transform_type = adios_transform_none + 1; transform_type < num_adios_transform_types; transform_type++) {
        if (!transform_type_seen[transform_type])
            continue;

        printf(">>> WORST CASE FOR TRANSFORM TYPE %d\n", transform_type);
        transformed_group_size = adios_transform_calc_vars_transformed_size(transform_type, group_size, non_scalar_var_count);

        if (transformed_group_size > max_transformed_group_size) {
            max_transformed_group_size = transformed_group_size;
        }
    }

    // Return the maximum worst case for the group size. Note that this is
    // always at least group_size, since it is initialized to that value,
    // and never decreases.
    return max_transformed_group_size;
}

////////////////////////////////////////
// Variable conversion to byte array (preparation for transform)
////////////////////////////////////////
static struct adios_dimension_struct * new_dimension() {
    struct adios_dimension_struct *dim = (struct adios_dimension_struct *)malloc(sizeof(struct adios_dimension_struct));
    dim->dimension.rank = 0;
    dim->dimension.id = 0;
    dim->dimension.time_index = adios_flag_no;
    dim->global_dimension.rank = 0;
    dim->global_dimension.id = 0;
    dim->global_dimension.time_index = adios_flag_no;
    dim->local_offset.rank = 0;
    dim->local_offset.id = 0;
    dim->local_offset.time_index = adios_flag_no;
    dim->next = 0;
    return dim;
}

static void set_dimension_is_time_index(struct adios_dimension_struct *dim, enum ADIOS_FLAG is_time_index) {
    dim->dimension.time_index = is_time_index;
    dim->global_dimension.time_index = is_time_index;
    dim->local_offset.time_index = is_time_index;
}

static int get_time_dimension_position(struct adios_var_struct *var) {
    struct adios_dimension_struct *dim;
    int i;

    for (dim = var->dimensions, i = 0; dim != 0; dim = dim->next, i++) {
        if (dim->dimension.time_index == adios_flag_yes)
            return i;
    }

    return -1;
}

static void adios_transform_attach_byte_array_dimension(struct adios_group_struct *grp, struct adios_var_struct *var) {
    // Create dimensions for the byte array (including a time dimension, if one
    // existed before).
    int ndim = count_dimensions(var->pre_transform_dimensions);

    // -1 => no time dim, 0...ndim-1 => index of the time dim
    int time_dim_pos = get_time_dimension_position(var);
    assert(time_dim_pos == -1 || time_dim_pos == 0 || time_dim_pos == ndim - 1); // I think...check with Gary/Norbert

    adios_append_dimension(&var->dimensions, new_dimension());
    if (time_dim_pos != -1) {
        adios_append_dimension(&var->dimensions, new_dimension());

        // Mark the first or last dimension as a time dimension, depending on
        // where the time dimension was before
        if (time_dim_pos == 0)
            set_dimension_is_time_index(var->dimensions, adios_flag_yes);
        else
            set_dimension_is_time_index(var->dimensions->next, adios_flag_yes);
    }
}

static void adios_transform_convert_var_to_byte_array(struct adios_group_struct *grp, struct adios_var_struct *var) {
    // Save old metadata
    var->pre_transform_type = var->type;
    var->pre_transform_dimensions = var->dimensions;

    var->type = adios_byte;
    var->dimensions = 0;

    // Attach the new 1D dimension to the variable
    adios_transform_attach_byte_array_dimension(grp, var);
}

////////////////////////////////////////
// Definition phase - set up transform parameters
////////////////////////////////////////
struct adios_var_struct * adios_transform_define_var(struct adios_group_struct *orig_var_grp,
                                                     struct adios_var_struct *orig_var,
                                                     enum ADIOS_TRANSFORM_TYPE transform_type,
                                                    char* transform_param) {
    log_debug("Transforming variable %s with type %d\n", orig_var->name, transform_type);

    // Check for the simple and error cases
    if (transform_type == adios_transform_none) {
        orig_var->transform_type = adios_transform_none;
        return orig_var;
    }

    // If we get here, transform_type is an actual transformation, so prepare
    // the variable. This entails 1) adding a new dimension variable for
    // the variable (it will become a 1D byte array), and 2) making the
    // variable into a 1D byte array.

    // Convert variable to 1D byte array
    adios_transform_convert_var_to_byte_array(orig_var_grp, orig_var);
    log_debug("Converted variable %s into byte array\n", orig_var->name);

    // Set transform type and allocate the metadata buffer
    orig_var->transform_type = transform_type;
    orig_var->transform_metadata_len = adios_transform_get_metadata_size(transform_type);
    if (orig_var->transform_metadata_len)
        orig_var->transform_metadata = malloc(orig_var->transform_metadata_len);

    // set the parameter string
    if(transform_param && transform_param[0] != '\0')
    {
        size_t param_len = strlen(transform_param);
        if(param_len > UINT16_MAX)
        {
            param_len = UINT16_MAX;
        }

        orig_var->transform_type_param_len = (uint16_t)param_len;
        orig_var->transform_type_param = strdup(transform_param);
    }

    // Return the modified variable
    return orig_var;
}

////////////////////////////////////////
// Write phase - transformed byte array dimension management
////////////////////////////////////////

// NCSU ALACRITY-ADIOS - Compute the pre-transform size of a variable, in bytes
// Precondition: var is a "dimensioned" variable; that is, not a scalar, and not a string
uint64_t adios_transform_get_pre_transform_var_size(struct adios_group_struct *group, struct adios_var_struct *var) {
    assert(var->dimensions);
    assert(var->type != adios_string);
    return adios_get_dimension_space_size(var,
                                          var->pre_transform_type,
                                          var->pre_transform_dimensions,
                                          group,
                                          NULL); // NULL because it's not a string, so unneeded
}

static int adios_transform_store_transformed_length(struct adios_file_struct * fd, struct adios_var_struct *var, uint64_t len) {
    // Assume a single dimension, since this has been converted to a byte array
    assert(var->dimensions);
    assert(var->dimensions->next == 0);

    var->dimensions->dimension.rank = len;
    return 1;
}

int adios_transform_variable_data(struct adios_file_struct * fd,
                                  struct adios_var_struct *var,
                                  int use_shared_buffer,
                                  int *wrote_to_shared_buffer) {
    //printf("[TRANSFORM] Would be doing transform ID %d on variable %s here, if it were implemented\n", var->transform_type, var->name);
    //printf("[TRANSFORM] Apparent type of variable is %s, but was originally %s\n", adios_type_to_string_int(var->type), adios_type_to_string_int(var->pre_transform_type));
    //printf("[TRANSFORM] Original size of variable data is %llu\n", adios_transform_get_pre_transform_var_size(fd->group, var));

    assert(fd);
    assert(var);

    if (var->transform_type == adios_transform_none) {
        // If no transform is required, do nothing, and delegate payload
        // writing to the caller by not using the shared buffer
        *wrote_to_shared_buffer = 0;
        return 1;
    }

    assert(var->type == adios_byte); // Assume byte array
    assert(var->transform_type != adios_transform_none);

    // Transform the data, get the new length
    uint64_t transformed_len;
    int success = adios_transform_apply(var->transform_type, fd, var, &transformed_len, use_shared_buffer, wrote_to_shared_buffer);

    if (!success)
        return 0;

    // Store the new length in the metadata
    adios_transform_store_transformed_length(fd, var, transformed_len);

    return 1;
}



//////////////////////////////////////////////////////////////////////////
// Characteristic support, used in adios_internals.c so cannot be
// in the adios_transforms_common.c, as that isn't always linked
// with adios_internals.c.
//////////////////////////////////////////////////////////////////////////

// TODO: There are 3 exact copies of this function:
//       1) here, 2) core/adios_internals.c, 3) write/adios_adaptive.c
//       Someone should extract to a single copy and make it public via
//       a header file.
static void buffer_write (char ** buffer, uint64_t * buffer_size
                         ,uint64_t * buffer_offset
                         ,const void * data, uint64_t size
                         )
{
    if (*buffer_offset + size > *buffer_size || *buffer == 0)
    {
        char * b = realloc (*buffer, *buffer_offset + size + 1000);
        if (b)
        {
            *buffer = b;
            *buffer_size = (*buffer_offset + size + 1000);
        }
        else
        {
            fprintf (stderr, "Cannot allocate memory in buffer_write.  "
                             "Requested: %llu\n", *buffer_offset + size + 1000);

            return;
        }
    }

    memcpy (*buffer + *buffer_offset, data, size);
    *buffer_offset += size;
}

static void adios_transform_dereference_dimensions_characteristic(struct adios_file_struct *fd, struct adios_index_characteristic_dims_struct_v1 *dst_char_dims, const struct adios_dimension_struct *src_var_dims) {
    uint8_t i;
    uint8_t c = count_dimensions(src_var_dims);

    dst_char_dims->count = c;
    dst_char_dims->dims = malloc(3 * 8 * c); // (local, global, local offset) * count
    assert(dst_char_dims->dims);
    uint64_t *ptr = dst_char_dims->dims;
    for (i = 0; i < c; i++)
    {
        ptr[0] = get_value_for_dim(fd, &src_var_dims->dimension);
        ptr[1] = get_value_for_dim(fd, &src_var_dims->global_dimension);
        ptr[2] = get_value_for_dim(fd, &src_var_dims->local_offset);
        src_var_dims = src_var_dims->next;
        ptr += 3; // Go to the next set of 3
    }
}

static void adios_transform_dereference_dimensions_var(struct adios_file_struct *fd, struct adios_dimension_struct **dst_var_dims, const struct adios_dimension_struct *src_var_dims) {
    uint8_t i;
    uint8_t c = count_dimensions(src_var_dims);

    for (i = 0; i < c; i++) {
        struct adios_dimension_struct * d_new =
            (struct adios_dimension_struct *)malloc(sizeof (struct adios_dimension_struct));

        // de-reference dimension id
        d_new->dimension.id = 0;
        d_new->dimension.rank = get_value_for_dim(fd, &src_var_dims->dimension);
        d_new->dimension.time_index = src_var_dims->dimension.time_index;
        d_new->global_dimension.id = 0;
        d_new->global_dimension.rank = get_value_for_dim(fd, &src_var_dims->global_dimension);
        d_new->global_dimension.time_index = src_var_dims->global_dimension.time_index;
        d_new->local_offset.id = 0;
        d_new->local_offset.rank = get_value_for_dim(fd, &src_var_dims->local_offset);
        d_new->local_offset.time_index = src_var_dims->local_offset.time_index;
        d_new->next = 0;

        adios_append_dimension(dst_var_dims, d_new);

        src_var_dims = src_var_dims->next;
    }
}

static void adios_transform_clean_dimensions(struct adios_index_characteristic_dims_struct_v1 *dst_char_dims) {
    dst_char_dims->count = 0;
    if (dst_char_dims->dims)
        free(dst_char_dims->dims);
    dst_char_dims->dims = 0;
}

static uint8_t adios_transform_serialize_transform(enum ADIOS_TRANSFORM_TYPE transform_type,
                                                   enum ADIOS_DATATYPES pre_transform_type,
                                                   const struct adios_index_characteristic_dims_struct_v1 *pre_transform_dimensions,
                                                   uint16_t transform_metadata_len,
                                                   void *transform_metadata,
                                                   uint64_t *write_length, char **buffer, uint64_t *buffer_size, uint64_t *buffer_offset) {

    // Either there is no metadata, or the metadata buffer is non-null
    assert(!transform_metadata_len || transform_metadata);

    *write_length = 0;

    // No transform case
    if (transform_type == adios_transform_none)
        return 0;

    // From this point on, this is an actual transform type

    // Write characteristic flag
    uint8_t flag = (uint8_t)adios_characteristic_transform_type;
    buffer_write(buffer, buffer_size, buffer_offset, &flag, 1);
    *write_length += 1;

    // Write transform type
    buffer_write(buffer, buffer_size, buffer_offset, &transform_type, 1);
    *write_length += 1;

    // Write the pre-transform datatype
    buffer_write(buffer, buffer_size, buffer_offset, &pre_transform_type, 1);
    *write_length += 1;

    // Write the number of pre-transform dimensions
    buffer_write (buffer, buffer_size, buffer_offset, &pre_transform_dimensions->count, 1);
    *write_length += 1;

    // Write the length of pre-transform dimension data about to be written
    uint16_t len = 3 * 8 * pre_transform_dimensions->count;
    buffer_write (buffer, buffer_size, buffer_offset, &len, 2);
    *write_length += 2;

    // Write the pre-transform dimensions
    buffer_write (buffer, buffer_size, buffer_offset, pre_transform_dimensions->dims, len);
    *write_length += len;

    buffer_write (buffer, buffer_size, buffer_offset, &transform_metadata_len, 2);
    *write_length += 2;

    if (transform_metadata_len) {
        buffer_write (buffer, buffer_size, buffer_offset, transform_metadata, transform_metadata_len);
        *write_length += transform_metadata_len;
    }

    return 1; // Return that we wrote 1 characteristic flag
}

uint8_t adios_transform_serialize_transform_characteristic(const struct adios_index_characteristic_transform_struct *transform, uint64_t *write_length,
                                                           char **buffer, uint64_t *buffer_size, uint64_t *buffer_offset) {
    return adios_transform_serialize_transform(
                transform->transform_type, transform->pre_transform_type, &transform->pre_transform_dimensions,
                transform->transform_metadata_len, transform->transform_metadata,
                write_length, buffer, buffer_size, buffer_offset
           );
}

uint8_t adios_transform_serialize_transform_var(struct adios_file_struct *fd, const struct adios_var_struct *var, uint64_t *write_length,
                                                char **buffer, uint64_t *buffer_size, uint64_t *buffer_offset) {

    // In this case, we are going to actually serialize the dimensions as a
    // adios_index_characteristic_dims_struct_v1, but it is currently in the
    // form of an adios_dimension_struct. We must convert here before passing
    // to the common serialization routine.

    struct adios_index_characteristic_dims_struct_v1 tmp_dims;
    adios_transform_dereference_dimensions_characteristic(fd, &tmp_dims, var->pre_transform_dimensions);

    // Perform the serialization using the common function with the temp dimension structure
    uint8_t char_write_count =
            adios_transform_serialize_transform(
                var->transform_type, var->pre_transform_type, &tmp_dims,
                var->transform_metadata_len, var->transform_metadata,
                write_length, buffer, buffer_size, buffer_offset
            );

    // Free the temp dimension structure
    adios_transform_clean_dimensions(&tmp_dims);

    return char_write_count;
}

int adios_transform_copy_transform_characteristic(struct adios_file_struct *fd, struct adios_index_characteristic_transform_struct *dst_transform, const struct adios_var_struct *src_var) {
    adios_transform_init_transform_characteristic(dst_transform);

    dst_transform->transform_type = src_var->transform_type;
    dst_transform->pre_transform_type = src_var->pre_transform_type;

    adios_transform_dereference_dimensions_characteristic(fd, &dst_transform->pre_transform_dimensions, src_var->pre_transform_dimensions);

    dst_transform->transform_metadata_len = src_var->transform_metadata_len;
    if (src_var->transform_metadata_len) {
        dst_transform->transform_metadata = malloc(src_var->transform_metadata_len);
        memcpy(dst_transform->transform_metadata, src_var->transform_metadata, src_var->transform_metadata_len);
    } else {
        dst_transform->transform_metadata = 0;
    }

    return 1;
}

int adios_transform_copy_var_transform(struct adios_file_struct *fd, struct adios_var_struct *dst_var, const struct adios_var_struct *src_var) {
    adios_transform_init_transform_var(dst_var);

    dst_var->transform_type = src_var->transform_type;
    dst_var->pre_transform_type = src_var->pre_transform_type;

    adios_transform_dereference_dimensions_var(fd, &dst_var->pre_transform_dimensions, src_var->pre_transform_dimensions);

    // for parameter
    dst_var->transform_type_param_len = src_var->transform_type_param_len;
    if (src_var->transform_type_param_len) {
        dst_var->transform_type_param = malloc(src_var->transform_type_param_len);
        memcpy(dst_var->transform_type_param, src_var->transform_type_param, src_var->transform_type_param_len);
    } else {
        dst_var->transform_type_param = 0;
    }

    dst_var->transform_metadata_len = src_var->transform_metadata_len;
    if (src_var->transform_metadata_len) {
        dst_var->transform_metadata = malloc(src_var->transform_metadata_len);
        memcpy(dst_var->transform_metadata, src_var->transform_metadata, src_var->transform_metadata_len);
    } else {
        dst_var->transform_metadata = 0;
    }

    return 1;
}

uint64_t adios_transform_calc_transform_characteristic_overhead(struct adios_var_struct *var) {
    if (var->transform_type == adios_transform_none) {
        return 0; // No overhead needed, since characteristic won't be written
    } else {
        return 1 +	// For characterstic flag
               1 +	// For transform_type field
               1 +  // For pre_transform_type field
               adios_calc_var_characteristics_dims_overhead(var->pre_transform_dimensions) + // For pre-transform dimensions field
               2 +	// For transform_metadata_len
               var->transform_metadata_len;	// For transform_metadata
    }
}
