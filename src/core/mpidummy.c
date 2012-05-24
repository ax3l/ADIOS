/* 
 * ADIOS is freely available under the terms of the BSD license described
 * in the COPYING file in the top level directory of this source distribution.
 *
 * Copyright (c) 2008 - 2009.  UT-BATTELLE, LLC. All rights reserved.
 */

/*
   A dummy MPI implementation for the BP READ API, to have an MPI-free version of the API
*/

#include <stdint.h>
#include <stdio.h>
#include <string.h>
//#define _LARGEFILE64_SOURCE
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#include "public/mpidummy.h"

#if defined(__APPLE__) || defined(__WIN32__) 
#    define lseek64 lseek
#    define open64  open
#endif

static char mpierrmsg[MPI_MAX_ERROR_STRING];

int MPI_Init(int *argc, char ***argv) 
{ 
    mpierrmsg[0] = '\0'; 
    return MPI_SUCCESS; 
}

int MPI_Finalize() 
{ 
    mpierrmsg[0] = '\0'; 
    return MPI_SUCCESS; 
}

int MPI_Comm_split ( MPI_Comm comm, int color, int key, MPI_Comm *comm_out ) {return MPI_SUCCESS;}

int MPI_Barrier(MPI_Comm comm) { return MPI_SUCCESS; }
int MPI_Bcast(void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm) { return MPI_SUCCESS; }

int MPI_Comm_dup(MPI_Comm comm, MPI_Comm *newcomm) { *newcomm = comm; return MPI_SUCCESS; }
int MPI_Comm_rank(MPI_Comm comm, int *rank) { *rank = 0; return MPI_SUCCESS; }
int MPI_Comm_size(MPI_Comm comm, int *size) { *size = 1; return MPI_SUCCESS; }
MPI_Comm MPI_Comm_f2c(MPI_Fint comm) { return comm; }


int MPI_File_open(MPI_Comm comm, char *filename, int amode, MPI_Info info, MPI_File *fh) 
{
    *fh = open64 (filename, amode);
    if (*fh == -1) {
        snprintf(mpierrmsg, MPI_MAX_ERROR_STRING, "File not found: %s", filename);
        return -1;
    }
    return MPI_SUCCESS;
}

int MPI_File_close(MPI_File *fh) { return close(*fh); }

int MPI_File_get_size(MPI_File fh, MPI_Offset *size) {
    uint64_t curpos = lseek64(fh, 0, SEEK_CUR); // get the current seek pos
    uint64_t endpos = lseek64(fh, 0, SEEK_END); // go to end, returned is the size in bytes
    lseek64(fh, curpos, SEEK_SET);             // go back where we were
    *size = (MPI_Offset) endpos;
    //printf("MPI_File_get_size: fh=%d, size=%lld\n", fh, *size);
    return MPI_SUCCESS;
}

int MPI_File_read(MPI_File fh, void *buf, int count, MPI_Datatype datatype, MPI_Status *status)
{
    // FIXME: int count can read only 2GB (*datatype size) array at max
    uint64_t bytes_to_read = count * datatype;  // datatype should hold the size of the type, not an id
    uint64_t bytes_read;
    bytes_read = read (fh, buf, bytes_to_read);
    if (bytes_read != bytes_to_read) {
        snprintf(mpierrmsg, MPI_MAX_ERROR_STRING, "could not read %llu bytes. read only: %llu\n", bytes_to_read, bytes_read);
        return -2;
    }
    *status = bytes_read;
    //printf("MPI_File_read: fh=%d, count=%d, typesize=%d, bytes read=%lld\n", fh, count, datatype, *status);
    return MPI_SUCCESS;
}

int MPI_File_seek(MPI_File fh, MPI_Offset offset, int whence)
{
    uint64_t off = (uint64_t) offset;
    lseek64 (fh, off, whence);
    //printf("MPI_File_seek: fh=%d, offset=%lld, whence=%d\n", fh, off, whence);
    return MPI_SUCCESS;
}

int MPI_Get_count(MPI_Status *status, MPI_Datatype datatype, int *count) 
{ 
    *count = (int) *status;
    return MPI_SUCCESS;
}

int MPI_Error_string(int errorcode, char *string, int *resultlen)
{
    //sprintf(string, "Dummy lib does not know error strings. Code=%d\n",errorcode); 
    strcpy(string, mpierrmsg);
    *resultlen = strlen(string);
    return MPI_SUCCESS;
}

double MPI_Wtime()
{
	// Implementation not tested
	struct timeval tv;
	gettimeofday (&tv, NULL);
	return (double)(tv.tv_sec) + (double)(tv.tv_usec) / 1000000;	
}
