/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 *
 *  (C) 2012 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <mpi.h>

/* MPI-3 is not yet standardized -- allow MPI-3 routines to be switched off.
 */

#if !defined(USE_STRICT_MPI) && defined(MPICH2)
#  define TEST_MPI3_ROUTINES 1
#endif

#define ELEM_PER_PROC 10000

const int verbose = 0;

int main(int argc, char **argv) {
    int      i, j, rank, nproc;
    int      shm_rank, shm_nproc;
    MPI_Aint size;
    int      errors = 0, all_errors = 0;
    int     *base, *my_base;
    MPI_Win  shm_win;
    MPI_Comm shm_comm;

    MPI_Init(&argc, &argv);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nproc);

#ifdef TEST_MPI3_ROUTINES

    MPIX_Comm_split_type(MPI_COMM_WORLD, MPIX_COMM_TYPE_SHARED, rank, MPI_INFO_NULL, &shm_comm);

    MPI_Comm_rank(shm_comm, &shm_rank);
    MPI_Comm_size(shm_comm, &shm_nproc);

    /* Allocate ELEM_PER_PROC integers for each process */
    MPIX_Win_allocate_shared(sizeof(int)*ELEM_PER_PROC, MPI_INFO_NULL, 
                             shm_comm, &my_base, &shm_win);

    /* Locate absolute base */
    MPIX_Win_shared_query(shm_win, MPI_PROC_NULL, &size, &base); 

    if (verbose) printf("%d -- size = %d baseptr = %p my_baseptr = %p\n", shm_rank, 
                        (int) size, (void*) base, (void*) my_base);

    assert(size == ELEM_PER_PROC * sizeof(int));
    if (shm_rank == 0)
        assert(base == my_base);
    else
        assert(base != my_base);

    MPIX_Win_lock_all(MPI_MODE_NOCHECK, shm_win);

    /* Write to all my data */
    for (i = 0; i < ELEM_PER_PROC; i++) {
        my_base[i] = i;
    }

    MPIX_Win_sync(shm_win);
    MPI_Barrier(shm_comm);
    MPIX_Win_sync(shm_win);

    /* Read and verify everyone's data */
    for (i = 0; i < shm_nproc; i++) {
        for (j = 0; j < ELEM_PER_PROC; j++) {
            if ( base[i*ELEM_PER_PROC + j] != j ) {
                errors++;
                printf("%d -- Got %d at rMPI_COMM_WORLoank %d index %d, expected %d\n", shm_rank, 
                       base[i*ELEM_PER_PROC + j], i, j, j);
            }
        }
    }

    MPIX_Win_unlock_all(shm_win);
    MPI_Win_free(&shm_win);
    MPI_Comm_free(&shm_comm);

#endif /* TEST_MPI3_ROUTINES */

    MPI_Reduce(&errors, &all_errors, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0 && all_errors == 0)
        printf(" No Errors\n");

    MPI_Finalize();

    return 0;
}
