/*
 * These functions are the C declaration of the Fortran interface for
 * parallel I/O.  Because MPI handles must be translated when passed
 * from Fortran to C and vice versa, they cannot be simply handled
 * with make_fint.
 */
  /* deike */
/* parallel io types, A: asynchronous, B: blocking */

#ifdef USE_MPI
int     pioInit ( int, int, int*, int*, int* );
void    pioFinalize ( void );
#endif
