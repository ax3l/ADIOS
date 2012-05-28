/*
 * ADIOS is freely available under the terms of the BSD license described
 * in the COPYING file in the top level directory of this source distribution.
 *
 * Copyright (c) 2008 - 2009.  UT-BATTELLE, LLC. All rights reserved.
 */

#ifndef ADIOS_READ_HOOKS_H
#define ADIOS_READ_HOOKS_H

#include "config.h"
#include <stdint.h>
#include <string.h>
#include "public/adios_read.h"

#define FORWARD_DECLARE(a) \
int adios_read_##a##_init_method (MPI_Comm comm, const char * parameters); \
int adios_read_##a##_finalize_method (); \
ADIOS_FILE * adios_read_##a##_open_stream (const char * fname, MPI_Comm comm, enum ADIOS_LOCKMODE lock_mode, int timeout_msec); \
ADIOS_FILE * adios_read_##a##_open_file (const char * fname, MPI_Comm comm); \
int adios_read_##a##_close (const ADIOS_FILE *fp); \
int adios_read_##a##_advance_step (const ADIOS_FILE *fp, int last, int wait_for_step); \
void adios_read_##a##_release_step (const ADIOS_FILE *fp); \
ADIOS_VARINFO * adios_read_##a##_inq_var (const ADIOS_FILE *fp, const char * varname); \
ADIOS_VARINFO * adios_read_##a##_inq_var_byid (const ADIOS_FILE *gp, int varid); \
int adios_read_##a##_inq_var_stat (const ADIOS_FILE *fp, const ADIOS_VARINFO * varinfo, int per_step_stat, int per_block_stat); \
int adios_read_##a##_inq_var_blockinfo (const ADIOS_FILE *fp, const ADIOS_VARINFO * varinfo); \
int adios_read_##a##_schedule_read (const ADIOS_FILE * fp, const ADIOS_SELECTION * sel, const char * varname, int from_steps, int nsteps, void * data); \
int adios_read_##a##_schedule_read_byid (const ADIOS_FILE * fp, const ADIOS_SELECTION * sel, int varid, int from_steps, int nsteps, void * data) ; \
int adios_read_##a##_perform_reads (const ADIOS_FILE *fp, int blocking); \
int adios_read_##a##_check_reads (const ADIOS_FILE * fp, ADIOS_VARCHUNK ** chunk); \
int adios_read_##a##_get_attr (const ADIOS_FILE * fp, const char * attrname, enum ADIOS_DATATYPES * type, int * size, void ** data); \
int adios_read_##a##_get_attr_byid (const ADIOS_FILE * fp, int attrid, enum ADIOS_DATATYPES * type, int * size, void ** data); \
void adios_read_##a##_reset_dimension_order (const ADIOS_FILE *fp, int is_fortran); \

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//// SETUP YOUR NEW READ METHODS BELOW (FOLLOW THE PATTERN):                  ////
//// 1. Add an entry to the adios_read.h/ADIOS_READ_METHOD                    ////
//// 2. Update the ADIOS_METHOD_COUNT                                         ////
//// 2. Add a FOWARD_DECLARE line (assuming standard naming)                  ////
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

#define ADIOS_READ_METHOD_COUNT 8

// forward declare the functions (or dummies for internals use)
FORWARD_DECLARE(bp)
FORWARD_DECLARE(bp_staged)
FORWARD_DECLARE(bp_staged1)
#if HAVE_DART
FORWARD_DECLARE(dart)
#endif
#if HAVE_DIMES
FORWARD_DECLARE(dimes)
#endif
#if HAVE_NSSI
FORWARD_DECLARE(nssi)
#endif
#if HAVE_DATATAP
FORWARD_DECLARE(datatap)
#endif
//FORWARD_DECLARE(hdf5)


typedef int  (* ADIOS_INIT_METHOD_FN) (MPI_Comm comm, const char * parameters);
typedef int  (* ADIOS_FINALIZE_METHOD_FN) ();
typedef ADIOS_FILE * (* ADIOS_OPEN_STREAM_FN) (const char * fname, MPI_Comm comm, 
                                 enum ADIOS_LOCKMODE lock_mode, int timeout_msec);
typedef ADIOS_FILE * (* ADIOS_OPEN_FILE_FN) (const char * fname, MPI_Comm comm);
typedef int  (* ADIOS_CLOSE_FN) (const ADIOS_FILE *fp);
typedef int  (* ADIOS_ADVANCE_STEP_FN) (const ADIOS_FILE *fp, int last, int wait_for_step);
typedef void (* ADIOS_RELEASE_STEP_FN) (const ADIOS_FILE *fp);
typedef ADIOS_VARINFO * (* ADIOS_INQ_VAR_FN) (const ADIOS_FILE *fp, const char * varname);
typedef ADIOS_VARINFO * (* ADIOS_INQ_VAR_BYID_FN) (const ADIOS_FILE *fp, int varid);
typedef int  (* ADIOS_INQ_VAR_STAT_FN) (const ADIOS_FILE *fp, const ADIOS_VARINFO *varinfo,
                                 int per_step_stat, int per_block_stat);
typedef int  (* ADIOS_INQ_VAR_BLOCKINFO_FN) (const ADIOS_FILE *fp, const ADIOS_VARINFO *varinfo);
typedef int  (* ADIOS_SCHEDULE_READ_FN) (const ADIOS_FILE * fp, const ADIOS_SELECTION * sel, 
                                 const char * varname, int from_steps, int nsteps, void * data);
typedef int  (* ADIOS_SCHEDULE_READ_BYID_FN) (const ADIOS_FILE * fp, const ADIOS_SELECTION * sel, 
                                 int varid, int from_steps, int nsteps, void * data);
typedef int  (* ADIOS_PERFORM_READS_FN) (const ADIOS_FILE *fp, int blocking);
typedef int  (* ADIOS_CHECK_READS_FN) (const ADIOS_FILE * fp, ADIOS_VARCHUNK ** chunk);
typedef int  (* ADIOS_GET_ATTR_FN) (const ADIOS_FILE * fp, const char * attrname, 
                                 enum ADIOS_DATATYPES * type, int * size, void ** data);
typedef int  (* ADIOS_GET_ATTR_BYID_FN) (const ADIOS_FILE * fp, int attrid, 
                                 enum ADIOS_DATATYPES * type, int * size, void ** data);
typedef void (* ADIOS_RESET_DIMENSION_ORDER_FN) (const ADIOS_FILE *fp, int is_fortran);

struct adios_read_hooks_struct
{
    ADIOS_INIT_METHOD_FN            adios_init_method_fn;
    ADIOS_FINALIZE_METHOD_FN        adios_finalize_method_fn;
    ADIOS_OPEN_STREAM_FN            adios_open_stream_fn;
    ADIOS_OPEN_FILE_FN              adios_open_file_fn;
    ADIOS_CLOSE_FN                  adios_close_fn;
    ADIOS_ADVANCE_STEP_FN           adios_advance_step_fn;
    ADIOS_RELEASE_STEP_FN           adios_release_step_fn;
    ADIOS_INQ_VAR_FN                adios_inq_var_fn;
    ADIOS_INQ_VAR_BYID_FN           adios_inq_var_byid_fn;
    ADIOS_INQ_VAR_STAT_FN           adios_inq_var_stat_fn;
    ADIOS_INQ_VAR_BLOCKINFO_FN      adios_inq_var_blockinfo_fn;
    ADIOS_SCHEDULE_READ_FN          adios_schedule_read_fn;
    ADIOS_SCHEDULE_READ_BYID_FN     adios_schedule_read_byid_fn;
    ADIOS_PERFORM_READS_FN          adios_perform_reads_fn;
    ADIOS_CHECK_READS_FN            adios_check_reads_fn;
    ADIOS_GET_ATTR_FN               adios_get_attr_fn;
    ADIOS_GET_ATTR_BYID_FN          adios_get_attr_byid_fn;
    ADIOS_RESET_DIMENSION_ORDER_FN  adios_reset_dimension_order_fn;
};

#undef FORWARD_DECLARE
#endif