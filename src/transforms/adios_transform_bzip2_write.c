#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <sys/time.h>

#include "adios_logger.h"
#include "adios_transforms_common.h"
#include "adios_transforms_write.h"
#include "adios_transforms_hooks_write.h"

#ifdef BZIP2

#include "bzlib.h"

int compress_bzip2_pre_allocated(const void* input_data, const uint64_t input_len, void* output_data, uint64_t* output_len, int blockSize100k) {
    // bzip2 only support input size of 32 bit integer

    assert(input_data != NULL
            && input_len > 0 && input_len <= UINT_MAX
            && output_data != NULL
            && output_len != NULL && *output_len > 0 && *output_len < UINT_MAX);

    unsigned int input_len_32 = (unsigned int)input_len;
    unsigned int output_len_32 = (unsigned int)(*output_len);

    int bz_rtn = BZ2_bzBuffToBuffCompress((char*)output_data, &output_len_32,
                                          (char*)input_data, input_len_32,
                                          5, 0, 30);

    if(bz_rtn != BZ_OK)
        return -1;

    *output_len = output_len_32;
    return 0;
}

uint16_t adios_transform_bzip2_get_metadata_size()
{
    return 0;
}

uint64_t adios_transform_bzip2_calc_vars_transformed_size(uint64_t orig_size, int num_vars)
{
    return orig_size;
}

int adios_transform_bzip2_apply(struct adios_file_struct *fd,
                                struct adios_var_struct *var,
                                uint64_t *transformed_len,
                                int use_shared_buffer,
                                int *wrote_to_shared_buffer)
{
    // Get the input data and data length
    const uint64_t input_size = adios_transform_get_pre_transform_var_size(fd->group, var);
    const void *input_buff = var->data;

    // parse the compressiong parameter
    int compress_level = 9;
    if(var->transform_type_param
        && strlen(var->transform_type_param) > 0)
    {
        compress_level = atoi(var->transform_type_param);
        if(compress_level > 9 || compress_level < 1)
        {
            compress_level = 9;
        }
    }

    // decide the output buffer
    uint64_t output_size = adios_transform_bzip2_calc_vars_transformed_size(input_size, 1);
    void* output_buff = NULL;

    if (use_shared_buffer) {
        // If shared buffer is permitted, serialize to there
        assert(shared_buffer_reserve(fd, output_size));

        // Write directly to the shared buffer
        output_buff = fd->buffer + fd->offset;
    } else { // Else, fall back to var->data memory allocation
        output_buff = malloc(output_size);
        assert(output_buff);
    }
    *wrote_to_shared_buffer = use_shared_buffer;

    int rtn = compress_bzip2_pre_allocated(input_buff, input_size, output_buff, &output_size, compress_level);

    if(0 != rtn 					// compression failed for some reason, then just copy the buffer
        || output_size > input_size)  // or size after compression is even larger (not likely to happen since compression lib will return non-zero in this case)
    {
        return 0;
    }

    // Wrap up, depending on buffer mode
    if (*wrote_to_shared_buffer) {
        shared_buffer_mark_written(fd, output_size);
    } else {
        var->data = output_buff;
        var->data_size = output_size;
        var->free_data = adios_flag_yes;
    }

    *transformed_len = output_size; // Return the size of the data buffer
    return 1;
}

#else

DECLARE_TRANSFORM_WRITE_METHOD_UNIMPL(bzip2)

#endif
