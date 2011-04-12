/*
 * These functions are the C declaration of the Fortran interface for
 * parallel I/O.  Because MPI handles must be translated when passed
 * from Fortran to C and vice versa, they cannot be simply handled
 * with make_fint.
 */
  /* deike */
/* parallel io types, A: asynchronous, B: blocking */

#define  PIO_NONE                           0
#define  PIO_MPI_NONB                       1
#define  PIO_POSIX_NONB                     2
#define  PIO_POSIX_ASYNCH                   3
#define  PIO_POSIX_FPGUARD_SENDRECV         4
#define  PIO_POSIX_FPGUARD_THREAD_REFUSE    5
#define  PIO_POSIX_FPGUARD_THREAD           6

#ifdef USE_MPI
int     pioInit ( int, int, int*, int*, int* );
void    pioFinalize ( void );
#endif
