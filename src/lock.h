#define LOCK(lock) \
        { \
           if ( lock ) \
              fprintf(stderr, "lock(%s:%p) already locked\n", __FILE__, __LINE__); \
           lock = 1; \
	}

#define UNLOCK(lock) \
        { \
           if ( ! lock ) \
              fprintf(stderr, "unlock(%s:%p) not locked\n", __FILE__, __LINE__); \
	   while ( lock == 1 ) sleep(1); \
           lock = 0; \
	}
