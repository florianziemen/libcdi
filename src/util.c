#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <float.h>
#include <sys/types.h>

#include "stream_int.h"
#include "dmemory.h"
#include "binary.h"
#include "util.h"


#undef  IsBigendian
#define IsBigendian()  ( u_byteorder.c[sizeof(long) - 1] )

void cdiPrintDatatypes(void)
{
  /* IsBigendian returns 1 for big endian byte order */
  static union {unsigned long l; unsigned char c[sizeof(long)];} u_byteorder = {1};

  fprintf (stderr, "+-------------+-------+\n"); 
  fprintf (stderr, "| types       | bytes |\n"); 
  fprintf (stderr, "+-------------+-------+\n"); 
  fprintf (stderr, "| void *      |   %3d |\n", (int) sizeof(void *));
  fprintf (stderr, "+-------------+-------+\n"); 
  fprintf (stderr, "| char        |   %3d |\n", (int) sizeof(char));
  fprintf (stderr, "+-------------+-------+\n"); 
  fprintf (stderr, "| short       |   %3d |\n", (int) sizeof(short));
  fprintf (stderr, "| int         |   %3d |\n", (int) sizeof(int));
  fprintf (stderr, "| long        |   %3d |\n", (int) sizeof(long));
  fprintf (stderr, "| long long   |   %3d |\n", (int) sizeof(long long));
  fprintf (stderr, "| size_t      |   %3d |\n", (int) sizeof(size_t));
  fprintf (stderr, "| off_t       |   %3d |\n", (int) sizeof(off_t));
  fprintf (stderr, "+-------------+-------+\n"); 
  fprintf (stderr, "| float       |   %3d |\n", (int) sizeof(float));
  fprintf (stderr, "| double      |   %3d |\n", (int) sizeof(double));
  fprintf (stderr, "| long double |   %3d |\n", (int) sizeof(long double));
  fprintf (stderr, "+-------------+-------+\n\n"); 
#define XSTRING(x)	#x
#define STRING(x)	XSTRING(x)
  fprintf (stderr, "+-------------+-----------+\n"); 
  fprintf (stderr, "| INT32       | %-9s |\n", STRING(INT32));
  fprintf (stderr, "| INT64       | %-9s |\n", STRING(INT64));
  fprintf (stderr, "| REAL4       | %-9s |\n", STRING(REAL4));
  fprintf (stderr, "| REAL8       | %-9s |\n", STRING(REAL8));
  fprintf (stderr, "+-------------+-----------+\n");

  if ( IsBigendian() )
    fprintf (stderr, "\n  byte ordering is BIGENDIAN\n\n");
  else
    fprintf (stderr, "\n  byte ordering is LITTLEENDIAN\n\n");
}


void gaussaw(double pa[], double pw[], int nlat)
{
  /*
   * Compute Gaussian latitudes.  On return pa contains the
   * sine of the latitudes starting closest to the north pole and going
   * toward the south
   *
   */

  const int itemax = 20;

  int isym, iter, ins2, jn, j;
  double za, zw, zan;
  double z, zk, zkm1, zkm2, zx, zxn, zldn, zmod;

  /*
   * Perform the Newton loop
   * Find 0 of Legendre polynomial with Newton loop
   */

  ins2 = nlat/2 + nlat%2;

  for ( j = 0; j < ins2; j++ )
    {
      z = (double) (4*(j+1)-1)*M_PI / (double) (4*nlat+2);
      pa[j] = cos(z + 1.0/(tan(z)*(double)(8*nlat*nlat)));
    }

  for ( j = 0; j < ins2; j++ )
    {

      za = pa[j];

      iter = 0;
      do
	{
	  iter++;
	  zk = 0.0;

	  /* Newton iteration step */

	  zkm2 = 1.0;
	  zkm1 = za;
	  zx = za;
	  for ( jn = 2; jn <= nlat; jn++ )
	    {
	      zk = ((double) (2*jn-1)*zx*zkm1-(double)(jn-1)*zkm2) / (double)(jn);
	      zkm2 = zkm1;
	      zkm1 = zk;
	    }
	  zkm1 = zkm2;
	  zldn = ((double) (nlat)*(zkm1-zx*zk)) / (1.-zx*zx);
	  zmod = -zk/zldn;
	  zxn = zx+zmod;
	  zan = zxn;

	  /* computes weight */

	  zkm2 = 1.0;
	  zkm1 = zxn;
	  zx = zxn;
	  for ( jn = 2; jn <= nlat; jn++ )
	    {
	      zk = ((double) (2*jn-1)*zx*zkm1-(double)(jn-1)*zkm2) / (double) (jn);
	      zkm2 = zkm1;
	      zkm1 = zk;
	    }
	  zkm1 = zkm2;
	  zw = (1.0-zx*zx) / ((double) (nlat*nlat)*zkm1*zkm1);
	  za = zan;
	}
      while ( iter <= itemax && fabs(zmod) >= DBL_EPSILON );

      pa[j] = zan;
      pw[j] = 2.0*zw;
    }

#if defined (SX)
#pragma vdir nodep
#endif
  for (j = 0; j < nlat/2; j++)
    {
      isym = nlat-(j+1);
      pa[isym] = -pa[j];
      pw[isym] =  pw[j];
    }

  return;
}


void globlons(int nlon, double lons[])
{
  int i;
  double dlon;

  dlon = 360. / nlon;
  lons [0] = 0.0;
  for (i = 1; i < nlon; i++)
    {
      lons[i] = lons[i-1] + dlon;
    }
}


void gausslats(int nlat, double lats[])
{
  static char func[] = "gausslats";
  int i;
  double *tmp;

  tmp = (double *) malloc(sizeof (double) * nlat);
  gaussaw (lats, tmp, nlat);
  free (tmp);

  for (i = 0; i < nlat; i++)
    {
      lats[i] = 180.*asin(lats[i])/M_PI;
    }
}
