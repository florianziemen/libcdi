      INTEGER    PIO_NONE              
      PARAMETER (PIO_NONE               =  0)
      INTEGER    PIO_MPI_NONB          
      PARAMETER (PIO_MPI_NONB           =  1)
      INTEGER    PIO_POSIX_NONB        
      PARAMETER (PIO_POSIX_NONB         =  2)
      INTEGER    PIO_POSIX_ASYNCH      
      PARAMETER (PIO_POSIX_ASYNCH       =  3)
      INTEGER    PIO_POSIX_FPGUARD_SENDRECV
      PARAMETER (PIO_POSIX_FPGUARD_SENDRECV =  4)
      INTEGER    PIO_POSIX_FPGUARD_THREAD_REFUSE
      PARAMETER (PIO_POSIX_FPGUARD_THREAD_REFUSE =  5)
      INTEGER    PIO_POSIX_FPGUARD_THREAD
      PARAMETER (PIO_POSIX_FPGUARD_THREAD =  6)
      INTEGER         pioInit
!                                    (INTEGER         ,
!                                     INTEGER         ,
!                                     INTEGER         ,
!                                     INTEGER         ,
!                                     INTEGER         )
      EXTERNAL        pioInit

!                     pioFinalize
      EXTERNAL        pioFinalize
