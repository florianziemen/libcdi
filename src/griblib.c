
/* Generated automatically from m214003 on Wed Jul 23 15:50:19 CEST 2008 */

/* GRIBLIB_VERSION="1.2.0" */

#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/types.h>

#include "file.h"
#include "dmemory.h"
#include "dtypes.h"
#include "timebase.h"

#ifndef _GRIB_INT_H
#define _GRIB_INT_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>


#if ! defined   (_GRIB_H)
#  include "grib.h"
#endif
#if ! defined   (_ERROR_H)
#  include "error.h"
#endif
#if ! defined   (_DTYPES_H)
#  include "dtypes.h"
#endif

#if ! defined   (FALSE)
#  define  FALSE  0
#endif

#if ! defined   (TRUE)
#  define  TRUE  1
#endif

#if ! defined   (UCHAR)
#  define  UCHAR  unsigned char
#endif

#if  defined  (INT32)
#  define  GRIBPACK     unsigned INT32
#  define  PACK_GRIB    packInt32
#  define  UNPACK_GRIB  unpackInt32
#else
#  define  GRIBPACK     unsigned INT64
#  define  PACK_GRIB    packInt64
#  define  UNPACK_GRIB  unpackInt64
#endif


#ifndef DBL_IS_EQUAL
/*
#define DBL_IS_EQUAL(x,y) (fabs(x - y) <= 2.0*(y*DBL_EPSILON + DBL_MIN))
*/
#define DBL_IS_EQUAL(x,y) (!(fabs(x - y) > 0))
#endif


#define POW_2_M24  0.000000059604644775390625  /*  pow(2.0, -24.0) */

double intpow2(int x);


extern FILE *grprsm;

extern int  GRB_Debug;

void   gprintf(const char *caller, const char *fmt, ...);

void   grsdef(void);

void   prtbin(int kin, int knbit, int *kout, int *kerr);
void   confp3(double pval, int *kexp, int *kmant, int kbits, int kround);
double decfp2(int kexp, int kmant);
void   ref2ibm(double *pref, int kbits);

void   scaleComplex(double *fpdata, int pcStart, int pcScale, int truncation);
void   scatterComplex(double *fpdata, int pcStart, int truncation, int dimSP);
void   scm0(double *pdl, double *pdr, double *pfl, double *pfr, int klg);
int    rowina2(double *p, int ko, int ki, double *pw,
	       int kcode, double msval, int *kret);
int    rowina3(double *p, int ko, int ki, double *pw,
	       int kcode, double msval, int *kret, int omisng, int operio, int oveggy);
int    qu2reg2(double *pfield, int *kpoint, int klat, int klon,
	       double *ztemp, double msval, int *kret);
int    qu2reg3(double *pfield, int *kpoint, int klat, int klon,
	       double msval, int *kret, int omisng, int operio, int oveggy);

#if  defined  (INT32)
long   packInt32(unsigned INT32 *up, char *cp, long bc, long tc);
#endif
long   packInt64(unsigned INT64 *up, char *cp, long bc, long tc);
#if  defined  (INT32)
long   unpackInt32(unsigned char *cp, unsigned INT32 *up, long bc, long tc);
#endif
long   unpackInt64(unsigned char *cp, unsigned INT64 *up, long bc, long tc);

void  gribEncode(int *isec0, int *isec1, int *isec2, double *fsec2, int *isec3,
		 double *fsec3, int *isec4, double *fsec4, int klenp, int *kgrib,
		 int kleng, int *kword, int efunc, int *kret);

void  gribDecode(int *isec0, int *isec1, int *isec2, double *fsec2, int *isec3,
		 double *fsec3, int *isec4, double *fsec4, int klenp, int *kgrib,
		 int kleng, int *kword, int dfunc, int *kret);

#endif  /* _GRIB_INT_H */
#ifndef _GRIBDECODE_H
#define _GRIBDECODE_H

#define  UNDEFINED          9.999e20


#define  GET_INT3(a,b,c)    ((1-(int) ((unsigned) (a & 128) >> 6)) * (int) (((a & 127) << 16)+(b<<8)+c))
#define  GET_INT2(a,b)      ((1-(int) ((unsigned) (a & 128) >> 6)) * (int) (((a & 127) << 8) + b))
#define  GET_INT1(a)        ((1-(int) ((unsigned) (a & 128) >> 6)) * (int) (a&127))

/* this requires a 32-bit default integer machine */
#define  GET_UINT4(a,b,c,d) ((int) ((a << 24) + (b << 16) + (c << 8) + (d)))
#define  GET_UINT3(a,b,c)   ((int) ((a << 16) + (b << 8)  + (c)))
#define  GET_UINT2(a,b)     ((int) ((a << 8)  + (b)))
#define  GET_UINT1(a)       ((int)  (a))


/* Section 0: Indicator Section (IS) */

#define  IS_GRIB_Len         GET_UINT3(is[ 4], is[ 5], is[ 6])
#define  IS_GRIB_Version     GET_UINT1(is[ 7])

/* Section 1: Product Definition Section (PDS) */

#define  PDS_Len             GET_UINT3(pds[ 0], pds[ 1], pds[ 2])
#define  PDS_CodeTable       GET_UINT1(pds[ 3])
#define  PDS_CenterID        GET_UINT1(pds[ 4])
#define  PDS_ModelID         GET_UINT1(pds[ 5])
#define  PDS_GridDefinition  GET_UINT1(pds[ 6])
#define  PDS_Sec2Or3Flag     GET_UINT1(pds[ 7])
#define  PDS_HAS_GDS         ((pds[7] & 128) != 0)
#define  PDS_HAS_BMS         ((pds[7] &  64) != 0)
#define  PDS_Parameter       GET_UINT1(pds[ 8])
#define  PDS_LevelType       GET_UINT1(pds[ 9])
#define  PDS_Level1          (pds[10])
#define  PDS_Level2	     (pds[11])
#define  PDS_Level	     GET_UINT2(pds[10], pds[11])
#define  PDS_Year            GET_INT1(pds[12])
#define  PDS_Month           GET_UINT1(pds[13])
#define  PDS_Day             GET_UINT1(pds[14])
#define  PDS_Hour            GET_UINT1(pds[15])
#define  PDS_Minute          GET_UINT1(pds[16])
#define  PDS_Date            (PDS_Year*10000+PDS_Month*100+PDS_Day)
#define  PDS_Time            (PDS_Hour*100+PDS_Minute)
#define  PDS_TimeUnit        GET_UINT1(pds[17])
#define  PDS_TimePeriod1     GET_UINT1(pds[18])
#define  PDS_TimePeriod2     GET_UINT1(pds[19])
#define  PDS_TimeRange       GET_UINT1(pds[20])
#define  PDS_AvgNum          GET_UINT2(pds[21], pds[22])
#define  PDS_AvgMiss         GET_UINT1(pds[23])
#define  PDS_Century         GET_UINT1(pds[24])
#define  PDS_Subcenter       GET_UINT1(pds[25])
#define  PDS_DecimalScale    GET_INT2(pds[26],pds[27])


/* Section 2: Grid Description Section (GDS) */

#define  GDS_Len             ((gds) == NULL ? 0 : GET_UINT3(gds[ 0], gds[ 1], gds[ 2]))
#define  GDS_NV              GET_UINT1(gds[ 3])
#define  GDS_PVPL            GET_UINT1(gds[ 4])
#define  GDS_PV	             ((gds[3] == 0) ? -1 : (int) gds[4] - 1)
#define  GDS_PL	             ((gds[4] == 255) ? -1 : (int) gds[3] * 4 + (int) gds[4] - 1)
#define  GDS_GridType        GET_UINT1(gds[ 5])


/* Triangular grid of DWD */
#define  GDS_GME_NI2         GET_UINT2(gds[ 6], gds[ 7])
#define  GDS_GME_NI3         GET_UINT2(gds[ 8], gds[ 9])
#define  GDS_GME_ND          GET_UINT3(gds[10], gds[11], gds[12])
#define  GDS_GME_NI          GET_UINT3(gds[13], gds[14], gds[15])
#define  GDS_GME_AFlag       GET_UINT1(gds[16])
#define  GDS_GME_LatPP       GET_INT3(gds[17], gds[18], gds[19])
#define  GDS_GME_LonPP       GET_INT3(gds[20], gds[21], gds[22])
#define  GDS_GME_LonMPL      GET_INT3(gds[23], gds[24], gds[25])
#define  GDS_GME_BFlag       GET_UINT1(gds[27])

/* Spectral */
#define  GDS_PentaJ          GET_UINT2(gds[ 6], gds[ 7])
#define  GDS_PentaK          GET_UINT2(gds[ 8], gds[ 9])
#define  GDS_PentaM          GET_UINT2(gds[10], gds[11])
#define  GDS_RepType         GET_UINT1(gds[12])
#define  GDS_RepMode         GET_UINT1(gds[13])

/* Regular grid */
#define  GDS_NumLon          GET_UINT2(gds[ 6], gds[ 7])
#define  GDS_NumLat          GET_UINT2(gds[ 8], gds[ 9])
#define  GDS_FirstLat        GET_INT3(gds[10], gds[11], gds[12])
#define  GDS_FirstLon        GET_INT3(gds[13], gds[14], gds[15])
#define  GDS_ResFlag         GET_UINT1(gds[16])
#define  GDS_LastLat         GET_INT3(gds[17], gds[18], gds[19])
#define  GDS_LastLon         GET_INT3(gds[20], gds[21], gds[22])
#define  GDS_LonIncr         GET_UINT2(gds[23], gds[24])
#define  GDS_LatIncr         GET_UINT2(gds[25], gds[26])
#define  GDS_NumPar          GET_UINT2(gds[25], gds[26])
#define  GDS_ScanFlag        GET_UINT1(gds[27])
#define  GDS_LatSP           GET_INT3(gds[32], gds[33], gds[34])
#define  GDS_LonSP           GET_INT3(gds[35], gds[36], gds[37])
#define  GDS_RotAngle        GET_Real(&(gds[38]))

/* Lambert */
#define  GDS_LoV             GET_INT3(gds[17], gds[18], gds[19])
#define  GDS_Xinc            GET_INT3(gds[20], gds[21], gds[22])
#define  GDS_Yinc            GET_INT3(gds[23], gds[24], gds[25])
#define  GDS_LatS1           GET_INT3(gds[28], gds[29], gds[30])
#define  GDS_LatS2           GET_INT3(gds[31], gds[32], gds[33])

/* Section 3: Bit Map Section (BMS) */

#define  BMS_Len	     ((bms) == NULL ? 0 : (int) (bms[0]<<16)+(bms[1]<<8)+bms[2])
#define  BMS_UnusedBits      (bms[3])
#define  BMS_Numeric         
#define  BMS_Bitmap	     ((bms) == NULL ? NULL : (bms)+6)
#define  BMS_BitmapSize      (((((bms[0]<<16)+(bms[1]<<8)+bms[2]) - 6)<<3) - bms[3])

/* Section 4: Binary Data Section (BDS) */

#define  BDS_Len	    ((int) ((bds[0]<<16)+(bds[1]<<8)+bds[2]))
#define  BDS_Flag	    (bds[3])
#define  BDS_BinScale       GET_INT2(bds[ 4], bds[ 5])
#define  BDS_RefValue       decfp2((int)bds[ 6], GET_UINT3(bds[ 7], bds[ 8], bds[ 9]))
#define  BDS_NumBits        ((int) bds[10])
#define  BDS_RealCoef       decfp2((int)bds[zoff+11], GET_UINT3(bds[zoff+12], bds[zoff+13], bds[zoff+14]))
#define  BDS_Power          GET_INT2(bds[13], bds[14])
#define  BDS_Z              (bds[13])

/* Section 5: End Section (ES) */

#endif  /* _GRIBDECODE_H */
#ifndef _GRIBENCODE_H
#define _GRIBENCODE_H


#define PutnZero(n) \
{ \
  int i; \
  for ( i = z; i < z+n; i++ ) lGrib[i] = 0; \
  z += n; \
}

#define Put1Byte(Value)  (lGrib[z++] = (Value))
#define Put2Byte(Value) ((lGrib[z++] = (Value) >>  8), \
                         (lGrib[z++] = (Value)))
#define Put3Byte(Value) ((lGrib[z++] = (Value) >> 16), \
                         (lGrib[z++] = (Value) >>  8), \
                         (lGrib[z++] = (Value)))
#define Put4Byte(Value) ((lGrib[z++] = (Value) >> 24), \
                         (lGrib[z++] = (Value) >> 16), \
                         (lGrib[z++] = (Value) >>  8), \
                         (lGrib[z++] = (Value)))

#define Put1Int(Value)  {ival = Value; if ( ival < 0 ) ival =     128 - ival; Put1Byte(ival);}
#define Put3Int(Value)  {ival = Value; if ( ival < 0 ) ival = 8388608 - ival; Put3Byte(ival);}

#define Put1Real(Value)          \
{                                \
  confp3(Value, &exponent, &mantissa, BitsPerInt, 1); \
  Put1Byte(exponent);            \
  Put3Byte(mantissa);            \
}

#endif  /* _GRIBENCODE_H */


double _pow2tab[158] = {
 /* pow(2.0,  0.0) */  1.0,
 /* pow(2.0,  1.0) */  2.0,
 /* pow(2.0,  2.0) */  4.0,
 /* pow(2.0,  3.0) */  8.0,
 /* pow(2.0,  4.0) */  16.0,
 /* pow(2.0,  5.0) */  32.0,
 /* pow(2.0,  6.0) */  64.0,
 /* pow(2.0,  7.0) */  128.0,
 /* pow(2.0,  8.0) */  256.0,
 /* pow(2.0,  9.0) */  512.0,
 /* pow(2.0, 10.0) */  1024.0,
 /* pow(2.0, 11.0) */  2048.0,
 /* pow(2.0, 12.0) */  4096.0,
 /* pow(2.0, 13.0) */  8192.0,
 /* pow(2.0, 14.0) */  16384.0,
 /* pow(2.0, 15.0) */  32768.0,
 /* pow(2.0, 16.0) */  65536.0,
 /* pow(2.0, 17.0) */  131072.0,
 /* pow(2.0, 18.0) */  262144.0,
 /* pow(2.0, 19.0) */  524288.0,
 /* pow(2.0, 20.0) */  1048576.0,
 /* pow(2.0, 21.0) */  2097152.0,
 /* pow(2.0, 22.0) */  4194304.0,
 /* pow(2.0, 23.0) */  8388608.0,
 /* pow(2.0, 24.0) */  16777216.0,
 /* pow(2.0, 25.0) */  33554432.0,
 /* pow(2.0, 26.0) */  67108864.0,
 /* pow(2.0, 27.0) */  134217728.0,
 /* pow(2.0, 28.0) */  268435456.0,
 /* pow(2.0, 29.0) */  536870912.0,
 /* pow(2.0, 30.0) */  1073741824.0,
 /* pow(2.0, 31.0) */  2147483648.0,
 /* pow(2.0, 32.0) */  4294967296.0,
 /* pow(2.0, 33.0) */  8589934592.0,
 /* pow(2.0, 34.0) */  17179869184.0,
 /* pow(2.0, 35.0) */  34359738368.0,
 /* pow(2.0, 36.0) */  68719476736.0,
 /* pow(2.0, 37.0) */  137438953472.0,
 /* pow(2.0, 38.0) */  274877906944.0,
 /* pow(2.0, 39.0) */  549755813888.0,
 /* pow(2.0, 40.0) */  1099511627776.0,
 /* pow(2.0, 41.0) */  2199023255552.0,
 /* pow(2.0, 42.0) */  4398046511104.0,
 /* pow(2.0, 43.0) */  8796093022208.0,
 /* pow(2.0, 44.0) */  17592186044416.0,
 /* pow(2.0, 45.0) */  35184372088832.0,
 /* pow(2.0, 46.0) */  70368744177664.0,
 /* pow(2.0, 47.0) */  140737488355328.0,
 /* pow(2.0, 48.0) */  281474976710656.0,
 /* pow(2.0, 49.0) */  562949953421312.0,
 /* pow(2.0, 50.0) */  1125899906842624.0,
 /* pow(2.0, 51.0) */  2251799813685248.0,
 /* pow(2.0, 52.0) */  4503599627370496.0,
 /* pow(2.0, 53.0) */  9007199254740992.0,
 /* pow(2.0, 54.0) */  18014398509481984.0,
 /* pow(2.0, 55.0) */  36028797018963968.0,
 /* pow(2.0, 56.0) */  72057594037927936.0,
 /* pow(2.0, 57.0) */  144115188075855872.0,
 /* pow(2.0, 58.0) */  288230376151711744.0,
 /* pow(2.0, 59.0) */  576460752303423488.0,
 /* pow(2.0, 60.0) */  1152921504606846976.0,
 /* pow(2.0, 61.0) */  2305843009213693952.0,
 /* pow(2.0, 62.0) */  4611686018427387904.0,
 /* pow(2.0, 63.0) */  9223372036854775808.0,
 /* pow(2.0, 64.0) */  18446744073709551616.0,
 /* pow(2.0, 65.0) */  36893488147419103232.0,
 /* pow(2.0, 66.0) */  73786976294838206464.0,
 /* pow(2.0, 67.0) */  147573952589676412928.0,
 /* pow(2.0, 68.0) */  295147905179352825856.0,
 /* pow(2.0, 69.0) */  590295810358705651712.0,
 /* pow(2.0, 70.0) */  1180591620717411303424.0,
 /* pow(2.0, 71.0) */  2361183241434822606848.0,
 /* pow(2.0, 72.0) */  4722366482869645213696.0,
 /* pow(2.0, 73.0) */  9444732965739290427392.0,
 /* pow(2.0, 74.0) */  18889465931478580854784.0,
 /* pow(2.0, 75.0) */  37778931862957161709568.0,
 /* pow(2.0, 76.0) */  75557863725914323419136.0,
 /* pow(2.0, 77.0) */  151115727451828646838272.0,
 /* pow(2.0, 78.0) */  302231454903657293676544.0,
 /* pow(2.0, 79.0) */  604462909807314587353088.0,
 /* pow(2.0, 80.0) */  1208925819614629174706176.0,
 /* pow(2.0, 81.0) */  2417851639229258349412352.0,
 /* pow(2.0, 82.0) */  4835703278458516698824704.0,
 /* pow(2.0, 83.0) */  9671406556917033397649408.0,
 /* pow(2.0, 84.0) */  19342813113834066795298816.0,
 /* pow(2.0, 85.0) */  38685626227668133590597632.0,
 /* pow(2.0, 86.0) */  77371252455336267181195264.0,
 /* pow(2.0, 87.0) */  154742504910672534362390528.0,
 /* pow(2.0, 88.0) */  309485009821345068724781056.0,
 /* pow(2.0, 89.0) */  618970019642690137449562112.0,
 /* pow(2.0, 90.0) */  1237940039285380274899124224.0,
 /* pow(2.0, 91.0) */  2475880078570760549798248448.0,
 /* pow(2.0, 92.0) */  4951760157141521099596496896.0,
 /* pow(2.0, 93.0) */  9903520314283042199192993792.0,
 /* pow(2.0, 94.0) */  19807040628566084398385987584.0,
 /* pow(2.0, 95.0) */  39614081257132168796771975168.0,
 /* pow(2.0, 96.0) */  79228162514264337593543950336.0,
 /* pow(2.0, 97.0) */  158456325028528675187087900672.0,
 /* pow(2.0, 98.0) */  316912650057057350374175801344.0,
 /* pow(2.0, 99.0) */  633825300114114700748351602688.0,
 /* pow(2.0, 100.0) */  1267650600228229401496703205376.0,
 /* pow(2.0, 101.0) */  2535301200456458802993406410752.0,
 /* pow(2.0, 102.0) */  5070602400912917605986812821504.0,
 /* pow(2.0, 103.0) */  10141204801825835211973625643008.0,
 /* pow(2.0, 104.0) */  20282409603651670423947251286016.0,
 /* pow(2.0, 105.0) */  40564819207303340847894502572032.0,
 /* pow(2.0, 106.0) */  81129638414606681695789005144064.0,
 /* pow(2.0, 107.0) */  162259276829213363391578010288128.0,
 /* pow(2.0, 108.0) */  324518553658426726783156020576256.0,
 /* pow(2.0, 109.0) */  649037107316853453566312041152512.0,
 /* pow(2.0, 110.0) */  1298074214633706907132624082305024.0,
 /* pow(2.0, 111.0) */  2596148429267413814265248164610048.0,
 /* pow(2.0, 112.0) */  5192296858534827628530496329220096.0,
 /* pow(2.0, 113.0) */  10384593717069655257060992658440192.0,
 /* pow(2.0, 114.0) */  20769187434139310514121985316880384.0,
 /* pow(2.0, 115.0) */  41538374868278621028243970633760768.0,
 /* pow(2.0, 116.0) */  83076749736557242056487941267521536.0,
 /* pow(2.0, 117.0) */  166153499473114484112975882535043072.0,
 /* pow(2.0, 118.0) */  332306998946228968225951765070086144.0,
 /* pow(2.0, 119.0) */  664613997892457936451903530140172288.0,
 /* pow(2.0, 120.0) */  1329227995784915872903807060280344576.0,
 /* pow(2.0, 121.0) */  2658455991569831745807614120560689152.0,
 /* pow(2.0, 122.0) */  5316911983139663491615228241121378304.0,
 /* pow(2.0, 123.0) */  10633823966279326983230456482242756608.0,
 /* pow(2.0, 124.0) */  21267647932558653966460912964485513216.0,
 /* pow(2.0, 125.0) */  42535295865117307932921825928971026432.0,
 /* pow(2.0, 126.0) */  85070591730234615865843651857942052864.0,
 /* pow(2.0, 127.0) */  170141183460469231731687303715884105728.0,
 /* pow(2.0, 128.0) */  340282366920938463463374607431768211456.0,
 /* pow(2.0, 129.0) */  680564733841876926926749214863536422912.0,
 /* pow(2.0, 130.0) */  1361129467683753853853498429727072845824.0,
 /* pow(2.0, 131.0) */  2722258935367507707706996859454145691648.0,
 /* pow(2.0, 132.0) */  5444517870735015415413993718908291383296.0,
 /* pow(2.0, 133.0) */  10889035741470030830827987437816582766592.0,
 /* pow(2.0, 134.0) */  21778071482940061661655974875633165533184.0,
 /* pow(2.0, 135.0) */  43556142965880123323311949751266331066368.0,
 /* pow(2.0, 136.0) */  87112285931760246646623899502532662132736.0,
 /* pow(2.0, 137.0) */  174224571863520493293247799005065324265472.0,
 /* pow(2.0, 138.0) */  348449143727040986586495598010130648530944.0,
 /* pow(2.0, 139.0) */  696898287454081973172991196020261297061888.0,
 /* pow(2.0, 140.0) */  1393796574908163946345982392040522594123776.0,
 /* pow(2.0, 141.0) */  2787593149816327892691964784081045188247552.0,
 /* pow(2.0, 142.0) */  5575186299632655785383929568162090376495104.0,
 /* pow(2.0, 143.0) */  11150372599265311570767859136324180752990208.0,
 /* pow(2.0, 144.0) */  22300745198530623141535718272648361505980416.0,
 /* pow(2.0, 145.0) */  44601490397061246283071436545296723011960832.0,
 /* pow(2.0, 146.0) */  89202980794122492566142873090593446023921664.0,
 /* pow(2.0, 147.0) */  178405961588244985132285746181186892047843328.0,
 /* pow(2.0, 148.0) */  356811923176489970264571492362373784095686656.0,
 /* pow(2.0, 149.0) */  713623846352979940529142984724747568191373312.0,
 /* pow(2.0, 150.0) */  1427247692705959881058285969449495136382746624.0,
 /* pow(2.0, 151.0) */  2854495385411919762116571938898990272765493248.0,
 /* pow(2.0, 152.0) */  5708990770823839524233143877797980545530986496.0,
 /* pow(2.0, 153.0) */  11417981541647679048466287755595961091061972992.0,
 /* pow(2.0, 154.0) */  22835963083295358096932575511191922182123945984.0,
 /* pow(2.0, 155.0) */  45671926166590716193865151022383844364247891968.0,
 /* pow(2.0, 156.0) */  91343852333181432387730302044767688728495783936.0,
 /* pow(2.0, 157.0) */  182687704666362864775460604089535377456991567872.0,
};


double _pow16tab[71] = {
 /* pow(16.0,  0.0) */  1.0,
 /* pow(16.0,  1.0) */  16.0,
 /* pow(16.0,  2.0) */  256.0,
 /* pow(16.0,  3.0) */  4096.0,
 /* pow(16.0,  4.0) */  65536.0,
 /* pow(16.0,  5.0) */  1048576.0,
 /* pow(16.0,  6.0) */  16777216.0,
 /* pow(16.0,  7.0) */  268435456.0,
 /* pow(16.0,  8.0) */  4294967296.0,
 /* pow(16.0,  9.0) */  68719476736.0,
 /* pow(16.0, 10.0) */  1099511627776.0,
 /* pow(16.0, 11.0) */  17592186044416.0,
 /* pow(16.0, 12.0) */  281474976710656.0,
 /* pow(16.0, 13.0) */  4503599627370496.0,
 /* pow(16.0, 14.0) */  72057594037927936.0,
 /* pow(16.0, 15.0) */  1152921504606846976.0,
 /* pow(16.0, 16.0) */  18446744073709551616.0,
 /* pow(16.0, 17.0) */  295147905179352825856.0,
 /* pow(16.0, 18.0) */  4722366482869645213696.0,
 /* pow(16.0, 19.0) */  75557863725914323419136.0,
 /* pow(16.0, 20.0) */  1208925819614629174706176.0,
 /* pow(16.0, 21.0) */  19342813113834066795298816.0,
 /* pow(16.0, 22.0) */  309485009821345068724781056.0,
 /* pow(16.0, 23.0) */  4951760157141521099596496896.0,
 /* pow(16.0, 24.0) */  79228162514264337593543950336.0,
 /* pow(16.0, 25.0) */  1267650600228229401496703205376.0,
 /* pow(16.0, 26.0) */  20282409603651670423947251286016.0,
 /* pow(16.0, 27.0) */  324518553658426726783156020576256.0,
 /* pow(16.0, 28.0) */  5192296858534827628530496329220096.0,
 /* pow(16.0, 29.0) */  83076749736557242056487941267521536.0,
 /* pow(16.0, 30.0) */  1329227995784915872903807060280344576.0,
 /* pow(16.0, 31.0) */  21267647932558653966460912964485513216.0,
 /* pow(16.0, 32.0) */  340282366920938463463374607431768211456.0,
 /* pow(16.0, 33.0) */  5444517870735015415413993718908291383296.0,
 /* pow(16.0, 34.0) */  87112285931760246646623899502532662132736.0,
 /* pow(16.0, 35.0) */  1393796574908163946345982392040522594123776.0,
 /* pow(16.0, 36.0) */  22300745198530623141535718272648361505980416.0,
 /* pow(16.0, 37.0) */  356811923176489970264571492362373784095686656.0,
 /* pow(16.0, 38.0) */  5708990770823839524233143877797980545530986496.0,
 /* pow(16.0, 39.0) */  91343852333181432387730302044767688728495783936.0,
 /* pow(16.0, 40.0) */  1461501637330902918203684832716283019655932542976.0,
 /* pow(16.0, 41.0) */  23384026197294446691258957323460528314494920687616.0,
 /* pow(16.0, 42.0) */  374144419156711147060143317175368453031918731001856.0,
 /* pow(16.0, 43.0) */  5986310706507378352962293074805895248510699696029696.0,
 /* pow(16.0, 44.0) */  95780971304118053647396689196894323976171195136475136.0,
 /* pow(16.0, 45.0) */  1532495540865888858358347027150309183618739122183602176.0,
 /* pow(16.0, 46.0) */  24519928653854221733733552434404946937899825954937634816.0,
 /* pow(16.0, 47.0) */  392318858461667547739736838950479151006397215279002157056.0,
 /* pow(16.0, 48.0) */  6277101735386680763835789423207666416102355444464034512896.0,
 /* pow(16.0, 49.0) */  100433627766186892221372630771322662657637687111424552206336.0,
 /* pow(16.0, 50.0) */  1606938044258990275541962092341162602522202993782792835301376.0,
 /* pow(16.0, 51.0) */  25711008708143844408671393477458601640355247900524685364822016.0,
 /* pow(16.0, 52.0) */  411376139330301510538742295639337626245683966408394965837152256.0,
 /* pow(16.0, 53.0) */  6582018229284824168619876730229402019930943462534319453394436096.0,
 /* pow(16.0, 54.0) */  105312291668557186697918027683670432318895095400549111254310977536.0,
 /* pow(16.0, 55.0) */  1684996666696914987166688442938726917102321526408785780068975640576.0,
 /* pow(16.0, 56.0) */  26959946667150639794667015087019630673637144422540572481103610249216.0,
 /* pow(16.0, 57.0) */  431359146674410236714672241392314090778194310760649159697657763987456.0,
 /* pow(16.0, 58.0) */  6901746346790563787434755862277025452451108972170386555162524223799296.0,
 /* pow(16.0, 59.0) */  110427941548649020598956093796432407239217743554726184882600387580788736.0,
 /* pow(16.0, 60.0) */  1766847064778384329583297500742918515827483896875618958121606201292619776.0,
 /* pow(16.0, 61.0) */  28269553036454149273332760011886696253239742350009903329945699220681916416.0,
 /* pow(16.0, 62.0) */  452312848583266388373324160190187140051835877600158453279131187530910662656.0,
 /* pow(16.0, 63.0) */  7237005577332262213973186563042994240829374041602535252466099000494570602496.0,
 /* pow(16.0, 64.0) */  115792089237316195423570985008687907853269984665640564039457584007913129639936.0,
 /* pow(16.0, 65.0) */  1852673427797059126777135760139006525652319754650249024631321344126610074238976.0,
 /* pow(16.0, 66.0) */  29642774844752946028434172162224104410437116074403984394101141506025761187823616.0,
 /* pow(16.0, 67.0) */  474284397516047136454946754595585670566993857190463750305618264096412179005177856.0,
 /* pow(16.0, 68.0) */  7588550360256754183279148073529370729071901715047420004889892225542594864082845696.0,
 /* pow(16.0, 69.0) */  121416805764108066932466369176469931665150427440758720078238275608681517825325531136.0,
 /* pow(16.0, 70.0) */  1942668892225729070919461906823518906642406839052139521251812409738904285205208498176.0,
};

static int _pow2tab_size = sizeof(_pow2tab)/sizeof(double);

void gen_pow2tab(void)
{
  int jloop;

  for ( jloop = 0; jloop < 158; jloop++ )
    printf(" /* pow(2.0, %2d.0) */  %.1f,\n", jloop,  pow(2.0, (double) jloop));
}


void gen_pow16tab(void)
{
  double pval;
  int iexp;

  for ( iexp = 0; iexp < 71; iexp++ )
    {
      pval = pow(16.0, (double)(iexp));
      printf(" /* pow(16.0, %2d.0) */  %.1f,\n", iexp, pval);
    }
}


double intpow2(int x)
{
  if ( x < _pow2tab_size )
    return (_pow2tab[x]);
  else
    return (pow(2.0, (double) x));
}

#define  NINT(x)  ((x) < 0 ? (int)((x)-.5) : (int)((x)+.5))


void confp3(double pval, int *kexp, int *kmant, int kbits, int kround)
{
  /*

    Purpose:
    --------

    Convert floating point number from machine
    representation to GRIB representation.

    Input Parameters:
    -----------------

       pval    - Floating point number to be converted.
       kbits   - Number of bits in computer word.
       kround  - Conversion type.
                 0 , Closest number in GRIB format less than
                     original number.
                 1 , Closest number in GRIB format to the
                     original number (equal to, greater than or
                     less than original number).

    Output Parameters:
    ------------------

       kexp    - 8 Bit signed exponent.
       kmant   - 24 Bit mantissa.

    Method:
    -------

    Floating point number represented as 8 bit signed
    exponent and 24 bit mantissa in integer values.

    Externals.
    ----------

    decfp2    - Decode from IBM floating point format.

    Reference:
    ----------

    WMO Manual on Codes re GRIB representation.

    Comments:
    ---------

    Routine aborts if an invalid conversion type parameter
    is used or if a 24 bit mantissa is not produced.

    Author:
    -------
     
    John Hennessy   ECMWF   18.06.91

    Modifications:
    --------------

    Uwe Schulzweida   MPIfM   01/04/2001

    Convert to C from EMOS library version 130

    Uwe Schulzweida   MPIfM   02/08/2002

     - speed up by factor 1.6 on NEC SX6
        - replace 1.0 / pow(16.0, (double)(iexp - 70)) by rpow16m70tab[iexp]
  */

  static char func[] = "confp3";
  double zval, rpowref;
  double zref, zeps;
  int iexp, isign;
  int iround;
  extern int GRB_Debug;
  extern double _pow16tab[71];

  /* ----------------------------------------------------------------- */
  /*   Section 1 . Initialise                                          */
  /* ----------------------------------------------------------------- */

  /*  Check conversion type parameter. */

  iround = kround;
  if ( iround != 0 && iround != 1 )
    {
      Error(func, "Invalid conversion type = %d", iround);

      /*  If not aborting, arbitrarily set rounding to 'up'. */
     iround = 1;
    }

  /* ----------------------------------------------------------------- */
  /*   Section 2 . Convert value of zero.                              */
  /* ----------------------------------------------------------------- */

  if ( ! (fabs(pval) > 0))
    {
      *kexp  = 0;
      *kmant = 0;
      iexp   = 0;
      isign  = 0;
      goto LABEL900;
    }

  /* ----------------------------------------------------------------- */
  /*   Section 3 . Convert other values.                               */
  /* ----------------------------------------------------------------- */

  zeps = 1.0e-12;
  if ( kbits == 32 ) zeps = 1.0e-8;
  zref = pval;

  /*  Sign of value. */

  isign = 0;
  if ( zref < 0.0 )
    {
      isign = 128;
      zref  = - zref;
    }

  /*  Exponent. */

  iexp = (int) (log(zref)/log(16.0) + 65.0 + zeps);

  /* only ANSI C99 has log2 */
  /* iexp = (int) (log2(zref) * 0.25 + 65.0 + zeps); */

  if ( iexp < 0   ) iexp = 0;
  if ( iexp > 127 ) iexp = 127;

  /*
  rpowref = zref / pow(16.0, (double)(iexp - 70));
  */

  if ( (iexp - 70) < 0 )
    rpowref = zref * _pow16tab[-(iexp - 70)];
  else
    rpowref = zref / _pow16tab[(iexp - 70)];

  /*  Mantissa. */

  if ( iround == 0 )
    {
      /*  Closest number in GRIB format less than original number. */
      /*  Truncate for positive numbers. */
      /*  Round up for negative numbers. */

      if ( isign == 0 )
	*kmant = (int) rpowref;
      else
	*kmant = NINT(rpowref + 0.5);
    }
  else
    {
      /*  Closest number in GRIB format to the original number   */
      /*  (equal to, greater than or less than original number). */

      *kmant = NINT(rpowref);
    }

  /*  Check that mantissa value does not exceed 24 bits. */
  /*  If it does, adjust the exponent upwards and recalculate */
  /*  the mantissa. */
  /*  16777215 = 2**24 - 1 */

  if ( *kmant > 16777215 )
    {

    LABEL350:

      ++iexp;

      /*  Check for exponent overflow during adjustment  */

      if ( iexp > 127 )
	{
          Message(func, "Exponent overflow");
          Message(func, "Original number = %30.20f", pval);
          Message(func, "Sign = %3d, Exponent = %3d, Mantissa = %12d",
		  isign, iexp, *kmant);

	  Error(func, "Exponent overflow");

	  /*  If not aborting, arbitrarily set value to zero  */

          Message(func, "Value arbitrarily set to zero.");
          *kexp  = 0;
          *kmant = 0;
          iexp  = 0;
          isign = 0;
          goto LABEL900;
	}

      if ( (iexp - 70) < 0 )
	rpowref = zref * _pow16tab[-(iexp - 70)];
      else
	rpowref = zref / _pow16tab[(iexp - 70)];

      if ( iround == 0 )
	{
	  /*  Closest number in GRIB format less than original number. */
	  /*  Truncate for positive numbers. */
	  /*  Round up for negative numbers. */

	  if ( isign == 0 )
	    *kmant = (int) rpowref;
	  else
	    *kmant = NINT(rpowref + 0.5);
	}
      else
	{
	  /*  Closest number in GRIB format to the original number */
	  /*  (equal to, greater or less than original number). */

	  *kmant = NINT(rpowref);
	}

      /*  Repeat calculation (with modified exponent) if still have */
      /*  mantissa overflow. */

      if ( *kmant > 16777215 ) goto LABEL350;
    }

  /*  Add sign bit to exponent. */

  *kexp = iexp + isign;

  /* ----------------------------------------------------------------- */
  /*   Section 9. Return                                               */
  /* ----------------------------------------------------------------- */

LABEL900:

  if ( GRB_Debug )
    {
      Message(func, "Conversion type parameter = %4d", kround);
      Message(func, "Original number = %30.20f", pval);

      zval = decfp2(*kexp, *kmant);

      Message(func, "Converted to      %30.20f", zval);
      Message(func, "Sign = %3d, Exponent = %3d, Mantissa = %12d",
	      isign, iexp, *kmant);
    }

  return;
} /* confp3 */


double decfp2(int kexp, int kmant)
{
  /*

    Purpose:
    --------

    Convert GRIB representation of a floating point
    number to machine representation.

    Input Parameters:
    -----------------

    kexp    - 8 Bit signed exponent.
    kmant   - 24 Bit mantissa.

    Output Parameters:
    ------------------

    Return value   - Floating point number represented
                     by kexp and kmant.

    Method:
    -------

    Floating point number represented as 8 bit exponent
    and 24 bit mantissa in integer values converted to
    machine floating point format.

    Externals:
    ----------

    None.

    Reference:
    ----------

    WMO Manual on Codes re GRIB representation.

    Comments:
    ---------

    Rewritten from DECFP, to conform to programming standards.
    Sign bit on 0 value now ignored, if present.
    If using 32 bit reals, check power of 16 is not so small as to
    cause overflows (underflows!); this causes warning to be given
    on Fujitsus.

    Author:
    -------

    John Hennessy   ECMWF   18.06.91

    Modifications:
    --------------

    Uwe Schulzweida   MPIfM   01/04/2001

     - Convert to C from EMOS library version 130
     
    Uwe Schulzweida   MPIfM   02/08/2002

     - speed up by factor 2 on NEC SX6
        - replace pow(2.0, -24.0) by constant POW_2_M24
        - replace pow(16.0, (double)(iexp - 64)) by pow16m64tab[iexp]
  */

  static char func[] = "decfp2";
  double pval;
  int iexp, isign;
  extern int GRB_Debug;
  extern double _pow16tab[71];
  
  /* ----------------------------------------------------------------- */
  /*   Section 1 . Convert value of 0.0. Ignore sign bit.              */
  /* ----------------------------------------------------------------- */

  if ( GRB_Debug ) Message(func, "KEXP = %d  KMANT = %d", kexp, kmant);
  /*
  if ( (kexp == 128 || kexp == 0) && kmant == 0 )
  */
  if ( (kexp == 128) || (kexp == 0) || (kexp == 255) )
    {
      pval = 0.0;
      goto LABEL900;
    }

  /* ----------------------------------------------------------------- */
  /*   Section 2 . Convert other values.                               */
  /* ----------------------------------------------------------------- */

  /*  Sign of value. */

  iexp  = kexp;
  isign = 1;

  if ( iexp >= 128 )
    {
      iexp -= 128;
      isign = -1;
    }

  /*  Decode value. */

  /* pval = isign * pow(2.0, -24.0) * kmant * pow(16.0, (double)(iexp - 64)); */

  iexp -= 64;

  if ( iexp < 0 )
    pval = 1./_pow16tab[-iexp];
  else
    pval = _pow16tab[iexp];

  pval *= isign * POW_2_M24 * kmant;

  /* ----------------------------------------------------------------- */
  /*   Section 9. Return to calling routine.                           */
  /* ----------------------------------------------------------------- */

LABEL900:

  if ( GRB_Debug ) Message(func, "Returned value = %f", pval);

  return (pval);
} /* decfp2 */




int gribRefDate(int *isec1)
{
  int date, ryear, rmonth, rday;
  int century;

  century = ISEC1_Century;
  if ( century < 0 ) century = -century;
  century -= 1;

  ryear   = ISEC1_Year;

  /* if ( century != 0 ) */
    {
      if ( ryear == 100 )
	{
	  ryear = 0;
	  century += 1;
	}

      if ( ryear != 255 )
	{
	  ryear = century*100 + ryear;
	  if ( ISEC1_Century < 0 ) ryear = -ryear;
	}
      else
	ryear = 1;
    }

  rmonth  = ISEC1_Month;
  rday    = ISEC1_Day;

  date = encode_date(ryear, rmonth, rday);

  return (date) ;
}


int gribRefTime(int *isec1)
{
  int time, rhour, rminute;

  rhour   = ISEC1_Hour;
  rminute = ISEC1_Minute;

  time = encode_time(rhour, rminute);

  return (time) ;
}


int gribTimeIsFC(int *isec1)
{
  int isFC = FALSE;
  int time_period;

  if ( ISEC1_TimeRange == 10 )
    time_period = (ISEC1_TimePeriod1<<8) + ISEC1_TimePeriod2;
  else
    time_period = ISEC1_TimePeriod1;

  if ( time_period > 0 && ISEC1_Day > 0 )
    {
      if ( ISEC1_TimeRange == 0 || ISEC1_TimeRange == 10 ) isFC = TRUE;
    }

  return (isFC);
}


void gribDateTime(int *isec1, int *date, int *time)
{
  static char func[] = "gribDateTime";
  static int lprint = TRUE;
  int ryear, rmonth, rday, rhour, rminute;
  int time_period;
  int julday, secofday, addsec;
  int century;

  century = ISEC1_Century;
  if ( century < 0 ) century = -century;
  century -= 1;

  ryear   = ISEC1_Year;

  /* if ( century != 0 ) */
    {
      if ( ryear == 100 )
	{
	  ryear = 0;
	  century += 1;
	}

      if ( ryear != 255 )
	{
	  ryear = century*100 + ryear;
	  if ( ISEC1_Century < 0 ) ryear = -ryear;
	}
      else
	ryear = 1;
    }

  rmonth  = ISEC1_Month;
  rday    = ISEC1_Day;

  rhour   = ISEC1_Hour;
  rminute = ISEC1_Minute;

  /* printf("ref %d/%d/%d %d:%d\n", ryear, rmonth, rday, rhour, rminute); */

  if ( ISEC1_TimeRange == 10 )
    time_period = (ISEC1_TimePeriod1<<8) + ISEC1_TimePeriod2;
  else
    time_period = ISEC1_TimePeriod1;

  if ( time_period > 0 && rday > 0 )
    {
      if ( ISEC1_TimeRange == 0 || ISEC1_TimeRange == 10 )
	{
	  encode_juldaysec(ryear, rmonth, rday, rhour, rminute, &julday, &secofday);

	  addsec = 0;
	  switch ( ISEC1_TimeUnit )
	    {
	    case ISEC1_TABLE4_MINUTE:  addsec =    60 * time_period; break;
	    case ISEC1_TABLE4_HOUR:    addsec =  3600 * time_period; break;
	    case ISEC1_TABLE4_DAY:     addsec = 86400 * time_period; break;
	    default:
	      if ( lprint )
		{
		  gprintf(func, "Time unit %d unsupported", ISEC1_TimeUnit);
		  lprint = FALSE;
		}
	    }

	  julday_add_seconds(addsec, &julday, &secofday);

	  decode_juldaysec(julday, secofday, &ryear, &rmonth, &rday, &rhour, &rminute);
	}
    }
  /*
  printf("new %d/%d/%d %d:%d\n", ryear, rmonth, rday, rhour, rminute);
  */
  *date = encode_date(ryear, rmonth, rday);
  *time = encode_time(rhour, rminute);

  return;
}


void gprintf(const char *caller, const char *fmt, ...)
{
  static char func[] = "gprintf";
  va_list args;

  if ( grprsm == NULL ) Error(func, "GRIBEX initialization missing!\n");
	
  va_start(args, fmt);

   fprintf(grprsm, "%-18s : ", caller);
  vfprintf(grprsm, fmt, args);
   fprintf(grprsm, "\n");

  va_end(args);
}


void
gribExDP(int *isec0, int *isec1, int *isec2, double *fsec2, int *isec3,
	 double *fsec3, int *isec4, double *fsec4, int klenp, int *kgrib,
	 int kleng, int *kword, char *hoper, int *kret)
{
  static char func[] = "gribExDP";
  int yfunc = *hoper;

  if ( yfunc == 'D' || yfunc == 'J' || yfunc == 'R' )
    gribDecode(isec0, isec1, isec2, fsec2, isec3,
	       fsec3, isec4, fsec4, klenp, kgrib,
	       kleng, kword, yfunc, kret);
  else if ( yfunc == 'C' )
    gribEncode(isec0, isec1, isec2, fsec2, isec3,
	       fsec3, isec4, fsec4, klenp, kgrib,
	       kleng, kword, yfunc, kret);
  else if ( yfunc == 'V' )
    fprintf(stderr, " c-gribex: Version is %s\n", gribLibraryVersion());
  else
    {
      Error(func, "oper %c unsupported\n", yfunc);
      *kret=-9;
    }
}


void
gribExSP(int *isec0, int *isec1, int *isec2, float *fsec2sp, int *isec3,
	 float *fsec3sp, int *isec4, float *fsec4sp, int klenp, int *kgrib,
	 int kleng, int *kword, char *hoper, int *kret)
{
  static char func[] = "gribExSP";
  int inum, j;
  double fsec2dp[1024];
  double fsec3dp[2];
  double *fsec4dp = NULL;
  int yfunc = *hoper;

  if ( yfunc == 'C' )
    {
      inum = 10 + isec2[11];
      for ( j = 0; j < inum; j++ ) fsec2dp[j] = fsec2sp[j];

      fsec3dp[0] = fsec3sp[0];
      fsec3dp[1] = fsec3sp[1];

      inum = isec4[0];
      fsec4dp = (double *) malloc(inum*sizeof(double));
      if ( fsec4dp == NULL ) SysError(func, "No Memory!");

      for ( j = 0; j < inum; j++ ) fsec4dp[j] = fsec4sp[j];

      gribExDP(isec0, isec1, isec2, fsec2dp, isec3,
	       fsec3dp, isec4, fsec4dp, klenp, kgrib,
	       kleng, kword, hoper, kret);

      free(fsec4dp);
    }
  else if ( yfunc == 'D' || yfunc == 'J' || yfunc == 'R' )
    {
      fsec4dp = (double *) malloc(klenp*sizeof(double));
      if ( fsec4dp == NULL ) SysError(func, "No Memory!");

      for ( j = 0; j < 10; j++ ) fsec2dp[j] = 0.0;
      for ( j = 0; j <  2; j++ ) fsec3dp[j] = 0.0;

      gribExDP(isec0, isec1, isec2, fsec2dp, isec3,
	       fsec3dp, isec4, fsec4dp, klenp, kgrib,
	       kleng, kword, hoper, kret);

      inum = 10 + isec2[11];
      for ( j = 0; j < inum; j++ ) fsec2sp[j] = fsec2dp[j];

      fsec3sp[0] = fsec3dp[0];
      fsec3sp[1] = fsec3dp[1];

      inum = isec4[0];
      for ( j = 0; j < inum; j++ )
	{
	  if ( fsec4dp[j] > -FLT_MIN && fsec4dp[j] < FLT_MIN )
	    fsec4sp[j] = 0;
	  else if ( fsec4dp[j] > FLT_MAX )
	    fsec4sp[j] = FLT_MAX;
	  else if ( fsec4dp[j] < -FLT_MAX )
	    fsec4sp[j] = -FLT_MAX;
	  else
	    fsec4sp[j] = fsec4dp[j];
	}

      free(fsec4dp);
    }
  else if ( yfunc == 'V' )
    fprintf(stderr, " c-gribex: Version is %s\n", gribLibraryVersion());
  else
    {
      Error(func, "oper %c unsupported\n", yfunc);
      *kret=-9;
    }
}

int GRB_Debug    = 0;    /* If set to 1, debugging */

void gribSetDebug(int debug)
{
  static char func[] = "gribDebug";

  GRB_Debug = debug;

  if ( GRB_Debug )
    Message(func, "debug level %d", debug);
}

void gribSetRound(int round)
{
}

void gribSetRefDP(double refval)
{
}

void gribSetRefSP(float refval)
{
  gribSetRefDP((double) refval);
}

void gribSetValueCheck(int vcheck)
{
}



void gribPrintSec0(int *isec0)
{
  /*

    Print the information in the Indicator
    Section (Section 0) of decoded GRIB data.

    Input Parameters:

       isec0 - Array of decoded integers from Section 0


    Converted from EMOS routine GRPRS0.

       Uwe Schulzweida   MPIfM   01/04/2001

  */

  grsdef();

  fprintf(grprsm, " \n");
  fprintf(grprsm, " Section 0 - Indicator Section.       \n");
  fprintf(grprsm, " -------------------------------------\n");
  fprintf(grprsm, " Length of GRIB message (octets).     %9d\n", ISEC0_GRIB_Len);
  fprintf(grprsm, " GRIB Edition Number.                 %9d\n", ISEC0_GRIB_Version);
}

void gribPrintSec1(int *isec0, int *isec1)
{
  /*

    Print the information in the Product Definition
    Section (Section 1) of decoded GRIB data.

    Input Parameters:

       isec0 - Array of decoded integers from Section 0

       isec1 - Array of decoded integers from Section 1

    Comments:

       When decoding data from Experimental Edition or Edition 0,
       routine GRIBEX adds the additional fields available in
       Edition 1.


    Converted from EMOS routine GRPRS1.

       Uwe Schulzweida   MPIfM   01/04/2001

  */

  int iprev, icurr, icount, ioffset;
  int ibit, ierr, iout, iyear;
  int jloop, jiloop;
  float value;

  char hversion[9];
  /*
  char hfirst[121], hsecond[121], hthird[121], hfourth[121];
  */

  grsdef();

  /*
    -----------------------------------------------------------------
    Section 0 . Print required information.
    -----------------------------------------------------------------
  */

  fprintf(grprsm, " \n");
  fprintf(grprsm, " Section 1 - Product Definition Section.\n");
  fprintf(grprsm, " ---------------------------------------\n");

  fprintf(grprsm, " Code Table 2 Version Number.         %9d\n", isec1[0]);
  fprintf(grprsm, " Originating centre identifier.       %9d\n", isec1[1]);
  fprintf(grprsm, " Model identification.                %9d\n", isec1[2]);
  fprintf(grprsm, " Grid definition.                     %9d\n", isec1[3]);

  ibit = 8;
  prtbin(isec1[4], ibit, &iout, &ierr);
  fprintf(grprsm, " Flag (Code Table 1)                   %8.8d\n", iout);
  fprintf(grprsm, " Parameter identifier (Code Table 2). %9d\n", isec1[5]);

  /*
      IERR = CHKTAB2(ISEC1,HFIRST,HSECOND,HTHIRD,HFOURTH)
      IF( IERR .EQ. 0 ) THEN
       DO JLOOP = 121, 1, -1
          IF( HSECOND(JLOOP:JLOOP).NE.' ' ) THEN
            IOFFSET = JLOOP
            GOTO 110
          ENDIF
        ENDDO
        GOTO 120
 110    CONTINUE
        WRITE(*,'(2H ",A,1H")') HSECOND(1:IOFFSET)
 120    CONTINUE
      ENDIF
  */

  if ( isec1[5] != 127 )
    {
      fprintf(grprsm, " Type of level (Code Table 3).        %9d\n", isec1[6]);
      fprintf(grprsm, " Value 1 of level (Code Table 3).     %9d\n", isec1[7]);
      fprintf(grprsm, " Value 2 of level (Code Table 3).     %9d\n", isec1[8]);
    }
  else
    {
      fprintf(grprsm, " Satellite identifier.                %9d\n", isec1[6]);
      fprintf(grprsm, " Spectral band.                       %9d\n", isec1[7]);
    }

  iyear = isec1[9];
  if ( iyear != 255 )
    {
      int date, time;
      /* iyear  = ((isec1[20]-1)*100 + isec1[9]); */
      gribDateTime(isec1, &date, &time);
      iyear = date/10000;
      fprintf(grprsm, " Year of reference time of data.      %9d  (%4d)\n", isec1[9], iyear);
    }
  else
    {
      fprintf(grprsm, " Year of reference time of data MISSING  (=255)\n");
    }

  fprintf(grprsm, " Month of reference time of data.     %9d\n", isec1[10]);
  fprintf(grprsm, " Day of reference time of data.       %9d\n", isec1[11]);
  fprintf(grprsm, " Hour of reference time of data.      %9d\n", isec1[12]);
  fprintf(grprsm, " Minute of reference time of data.    %9d\n", isec1[13]);
  fprintf(grprsm, " Time unit (Code Table 4).            %9d\n", isec1[14]);
  fprintf(grprsm, " Time range one.                      %9d\n", isec1[15]);
  fprintf(grprsm, " Time range two.                      %9d\n", isec1[16]);
  fprintf(grprsm, " Time range indicator (Code Table 5)  %9d\n", isec1[17]);
  fprintf(grprsm, " Number averaged.                     %9d\n", isec1[18]);
  fprintf(grprsm, " Number missing from average.         %9d\n", isec1[19]);
  /*
     All ECMWF data in GRIB Editions before Edition 1 is decoded
     as 20th century data. Other centres are decoded as missing.
  */
  if ( isec0[1] < 1 && isec1[1] != 98 )
    fprintf(grprsm, " Century of reference time of data.   Not given\n");
  else
    fprintf(grprsm, " Century of reference time of data.   %9d\n", isec1[20]);

  /*   Print sub-centre  */
  fprintf(grprsm, " Sub-centre identifier.               %9d\n", isec1[21]);

  /*   Decimal scale factor  */
  fprintf(grprsm, " Units decimal scaling factor.        %9d\n", isec1[22]);

  /*
    -----------------------------------------------------------------
    Section 1 . Print local DWD information.
    -----------------------------------------------------------------
  */
  if ( (isec1[ 1] == 78 && isec1[36] == 253) ||
       (isec1[ 1] == 78 && isec1[36] == 254) )
    {
      fprintf(grprsm, " DWD local usage identifier.          %9d\n", isec1[36]);
      if ( isec1[36] == 253 )
	fprintf(grprsm, " (Database labelling and ensemble forecast)\n");
      if ( isec1[36] == 254 )
	fprintf(grprsm, " (Database labelling)\n");

      fprintf(grprsm, " Year of database entry                     %3d  (%4d)\n", isec1[43], 1900+isec1[43]);
      fprintf(grprsm, " Month of database entry                    %3d\n", isec1[44]);
      fprintf(grprsm, " Day of database entry                      %3d\n", isec1[45]);
      fprintf(grprsm, " Hour of database entry                     %3d\n", isec1[46]);
      fprintf(grprsm, " Minute of database entry                   %3d\n", isec1[47]);
      fprintf(grprsm, " DWD experiment number                %9d\n",isec1[48]);
      fprintf(grprsm, " DWD run type                         %9d\n",isec1[49]);
      if ( isec1[36] == 253 ) 
	{
	  fprintf(grprsm, " User id                              %9d\n",isec1[50]);
	  fprintf(grprsm, " Experiment identifier                %9d\n",isec1[51]);
	  fprintf(grprsm, " Ensemble identification type         %9d\n",isec1[52]);
	  fprintf(grprsm, " Number of ensemble members           %9d\n",isec1[53]);
	  fprintf(grprsm, " Actual number of ensemble member     %9d\n",isec1[54]);
	  fprintf(grprsm, " Model version                            %2d.%2.2d\n",isec1[55],isec1[56]);
	}
    }

  /*
    -----------------------------------------------------------------
    Section 2 . Print local ECMWF information.
    -----------------------------------------------------------------
  */
  /*
    Regular MARS labelling, or reformatted Washington EPS products.
  */
  if ( (ISEC1_CenterID    == 98 && ISEC1_LocalFLag ==  1) ||
       (ISEC1_SubCenterID == 98 && ISEC1_LocalFLag ==  1) ||
       (ISEC1_CenterID    ==  7 && ISEC1_SubCenterID == 98) )
    {
      /*   Parameters common to all definitions.  */

      fprintf(grprsm, " ECMWF local usage identifier.        %9d\n", isec1[36]);
      if ( isec1[36] == 1 )
	fprintf(grprsm, " (Mars labelling or ensemble forecast)\n");
      if ( isec1[36] == 2 )
        fprintf(grprsm, " (Cluster means and standard deviations)\n");
      if ( isec1[36] == 3 )
        fprintf(grprsm, " (Satellite image data)\n");
      if ( isec1[36] == 4 )
        fprintf(grprsm, " (Ocean model data)\n");
      if ( isec1[36] == 5 )
        fprintf(grprsm, " (Forecast probability data)\n");
      if ( isec1[36] == 6 )
        fprintf(grprsm, " (Surface temperature data)\n");
      if ( isec1[36] == 7 )
        fprintf(grprsm, " (Sensitivity data)\n");
      if ( isec1[36] == 8 )
        fprintf(grprsm, " (ECMWF re-analysis data)\n");
      if ( isec1[36] == 9 )
        fprintf(grprsm, " (Singular vectors and ensemble perturbations)\n");
      if ( isec1[36] == 10 )
        fprintf(grprsm, " (EPS tubes)\n");
      if ( isec1[36] == 11 )
        fprintf(grprsm, " (Supplementary data used by analysis)\n");
      if ( isec1[36] == 13 )
        fprintf(grprsm, " (Wave 2D spectra direction and frequency)\n");

      fprintf(grprsm, " Class.                               %9d\n", isec1[37]);
      fprintf(grprsm, " Type.                                %9d\n", isec1[38]);
      fprintf(grprsm, " Stream.                              %9d\n", isec1[39]);
      sprintf(hversion, "%4s", (char*)&isec1[40]); hversion[4] = 0;
      fprintf(grprsm, " Version number or Experiment identifier.  %4s\n", hversion);
      /*
	ECMWF Local definition 1.
	(MARS labelling or ensemble forecast data)
      */
      if ( isec1[36] == 1 )
	{
	  fprintf(grprsm, " Forecast number.                     %9d\n", isec1[41]);
	  if ( isec1[39] != 1090 )
	    fprintf(grprsm, " Total number of forecasts.           %9d\n", isec1[42]);

	  return;
	}
      /*
	ECMWF Local definition 2.
	(Cluster means and standard deviations)
      */
      if ( isec1[36] == 2 )
	{
	  fprintf(grprsm, " Cluster number.                      %9d\n", isec1[41]);
	  fprintf(grprsm, " Total number of clusters.            %9d\n", isec1[42]);
	  fprintf(grprsm, " Clustering method.                   %9d\n", isec1[43]);
	  fprintf(grprsm, " Start time step when clustering.     %9d\n", isec1[44]);
	  fprintf(grprsm, " End time step when clustering.       %9d\n", isec1[45]);
	  fprintf(grprsm, " Northern latitude of domain.         %9d\n", isec1[46]);
	  fprintf(grprsm, " Western longitude of domain.         %9d\n", isec1[47]);
	  fprintf(grprsm, " Southern latitude of domain.         %9d\n", isec1[48]);
	  fprintf(grprsm, " Eastern longitude of domain.         %9d\n", isec1[49]);
	  fprintf(grprsm, " Operational forecast in cluster      %9d\n", isec1[50]);
	  fprintf(grprsm, " Control forecast in cluster          %9d\n", isec1[51]);
	  fprintf(grprsm, " Number of forecasts in cluster.      %9d\n", isec1[52]);

	  for (jloop = 0; jloop < isec1[52]; jloop++)
	    fprintf(grprsm, " Forecast number                      %9d\n", isec1[jloop+53]);

	  return;
	}
      /*
	ECMWF Local definition 3.
	(Satellite image data)
      */
      if ( isec1[36] == 3 )
	{
	  fprintf(grprsm, " Satellite spectral band.             %9d\n", isec1[41]);
	  fprintf(grprsm, " Function code.                       %9d\n", isec1[42]);
	  return;
	}
      /*
	ECMWF Local definition 4.
	(Ocean model data)
      */
      if ( isec1[36] == 4 )
	{
	  fprintf(grprsm, " Satellite spectral band.             %9d\n", isec1[41]);
	  if ( isec1[39] != 1090 )
	    fprintf(grprsm, " Function code.                       %9d\n", isec1[42]);
	  fprintf(grprsm, " Coordinate structure definition.\n");
	  fprintf(grprsm, " Fundamental spatial reference system.%9d\n", isec1[43]);
	  fprintf(grprsm, " Fundamental time reference.          %9d\n", isec1[44]);
	  fprintf(grprsm, " Space unit flag.                     %9d\n", isec1[45]);
	  fprintf(grprsm, " Vertical coordinate definition.      %9d\n", isec1[46]);
	  fprintf(grprsm, " Horizontal coordinate definition.    %9d\n", isec1[47]);
	  fprintf(grprsm, " Time unit flag.                      %9d\n", isec1[48]);
	  fprintf(grprsm, " Time coordinate definition.          %9d\n", isec1[49]);
	  fprintf(grprsm, " Position definition.     \n");
	  fprintf(grprsm, " Mixed coordinate field flag.         %9d\n", isec1[50]);
	  fprintf(grprsm, " Coordinate 1 flag.                   %9d\n", isec1[51]);
	  fprintf(grprsm, " Averaging flag.                      %9d\n", isec1[52]);
	  fprintf(grprsm, " Position of level 1.                 %9d\n", isec1[53]);
	  fprintf(grprsm, " Position of level 2.                 %9d\n", isec1[54]);
	  fprintf(grprsm, " Coordinate 2 flag.                   %9d\n", isec1[55]);
	  fprintf(grprsm, " Averaging flag.                      %9d\n", isec1[56]);
	  fprintf(grprsm, " Position of level 1.                 %9d\n", isec1[57]);
	  fprintf(grprsm, " Position of level 2.                 %9d\n", isec1[58]);
	  fprintf(grprsm, " Grid Definition.\n");
	  fprintf(grprsm, " Coordinate 3 flag (x-axis)           %9d\n", isec1[59]);
	  fprintf(grprsm, " Coordinate 4 flag (y-axis)           %9d\n", isec1[60]);
	  fprintf(grprsm, " Coordinate 4 of first grid point.    %9d\n", isec1[61]);
	  fprintf(grprsm, " Coordinate 3 of first grid point.    %9d\n", isec1[62]);
	  fprintf(grprsm, " Coordinate 4 of last grid point.     %9d\n", isec1[63]);
	  fprintf(grprsm, " Coordinate 3 of last grid point.     %9d\n", isec1[64]);
	  fprintf(grprsm, " i - increment.                       %9d\n", isec1[65]);
	  fprintf(grprsm, " j - increment.                       %9d\n", isec1[66]);
	  fprintf(grprsm, " Flag for irregular grid coordinates. %9d\n", isec1[67]);
	  fprintf(grprsm, " Flag for normal or staggered grids.  %9d\n", isec1[68]);
	  fprintf(grprsm, " Further information.\n");
	  fprintf(grprsm, " Further information flag.            %9d\n", isec1[69]);
	  fprintf(grprsm, " Auxiliary information.\n");
	  fprintf(grprsm, " No. entries in horizontal coordinate %9d\n", isec1[70]);
	  fprintf(grprsm, " No. entries in mixed coordinate defn.%9d\n", isec1[71]);
	  fprintf(grprsm, " No. entries in grid coordinate list. %9d\n", isec1[72]);
	  fprintf(grprsm, " No. entries in auxiliary array.      %9d\n", isec1[73]);
	  /*
	    Horizontal coordinate supplement.
	  */
	  fprintf(grprsm, " Horizontal coordinate supplement.\n");
	  if ( isec1[70] == 0 )
	    {
	      fprintf(grprsm, "(None).\n");
	    }
	  else
	    {
	      fprintf(grprsm, "Number of items = %d\n", isec1[70]);
	      for (jloop = 0; jloop < isec1[70]; jloop++)
		fprintf(grprsm, "         %12d\n", isec1[74+jloop]);
	    }
	  /*
	    Mixed coordinate definition.
	  */
	  fprintf(grprsm, " Mixed coordinate definition.\n");
	  if ( isec1[71] == 0 )
	    {
	      fprintf(grprsm, "(None).\n");
	    }
	  else
	    {
	      fprintf(grprsm, "Number of items = %d\n", isec1[71]);
	      ioffset = 74 + isec1[70];
	      for (jloop = 0; jloop < isec1[71]; jloop++)
		fprintf(grprsm, "         %12d\n", isec1[ioffset+jloop]);
	    }
	  /*
	    Grid coordinate list.
	  */
	  fprintf(grprsm, " Grid coordinate list. \n");
	  if ( isec1[72] == 0 )
	    {
	      fprintf(grprsm, "(None).\n");
	    }
	  else
	    {
	      fprintf(grprsm, "Number of items = %d\n", isec1[72]);
	      ioffset = 74 + isec1[70] + isec1[71];
	      for (jloop = 0; jloop < isec1[72]; jloop++)
		fprintf(grprsm, "         %12d\n", isec1[ioffset+jloop]);
	    }
	  /*
	    Auxiliary array.
	  */
	  fprintf(grprsm, " Auxiliary array.      \n");
	  if ( isec1[73] == 0 )
	    {
	      fprintf(grprsm, "(None).\n");
	    }
	  else
	    {
	      fprintf(grprsm, "Number of items = %d\n", isec1[73]);
	      ioffset = 74 + isec1[70] + isec1[71] + isec1[72];
	      for (jloop = 0; jloop < isec1[73]; jloop++)
		fprintf(grprsm, "         %12d\n", isec1[ioffset+jloop]);
	    }
	  /*
	    Post-auxiliary array.
	  */
	  fprintf(grprsm, " Post-auxiliary array. \n");
	  ioffset = 74 + isec1[70] + isec1[71] + isec1[72] + isec1[73];
	  if ( isec1[ioffset] == 0 )
	    {
	      fprintf(grprsm, "(None).\n");
	    }
	  else
	    {
	      fprintf(grprsm, "Number of items = %d\n", isec1[ioffset]);
	      for (jloop = 1; jloop < isec1[ioffset]; jloop++)
		fprintf(grprsm, "         %12d\n", isec1[ioffset+jloop]);
	    }

	  return;
	}
      /*
	ECMWF Local definition 5.
	(Forecast probability data)
      */
      if ( isec1[36] == 5 )
	{
	  fprintf(grprsm, " Forecast probability number          %9d\n", isec1[41]);
	  fprintf(grprsm, " Total number of forecast probabilities %7d\n", isec1[42]);
	  fprintf(grprsm, " Threshold units decimal scale factor %9d\n", isec1[43]);
	  fprintf(grprsm, " Threshold indicator(1=lower,2=upper,3=both) %2d\n", isec1[44]);
	  if ( isec1[44]  !=  2 )
	    fprintf(grprsm, " Lower threshold value                %9d\n", isec1[45]);
	  if ( isec1[44]  !=  1 )
	    fprintf(grprsm, " Upper threshold value                %9d\n", isec1[46]);
	  return;
	}
      /*
	ECMWF Local definition 6.
	(Surface temperature data)
      */
      if ( isec1[36] == 6 )
	{
	  iyear = isec1[43];
	  if ( iyear > 100 )
	    {
	      if ( iyear < 19000000 ) iyear = iyear + 19000000;
	      fprintf(grprsm, " Date of SST field used               %9d\n", iyear);
	    }
	  else
	    fprintf(grprsm, "Date of SST field used               Not given\n");
	}
      if ( isec1[44] == 0 )
	fprintf(grprsm, " Type of SST field (= climatology)    %9d\n", isec1[44]);
      if ( isec1[44] == 1 )
	fprintf(grprsm, " Type of SST field (= 1/1 degree)     %9d\n", isec1[44]);
      if ( isec1[44] == 2 )
	fprintf(grprsm, " Type of SST field (= 2/2 degree)     %9d\n", isec1[44]);

      fprintf(grprsm, " Number of ICE fields used:           %9d\n", isec1[45]);

      for (jloop = 1; jloop <= isec1[45]; jloop++)
	{
	  iyear = isec1[44+(jloop*2)];
	  if ( iyear > 100 )
	    {
              if ( iyear < 19000000 ) iyear = iyear + 19000000;
	      fprintf(grprsm, " Date of ICE field%3d                 %9d\n", jloop, iyear);
	      fprintf(grprsm, " Satellite number (ICE field%3d)      %9d\n", jloop,
		     isec1[45+(jloop*2)]);
	    }
	  else
	    fprintf(grprsm, "Date of SST field used               Not given\n");
	}
      /*
	ECMWF Local definition 7.
	(Sensitivity data)
      */
      if ( isec1[36] == 7 )
	{
	  if ( isec1[38]  ==  51 )
	    fprintf(grprsm, " Forecast number                      %9d\n", isec1[41]);
	  if ( isec1[38]  !=  51 )
	    fprintf(grprsm, " Iteration number                     %9d\n", isec1[41]);
	  if ( isec1[38]  !=  52 )
	    fprintf(grprsm, " Total number of diagnostics          %9d\n", isec1[42]);
	  if ( isec1[38]  ==  52 )
	    fprintf(grprsm, " No.interations in diag. minimisation %9d\n", isec1[42]);
	  fprintf(grprsm, " Domain(0=Global,1=Europe,2=N.Hem.,3=S.Hem.) %2d\n", isec1[43]);
	  fprintf(grprsm, " Diagnostic number                    %9d\n", isec1[44]);
	}
      /*
	ECMWF Local definition 8.
	(ECMWF re-analysis data)
      */
      if ( isec1[36] == 8 )
	{
	  if ( (isec1[39] == 1043) ||
	       (isec1[39] == 1070) ||
	       (isec1[39] == 1071) )
	    {
	      fprintf(grprsm, " Interval between reference times     %9d\n", isec1[41]);
	      for (jloop = 43; jloop <= 54; jloop++)
		{
		  jiloop = jloop + 8;
		  fprintf(grprsm, " ERA section 1 octet %2d.              %9d\n",
			 jiloop, isec1[jloop-1]);
		}
	    }
	  else
	    {
	      for (jloop = 42; jloop <= 54; jloop++)
		{
		  jiloop = jloop + 8;
		  fprintf(grprsm, " ERA section 1 octet %2d.              %9d\n",
			 jiloop, isec1[jloop-1]);
		}
	    }
	  return;
	}

      if ( isec1[38] > 4  && isec1[38] < 9 )
	{
	  fprintf(grprsm, " Simulation number.                   %9d\n", isec1[41]);
	  fprintf(grprsm, " Total number of simulations.         %9d\n", isec1[42]);
	}
      /*
	ECMWF Local definition 9.
	(Singular vectors and ensemble perturbations)
      */
      if ( isec1[36] == 9 )
	{
	  if ( isec1[38] == 60 )
	    fprintf(grprsm, " Perturbed ensemble forecast number   %9d\n", isec1[41]);
	  if ( isec1[38] == 61 )
	    fprintf(grprsm, " Initial state perturbation number    %9d\n", isec1[41]);
	  if ( isec1[38] == 62 )
	    fprintf(grprsm, " Singular vector number               %9d\n", isec1[41]);
	  if ( isec1[38] == 62 )
	    {
	      fprintf(grprsm, " Number of iterations                 %9d\n", isec1[42]);
	      fprintf(grprsm, " Number of singular vectors computed  %9d\n", isec1[43]);
	      fprintf(grprsm, " Norm used at initial time            %9d\n", isec1[44]);
	      fprintf(grprsm, " Norm used at final time              %9d\n", isec1[45]);
	      fprintf(grprsm, " Multiplication factor                %9d\n", isec1[46]);
    	      fprintf(grprsm, " Latitude of north-west corner        %9d\n", isec1[47]);
    	      fprintf(grprsm, " Longitude of north-west corner       %9d\n", isec1[48]);
	      fprintf(grprsm, " Latitude of south-east corner        %9d\n", isec1[49]);
	      fprintf(grprsm, " Longitude of south-east corner       %9d\n", isec1[50]);
	      fprintf(grprsm, " Accuracy                             %9d\n", isec1[51]);
	      fprintf(grprsm, " Number of singular vectors evolved   %9d\n", isec1[52]);
	      fprintf(grprsm, " Ritz number one                      %9d\n", isec1[53]);
	      fprintf(grprsm, " Ritz number two                      %9d\n", isec1[54]);
	    }
	}
      /*
	ECMWF Local definition 10.
	(EPS tubes)
      */
      if ( isec1[36] == 10 )
	{
	  fprintf(grprsm, " Tube number                          %9d\n", isec1[41]);
          fprintf(grprsm, " Total number of tubes                %9d\n", isec1[42]);
          fprintf(grprsm, " Central cluster definition           %9d\n", isec1[43]);
          fprintf(grprsm, " Parameter                            %9d\n", isec1[44]);
          fprintf(grprsm, " Type of level                        %9d\n", isec1[45]);
          fprintf(grprsm, " Northern latitude of domain of tubing%9d\n", isec1[46]);
          fprintf(grprsm, " Western longitude of domain of tubing%9d\n", isec1[47]);
          fprintf(grprsm, " Southern latitude of domain of tubing%9d\n", isec1[48]);
          fprintf(grprsm, " Eastern longitude of domain of tubing%9d\n", isec1[49]);
          fprintf(grprsm, " Tube number of operational forecast  %9d\n", isec1[50]);
          fprintf(grprsm, " Tube number of control forecast      %9d\n", isec1[51]);
          fprintf(grprsm, " Height/pressure of level             %9d\n", isec1[52]);
          fprintf(grprsm, " Reference step                       %9d\n", isec1[53]);
          fprintf(grprsm, " Radius of central cluster            %9d\n", isec1[54]);
          fprintf(grprsm, " Ensemble standard deviation          %9d\n", isec1[55]);
          fprintf(grprsm, " Dist.of tube extreme to ensemble mean%9d\n", isec1[56]);
          fprintf(grprsm, " Number of forecasts in the tube      %9d\n", isec1[57]);

          fprintf(grprsm, " List of ensemble forecast numbers:\n");
          for (jloop = 1; jloop <=  isec1[57]; jloop++)
	    fprintf(grprsm, "    %9d\n", isec1[57+jloop]);
	}
      /*
	ECMWF Local definition 11.
	(Supplementary data used by the analysis)
      */
      if ( isec1[36] == 11 )
	{
	  fprintf(grprsm, " Details of analysis which used the supplementary data:\n");
	  fprintf(grprsm, "   Class                              %9d\n", isec1[41]);
	  fprintf(grprsm, "   Type                               %9d\n", isec1[42]);
	  fprintf(grprsm, "   Stream                             %9d\n", isec1[43]);
	  /*
	  sprintf(hversion, "%8d", isec1[44]);
	  fprintf(grprsm, "   Version number/experiment identifier:   %4s\n", &hversion[4]);
	  */
	  iyear = isec1[45];
	  if ( iyear > 50 )
	    iyear = iyear + 1900;
	  else
	    iyear = iyear + 2000;

	  fprintf(grprsm, "   Year                               %9d\n", iyear);
	  fprintf(grprsm, "   Month                              %9d\n", isec1[46]);
	  fprintf(grprsm, "   Day                                %9d\n", isec1[47]);
	  fprintf(grprsm, "   Hour                               %9d\n", isec1[48]);
	  fprintf(grprsm, "   Minute                             %9d\n", isec1[49]);
	  fprintf(grprsm, "   Century                            %9d\n", isec1[50]);
	  fprintf(grprsm, "   Originating centre                 %9d\n", isec1[51]);
	  fprintf(grprsm, "   Sub-centre                         %9d\n", isec1[52]);
	}
      /*
	ECMWF Local definition 12.
      */
      if ( isec1[36] == 12 )
	{
	  fprintf(grprsm, " (Mean, average, etc)\n");
          fprintf(grprsm, " Start date of the period              %8d\n", isec1[41]);
          fprintf(grprsm, " Start time of the period                  %4.4d\n", isec1[42]);
          fprintf(grprsm, " Finish date of the period             %8d\n", isec1[43]);
          fprintf(grprsm, " Finish time of the period                 %4.4d\n", isec1[44]);
          fprintf(grprsm, " Verifying date of the period          %8d\n", isec1[45]);
          fprintf(grprsm, " Verifying time of the period              %4.4d\n", isec1[46]);
          fprintf(grprsm, " Code showing method                   %8d\n", isec1[47]);
          fprintf(grprsm, " Number of different time intervals used  %5d\n", isec1[48]);
          fprintf(grprsm, " List of different time intervals used:\n");
          iprev  = isec1[49];
          icurr  = 0;
          icount = 0;
          for (jloop = 1; jloop <= isec1[48]; jloop++)
	    {
	      icurr = isec1[48+jloop];
	      if ( icurr != iprev )
		{
		  if ( icount == 1 )
		    fprintf(grprsm, "  - interval %5.4d used       once\n", iprev);
		  if ( icount == 2 )
		    fprintf(grprsm, "  - interval %5.4d used       twice\n", iprev);
		  if ( icount > 2 )
		    fprintf(grprsm, "  - interval %5.4d used %5d times\n",  iprev, icount);
		  iprev  = icurr;
		  icount = 1;
		}
	      else
		icount = icount + 1;
	    }
	  if ( icount == 1 )
	    fprintf(grprsm, "  - interval %5.4d used       once\n", iprev);
	  if ( icount == 2 )
	    fprintf(grprsm, "  - interval %5.4d used       twice\n", iprev);
	  if ( icount > 2 )
	    fprintf(grprsm, "  - interval %5.4d used %5d times\n",  iprev, icount);
	}
      /*
	ECMWF Local definition 13.
	(Wave 2D spectra direction and frequency)
      */
      if ( isec1[36] == 13 )
	{
          fprintf(grprsm, " Direction number                     %9d\n", isec1[43]);
	  fprintf(grprsm, " Frequency number                     %9d\n", isec1[44]);
	  fprintf(grprsm, " Total number of directions           %9d\n", isec1[45]);
	  fprintf(grprsm, " Total number of frequencies          %9d\n", isec1[46]);
	  fprintf(grprsm, " Scale factor applied to directions   %9d\n", isec1[47]);
	  fprintf(grprsm, " Scale factor applied to frequencies  %9d\n", isec1[48]);
	  fprintf(grprsm, " List of directions:\n");
          for (jloop = 1; jloop <= isec1[45]; jloop++)
            {
	      value = (float)(isec1[48+jloop])/(float)(isec1[47]);
	      if ( isec1[43] == jloop )
		fprintf(grprsm, " %2.2d:%15.7f   <-- this field value\n",  jloop, value);
	      else
		fprintf(grprsm, "%2.2d:%15.7f\n",  jloop, value);
            }
	  fprintf(grprsm, " List of frequencies:\n");
          for (jloop = 1; jloop <= isec1[46]; jloop++)
	    {
	      value = (float)(isec1[48+isec1[45]+jloop])/(float)(isec1[48]);
	      if ( isec1[44] == jloop )
		fprintf(grprsm, " %2.2d:%15.7f   <-- this field value\n",  jloop, value);
	      else
		fprintf(grprsm, "%2.2d:%15.7f\n",  jloop, value);

	      if ( isec1[49+isec1[45]+isec1[46]] != 0 )
		{
		  fprintf(grprsm, " System number (65535 = missing)      %9d\n",
			 isec1[49+isec1[45]+isec1[46]]);
		  fprintf(grprsm, " Method number (65535 = missing)      %9d\n",
			 isec1[50+isec1[45]+isec1[46]]);
		}
	    }
	  /*
	    ECMWF Local definition 14.
	    (Brightness temperature)
	  */
	  if ( isec1[36] == 14 )
	    {
	      fprintf(grprsm, " Channel number                       %9d\n", isec1[43]);
	      fprintf(grprsm, " Scale factor applied to frequencies  %9d\n", isec1[44]);
	      fprintf(grprsm, " Total number of frequencies          %9d\n", isec1[45]);
	      fprintf(grprsm, " List of frequencies:\n");
              for (jloop = 1; jloop <= isec1[45]; jloop++)
		{
		  value = (float)(isec1[45+jloop])/(float)(isec1[44]);
		  if ( isec1[43] == jloop )
		    fprintf(grprsm, " %3d:%15.9f   <-- this channel\n", jloop, value);
		  else
		    fprintf(grprsm, " %3d:%15.9f\n", jloop, value);
		}
	    }
	  /*
	    ECMWF Local definition 15.
	    (Ocean ensemble seasonal forecast)
	  */
	  if ( isec1[36] == 15 )
	    {
	      fprintf(grprsm, " Ensemble member number               %9d\n", isec1[41]);
	      fprintf(grprsm, " System number                        %9d\n", isec1[42]);
	      fprintf(grprsm, " Method number                        %9d\n", isec1[43]);
	    }
	  /*
	    ECMWF Local definition 16.
	    (Seasonal forecast monthly mean atmosphere data)
	  */
        if ( isec1[36] == 16 )
	  {
	    fprintf(grprsm, " Ensemble member number               %9d\n", isec1[41]);
	    fprintf(grprsm, " System number                        %9d\n", isec1[43]);
	    fprintf(grprsm, " Method number                        %9d\n", isec1[44]);
	    fprintf(grprsm, " Verifying month                      %9d\n", isec1[45]);
	    fprintf(grprsm, " Averaging period                     %9d\n", isec1[46]);
	  }
	/*
	  ECMWF Local definition 17.
	  (Sst or sea-ice used by analysis)
	*/
        if ( isec1[36] == 17 )
	  {
	    iyear = isec1[43];
	    if ( iyear > 100 )
	      {
		if ( iyear < 19000000 ) iyear = iyear + 19000000;
		fprintf(grprsm, " Date of sst/ice field used           %9d\n", iyear);
	      }
	    else
              fprintf(grprsm, " Date of sst/ice field used           Not given\n");
      
	    if ( isec1[44] == 0 )
	      fprintf(grprsm, " Type of sst/ice field (= climatology)%9d\n", isec1[44]);
	    if ( isec1[44] == 1 )
	      fprintf(grprsm, " Type of sst/ice field (= 1/1 degree) %9d\n", isec1[44]);
	    if ( isec1[44] == 2 )
	      fprintf(grprsm, " Type of sst/ice field (= 2/2 degree) %9d\n", isec1[44]);

	    fprintf(grprsm, " Number of ICE fields used:           %9d\n", isec1[45]);

	    for (jloop = 1; jloop < isec1[45]; jloop++)
	      {
		iyear = isec1[44+(jloop*2)];
		if ( iyear > 100 )
		  {
		    if ( iyear < 19000000 ) iyear = iyear + 19000000;
		    fprintf(grprsm, " Date of ICE field%3d                 %9d\n", jloop,
			   iyear);
		    fprintf(grprsm, " Satellite number (ICE field%3d)      %9d\n", jloop,
			   isec1[45+(jloop*2)]);
		  }
		else
		  fprintf(grprsm, "Date of sst/ice field used           Not given\n");
	      } 
	  }
	}
    }
  /*
    -----------------------------------------------------------------
    Section 3 . Print Washington ensemble product information.
    -----------------------------------------------------------------
  */
  /*
    Washington EPS products (but not reformatted Washington EPS
    products.
  */
  if ( (isec1[1] == 7 && isec1[23] == 1) && (! isec1[21] == 98) )
    {
      /*   CALL KWPRS1 (iSEC0,iSEC1)*/
    }
}

void printQuasi(int *isec2)
{
  /*

    Print the qusai-regular information in the Grid Description
    Section (Section 2) of decoded GRIB data.

    Input Parameters:

       isec2 - Array of decoded integers from Section 2.

    Comments:

       Only data representation types catered for are Gaussian
       grid, latitude/longitude grid, Spherical Harmonics,
       Polar stereographic and Space view perspective.

    Converted from EMOS routine PTQUASI.

       Uwe Schulzweida   MPIfM   01/04/2001

  */

  char yout[64];
  int nextlat, nrepeat, latcnt;
  int j;
  int ntos;

  /*
    -----------------------------------------------------------------
    Section 1. Print quasi-grid data.
    -----------------------------------------------------------------
  */
  /*
    See if scanning is north->south or south->north
  */
  fprintf(grprsm, "  Number of points along a parallel varies.\n");

  ntos = ( fmod((double) isec2[10], 128.) < 64 );

  if ( ntos )
    fprintf(grprsm, "  Number of points.   Parallel. (North to South)\n");
  else
    fprintf(grprsm, "  Number of points.   Parallel. (South to North)\n");

  /*  Display number of points for each latitude */
  latcnt  = isec2[2];
  nextlat = 0;
  memset(yout, ' ', (size_t) 11);

  for ( j = 0; j < latcnt; j++ )
    {
      nextlat = nextlat + 1;
      sprintf(yout, "%4d", nextlat);

      /*       Finished?  */
      if ( nextlat > latcnt ) break;
      if ( nextlat == latcnt )
	{
	  fprintf(grprsm, " %5d                %-12s\n", isec2[nextlat+21], yout);
	  break;
	}
      /*
	Look for neighbouring latitudes with same number of points
      */
      nrepeat = 0;

    LABEL110:
      /*
	If neighbouring latitudes have same number of points
	increase the repeat count.
      */
      if ( isec2[nextlat+21+1] == isec2[nextlat+21] )
	{
          nrepeat = nrepeat + 1;
          nextlat = nextlat + 1;
	  if ( nextlat < latcnt ) goto LABEL110;
	}
      /*
	Display neighbouring latitudes with same number of points as
	'nn to mm'.
      */
      if ( nrepeat >= 1 )
	{
	  strncpy(yout+4, " to", 3);
	  sprintf(yout+7, "%5d", nextlat);
        }
      fprintf(grprsm, " %5d                %-12s\n", isec2[nextlat+21], yout);
      memset(yout, ' ', (size_t) 11);
    }
}

void gribPrintSec2DP(int *isec0, int *isec2, double *fsec2)
{
  /*

    Print the information in the Grid Description
    Section (Section 2) of decoded GRIB data.

    Input Parameters:

       isec0  - Array of decoded integers from Section 0

       isec2  - Array of decoded integers from Section 2

       fsec2  - Array of decoded floats from Section 2

    Comments:

       Only data representation types catered for are Gaussian
       grid, latitude/longitude grid, Spherical Harmonics,
       Polar stereographic and Space view perspective.


    Converted from EMOS routine GRPRS2.

       Uwe Schulzweida   MPIfM   01/04/2001

  */

  int i, ibit, iedit, ierr, iout, iresol;

  grsdef();
  /*
    -----------------------------------------------------------------
    Section 1 . Print GRIB Edition number.
    -----------------------------------------------------------------
  */
  iedit = isec0[1];
  fprintf(grprsm, " \n");
  fprintf(grprsm, " Section 2 - Grid Description Section.\n");
  fprintf(grprsm, " -------------------------------------\n");
  /*
    -----------------------------------------------------------------
    Section 2 . Print spherical harmonic data.
    -----------------------------------------------------------------
  */
  if ( isec2[0] == 50 || isec2[0] == 60 || 
       isec2[0] == 70 || isec2[0] == 80 )
    {
      fprintf(grprsm, " Data represent type = spectral     (Table 6) %9d\n", isec2[0]);
      fprintf(grprsm, " J - Pentagonal resolution parameter.         %9d\n", isec2[1]);
      fprintf(grprsm, " K - Pentagonal resolution parameter.         %9d\n", isec2[2]);
      fprintf(grprsm, " M - Pentagonal resolution parameter.         %9d\n", isec2[3]);
      fprintf(grprsm, " Representation type (Table 9)                %9d\n", isec2[4]);
      fprintf(grprsm, " Representation mode (Table 10).              %9d\n", isec2[5]);
      for (i = 7; i <= 11; i++)
        fprintf(grprsm, " Not used.                                    %9d\n", isec2[i-1]);
      fprintf(grprsm, " Number of vertical coordinate parameters.    %9d\n", isec2[11]);
      goto LABEL800;
    }
  /*
    -----------------------------------------------------------------
    Section 3 . Print Gaussian grid data.
    -----------------------------------------------------------------
  */
  if ( isec2[0] ==  4 || isec2[0] == 14 || 
       isec2[0] == 24 || isec2[0] == 34 )
    {
      fprintf(grprsm, " (Southern latitudes and Western longitudes are negative.)\n");
      fprintf(grprsm, " Data represent type = gaussian     (Table 6) %9d\n", isec2[0]);
      /*
	Quasi-regular grids introduced in Edition 1.
      */
      if ( isec2[16] == 0 || iedit < 1 )
	fprintf(grprsm, " Number of points along a parallel.           %9d\n", isec2[1]);
      else
      	printQuasi(isec2);

      fprintf(grprsm, " Number of points along a meridian.           %9d\n", isec2[2]);
      fprintf(grprsm, " Latitude of first grid point.                %9d\n", isec2[3]);
      fprintf(grprsm, " Longitude of first grid point.               %9d\n", isec2[4]);

      ibit = 8;
      iresol = isec2[5] + isec2[17] + isec2[18];
      prtbin(iresol, ibit, &iout, &ierr);

      fprintf(grprsm, " Resolution and components flag.               %8.8d\n", iout);
      fprintf(grprsm, " Latitude of last grid point.                 %9d\n", isec2[6]);
      fprintf(grprsm, " Longitude of last grid point.                %9d\n", isec2[7]);
      /*
	Print increment if given.
      */
      if ( isec2[5] == 128 )
	fprintf(grprsm, " i direction (East-West) increment.           %9d\n", isec2[8]);
      else
	fprintf(grprsm, " i direction (East-West) increment            Not given\n");

      fprintf(grprsm, " Number of parallels between pole and equator.%9d\n", isec2[9]);

      ibit = 8;
      prtbin(isec2[10], ibit, &iout, &ierr);

      fprintf(grprsm, " Scanning mode flags (Code Table 8)            %8.8d\n", iout);
      fprintf(grprsm, " Number of vertical coordinate parameters.    %9d\n", isec2[11]);
      goto LABEL800;
    }
  /*
    -----------------------------------------------------------------
    Section 4 . Print Latitude / longitude grid data.
    -----------------------------------------------------------------
  */
  if ( isec2[0] ==  0 || isec2[0] == 10 || 
       isec2[0] == 20 || isec2[0] == 30 )
    {
      fprintf(grprsm, " (Southern latitudes and Western longitudes are negative.)\n");
      fprintf(grprsm, " Data represent type = lat/long     (Table 6) %9d\n", isec2[0]);
      /*
	Quasi-regular lat/long grids also possible.
      */
      if ( isec2[16] == 0 )
	fprintf(grprsm, " Number of points along a parallel.           %9d\n", isec2[1]);
      else
        printQuasi(isec2);

      fprintf(grprsm, " Number of points along a meridian.           %9d\n", isec2[2]);
      fprintf(grprsm, " Latitude of first grid point.                %9d\n", isec2[3]);
      fprintf(grprsm, " Longitude of first grid point.               %9d\n", isec2[4]);

      ibit = 8;
      iresol = isec2[5] + isec2[17] + isec2[18];
      prtbin(iresol, ibit, &iout, &ierr);

      fprintf(grprsm, " Resolution and components flag.               %8.8d\n", iout);
      fprintf(grprsm, " Latitude of last grid point.                 %9d\n", isec2[6]);
      fprintf(grprsm, " Longitude of last grid point.                %9d\n", isec2[7]);
      /*
	Print increment if given.
      */
      if ( isec2[8] < 0 )
	fprintf(grprsm, " i direction (East-West) increment            Not given\n");
      else
	fprintf(grprsm, " i direction (East-West) increment.           %9d\n", isec2[8]);

      if ( isec2[9] < 0 )
	fprintf(grprsm, " j direction (North-South) increment          Not given\n");
      else
	fprintf(grprsm, " j direction (North-South) increment.         %9d\n", isec2[9]);
    
      ibit = 8;
      prtbin(isec2[10], ibit, &iout, &ierr);

      fprintf(grprsm, " Scanning mode flags (Code Table 8)            %8.8d\n", iout);
      fprintf(grprsm, " Number of vertical coordinate parameters.    %9d\n", isec2[11]);
      goto LABEL800;
    }
  /*
    -----------------------------------------------------------------
    Section 5 . Print polar stereographic data.
    -----------------------------------------------------------------
  */
  if ( isec2[0] == 5 )
    {
      fprintf(grprsm, " (Southern latitudes and Western longitudes are negative.)\n");
      fprintf(grprsm, " Data represent type = polar stereo (Table 6) %9d\n", isec2[0]);
      fprintf(grprsm, " Number of points along X axis.               %9d\n", isec2[1]);
      fprintf(grprsm, " Number of points along Y axis.               %9d\n", isec2[2]);
      fprintf(grprsm, " Latitude of first grid point.                %9d\n", isec2[3]);
      fprintf(grprsm, " Longitude of first grid point.               %9d\n", isec2[4]);
      ibit = 8;
      iresol = isec2[17] + isec2[18];
      prtbin(iresol, ibit, &iout, &ierr);
      fprintf(grprsm, " Resolution and components flag.               %8.8d\n", iout);
      fprintf(grprsm, " Orientation of the grid.                     %9d\n", isec2[6]);
      fprintf(grprsm, " X direction increment.                       %9d\n", isec2[8]);
      fprintf(grprsm, " Y direction increment.                       %9d\n", isec2[9]);
      ibit = 8;
      prtbin(isec2[10], ibit, &iout, &ierr);
      fprintf(grprsm, " Scanning mode flags (Code Table 8)            %8.8d\n", iout);
      fprintf(grprsm, " Number of vertical coordinate parameters.    %9d\n", isec2[11]);
      fprintf(grprsm, " Projection centre flag.                      %9d\n", isec2[12]);
      goto LABEL800;
    }
  /*
    -----------------------------------------------------------------
    Section 6 . Print Lambert conformal data.
    -----------------------------------------------------------------
  */
  if ( isec2[0] == 3 )
    {
      fprintf(grprsm, " (Southern latitudes and Western longitudes are negative.)\n");
      fprintf(grprsm, " Data represent type = Lambert      (Table 6) %9d\n", isec2[0]);
      fprintf(grprsm, " Number of points along X axis.               %9d\n", isec2[1]);
      fprintf(grprsm, " Number of points along Y axis.               %9d\n", isec2[2]);
      fprintf(grprsm, " Latitude of first grid point.                %9d\n", isec2[3]);
      fprintf(grprsm, " Longitude of first grid point.               %9d\n", isec2[4]);
      ibit = 8;
      iresol = isec2[17] + isec2[18] + isec2[5];
      prtbin(iresol, ibit, &iout, &ierr);
      fprintf(grprsm, " Resolution and components flag.               %8.8d\n", iout);
      fprintf(grprsm, " Orientation of the grid.                     %9d\n", isec2[6]);
      fprintf(grprsm, " X direction increment.                       %9d\n", isec2[8]);
      fprintf(grprsm, " Y direction increment.                       %9d\n", isec2[9]);
      ibit = 8;
      prtbin(isec2[10], ibit, &iout, &ierr);
      fprintf(grprsm, " Scanning mode flags (Code Table 8)            %8.8d\n", iout);
      fprintf(grprsm, " Number of vertical coordinate parameters.    %9d\n", isec2[11]);
      fprintf(grprsm, " Projection centre flag.                      %9d\n", isec2[12]);
      fprintf(grprsm, " Latitude intersection 1 - Latin 1 -.         %9d\n", isec2[13]);
      fprintf(grprsm, " Latitude intersection 2 - Latin 2 -.         %9d\n", isec2[14]);
      fprintf(grprsm, " Latitude of Southern Pole.                   %9d\n", isec2[19]);
      fprintf(grprsm, " Longitude of Southern Pole.                  %9d\n", isec2[20]);
      goto LABEL800;
    }
  /*
    -----------------------------------------------------------------
    Section 7 . Print space view perspective or orthographic data.
    -----------------------------------------------------------------
  */
  if ( isec2[0] == 90 )
    {
      fprintf(grprsm, " (Southern latitudes and Western longitudes are negative.)\n");
      fprintf(grprsm, " Data represent type = space/ortho  (Table 6) %9d\n", isec2[0]);
      fprintf(grprsm, " Number of points along X axis.               %9d\n", isec2[1]);
      fprintf(grprsm, " Number of points along Y axis.               %9d\n", isec2[2]);
      fprintf(grprsm, " Latitude of sub-satellite point.             %9d\n", isec2[3]);
      fprintf(grprsm, " Longitude of sub-satellite point.            %9d\n", isec2[4]);
      iresol = isec2[17] + isec2[18];
      fprintf(grprsm, " Diameter of the earth in x direction.        %9d\n", isec2[6]);
      fprintf(grprsm, " Y coordinate of sub-satellite point.         %9d\n", isec2[9]);
      ibit = 8;
      prtbin(isec2[10], ibit, &iout, &ierr);
      fprintf(grprsm, " Scanning mode flags (Code Table 8)            %8.8d\n", iout);
      fprintf(grprsm, " Number of vertical coordinate parameters.    %9d\n", isec2[11]);
      fprintf(grprsm, " Orientation of the grid.                     %9d\n", isec2[6]);
      fprintf(grprsm, " Altitude of the camera.                      %9d\n", isec2[13]);
      fprintf(grprsm, " Y coordinate of origin of sector image.      %9d\n", isec2[14]);
      fprintf(grprsm, " X coordinate of origin of sector image.      %9d\n", isec2[15]);
      goto LABEL800;
    }
  /*
    -----------------------------------------------------------------
    Section 7.5 . Print ocean data
    -----------------------------------------------------------------
  */
  /*
  if ( isec2[0] == 192 && isec1[1] == 98 )
    {
      fprintf(grprsm, " Data represent type = ECMWF ocean  (Table 6) %9d\n", isec2[0]);
      if ( isec2[1] ==  32767 )
	fprintf(grprsm, " Number of points along the first axis.       Not used\n");
      else
	fprintf(grprsm, " Number of points along the first axis.       %9d\n", isec2[1]);

      if ( isec2[2] ==  32767 )
	fprintf(grprsm, " Number of points along the second axis.      Not used\n");
      else
	fprintf(grprsm, " Number of points along the second axis.      %9d\n", isec2[2]);

      ibit = 8;
      prtbin(isec2[10], ibit, &iout, &ierr);
      fprintf(grprsm, " Scanning mode flags (Code Table 8)            %8.8d\n", iout);
      goto LABEL800;
    }
    */
  /*
    -----------------------------------------------------------------
    Section 7.6 . Print triangular data
    -----------------------------------------------------------------
  */
  if ( isec2[0] == 192 /* && isec1[1] == 78 */ )
    {
      fprintf(grprsm, " Data represent type = triangular   (Table 6) %9d\n", isec2[0]);
      fprintf(grprsm, " Number of factor 2 in factorisation of Ni.   %9d\n", isec2[1]);
      fprintf(grprsm, " Number of factor 3 in factorisation of Ni.   %9d\n", isec2[2]);
      fprintf(grprsm, " Number of diamonds (Nd).                     %9d\n", isec2[3]);
      fprintf(grprsm, " Number of triangular subdivisions of the\n");
      fprintf(grprsm, "           icosahedron (Ni).                  %9d\n", isec2[4]);
      fprintf(grprsm, " Flag for orientation of diamonds (Table A).  %9d\n", isec2[5]);
      fprintf(grprsm, " Latitude of pole point.                      %9d\n", isec2[6]);
      fprintf(grprsm, " Longitude of pole point.                     %9d\n", isec2[7]);
      fprintf(grprsm, " Longitude of the first diamond.              %9d\n", isec2[8]);
      fprintf(grprsm, " Flag for storage sequence (Table B).         %9d\n", isec2[9]);
      fprintf(grprsm, " Number of vertical coordinate parameters.    %9d\n", isec2[11]);
      goto LABEL800;
    }
  /*
    -----------------------------------------------------------------
    Drop through to here => representation type not catered for.
    -----------------------------------------------------------------
  */
  fprintf(grprsm, "GRPRS2 :Data representation type not catered for -%d\n", isec2[0]);

  goto LABEL900;
  /*
    -----------------------------------------------------------------
    Section 8 . Print vertical coordinate parameters,
                rotated grid information,
                stretched grid information, if any.
    -----------------------------------------------------------------
  */
 LABEL800:;
  /*
    Vertical coordinate parameters ...
  */
  if ( isec2[11] != 0 )
    {
      fprintf(grprsm, " \n");
      fprintf(grprsm, " Vertical Coordinate Parameters.\n");
      fprintf(grprsm, " -------------------------------\n");
      for ( i = 10; i < isec2[11]+10; i++ )
	fprintf(grprsm, "    %20.12f\n", fsec2[i]);
    }
  /*
    Rotated and stretched grids introduced in Edition 1.
  */
  if ( iedit < 1 ) goto LABEL900;
  /*
    Rotated grid information ...
  */
  if ( isec2[0] == 10 || isec2[0] == 30 || 
       isec2[0] == 14 || isec2[0] == 34 || 
       isec2[0] == 60 || isec2[0] == 80 || 
       isec2[0] == 30 )
    {
      fprintf(grprsm, " \n");
      fprintf(grprsm, " Latitude of southern pole of rotation.       %9d\n", isec2[12]);
      fprintf(grprsm, " Longitude of southern pole of rotation.      %9d\n", isec2[13]);
      fprintf(grprsm, " Angle of rotation.                     %20.10f\n", fsec2[0]);
    }
  /*
    Stretched grid information ...
  */
  if ( isec2[0] == 20 || isec2[0] == 30 || 
       isec2[0] == 24 || isec2[0] == 34 || 
       isec2[0] == 70 || isec2[0] == 80 )
    {
      fprintf(grprsm, " \n");
      fprintf(grprsm, " Latitude of pole of stretching.              %9d\n", isec2[14]);
      fprintf(grprsm, " Longitude of pole of stretching.             %9d\n", isec2[15]);
      fprintf(grprsm, " Stretching factor.                     %20.10f\n", fsec2[1]);
    }

 LABEL900:;

  return;
}

void gribPrintSec2SP(int *isec0, int *isec2, float  *fsec2sp)
{
  static char func[] = "grprs2sp";
  int inum;
  int j;
  double *fsec2;

  inum = 10 + isec2[11];

  fsec2 = (double *) malloc(inum*sizeof(double));
  if ( fsec2 == NULL ) SysError(func, "No Memory!");

  for ( j = 0; j < inum; j++ )
     fsec2[j] = fsec2sp[j];
  
  gribPrintSec2DP(isec0, isec2, fsec2);

  free(fsec2);
}

void gribPrintSec3DP(int *isec0, int *isec3, double *fsec3)
{
  /*

    Print the information in the Bit-Map Section
    (Section 3) of decoded GRIB data.

    Input Parameters:

       isec0  - Array of decoded integers from Section 0

       isec3  - Array of decoded integers from Section 3

       fsec3  - Array of decoded floats from Section 3


    Converted from EMOS routine GRPRS3.

       Uwe Schulzweida   MPIfM   01/04/2001

  */

  grsdef();

  fprintf(grprsm, " \n");
  fprintf(grprsm, " Section 3 - Bit-map Section.\n");
  fprintf(grprsm, " -------------------------------------\n");

  if ( isec3[0] != 0 )
    fprintf(grprsm, " Predetermined bit-map number.                %9d\n", isec3[0]);
  else
    fprintf(grprsm, " No predetermined bit-map.\n");

  fprintf(grprsm, " Missing data value for integer data.    %14d\n", isec3[1]);

  fprintf(grprsm, " Missing data value for real data. %20.6g\n", fsec3[1]);
}

void gribPrintSec3SP(int *isec0, int *isec3, float  *fsec3sp)
{
  double fsec3[2];

  fsec3[0] = fsec3sp[0];
  fsec3[1] = fsec3sp[1];
  
  gribPrintSec3DP(isec0, isec3, fsec3);
}

void gribPrintSec4DP(int *isec0, int *isec4, double *fsec4)
{
  /*

    Print the information in the Binary Data Section
    (Section 4) of decoded GRIB data.

    Input Parameters:

       isec0  - Array of decoded integers from Section 0

       isec4  - Array of decoded integers from Section 4

       fsec4  - Array of decoded floats from Section 4


    Converted from EMOS routine GRPRS4.

       Uwe Schulzweida   MPIfM   01/04/2001

  */

  int inum;
  int j;

  grsdef();

  /*
    -----------------------------------------------------------------
    Section 1 . Print integer information from isec4.
    -----------------------------------------------------------------
  */
  fprintf(grprsm, " \n");
  fprintf(grprsm, " Section 4 - Binary Data  Section.\n");
  fprintf(grprsm, " -------------------------------------\n");

  fprintf(grprsm, " Number of data values coded/decoded.         %9d\n", isec4[0]);
  fprintf(grprsm, " Number of bits per data value.               %9d\n", isec4[1]);
  fprintf(grprsm, " Type of data       (0=grid pt, 128=spectral).%9d\n", isec4[2]);
  fprintf(grprsm, " Type of packing    (0=simple, 64=complex).   %9d\n", isec4[3]);
  fprintf(grprsm, " Type of data       (0=float, 32=integer).    %9d\n", isec4[4]);
  fprintf(grprsm, " Additional flags   (0=none, 16=present).     %9d\n", isec4[5]);
  fprintf(grprsm, " Reserved.                                    %9d\n", isec4[6]);
  fprintf(grprsm, " Number of values   (0=single, 64=matrix).    %9d\n", isec4[7]);
  fprintf(grprsm, " Secondary bit-maps (0=none, 32=present).     %9d\n", isec4[8]);
  fprintf(grprsm, " Values width       (0=constant, 16=variable).%9d\n", isec4[9]);
  /*
    If complex packing ..
  */
  if ( isec4[3] == 64 )
    {
      if ( isec4[2] == 128 )
	{
	  fprintf(grprsm, " Byte offset of start of packed data (N).     %9d\n", isec4[15]);
	  fprintf(grprsm, " Power (P * 1000).                            %9d\n", isec4[16]);
	  fprintf(grprsm, " Pentagonal resolution parameter J for subset.%9d\n", isec4[17]);
	  fprintf(grprsm, " Pentagonal resolution parameter K for subset.%9d\n", isec4[18]);
	  fprintf(grprsm, " Pentagonal resolution parameter M for subset.%9d\n", isec4[19]);
	}
      else
	{
	  fprintf(grprsm, " Bits number of 2nd order values    (none=>0).%9d\n", isec4[10]);
	  fprintf(grprsm, " General extend. 2-order packing (0=no,8=yes).%9d\n", isec4[11]);
	  fprintf(grprsm, " Boustrophedonic ordering        (0=no,4=yes).%9d\n", isec4[12]);
	  fprintf(grprsm, " Spatial differencing order          (0=none).%9d\n", isec4[13]+isec4[14]);
        }
    }
  /*
    Number of non-missing values
  */
  if ( isec4[20] != 0 )
    fprintf(grprsm, " Number of non-missing values                 %9d\n", isec4[20]);
  /*
    Information on matrix of values , if present.
  */
  if ( isec4[7] == 64 )
    {
      fprintf(grprsm, " First dimension (rows) of each matrix.       %9d\n", isec4[49]);
      fprintf(grprsm, " Second dimension (columns) of each matrix.   %9d\n", isec4[50]);
      fprintf(grprsm, " First dimension coordinate values definition.%9d\n", isec4[51]);
      fprintf(grprsm, " (Code Table 12)\n");
      fprintf(grprsm, " NC1 - Number of coefficients for 1st dimension.%7d\n", isec4[52]);
      fprintf(grprsm, " Second dimension coordinate values definition.%8d\n", isec4[53]);
      fprintf(grprsm, " (Code Table 12)\n");
      fprintf(grprsm, " NC2 - Number of coefficients for 2nd dimension.%7d\n", isec4[54]);
      fprintf(grprsm, " 1st dimension physical signifance (Table 13). %8d\n", isec4[55]);
      fprintf(grprsm, " 2nd dimension physical signifance (Table 13).%8d\n", isec4[56]);
    }
  /*
    -----------------------------------------------------------------
    Section 2. Print values from fsec4.
    -----------------------------------------------------------------
  */

  inum = isec4[0];
  if ( inum <  0 ) inum = - inum;
  if ( inum > 20 ) inum = 20;
  /*
    Print first inum values.
  */
  fprintf(grprsm, " \n");
  fprintf(grprsm, " First %4d data values.\n", inum);

  if ( isec4[4] == 0 )
    {
      /*
	Print real values ...
      */
      for ( j = 0; j < inum; j++ )
	{
	  if ( fabs(fsec4[j]) > 0 )
	    {
	      if ( fabs(fsec4[j]) >= 0.1 && fabs(fsec4[j]) <= 1.e8 )
		fprintf(grprsm, " %#16.8G    \n", fsec4[j]);
	      else
		fprintf(grprsm, " %#20.8E\n", fsec4[j]);
	    }
	  else
	    fprintf(grprsm, " %#16.0f    \n", fabs(fsec4[j]));
	}
    }
  else
    {
      /*
	Print integer values ...
      */
      fprintf(grprsm, " Print of integer values not supported\n");
      /*
        CALL SETPAR(IBIT,IDUM,IDUM)
        DO 212 J=1,INUM
           INSPT = 0
           CALL INXBIT(IVALUE,1,INSPT,FSEC4(J),1,IBIT,IBIT,'C',IRET)
           WRITE (*,9033) IVALUE
 9033 FORMAT(' ',I15)
  212   CONTINUE
      ENDIF
      */
    }
}

void gribPrintSec4SP(int *isec0, int *isec4, float  *fsec4sp)
{
  int inum;
  int j;
  double fsec4[20];

  inum = isec4[0];
  if ( inum <  0 ) inum = -inum;
  if ( inum > 20 ) inum = 20;

  for ( j = 0; j < inum; j++ ) fsec4[j] = fsec4sp[j];
  
  gribPrintSec4DP(isec0, isec4, fsec4);
}

void gribPrintSec4Wave(int *isec4)
{
  /*

    Print the wave coordinate information in the Binary Data
    Section (Section 4) of decoded GRIB data.

    Input Parameters:

       isec4 - Array of decoded integers from Section 4

    Comments:

       Wave coordinate information held in isec4 are 32-bit floats,
       hence the PTEMP and NTEMP used for printing are 4-byte variables.


    Converted from EMOS routine GRPRS4W.

       Uwe Schulzweida   MPIfM   01/04/2001

  */
  int    jloop;
  int    ntemp[100];
  float *ptemp;

  grsdef();

  /*
    -----------------------------------------------------------------
    Section 1 . Print integer information from isec4.
    -----------------------------------------------------------------
  */
  fprintf(grprsm, " Coefficients defining first dimension coordinates:\n");
  for ( jloop = 0; jloop < isec4[52]; jloop++ )
    {
      ntemp[jloop] = isec4[59 + jloop];
      ptemp = (float *) &ntemp[jloop];
      fprintf(grprsm, "%20.10f\n", *ptemp);
    }
  fprintf(grprsm, " Coefficients defining second dimension coordinates:\n");
  for ( jloop = 0; jloop < isec4[54]; jloop++ )
    {
      ntemp[jloop] = isec4[59 + isec4[52] + jloop];
      ptemp = (float *) &ntemp[jloop];
      fprintf(grprsm, "%20.10f\n", *ptemp);
    }
}


#define COMP_Z      1
#define COMP_JASPER 3

/* #define COMPRESS  COMP_Z */

/* #define HAVE_LIBZ 1      */
/* #define HAVE_LIBJASPER 1 */

int  BitsPerInt = (int) (sizeof(int) * 8);


/* GRIB block 0 - indicator block */

void encodeIS(GRIBPACK *lGrib, int *gribLen)
{
  int z = *gribLen;

  lGrib[0] = 'G';
  lGrib[1] = 'R';
  lGrib[2] = 'I';
  lGrib[3] = 'B';

  /* 
   * lGrib[4]-lGrib[6] contains full length of grib record. 
   * included before finished CODEGB
   */

  z = 7;   
  Put1Byte(1); /* grib version */
  z = 8;

  *gribLen = z;
}


/* GRIB block 5 - end block */

void encodeES(GRIBPACK *lGrib, int *gribLen)
{
  int z = *gribLen;

  lGrib[z++] = '7';
  lGrib[z++] = '7';
  lGrib[z++] = '7';
  lGrib[z++] = '7';

  if ( z > 16777216 )
    {
      fprintf(stderr, "Abort: GRIB record to large (max = 16777216)!\n");
      exit(1);
    }

  lGrib[4] = z >> 16;
  lGrib[5] = z >>  8;
  lGrib[6] = z;

  while ( z & 7 ) lGrib[z++] = 0;

  *gribLen = z;
}

/* GRIB block 1 - product definition block. */

#define DWD_extension_253_len 37
#define DWD_extension_254_len 26
#define ECMWF_extension_1_len 24

int getLocalExtLen(int *isec1)
{
  int extlen = 0;

  if ( ISEC1_LocalFLag )
    {
      if ( ISEC1_CenterID == 78 ) 
	{
	  if ( isec1[36] == 254 ) 
	    {
	      extlen = DWD_extension_254_len;
	    }
	  else if ( isec1[36] == 253 )
	    { 
	      extlen = DWD_extension_253_len;
	    }
	}
      else if ( ISEC1_CenterID == 98 )
        {
	  if ( isec1[36] == 1 )
	    {
	      extlen = ECMWF_extension_1_len;
	    }
        }
    }

  return (extlen);
}


int getPdsLen(int *isec1)
{
  int pdslen = 28;

  pdslen += getLocalExtLen(isec1);

  return (pdslen);
}


void encodePDS_DWD_local_Extension_254(GRIBPACK *lGrib, int *zs, int *isec1)
{
  int i, localextlen, isvn;
  int z = *zs;

  localextlen = getLocalExtLen(isec1);
  for ( i = 0; i < localextlen-2; i++ )
    {
      Put1Byte(isec1[24+i]);
    }

  isvn = isec1[49] << 15 | isec1[48]; /* DWD experiment identifier    */
  Put2Byte(isvn);             /* DWD run type (0=main, 2=ass, 3=test) */

  *zs = z;
}


void encodePDS_DWD_local_Extension_253(GRIBPACK *lGrib, int *zs, int *isec1)
{
  int i, localextlen, isvn;
  int z = *zs;

  localextlen = DWD_extension_254_len;
  for ( i = 0; i < localextlen-2; i++ )
    {
      Put1Byte(isec1[24+i]);
    }

  isvn = isec1[49] << 15 | isec1[48]; /* DWD experiment identifier    */
  Put2Byte(isvn);             /* DWD run type (0=main, 2=ass, 3=test) */
  Put1Byte(isec1[50]);        /* 55 User id, specified by table       */
  Put2Byte(isec1[51]);        /* 56 Experiment identifier             */
  Put2Byte(isec1[52]);        /* 58 Ensemble identification by table  */
  Put2Byte(isec1[53]);        /* 60 Number of ensemble members        */
  Put2Byte(isec1[54]);        /* 62 Actual number of ensemble member  */
  Put1Byte(isec1[55]);        /* 64 Model major version number        */ 
  Put1Byte(isec1[56]);        /* 65 Model minor version number        */ 

  *zs = z;
}


void encodePDS_ECMWF_local_Extension_1 (GRIBPACK *lGrib, int *zs, int *isec1)
{
  int i, localextlen, isvn;
  int z = *zs;
  static char func[] = "encodePDS_ECMWF_local_Extension_1";

  localextlen = getLocalExtLen(isec1);
  for ( i = 0; i < localextlen-12; i++ )
    {
      Put1Byte(isec1[24+i]);
    }
                              /* 12 bytes explicitly encoded below: */
  Put1Byte(isec1[36]);        /* ECMWF local GRIB use definition identifier */
                              /*    1=MARS labelling or ensemble fcst. data */
  Put1Byte(isec1[37]);        /* Class                                      */
  Put1Byte(isec1[38]);        /* Type                                       */
  Put2Byte(isec1[39]);        /* Stream                                     */

  /* Version number or experiment identifier    */
  Put1Byte(((unsigned char*) &isec1[40])[0]);
  Put1Byte(((unsigned char*) &isec1[40])[1]);
  Put1Byte(((unsigned char*) &isec1[40])[2]);
  Put1Byte(((unsigned char*) &isec1[40])[3]);

  Put1Byte(isec1[41]);        /* Ensemble forecast number                   */
  Put1Byte(isec1[42]);        /* Total number of forecasts in ensemble      */
  Put1Byte(0);                /* (Spare)                                    */

  *zs = z;
}


/* GRIB BLOCK 1 - PRODUCT DESCRIPTION SECTION */

void encodePDS(GRIBPACK *lpds, int pdsLen, int *isec1)
{
  GRIBPACK *lGrib = lpds;
  int z = 0;
  int ival, century, year;

  century = ISEC1_Century;
  year    = ISEC1_Year;

  if ( century < 0 )
    {
      century = -century;
      year    = -year;
    }

  Put3Byte(pdsLen);               /*  0 Length of Block 1        */
  Put1Byte(ISEC1_CodeTable);      /*  3 Local table number       */
  Put1Byte(ISEC1_CenterID);       /*  4 Identification of centre */
  Put1Byte(ISEC1_ModelID);        /*  5 Identification of model  */
  Put1Byte(ISEC1_GridDefinition); /*  6 Grid definition          */
  Put1Byte(ISEC1_Sec2Or3Flag);    /*  7 Block 2 included         */
  Put1Byte(ISEC1_Parameter);      /*  8 Parameter Code           */
  Put1Byte(ISEC1_LevelType);      /*  9 Type of level            */
  if ( (ISEC1_LevelType !=  20) &&
       (ISEC1_LevelType != LTYPE_99)         &&
       (ISEC1_LevelType != LTYPE_ISOBARIC)   &&
       (ISEC1_LevelType != LTYPE_ALTITUDE)   &&
       (ISEC1_LevelType != LTYPE_HEIGHT)     &&
       (ISEC1_LevelType != LTYPE_SIGMA)      &&
       (ISEC1_LevelType != LTYPE_HYBRID)     &&
       (ISEC1_LevelType != LTYPE_LANDDEPTH)  &&
       (ISEC1_LevelType != LTYPE_ISENTROPIC) &&
       (ISEC1_LevelType != 115) &&
       (ISEC1_LevelType != 117) &&
       (ISEC1_LevelType != 125) &&
       (ISEC1_LevelType != 127) &&
       (ISEC1_LevelType != 160) &&
       (ISEC1_LevelType != 210) )
    {
      Put1Byte(ISEC1_Level1);
      Put1Byte(ISEC1_Level2);
    }
  else
    {
      Put2Byte(ISEC1_Level1);     /* 10 Level                    */    
    }

  Put1Int(year);                  /* 12 Year of Century          */
  Put1Byte(ISEC1_Month);          /* 13 Month                    */
  Put1Byte(ISEC1_Day);            /* 14 Day                      */
  Put1Byte(ISEC1_Hour);           /* 15 Hour                     */
  Put1Byte(ISEC1_Minute);         /* 16 Minute                   */
  if ( ISEC1_AvgNum > 0 )
    {
      Put1Byte(2);                    /* 17 Time unit                */
      Put1Byte(0);                    /* 18 Time 1                   */
      Put1Byte(0);                    /* 19 Time 2                   */
      Put1Byte(3);                    /* 20 Timerange flag           */
      Put2Byte(ISEC1_AvgNum);     /* 21 Average                  */
    }
  else
    {
      Put1Byte(ISEC1_TimeUnit);   /* 17 Time unit                */
      if ( ISEC1_TimeRange == 10 )
	{
	  Put1Byte(ISEC1_TimePeriod1);
	  Put1Byte(ISEC1_TimePeriod2);
	}
      else if ( ISEC1_TimeRange == 113 || ISEC1_TimeRange ==   0 )
	{
	  Put1Byte(ISEC1_TimePeriod1);
          Put1Byte(0);
	}
      else if ( ISEC1_TimeRange ==   4 || ISEC1_TimeRange ==   2 )
	{
          Put1Byte(0);
	  Put1Byte(ISEC1_TimePeriod2);
	}
      else
	{
          Put1Byte(0);
          Put1Byte(0); 
	}
      Put1Byte(ISEC1_TimeRange);      /* 20 Timerange flag           */
      Put2Byte(ISEC1_AvgNum);         /* 21 Average                  */
    }

  Put1Byte(ISEC1_AvgMiss);            /* 23 Missing from averages    */
  Put1Byte(century);                  /* 24 Century                  */
  Put1Byte(ISEC1_SubCenterID);        /* 25 Subcenter                */
  Put2Byte(ISEC1_DecScaleFactor);     /* 26 Decimal scale factor     */

  if ( ISEC1_LocalFLag )
    {
      if ( ISEC1_CenterID == 78 ) 
	{
	  if ( isec1[36] == 254 ) 
	    {
	      encodePDS_DWD_local_Extension_254(lGrib, &z, isec1);
	    }
	  else if ( isec1[36] == 253 )
	    { 
	      encodePDS_DWD_local_Extension_253(lGrib, &z, isec1);
	    }
	}
      else if ( ISEC1_CenterID == 98 )
	{
	  if ( isec1[36] == 1 )
	    {
	      encodePDS_ECMWF_local_Extension_1(lGrib, &z, isec1);
	    }
	}
      else
	{
	  int i, localextlen;
	  localextlen = getLocalExtLen(isec1);
	  for ( i = 0; i < localextlen; i++ )
	    {
	      Put1Byte(isec1[24+i]);
	    }
	}
    }
}


/* GRIB BLOCK 2 - GRID DESCRIPTION SECTION */

void encodeGDS(GRIBPACK *lGrib, int *gribLen, int *isec2, double *fsec2)
{
  static char func[] = "encodeGDS";
  int z = *gribLen;
  int exponent, mantissa;
  int i;
  int ival;
  int pvoffset = 255;
  int gdslen = 32;
  unsigned lonIncr, latIncr;

  if ( ISEC2_GridType == GTYPE_LCC ) gdslen += 10;

  if ( ISEC2_GridType == GTYPE_LATLON_ROT )  gdslen += 10;

  if ( ISEC2_NumVCP || ISEC2_Reduced ) pvoffset = gdslen + 1;

  if ( ISEC2_Reduced ) gdslen += 2 * ISEC2_NumLat;

  gdslen += ISEC2_NumVCP * 4;

  Put3Byte(gdslen);             /*  0- 2 Length of Block 2 Byte 0 */
  Put1Byte(ISEC2_NumVCP);       /*  3    NV */
  Put1Byte(pvoffset);           /*  4    PV */
  Put1Byte(ISEC2_GridType);     /*  5    LatLon=0 Gauss=4 Spectral=50 */

  if ( ISEC2_GridType == GTYPE_SPECTRAL )
    {
      Put2Byte(ISEC2_PentaJ);   /*  6- 7 Pentagonal resolution J  */
      Put2Byte(ISEC2_PentaK);   /*  8- 9 Pentagonal resolution K  */
      Put2Byte(ISEC2_PentaM);   /* 10-11 Pentagonal resolution M  */
      Put1Byte(ISEC2_RepType);  /* 12    Representation type      */
      Put1Byte(ISEC2_RepMode);  /* 13    Representation mode      */
      PutnZero(18);             /* 14-31 reserved                 */
    }
  else if ( ISEC2_GridType == GTYPE_GME )
    {
      Put2Byte(ISEC2_GME_NI2);
      Put2Byte(ISEC2_GME_NI3);
      Put3Byte(ISEC2_GME_ND);
      Put3Byte(ISEC2_GME_NI);
      Put1Byte(ISEC2_GME_AFlag);
      Put3Int(ISEC2_GME_LatPP);
      Put3Int(ISEC2_GME_LonPP);
      Put3Int(ISEC2_GME_LonMPL);
      Put1Byte(ISEC2_GME_BFlag);
      PutnZero(5);
    }
  else if ( ISEC2_GridType == GTYPE_LCC )
    {
      Put2Byte(ISEC2_NumLon);          /*  6- 7 Longitudes               */

      Put2Byte(ISEC2_NumLat);          /*  8- 9 Latitudes                */
      Put3Int(ISEC2_FirstLat);
      Put3Int(ISEC2_FirstLon);
      Put1Byte(ISEC2_ResFlag);         /* 16    Resolution flag          */
      Put3Int(ISEC2_LoV);              /* 17-19 */
      Put3Int(ISEC2_Xinc);             /* 20-22 */
      Put3Int(ISEC2_Yinc);             /* 23-25 */
      PutnZero(1);
      Put1Byte(ISEC2_ScanFlag);        /* 27    Scanning mode            */
      Put3Int(ISEC2_LatS1);            /* 28-30 */  
      Put3Int(ISEC2_LatS2);            /* 31-33 */
      PutnZero(8);                     /* 34-41 */
    }
  else if ( ISEC2_GridType == GTYPE_LATLON    ||
	    ISEC2_GridType == GTYPE_GAUSSIAN  ||
	    ISEC2_GridType == GTYPE_LATLON_ROT )
    {
      int numlon;
      if ( ISEC2_Reduced )
	numlon = 65535;
      else
	numlon = ISEC2_NumLon;

      Put2Byte(numlon);                /*  6- 7 Number of Longitudes     */

      Put2Byte(ISEC2_NumLat);          /*  8- 9 Number of Latitudes      */
      Put3Int(ISEC2_FirstLat);
      Put3Int(ISEC2_FirstLon);
      Put1Byte(ISEC2_ResFlag);         /* 16    Resolution flag          */
      Put3Int(ISEC2_LastLat);
      Put3Int(ISEC2_LastLon);
      if ( ISEC2_ResFlag == 0 )
	{
	  lonIncr = 65535;
	  latIncr = 65535;
	}
      else
	{
	  lonIncr = ISEC2_LonIncr;
	  latIncr = ISEC2_LatIncr;
	}
      Put2Byte(lonIncr);                  /* 23-24 i - direction increment  */
      if ( ISEC2_GridType == GTYPE_GAUSSIAN )
	Put2Byte(ISEC2_NumPar);        /* 25-26 Latitudes Pole->Equator  */
      else
	Put2Byte(latIncr);              /* 25-26 j - direction increment  */

      Put1Byte(ISEC2_ScanFlag);        /* 27    Scanning mode            */
      PutnZero(4);                     /* 28-31 reserved                 */

      if ( ISEC2_GridType == GTYPE_LATLON_ROT )
	{
	  Put3Int(ISEC2_LatSP);
	  Put3Int(ISEC2_LonSP);
	  Put1Real(FSEC2_RotAngle);
	}
    }
  else
    {
      Error(func, "Unsupported grid type %d", ISEC2_GridType);
    }

#if defined (SX)
#pragma vdir novector     /* vectorization gives wrong results on NEC */
#endif
  for ( i = 0; i < ISEC2_NumVCP; ++i )
    {
      Put1Real(fsec2[10+i]);
    }

  if ( ISEC2_Reduced )
    for ( i = 0; i < ISEC2_NumLat; i++ ) Put2Byte(ISEC2_RowLon(i));

  *gribLen = z;
}


/* GRIB BLOCK 3 - BIT MAP SECTION */

void encodeBMS(GRIBPACK *lGrib, int *gribLen, double *fsec3, int *isec4, double *data, int *datasize)
{
  static char func[] = "encodeBMS";
  GRIBPACK *bitmap;
  int bitmapSize;
  int imaskSize;
  int i;
  int bmsLen, bmsUnusedBits;
  int fsec4size;
  int z = *gribLen;
  unsigned int *imask;
  /*  unsigned int c, imask; */

  bitmapSize = ISEC4_NumValues;
  imaskSize = ((bitmapSize+7)>>3)<<3;
  bitmap = &lGrib[z+6];
  fsec4size = 0;
  /*
  for ( i = 0; i < imaskSize/8; i++ ) bitmap[i] = 0;

  for ( i = 0; i < imaskSize; i++ )
    {
      if ( data[i] != FSEC3_MissVal )
	{
	  data[fsec4size++] = data[i];
	  bitmap[i/8] |= 1<<(7-(i&7));
	}
    }
  */

  imask = (unsigned int *) malloc(imaskSize*sizeof(int));
  memset(imask, 0, imaskSize*sizeof(int));

#if defined (CRAY)
#pragma _CRI ivdep
#endif
#if defined (SX)
#pragma vdir nodep
#endif
#ifdef __uxpch__
#pragma loop novrec
#endif
  for ( i = 0; i < bitmapSize; i++ )
    {
      if ( !DBL_IS_EQUAL(data[i], FSEC3_MissVal) )
	{
	  data[fsec4size++] = data[i];
	  imask[i] = 1;
	}
    }

#if defined (CRAY)
#pragma _CRI ivdep
#endif
#if defined (SX)
#pragma vdir nodep
#endif
#ifdef __uxpch__
#pragma loop novrec
#endif
  for ( i = 0; i < imaskSize/8; i++ )
    {
      bitmap[i] = (imask[i*8+0] << 7) | (imask[i*8+1] << 6) |
	          (imask[i*8+2] << 5) | (imask[i*8+3] << 4) |
	          (imask[i*8+4] << 3) | (imask[i*8+5] << 2) |
	          (imask[i*8+6] << 1) | (imask[i*8+7]);
    }

  free(imask);

  bmsLen = imaskSize/8 + 6;
  bmsUnusedBits = imaskSize - bitmapSize;

  Put3Byte(bmsLen);   /*  0- 2 Length of Block 3 Byte 0 */
  Put1Byte(bmsUnusedBits);
  Put2Byte(0);

  *gribLen += bmsLen;

  *datasize = fsec4size;
}


/* GRIB BLOCK 4 - BINARY DATA SECTION */

int encodeBDS(GRIBPACK *lGrib, int *gribLen, int decscale, int *isec2, int *isec4, int datasize, double data[],
	      int *datstart, int *datsize)
{
  /* Uwe Schulzweida, 11/04/2003 : Check that number of bits per value is not exceeded */
  /* Uwe Schulzweida,  6/05/2003 : Copy result to fpval to prevent integer overflow */

  static char func[] = "encodeBDS";
  static int lwarn_cplx = TRUE;
  int z = *gribLen;
  int i, jloop;
  int blockLength, PackStart, Flag;
  int binscale = 0;
  unsigned int pval;
  int nbpv;
  /* ibits = BitsPerInt; */
  double max_nbpv_pow2;
  int exponent, mantissa;
  int unused_bits = 0;
  double fpval;
  double factor = 1, fmin, fmax, zref;
  double range, rangec;
  double jpepsln = 1.0e-12;     /* -----> tolerance used to check equality     */
                                /*        of floating point numbers - needed   */
		                /*        on some platforms (eg vpp700, linux) */
  extern double _pow2tab[158];

  if ( isec4[3] == 64 && lwarn_cplx )
    {
      lwarn_cplx = FALSE;
      Message(func, "complex packing of spectral data unsupported, using simple packing!");
    }

  if ( decscale )
    {
      double scale = pow(10.0, (double) decscale);
      for ( i = 0; i < datasize; ++i ) data[i] *= scale;
    }

  if ( ISEC2_GridType == GTYPE_SPECTRAL )
    {
      *datstart   = 15;
      PackStart   = 1;
      Flag        = 128;
      blockLength = 11 + 4 + (ISEC4_NumBits*(datasize - 1) + 7)/8;
    }
  else
    {
      *datstart   = 11;
      PackStart   = 0;
      Flag        = 0;
      blockLength = 11 + (ISEC4_NumBits*datasize + 7)/8;
    }

  if ( (blockLength%2) == 1 ) blockLength++;

  if ( ISEC2_GridType == GTYPE_SPECTRAL )
    unused_bits = blockLength*8 - 15*8 - ISEC4_NumBits*(datasize - 1);
  else
    unused_bits = blockLength*8 - 11*8 - ISEC4_NumBits*(datasize);

  Flag += unused_bits;

  fmin = fmax = data[PackStart];

#if   defined (CRAY)
#pragma _CRI ivdep
#elif defined (SX)
#pragma vdir nodep
#elif defined (__uxp__)
#pragma loop novrec
#endif
  for ( i = PackStart+1; i < datasize; ++i )
    {
      if ( fmin > data[i] ) fmin = data[i];
      if ( fmax < data[i] ) fmax = data[i];
    }

  zref = fmin;

  /*
    Adjust number of bits per value if full integer length to
    avoid hitting most significant bit (sign bit).
  */
  nbpv = ISEC4_NumBits;
  /* if( nbpv == ibits ) nbpv = nbpv - 1; */
  /*
    Calculate the binary scaling factor to spread the range of
    values over the number of bits per value.
    Limit scaling to 2**-126 to 2**127 (using IEEE 32-bit floats
    as a guideline).           
  */
  range = fabs(fmax - fmin);
  /*
    Have to allow tolerance in comparisons on some platforms
    (eg vpp700 and linux), such as 0.9999999999999999 = 1.0,
    to avoid clipping ranges which are a power of 2.
  */
  if ( range <= jpepsln )
    {
      binscale = 0;
    }
  else if ( (!DBL_IS_EQUAL(fmin, 0.0)) && (fabs(range/fmin) <= jpepsln) )
    {
      binscale = 0;
    }
  else if ( fabs(range-1.0) <= jpepsln )
    {
      binscale = 1 - nbpv;
    }
  else if ( range > 1.0 )
    {
      rangec = range + jpepsln;
      for ( jloop = 1; jloop < 128; jloop++ )
	{
	  if ( _pow2tab[jloop] > rangec ) break;
	}
      if ( jloop == 128 )
	{
	  gprintf(func, "Problem calculating binary scale value for encode");
	  gprintf(func, "> range %g rangec %g fmin %g fmax %g", range, rangec, fmin, fmax);
	  return (707);
	}
      else
	{
	  binscale = jloop - nbpv;
	}
    }
  else
    {
      rangec = range - jpepsln;
      for ( jloop = 1; jloop < 127; jloop++ )
	{
	  if ( 1.0/_pow2tab[jloop] < rangec ) break;
	}
      if ( jloop == 127 )
	{
	  gprintf(func, "Problem calculating binary scale value for encode");
	  gprintf(func, "< range %g rangec %g fmin %g fmax %g", range, rangec, fmin, fmax);
	  return (707);
	}
      else
	{
	  binscale = 1 - jloop - nbpv;
	}
    }

  if ( binscale != 0 )
    {
      if ( binscale < 0 ) factor =     intpow2(-binscale);
      else                factor = 1.0/intpow2( binscale);
    }

  max_nbpv_pow2 = (double) (intpow2(nbpv) - 1);

  ref2ibm(&zref, BitsPerInt);

  Put3Byte(blockLength);      /*  0-2 Length of Block 4        */
  Put1Byte(Flag);             /*  3   Flag & Unused bits       */
  if (binscale < 0) binscale = 32768 - binscale;
  Put2Byte(binscale);         /*  4-5 Scale factor             */
  Put1Real(zref);             /*  6-9 Reference value          */
  Put1Byte(nbpv);             /*   10 Packing size             */

  if ( PackStart ) Put1Real(data[0]);

  *datsize  = ((datasize-PackStart)*ISEC4_NumBits + 7)/8;

  if      ( ISEC4_NumBits ==  8 )
    {
#if   defined (CRAY)
#pragma _CRI ivdep
#elif defined (SX)
#pragma vdir nodep
#elif defined (__uxp__)
#pragma loop novrec
#endif
      for ( i = PackStart; i < datasize; i++ )
	{
          fpval = ((data[i] - zref) * factor + 0.5);
	  if ( fpval > max_nbpv_pow2 ) fpval = max_nbpv_pow2;
	  if ( fpval < 0 ) fpval = 0;
	  pval = (unsigned int) fpval;
	  lGrib[z  ] = pval;
          z++;
	}
    }
  else if ( ISEC4_NumBits == 16 )
    {
#if   defined (CRAY)
#pragma _CRI ivdep
#elif defined (SX)
#pragma vdir nodep
#elif defined (__uxp__)
#pragma loop novrec
#endif
      for ( i = PackStart; i < datasize; i++ )
	{
	  fpval = ((data[i] - zref) * factor + 0.5);
	  if ( fpval > max_nbpv_pow2 ) fpval = max_nbpv_pow2;
	  if ( fpval < 0 ) fpval = 0;
	  pval = (unsigned int) fpval;
	  lGrib[z  ] = pval >>  8;
          lGrib[z+1] = pval;
	  z += 2;
	}
    }
  else if ( ISEC4_NumBits == 24 )
    {
#if   defined (CRAY)
#pragma _CRI ivdep
#elif defined (SX)
#pragma vdir nodep
#elif defined (__uxp__)
#pragma loop novrec
#endif
      for ( i = PackStart; i < datasize; i++ )
	{
          fpval = ((data[i] - zref) * factor + 0.5);
	  if ( fpval > max_nbpv_pow2 ) fpval = max_nbpv_pow2;
	  if ( fpval < 0 ) fpval = 0;
	  pval = (unsigned int) fpval;
          lGrib[z  ] =  pval >> 16;
          lGrib[z+1] =  pval >>  8;
          lGrib[z+2] =  pval;
          z += 3;
	}
    }
  else if ( ISEC4_NumBits == 32 )
    {
#if   defined (CRAY)
#pragma _CRI ivdep
#elif defined (SX)
#pragma vdir nodep
#elif defined (__uxp__)
#pragma loop novrec
#endif
      for ( i = PackStart; i < datasize; i++ )
	{
          fpval = ((data[i] - zref) * factor + 0.5);
	  if ( fpval > max_nbpv_pow2 ) fpval = max_nbpv_pow2;
	  if ( fpval < 0 ) fpval = 0;
	  pval = (unsigned int) fpval;
          lGrib[z  ] =  pval >> 24;
          lGrib[z+1] =  pval >> 16;
          lGrib[z+2] =  pval >>  8;
          lGrib[z+3] =  pval;
          z += 4;
	}
    }
  else if ( ISEC4_NumBits > 0 && ISEC4_NumBits <= 32 )
    {
      int cbits, jbits;
      unsigned int c;
      static unsigned int mask[] = {0,1,3,7,15,31,63,127,255};
    
      /* code from gribw routine flist2bitstream */

      cbits = 8;
      c = 0;
      for ( i = PackStart; i < datasize; i++ )
	{
	  /* note float -> unsigned int .. truncate */
          fpval = ((data[i] - zref) * factor + 0.5);
	  if ( fpval > max_nbpv_pow2 ) fpval = max_nbpv_pow2;
	  if ( fpval < 0 ) fpval = 0;
	  pval = (unsigned int) fpval;

	  jbits = ISEC4_NumBits;
	  while ( cbits <= jbits ) 
	    {
	      if ( cbits == 8 )
		{
		  jbits -= 8;
		  lGrib[z++] = (pval >> jbits) & 255;
		}
	      else
		{
		  jbits -= cbits;
		  lGrib[z++] = (c << cbits) + ((pval >> jbits) & mask[cbits]);
		  cbits = 8;
		  c = 0;
		}
	    }
	  /* now jbits < cbits */
	  if ( jbits )
	    {
	      c = (c << jbits) + (pval & mask[jbits]);
	      cbits -= jbits;
	    }
	}
      if (cbits != 8) lGrib[z++] = c << cbits;
    }
  else if ( ISEC4_NumBits == 0 )
    {
    }
  else
    {
      Error(func, "Unimplemented packing factor %d", ISEC4_NumBits);
    }

  if ( unused_bits >= 8 )
    Put1Byte(0);              /*  Fillbyte                     */

  *gribLen = z;

  return (0);
}


void gribEncode(int *isec0, int *isec1, int *isec2, double *fsec2, int *isec3,
		double *fsec3, int *isec4, double *fsec4, int klenp, int *kgrib,
		int kleng, int *kword, int efunc, int *kret)
{
  static char func[] = "gribEncode";
  int gribLen = 0; /* Counter of GRIB length for output */
  long isLen, pdsLen;
  GRIBPACK *lpds;
  char *CGrib;
  int fsec4size = 0;
  int numBytes;
  int bmsIncluded;
  size_t len;
  GRIBPACK *lGrib;
  int datstart, datsize, bdsstart, bdsend;
  int status = 0;

  grsdef();

  CGrib = (char *) kgrib;

  bmsIncluded = ISEC1_Sec2Or3Flag & 64;

  /* set max header len */
  len = 3000;

  /* add data len */
  numBytes = (ISEC4_NumBits+7)>>3;

  len += numBytes*klenp;

  /* add bitmap len */
  if ( bmsIncluded ) len += (klenp+7)>>3;


  lGrib = (GRIBPACK *) malloc(len*sizeof(GRIBPACK));
  if ( lGrib == NULL ) SysError(func, "No Memory!");

  isLen = 8;
  encodeIS(lGrib, &gribLen);
  lpds = &lGrib[isLen];
  pdsLen = getPdsLen(isec1);

  encodePDS(lpds, pdsLen,  isec1);
  gribLen += pdsLen;

  encodeGDS(lGrib, &gribLen, isec2, fsec2);
  /*
    ----------------------------------------------------------------
    BMS Bit-Map Section Section (Section 3)
    ----------------------------------------------------------------
  */ 
  if ( bmsIncluded )
    {
      encodeBMS(lGrib, &gribLen, fsec3, isec4, fsec4, &fsec4size);
    }
  else
    {
      fsec4size = ISEC4_NumValues;
    }

  bdsstart = gribLen;
  status = encodeBDS(lGrib, &gribLen, ISEC1_DecScaleFactor, isec2,
		     isec4, fsec4size, fsec4, &datstart, &datsize);
  if ( status )
    {
      *kret = status;
      return;
    }

  bdsend = gribLen;
  encodeES(lGrib, &gribLen);

  if ( (size_t) gribLen > len )
    Error(func, "lGrib buffer too small! len = %d  gribLen = %d", len, gribLen);

  if ( (size_t) gribLen > kleng*sizeof(int) )
    Error(func, "kgrib buffer too small! kleng = %d  gribLen = %d", kleng, gribLen);

  (void) PACK_GRIB(lGrib, (char *)CGrib, gribLen, -1L);

#if  COMPRESS == COMP_Z
#  if  defined  (HAVE_LIBZ)
      {
#   include "zlib.h"
      int status;
      int level = 6;
      Bytef *dest, *source;
      uLongf destLen, sourceLen;
      /*  z_stream strm; */

      destLen = len;
      sourceLen = datsize;
      dest = (char *) lGrib;
      source = CGrib + datstart;

      /* deflateInit2 (&strm, 6, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY); */
      /* Perform compression from the source to the destination buffer */
      status = compress2(dest, &destLen, source, sourceLen, level);
      /*     fprintf(stderr, "compress %ld %ld\n", (long) sourceLen, (long) destLen); */

      /* Check for various zlib errors */
      if (Z_BUF_ERROR==status)
	Warning(func, "overflow");
      else if (Z_MEM_ERROR==status)
	Warning(func, "deflate memory error");
      else if (Z_OK!=status)
	Warning(func, "other deflate error");

      if ( destLen < datsize )
	{
	  int zz;
	  memcpy(CGrib + datstart, dest, destLen);
	  gribLen = datstart + destLen;
	  destLen += (bdsend - bdsstart - datsize);
	  CGrib[bdsstart  ] = 255 & (destLen >> 16);
	  CGrib[bdsstart+1] = 255 & (destLen >>  8);
	  CGrib[bdsstart+2] = 255 & (destLen);

	  CGrib[gribLen++] = 0;
	  CGrib[gribLen++] = '7';
	  CGrib[gribLen++] = '7';
	  CGrib[gribLen++] = '7';
	  CGrib[gribLen++] = '7';

	  CGrib[4] = 255 & (gribLen >> 16);
	  CGrib[5] = 255 & (gribLen >>  8);
	  CGrib[6] = 255 & (gribLen);

	  zz = gribLen;
	  while ( zz & 7 ) CGrib[zz++] = 0;
	}
      }
#  else
      {
      static int libzwarn = 1;
      if ( libzwarn )
	{
	  Warning(func, "Compression disabled, zlib not available!");
	  libzwarn = 0;
	}
      }
#  endif
#elif COMPRESS == COMP_JASPER
#  if  defined  (HAVE_LIBJASPER)
      {
#   include "jasper/jasper.h"
      int status;
      char *dest, *source;
      size_t destLen, sourceLen;
      int width, height, nbits;
      int ier,rwcnt;
      jas_image_t image;
      jas_stream_t *jpcstream,*istream;
      jas_image_cmpt_t cmpt,*pcmpt;
#define MAXOPTSSIZE 1024
      char opts[MAXOPTSSIZE];

      opts[0]=(char)0; /* lossless compression !*/

      destLen = len;
      sourceLen = datsize;
      dest = (char *) lGrib;
      source = CGrib + datstart;

      width  = fsec4size/8;
      height = 8;
      nbits  = ISEC4_NumBits;

      /*     Initialize the JasPer image structure describing the grayscale */
      /*     image to encode into the JPEG2000 code stream.*/

      image.tlx_=0;
      image.tly_=0;

      image.brx_=(jas_image_coord_t)width;
      image.bry_=(jas_image_coord_t)height;

      image.numcmpts_=1;
      image.maxcmpts_=1;

      image.clrspc_=JAS_CLRSPC_SGRAY;         /* grayscale Image */
      image.cmprof_=0; 

      image.inmem_=1;

      cmpt.tlx_=0;
      cmpt.tly_=0;
      cmpt.hstep_=1;
      cmpt.vstep_=1;

      cmpt.width_=(jas_image_coord_t)width;
      cmpt.height_=(jas_image_coord_t)height;
      cmpt.type_=JAS_IMAGE_CT_COLOR(JAS_CLRSPC_CHANIND_GRAY_Y);

      cmpt.prec_=nbits;
      cmpt.sgnd_=0;
      cmpt.cps_=(nbits+7)/8;

      pcmpt=&cmpt;
      image.cmpts_=&pcmpt;

      /*    Open a JasPer stream containing the input grayscale values */

      istream=jas_stream_memopen(source, height*width*cmpt.cps_);
      cmpt.stream_=istream;

      /*    Open an output stream that will contain the encoded jpeg2000 */
      /*    code stream. */

      jpcstream=jas_stream_memopen(dest, destLen);
      
      /*     Encode image. */

      status=jpc_encode(&image, jpcstream, opts);
      if ( status != 0 )
	Warning(func, "jasper error: %d", status);

      /*    Clean up JasPer work structures. */
    
      destLen=jpcstream->rwcnt_;
      ier=jas_stream_close(istream);
      ier=jas_stream_close(jpcstream);

      /*
      fprintf(stderr, "%3d %3d %5d %2d in: %5d out: %5d\n", ISEC1_Parameter, ISEC1_Level1, fsec4size, ISEC4_NumBits, sourceLen, destLen);
      */
      if ( destLen < datsize )
	{
	  int zz;
	  memcpy(CGrib + datstart, dest, destLen);
	  gribLen = datstart + destLen;
	  destLen += (bdsend - bdsstart - datsize);
	  CGrib[bdsstart  ] = 255 & (destLen >> 16);
	  CGrib[bdsstart+1] = 255 & (destLen >>  8);
	  CGrib[bdsstart+2] = 255 & (destLen);

	  CGrib[gribLen++] = 0;
	  CGrib[gribLen++] = '7';
	  CGrib[gribLen++] = '7';
	  CGrib[gribLen++] = '7';
	  CGrib[gribLen++] = '7';

	  CGrib[4] = 255 & (gribLen >> 16);
	  CGrib[5] = 255 & (gribLen >>  8);
	  CGrib[6] = 255 & (gribLen);

	  zz = gribLen;
	  while ( zz & 7 ) CGrib[zz++] = 0;
	}
      }
#endif
#endif

  free(lGrib);

  ISEC0_GRIB_Len     = gribLen;
  ISEC0_GRIB_Version = 1;

  *kword = gribLen / sizeof(int);
  if ( (size_t) gribLen != *kword * sizeof(int) ) *kword += 1;

  *kret = status;
}



int gribVersion(unsigned char *is, size_t buffersize)
{
  static char func[] = "gribVersion";

  if ( buffersize < 8 )
    Error(func, "Buffer too small (current size %d)!\n", (int) buffersize);

  return (IS_GRIB_Version);
}


static double GET_Real(unsigned char *grib)
{
  int iexp, imant;

  iexp  = GET_UINT1(grib[0]);
  imant = GET_UINT3(grib[1], grib[2], grib[3]);

  return (decfp2(iexp, imant));
}


static int decodeIS(unsigned char *is, int *isec0, int *iret)
{
  static char func[] = "decodeIS";
  int isLen = 0;
  int grib1offset;
  int lgrib = FALSE, lbudg = FALSE, ltide = FALSE;

  /*
    Octets 1 - 4 : The letters G R I B.
    Four 8 bit fields.
  */
  /*
    Check letters -> GRIB, BUDG or TIDE.
  */
  /*
    Check that 'GRIB' is found where expected.
  */
  if ( is[0] == 'G' && is[1] == 'R' && is[2] == 'I' && is[3] == 'B' ) lgrib = TRUE;
  /*
    ECMWF pseudo-grib data uses 'BUDG' and 'TIDE'.
  */
  if ( is[0] == 'B' && is[1] == 'U' && is[2] == 'D' && is[3] == 'G' ) lbudg = TRUE;
  if ( is[0] == 'T' && is[1] == 'I' && is[2] == 'D' && is[3] == 'E' ) ltide = TRUE;
  /*
    Data is not GRIB or pseudo-grib.
  */
  if ( lgrib == FALSE && lbudg == FALSE && ltide == FALSE )
    {
      *iret = 305;
      gprintf(func, "Input data is not GRIB or pseudo-grib.");
      gprintf(func, "Return code = %d", *iret);
    }
  if ( lbudg == TRUE || ltide == TRUE )
    {
      *iret = 305;
      gprintf(func, "Pseudo-grib data unsupported.");
      gprintf(func, "Return code = %d", *iret);
    }

  /*
    Octets 5 - 7 : Length of message.
    One 24 bit field.
  */
  ISEC0_GRIB_Len = IS_GRIB_Len;
  /*
    Octet 8 : GRIB Edition Number.
    One 8 bit field.
  */
  ISEC0_GRIB_Version = IS_GRIB_Version;

  if ( ISEC0_GRIB_Version > 1 )
    Error(func, "GRIB version %d unsupported!", ISEC0_GRIB_Version);

  grib1offset = ISEC0_GRIB_Version * 4;

  isLen = 4 + grib1offset;

  return (isLen);
}


static void decodePDS_ECMWF_local_Extension_1(unsigned char *pds, int *isec1)
{
  isec1[36] = GET_UINT1(pds[40]);         /* extension identifier       */
  isec1[37] = GET_UINT1(pds[41]);         /* Class                      */
  isec1[38] = GET_UINT1(pds[42]);         /* Type                       */
  isec1[39] = GET_UINT2(pds[43],pds[44]); /* Stream                     */
  /* isec1[40] = GET_UINT4(pds[45],pds[46],pds[47],pds[48]); */
  memcpy((char*) &isec1[40], &pds[45], 4);
  isec1[41] = GET_UINT1(pds[49]);         /* Forecast number            */
  isec1[42] = GET_UINT1(pds[50]);         /* Total number of forecasts  */
}


static void decodePDS_DWD_local_Extension_254(unsigned char *pds, int *isec1)
{
  int i, isvn;

  isec1[36] = GET_UINT1(pds[40]); /* extension identifier */
  for ( i = 0; i < 11; i++) 
    { 
      isec1[37+i] =  GET_UINT1(pds[41+i]);
    } 

  isvn = GET_UINT2(pds[52],pds[53]);
  
  isec1[48] =  isvn % 0x8000;              /* DWD experiment identifier            */
  isec1[49] =  isvn >> 15;                 /* DWD run type (0=main, 2=ass, 3=test) */

}


static void decodePDS_DWD_local_Extension_253(unsigned char *pds, int *isec1)
{
  int i, isvn;

  isec1[36] = GET_UINT1(pds[40]); /* extension identifier */
  for ( i = 0; i < 11; i++) 
    { 
      isec1[37+i] =  GET_UINT1(pds[41+i]);
    } 

  isvn = GET_UINT2(pds[52],pds[53]);
  
  isec1[48] =  isvn % 0x8000;              /* DWD experiment identifier            */
  isec1[49] =  isvn >> 15;                 /* DWD run type (0=main, 2=ass, 3=test) */
  isec1[50] =  GET_UINT1(pds[54]);         /* User id, specified by table          */
  isec1[51] =  GET_UINT2(pds[55],pds[56]); /* Experiment identifier                */
  isec1[52] =  GET_UINT2(pds[57],pds[58]); /* Ensemble identification by table     */
  isec1[53] =  GET_UINT2(pds[59],pds[60]); /* Number of ensemble members           */
  isec1[54] =  GET_UINT2(pds[61],pds[62]); /* Actual number of ensemble member     */
  isec1[55] =  GET_UINT1(pds[63]);         /* Model major version number           */
  isec1[56] =  GET_UINT1(pds[64]);         /* Model minor version number           */

}


static int decodePDS(unsigned char *pds, int *isec0, int *isec1)
{
  static char func[] = "decodePDS";
  int pdsLen;

  pdsLen = PDS_Len;

  ISEC1_CodeTable      = PDS_CodeTable;
  ISEC1_CenterID       = PDS_CenterID;
  ISEC1_ModelID        = PDS_ModelID;
  ISEC1_GridDefinition = PDS_GridDefinition;
  ISEC1_Sec2Or3Flag    = PDS_Sec2Or3Flag;
  ISEC1_Parameter      = PDS_Parameter;
  ISEC1_LevelType      = PDS_LevelType;

  if ( (ISEC1_LevelType !=  20) && 
       (ISEC1_LevelType != LTYPE_99)        && 
       (ISEC1_LevelType != LTYPE_ISOBARIC)  && 
       (ISEC1_LevelType != LTYPE_ALTITUDE)  && 
       (ISEC1_LevelType != LTYPE_HEIGHT)    && 
       (ISEC1_LevelType != LTYPE_SIGMA)     && 
       (ISEC1_LevelType != LTYPE_HYBRID)    && 
       (ISEC1_LevelType != LTYPE_LANDDEPTH) && 
       (ISEC1_LevelType != LTYPE_ISENTROPIC) && 
       (ISEC1_LevelType != 115) && 
       (ISEC1_LevelType != 117) && 
       (ISEC1_LevelType != 125) && 
       (ISEC1_LevelType != 127) && 
       (ISEC1_LevelType != LTYPE_SEADEPTH)  && 
       (ISEC1_LevelType != 210) )
    {
      ISEC1_Level1 = PDS_Level1;
      ISEC1_Level2 = PDS_Level2;
    }
  else
    {
      ISEC1_Level1 = PDS_Level;
      ISEC1_Level2 = 0;
    }

  /* ISEC1_Year        = PDS_Year; */
  ISEC1_Month          = PDS_Month;
  ISEC1_Day            = PDS_Day;
  ISEC1_Hour           = PDS_Hour;
  ISEC1_Minute         = PDS_Minute;
  ISEC1_TimeUnit       = PDS_TimeUnit;
  ISEC1_TimePeriod1    = PDS_TimePeriod1;
  ISEC1_TimePeriod2    = PDS_TimePeriod2;
  ISEC1_TimeRange      = PDS_TimeRange;
  ISEC1_AvgNum         = PDS_AvgNum;
  ISEC1_AvgMiss        = PDS_AvgMiss;

  if ( ISEC0_GRIB_Version == 1 )
    {
      ISEC1_Year           = PDS_Year;
      ISEC1_Century        = PDS_Century;
      ISEC1_SubCenterID    = PDS_Subcenter;
      ISEC1_DecScaleFactor = PDS_DecimalScale;
    }
  else
    {
      int year;
      year                 = GET_UINT1(pds[12]);
      if ( year <= 100 )
	{
	  ISEC1_Year       = year;
	  ISEC1_Century    = 1;
	}
      else
	{
	  ISEC1_Year       = year%100;
	  ISEC1_Century    = 1 + (year-ISEC1_Year)/100;
	}
      ISEC1_SubCenterID    = 0;
      ISEC1_DecScaleFactor = 0;
    }

  if ( ISEC1_Year < 0 )
    {
      ISEC1_Year    = -ISEC1_Year;
      ISEC1_Century = -ISEC1_Century;
    }

  ISEC1_LocalFLag = 0;
  if ( pdsLen > 28 )
    {
      int localextlen;
      localextlen = pdsLen-28;

      if ( localextlen > 4000 )
	{
	  Warning(func, "PDS larger than 4000 bytes not supported!\n");
	}
      else
	{
	  ISEC1_LocalFLag = 1;

	  if ( ISEC1_CenterID == 78 )
	    {
	      if ( pds[40] == 254 ) 
		{
		  decodePDS_DWD_local_Extension_254(pds, isec1);
		}
	      else if ( pds[40] == 253 )
		{ 
		  decodePDS_DWD_local_Extension_253(pds, isec1);
		}
	    }
	  else if ( (ISEC1_CenterID    == 98 && ISEC1_LocalFLag ==  1) ||
		    (ISEC1_SubCenterID == 98 && ISEC1_LocalFLag ==  1) ||
		    (ISEC1_CenterID    ==  7 && ISEC1_SubCenterID == 98) )
	    {
	      if ( pds[40] == 1 )
		decodePDS_ECMWF_local_Extension_1(pds, isec1);
	    }
	  else
	    {
	      int i;
	      for ( i = 0; i < localextlen; i++ )
		{
		  isec1[24+i] = pds[28+i];
		}
	    }
	}
    }

  return (pdsLen);
}


static int decodeGDS(unsigned char  *gds, int *isec0, int *isec2, double *fsec2, int dfunc)
{
  static char func[] = "decodeGDS";
  int imisng;
  int  ReducedGrid = FALSE, VertCoorTab = FALSE;
  int  locnv = 0, locnl;
  int  jlenl;
  int  i;
  int iexp, imant;
  int ipvpl, ipl;
  int gdsLen = 0;
#if defined (SX)
  unsigned char *igrib;
  GRIBPACK *lgrib = NULL;
  size_t lGribLen = 0;
#endif

  imisng = 0;

  memset(isec2, 0, 22*sizeof(int));

  gdsLen = GDS_Len;

  ipvpl = GDS_PVPL;
  if ( ipvpl == 0 ) ipvpl = 255;

  if ( ipvpl != 255 )
    { /* Either vct or reduced grid */
      if ( GDS_NV != 0 )
	{ /* we have vct */
	  VertCoorTab = TRUE;
	  ipl =  4*GDS_NV + ipvpl - 1;
	  if ( ipl < gdsLen )
	    {
	      ReducedGrid = TRUE;
	    }
	}
      else
	{
	  VertCoorTab = FALSE;
	  ReducedGrid = TRUE;
	}
      /*	  ReducedGrid = (gdsLen - 32 - 4*GDS_NV); */
    }
 
  if ( ISEC0_GRIB_Version == 0 )
    {
      if ((gdsLen - 32) > 0) VertCoorTab = TRUE;
      else                   VertCoorTab = FALSE;
    }
  
  if ( ReducedGrid )
    {
      locnl = GDS_PVPL - 1 + (VertCoorTab * 4 * GDS_NV);
      jlenl = (gdsLen - locnl)  >> 1;
      if ( jlenl == GDS_NumLat )
	{
	  ISEC2_Reduced = TRUE;
	  for ( i = 0; i < jlenl; i++ )
	    ISEC2_RowLon(i) = GET_UINT2(gds[locnl+2*i], gds[locnl+2*i+1]);
	}
      else
	{
	  ReducedGrid = FALSE;
	}
    }

  ISEC2_GridType = GDS_GridType;

  /*
     Gaussian grid definition.
  */
  if ( ISEC2_GridType == GTYPE_LATLON    ||
       ISEC2_GridType == GTYPE_GAUSSIAN  ||
       ISEC2_GridType == GTYPE_LATLON_ROT )
    {
      if ( ! ReducedGrid ) ISEC2_NumLon = GDS_NumLon;
      ISEC2_NumLat    = GDS_NumLat;
      ISEC2_FirstLat  = GDS_FirstLat;
      ISEC2_FirstLon  = GDS_FirstLon;
      ISEC2_ResFlag   = GDS_ResFlag;
      ISEC2_LastLat   = GDS_LastLat;
      ISEC2_LastLon   = GDS_LastLon;
      ISEC2_LonIncr   = GDS_LonIncr;

      ISEC2_NumPar    = GDS_NumPar;
      ISEC2_ScanFlag  = GDS_ScanFlag;
      if ( ISEC2_GridType == GTYPE_LATLON_ROT )
	{
	  ISEC2_LatSP     = GDS_LatSP;
	  ISEC2_LonSP     = GDS_LonSP;
	  FSEC2_RotAngle  = GDS_RotAngle;
	}
      /*
	if ( Lons != Longitudes || Lats != Latitudes )
	Error(func, "Latitude/Longitude Conflict");
      */
    }
  else if ( ISEC2_GridType == GTYPE_GAUSSIAN     ||
	    ISEC2_GridType == GTYPE_GAUSSIAN_ROT ||
	    ISEC2_GridType == GTYPE_GAUSSIAN_STR ||
	    ISEC2_GridType == GTYPE_GAUSSIAN_ROTSTR )
    {
      /*
      iret = decodeGDS_GG(gds, gdspos, isec0, isec2, imisng);
      */
    }
  else if ( ISEC2_GridType == GTYPE_LATLON     ||
	    ISEC2_GridType == GTYPE_LATLON_ROT ||
	    ISEC2_GridType == GTYPE_LATLON_STR ||
	    ISEC2_GridType == GTYPE_LATLON_ROTSTR )
    {
      /*
      iret = decodeGDS_LL(gds, gdspos, isec0, isec2, imisng);
      */
    }
  else if ( ISEC2_GridType == GTYPE_LCC )
    {
      ISEC2_NumLon    = GDS_NumLon;
      ISEC2_NumLat    = GDS_NumLat;
      ISEC2_FirstLat  = GDS_FirstLat;
      ISEC2_FirstLon  = GDS_FirstLon;
      ISEC2_ResFlag   = GDS_ResFlag;
      ISEC2_LoV       = GDS_LoV;
      ISEC2_Xinc      = GDS_Xinc;
      ISEC2_Yinc      = GDS_Yinc;
      ISEC2_LatS1     = GDS_LatS1;
      ISEC2_LatS2     = GDS_LatS2;
      ISEC2_ScanFlag  = GDS_ScanFlag;
    }
  else if ( ISEC2_GridType == GTYPE_SPECTRAL )
    {
      ISEC2_PentaJ  = GDS_PentaJ; /* Truncation */
      ISEC2_PentaK  = GDS_PentaK;
      ISEC2_PentaM  = GDS_PentaM;
      ISEC2_RepType = GDS_RepType;
      ISEC2_RepMode = GDS_RepMode;
      isec2[ 6] = 0;
      isec2[ 7] = 0;
      isec2[ 8] = 0;
      isec2[ 9] = 0;
      isec2[10] = 0;
      /*
      iret = decodeGDS_SH(gds, gdspos, isec0, isec2, imisng);
      */
    }
  else if ( ISEC2_GridType == GTYPE_GME )
    {
      ISEC2_GME_NI2    = GDS_GME_NI2;
      ISEC2_GME_NI3    = GDS_GME_NI3;
      ISEC2_GME_ND     = GDS_GME_ND;
      ISEC2_GME_NI     = GDS_GME_NI;
      ISEC2_GME_AFlag  = GDS_GME_AFlag;
      ISEC2_GME_LatPP  = GDS_GME_LatPP;
      ISEC2_GME_LonPP  = GDS_GME_LonPP;
      ISEC2_GME_LonMPL = GDS_GME_LonMPL;
      ISEC2_GME_BFlag  = GDS_GME_BFlag;
      /*
      iret = decodeGDS_TR(gds, gdspos, isec0, isec2, imisng);
      */
    }
  else
    {
      ISEC2_NumLon = GDS_NumLon;
      ISEC2_NumLat = GDS_NumLat;
      Message(func, "Gridtype %d unsupported", ISEC2_GridType);
    }

  /*    vertical coordinate parameters for hybrid levels.     */
  /*    get number of vertical coordinate parameters, if any. */

  ISEC2_NumVCP = 0;

  isec2[17] = 0;
  isec2[18] = 0;

  if ( VertCoorTab == TRUE )
    {
      if ( ISEC0_GRIB_Version  == 0 )
	{
	  locnv = 32;
	  ISEC2_NumVCP = (gdsLen - 32) >> 2;
	}
      else
	{
	  locnv = GDS_PVPL - 1;
	  ISEC2_NumVCP = GDS_NV;
	}
#if defined (SX)
      lGribLen = 4*ISEC2_NumVCP;	      
      lgrib    = (GRIBPACK *) malloc(lGribLen*sizeof(GRIBPACK));

      igrib = &gds[locnv];
      if ( ISEC2_NumVCP > 0 ) (void) UNPACK_GRIB(igrib, lgrib, lGribLen, -1L);
      for ( i = 0; i < ISEC2_NumVCP; i++ )
	{
	  iexp   = (lgrib[4*i  ]);
	  imant  =((lgrib[4*i+1]) << 16) +
	          ((lgrib[4*i+2]) <<  8) +
	           (lgrib[4*i+3]);
	  fsec2[10+i] = POW_2_M24 * imant * pow(16.0, (double)(iexp - 64));
	}

      free(lgrib);
#else
      for ( i = 0; i < ISEC2_NumVCP; i++ )
	{
	  iexp   = (gds[locnv+4*i  ]);
	  imant  =((gds[locnv+4*i+1]) << 16) +
	          ((gds[locnv+4*i+2]) <<  8) +
	           (gds[locnv+4*i+3]);
	  fsec2[10+i] = decfp2(iexp,imant);
	}
#endif
    }

  return (gdsLen);
}


static int decodeBDS(int decscale, unsigned char *bds, int *isec2, int *isec4, double *fsec4, int fsec4len, int dfunc, int *iret)
{
  static char func[] = "decodeBDS";
  unsigned char *igrib;
  GRIBPACK *lgrib = NULL;
  int cplx, jup, kup, mup;
  int locnd;
  int jlend, jlenc;
  int i;
  int bds_flag, bds_rep, jscale, imiss;
  int bds_ubits;
  int ioff;
  int iexp, imant;
  int pcStart, pcScale;
  int zoff;
  int bds_aflag;
  double fmin = 0., zscale = 0.;
  double *fpdata = fsec4;

  int bdsLen;

  *iret = 0;
  igrib = bds;

  memset(isec4, 0, 42*sizeof(int));

  /* get length of binary data block. */

  bdsLen = BDS_Len;

  /* 4 bit flag / 4 bit count of unused bits at end of block octet. */

  bds_flag = BDS_Flag;

  bds_aflag = (bds_flag >> 4)&1; /* compress */

  /* 0------- grid point           */
  /* 1------- spherical harmonics  */

  bds_rep  = bds_flag >> 7;

  if ( bds_rep == 0 ) isec4[2] = 0;
  else             isec4[2] = 128;

  /* -0------  simple packing */
  /* -1------ complex packing */

  cplx  = (bds_flag >> 6)&1;

  if ( cplx > 0 ) isec4[3] = 64;
  else            isec4[3] =  0;

  /* ---0---- No additional flags */
  /* ---1---- No additional flags */

  if ( (bds_flag >> 4)&1 )
    { isec4[5] = 16; isec4[6] = BDS_Z; zoff = 12; }
  else
    { isec4[5] =  0; isec4[6] = 0;     zoff = 0; }

  if ( isec4[5] ) isec4[6] = BDS_Z; /* compression type: sz = 128 */
  else            isec4[6] = 0;

  /* ----++++ number of unused bits at end of section) */

  bds_ubits = bds_flag & 15;
  
  /* scale factor (2 bytes) */;

  jscale = BDS_BinScale;

  /* check for missing data indicators. */

  iexp  = bds[ 6];
  imant = GET_UINT3(bds[ 7], bds[ 8], bds[ 9]);

  imiss = (jscale == 65535 && iexp == 255 && imant == 16777215);

  /* convert reference value and scale factor. */

  if ( imiss == 0 )
    {
      fmin = BDS_RefValue;

      if ( jscale < 0 )
	zscale = 1.0/intpow2(-jscale);
      else
	zscale = intpow2(jscale);
    }

  /* get number of bits in each data value. */

  ISEC4_NumBits = BDS_NumBits;

  /* octet number of start of packed data */
  /* calculated from start of block 4 - 1 */

  locnd = zoff + 11;

  /* if data is in spherical harmonic form, distinguish   */
  /* between simple/complex packing (cplx = 0/1)          */

  if ( bds_rep == 1 && cplx == 0 )
    {
      /*    no unpacked binary data present */

      jup = kup = mup = 0;

      /*    octet number of start of packed data */
      /*    calculated from start of block 4 - 1 */

      locnd = zoff + 15;

      /*    get real (0,0) coefficient in grib format and     */
      /*    convert to floating point.                        */

      if ( dfunc != 'J' )
	{
	  if ( imiss ) *fpdata++ = 0.0;
	  else         *fpdata++ = BDS_RealCoef;
	}
    }

  ioff = bds_rep;

  if ( bds_rep == 1 && cplx == 1 )
    {
      /*    scaling factor */
      isec4[16] = BDS_Power;

      /*    pentagonal resolution parameters of the */
      /*    unpacked section of data field          */

      jup = bds[zoff+15];
      kup = bds[zoff+16];
      mup = bds[zoff+17];

      isec4[zoff+17] = jup;
      isec4[zoff+18] = kup;
      isec4[zoff+19] = mup;

      /*    unpacked binary data */

      locnd = zoff + 18;

      if ( dfunc != 'J' )
	for (i = 0; i < ((jup+1)*(jup+2)); i++)
	  {
	    iexp   = (bds[locnd+4*i  ]);
	    imant  =((bds[locnd+4*i+1]) << 16) +
	            ((bds[locnd+4*i+2]) <<  8) +
                     (bds[locnd+4*i+3]);

	    if ( imiss ) *fpdata++ = 0.0;
	    else         *fpdata++ = decfp2(iexp,imant);
	  }

      locnd = zoff + 18 + 4*(jup+1)*(jup+2);
      ioff = (jup+1)*(jup+2);
    }

  /* Decode data values to floating point and store in fsec4. */
  /* first calculate the number of data values.                */
  /* Take into account that spherical harmonics can be packed  */
  /* simple (cplx = 0) or complex (cplx = 1)                   */

  jlend = bdsLen - locnd;
  if ( ISEC4_NumBits == 0 )
    {
      if ( jlend > 1 )
	Error(func, "Number of bits per data value = 0");

      jlend = ISEC2_NumLon*ISEC2_NumLat;
    }
  else
    jlend = (jlend * 8 - bds_ubits) / ISEC4_NumBits;


  ISEC4_NumValues        = jlend + ioff;
  ISEC4_NumNonMissValues = 0;

  if ( bds_aflag )
    {
      /*
      if ( bds_rep == 0 ) 
	ISEC4_NumValues = ISEC2_NumLon*ISEC2_NumLat;
      else
	ISEC4_NumValues = (ISEC2_PentaJ+1)*(ISEC2_PentaJ+2);
      */
      ISEC4_NumValues = GET_UINT3(bds[17],bds[18],bds[19])*8/ISEC4_NumBits;

      if ( bds_rep == 1 ) ISEC4_NumValues++;
    }

  if ( dfunc == 'J' ) return (bdsLen);

  /* check length of output array. */
  
  if ( ! (dfunc == 'J') )
    if ( jlend+ioff > fsec4len )
      {
	*iret = 710;
	gprintf(func, " Output array too small. Length = %d", fsec4len);
	gprintf(func, " Number of values = %d", jlend+ioff);
	gprintf(func, " Return code =  %d", *iret);
	return (0);
      }

  if ( imiss ) memset((char *)fpdata, 0, jlend*sizeof(double));
  else
    {
      igrib += locnd;
      jlenc = jlend * ISEC4_NumBits / 8;
      if ( ISEC4_NumBits ==  8 || ISEC4_NumBits == 16 ||
	   ISEC4_NumBits == 24 || ISEC4_NumBits == 32 )
	{
	  if ( jlenc > 0 ) 
	    {
	      lgrib = (GRIBPACK *) malloc(jlenc*sizeof(GRIBPACK));
	      if ( lgrib == NULL ) SysError(func, "No Memory!");

	      (void) UNPACK_GRIB(igrib, lgrib, jlenc, -1L);
	      /*
		for ( i = 0; i < jlenc; ++i ) lgrib[i] = (long) igrib[i];
	      */
	    }
	}

      if ( ISEC4_NumBits ==  0 )
	{
	  for ( i = 0; i < jlend; i++ )
	    fpdata[i] = fmin;
	}
      else if ( ISEC4_NumBits ==  8 )
         for ( i = 0; i < jlend; i++ )
	   {
             fpdata[i] = fmin + zscale * (int)lgrib[i];
	   }
      else if ( ISEC4_NumBits == 16 )
         for ( i = 0; i < jlend; i++ )
	   {
             fpdata[i] = fmin + zscale *
                         (((int)lgrib[2*i  ] <<  8) +  (int)lgrib[2*i+1]);
	   }
      else if ( ISEC4_NumBits == 24 )
         for ( i = 0; i < jlend; i++ )
	   {
             fpdata[i] = fmin + zscale *
                         (((int)lgrib[3*i  ] << 16) + ((int)lgrib[3*i+1] <<  8) +
                           (int)lgrib[3*i+2]);
	   }
      else if ( ISEC4_NumBits == 32 )
         for ( i = 0; i < jlend; i++ )
	   {
             fpdata[i] = fmin + zscale *
                         ((lgrib[4*i  ] << 24) + (lgrib[4*i+1] << 16) +
                          (lgrib[4*i+2] <<  8) +  lgrib[4*i+3]);
	   }
      else if ( ISEC4_NumBits <= 25 )
	{
	  unsigned char *bits = igrib;
	  unsigned int jmask;
	  int tbits = 0;
	  int n_bits = ISEC4_NumBits;
	  int t_bits = 0;

	  jmask = (1 << n_bits) - 1;
	  /* code from wgrib routine BDS_unpack */
	  for ( i = 0; i < jlend; i++ )
	    {
	      while ( t_bits < n_bits )
		{
		  tbits = (tbits * 256) + *bits++;
		  t_bits += 8;
                }
	      t_bits -= n_bits;
	      fpdata[i] = (tbits >> t_bits) & jmask;
            }
	  /* at least this vectorizes :) */
	  for ( i = 0; i < jlend; i++ )
	    fpdata[i] = fmin + zscale*fpdata[i];
	}
      else
	{
	  fprintf(stderr," Unimplemented packing factor %d\n", ISEC4_NumBits);
	  exit(EXIT_FAILURE);
	}
    }

  if ( lgrib ) free(lgrib);

  if ( bds_rep == 1 && cplx == 1 )
    {
      pcStart = isec4[19];
      pcScale = isec4[16];
      scatterComplex(fsec4, pcStart, ISEC2_PentaJ, ISEC4_NumValues);
      scaleComplex(fsec4, pcStart, pcScale, ISEC2_PentaJ);
    }


  if ( decscale )
    {
      double scale = pow(10.0, (double)-decscale);
      for ( i = 0; i < ISEC4_NumValues; i++ ) fsec4[i] *= scale;
    }

  return (bdsLen);
}


void gribDecode(int *isec0, int *isec1, int *isec2, double *fsec2, int *isec3,
		double *fsec3, int *isec4, double *fsec4, int fsec4len, int *kgrib,
		int kleng, int *kword, int dfunc, int *iret)
{
  static char func[] = "gribDecode";
  UCHAR *is = NULL, *pds = NULL, *gds = NULL, *bms = NULL, *bds = NULL;
  int isLen = 0, pdsLen = 0, gdsLen = 0, bmsLen = 0, bdsLen = 0, esLen = 0;
  int gribLen = 0;
  int gdsIncluded = FALSE;
  int bmsIncluded = FALSE;
  int bitmapSize = 0;
  int imaskSize = 0;
  int ldebug = FALSE;
  int llarge = FALSE, l_iorj = FALSE;
  int lsect2 = FALSE, lsect3 = FALSE;

  *iret = 0;

  grsdef();

  ISEC2_Reduced = FALSE;

  /*
    ----------------------------------------------------------------
    IS Indicator Section (Section 0)
    ----------------------------------------------------------------
  */
  is = (unsigned char *) &kgrib[0];

  isLen = decodeIS(is, isec0, iret);

  /*
    If count is negative, have to rescale by factor of -120.
    This is a fixup to get round the restriction on product lengths
    due to the count being only 24 bits. It is only possible because
    the (default) rounding for GRIB products is 120 bytes.
  */
  if (  ISEC0_GRIB_Len < 0 )
    {
      if ( ldebug )
	gprintf(func, "Special case, negative length multiplied by -120");
      llarge = TRUE;
      ISEC0_GRIB_Len *= (-120);
    }
  /*
    When decoding or calculating length, previous editions
    of the GRIB code must be taken into account.

    In the table below, covering sections 0 and 1 of the GRIB
    code, octet numbering is from the beginning of the GRIB
    message;
    * indicates that the value is not available in the code edition;
    R indicates reserved, should be set to 0;
    Experimental edition is considered as edition -1.

    GRIB code edition -1 has fixed length of 20 octets for
    section 1, the length not included in the message.
    GRIB code edition 0 has fixed length of 24 octets for
    section 1, the length being included in the message.
    GRIB code edition 1 can have different lengths for section
    1, the minimum being 28 octets, length being included in
    the message.

                                         Octet numbers for code
                                                  editions

                 Contents.                   -1      0      1
                 ---------                ----------------------
       Letters GRIB                          1-4    1-4    1-4
       Total length of GRIB message.          *      *     5-7
       GRIB code edition number               *      *      8
       Length of Section 1.                   *     5-7    9-11
       Reserved octet (R).                    *      8(R)   *
       Version no. of Code Table 2.           *      *     12
       Identification of centre.              5      9     13
       Generating process.                    6     10     14
       Grid definition .                      7     11     15
       Flag (Code Table 1).                   8     12     16
       Indicator of parameter.                9     13     17
       Indicator of type of level.           10     14     18
       Height, pressure etc of levels.      11-12  15-16  19-20
       Year of century.                      13     17     21
       Month.                                14     18     22
       Day.                                  15     19     23
       Hour.                                 16     20     24
       Minute.                               17     21     25
       Indicator of unit of time.            18     22     26
       P1 - Period of time.                  19     23     27
       P2 - Period of time                  20(R)   24     28
       or reserved octet (R).
       Time range indicator.                21(R)   25     29
       or reserved octet (R).
       Number included in average.       22-23(R)  26-27  30-31
       or reserved octet (R).
       Number missing from average.         24(R)  28(R)   32
       or reserved octet (R).
       Century of data.                       *      *     33
       Designates sub-centre if not 0.        *      *     34
       Decimal scale factor.                  *      *    35-36
       Reserved. Set to 0.                    *      *    37-48
       (Need not be present)
       For originating centre use only.       *      *    49-nn
       (Need not be present)

    Identify which GRIB code edition is being decoded.

    In GRIB edition 1, the edition number is in octet 8.
    In GRIB edition 0, octet 8 is reserved and set to 0.
    In GRIB edition -1, octet 8 is a flag field and can have a
    a valid value of 0, 1, 2 or 3.

    However, GRIB edition number 0 has a fixed
    length of 24, included in the message, for section 1, so
    if the value extracted from octets 5-7 is 24 and that from
    octet 8 is 0, it is safe to assume edition 0 of the code.

  */
  if ( ISEC0_GRIB_Len == 24 && ISEC0_GRIB_Version == 0 )
    {
      /*
	Set length of GRIB message to missing data value.
      */
      ISEC0_GRIB_Len = 0;
    }
  /*
    If Grib Edition 1 and only length is required, go to section 9.
  */
  if ( dfunc == 'L' ) goto LABEL900;

  /*
    ----------------------------------------------------------------
    PDS Product Definition Section (Section 1)
    ----------------------------------------------------------------
  */ 
  pds = is + isLen;

  pdsLen = decodePDS(pds, isec0, isec1);

  /*
    ----------------------------------------------------------------
    GDS Grid Description Section (Section 2)
    ----------------------------------------------------------------
  */
  gdsIncluded = ISEC1_Sec2Or3Flag & 128;

  if ( gdsIncluded )
    {
      gds = is + isLen + pdsLen;

      gdsLen = decodeGDS(gds, isec0, isec2, fsec2, dfunc);
    }

  /*
    ----------------------------------------------------------------
    BMS Bit-Map Section Section (Section 3)
    ----------------------------------------------------------------
  */ 
  bmsIncluded = ISEC1_Sec2Or3Flag & 64;

  isec3[0] = 0;
  if ( bmsIncluded )
    {
      bms = is + isLen + pdsLen + gdsLen;

      bmsLen = BMS_Len;
      imaskSize = (bmsLen - 6)<<3;
      bitmapSize = imaskSize - BMS_UnusedBits;
      /*
      fprintf(stderr," bitmapSize = %d %d %d\n", bitmapSize, imaskSize, BMS_UnusedBits);
      */
    }

  /*
    ----------------------------------------------------------------
    BDS Binary Data Section (Section 4)
    ----------------------------------------------------------------
  */
  bds = is + isLen + pdsLen + gdsLen + bmsLen;

  bdsLen = decodeBDS(ISEC1_DecScaleFactor, bds, isec2, isec4, fsec4, fsec4len, dfunc, iret);

  if ( *iret != 0 ) return;

  ISEC4_NumNonMissValues = ISEC4_NumValues;

  if ( bitmapSize > 0 )
    {
      /* ISEC4_NumNonMissValues = ISEC4_NumValues; */
      ISEC4_NumValues        = bitmapSize;

      if ( dfunc != 'J' || bitmapSize == ISEC4_NumNonMissValues )
	{
	  int i, j;
	  GRIBPACK bitmap;
	  GRIBPACK *imask;

	  /*
	  unsigned char *bitmap;
	  bitmap = BMS_Bitmap;
	  j = ISEC4_NumNonMissValues;
	  for ( i = ISEC4_NumValues-1; i >= 0; i-- )
	    {
	      if ( (bitmap[i/8]>>(7-(i&7)))&1 )
		fsec4[i] = fsec4[--j];
	      else
		fsec4[i] = FSEC3_MissVal;
	    }
	  */

	  imask = (GRIBPACK *) malloc(imaskSize*sizeof(GRIBPACK));

	  (void) UNPACK_GRIB(BMS_Bitmap, imask, imaskSize/8, -1L);

#if defined (CRAY)
#pragma _CRI ivdep
#endif
#if defined (SX)
#pragma vdir nodep
#endif
#ifdef __uxpch__
#pragma loop novrec
#endif
	  for ( i = imaskSize/8-1; i >= 0; i-- )
	    {
	      bitmap = imask[i];
	      imask[i*8+0] = 1 & (bitmap >> 7);
	      imask[i*8+1] = 1 & (bitmap >> 6);
	      imask[i*8+2] = 1 & (bitmap >> 5);
	      imask[i*8+3] = 1 & (bitmap >> 4);
	      imask[i*8+4] = 1 & (bitmap >> 3);
	      imask[i*8+5] = 1 & (bitmap >> 2);
	      imask[i*8+6] = 1 & (bitmap >> 1);
	      imask[i*8+7] = 1 & (bitmap);
	    }

	  j = 0;
	  for ( i = 0; i < ISEC4_NumValues; i++ )
	    if ( imask[i] ) j++;

	  if ( ISEC4_NumNonMissValues != j )
	    {
	      if ( dfunc != 'J' )
		Warning(func, "Bitmap (%d) and data (%d) section differ, using bitmap section!",
			j, ISEC4_NumNonMissValues);

	      ISEC4_NumNonMissValues = j;
	    }

	  if ( dfunc != 'J' )
	    {
#if defined (CRAY)
#pragma _CRI ivdep
#endif
#if defined (SX)
#pragma vdir nodep
#endif
#ifdef __uxpch__
#pragma loop novrec
#endif
	      for ( i = ISEC4_NumValues-1; i >= 0; i-- )
		fsec4[i] = imask[i] ? fsec4[--j] : FSEC3_MissVal;
	    }

	  free(imask);
	}
    }

  if ( dfunc == 'R' && ISEC2_Reduced )
    {
      int nlon, nlat;
      int lsect3, lperio, lveggy;
      ISEC2_Reduced = 0;
      ISEC2_NumLon = ISEC2_NumLat*2;
      nlon = ISEC2_NumLon;
      nlat = ISEC2_NumLat;
      ISEC4_NumValues = nlon*nlat;

      lsect3 = bitmapSize > 0;
      lperio = 1;
      lveggy = (ISEC1_CodeTable == 128) && (ISEC1_CenterID == 98) && 
	       ((ISEC1_Parameter == 27) || (ISEC1_Parameter == 28) || 
		(ISEC1_Parameter == 29) || (ISEC1_Parameter == 30));

      (void) qu2reg3(fsec4, ISEC2_RowLonPtr, nlat, nlon, FSEC3_MissVal, iret,
		     lsect3, lperio, lveggy);

      if ( bitmapSize > 0 )
	{
	  int i, j = 0;

	  for ( i = 0; i < ISEC4_NumValues; i++ )
	    if ( !DBL_IS_EQUAL(fsec4[i], FSEC3_MissVal) ) j++;

	  ISEC4_NumNonMissValues = j;
	}
    }


  if ( ISEC0_GRIB_Version == 1 ) isLen = 8;
  esLen = 4;

  gribLen = isLen + pdsLen + gdsLen + bmsLen + bdsLen + esLen;

  if ( ISEC0_GRIB_Len )
    if ( gribLen != ISEC0_GRIB_Len )
      {
	Warning(func, "grib1Len = %d gribLen = %d", ISEC0_GRIB_Len, gribLen);
      }

  ISEC0_GRIB_Len = gribLen;

  *kword = gribLen / sizeof(int);
  if ( (size_t) gribLen != *kword * sizeof(int) ) *kword += 1;

  /*
    ----------------------------------------------------------------
    Section 9 . Abort/return to calling routine.
    ----------------------------------------------------------------
  */
 LABEL900:;

  if ( ldebug )
    {
      gprintf(func, "Section 9.");
      gprintf(func, "Output values set -");

      gribPrintSec0(isec0);
      gribPrintSec1(isec0, isec1);
      /*
	Print section 2 if present.
      */
      if ( lsect2 ) gribPrintSec2DP(isec0, isec2, fsec2);

      if ( ! l_iorj )
	{
	  /*
	    Print section 3 if present.
	  */
	  if ( lsect3 ) gribPrintSec3DP(isec0, isec3, fsec3);

	  gribPrintSec4DP(isec0, isec4, fsec4);
	  /*
	    Special print for 2D spectra wave field real values in
	    section 4
	  */
	  if ( (isec1[ 0] ==  140) && 
	       (isec1[ 1] ==   98) && 
	       (isec1[23] ==    1) && 
	       ((isec1[39] == 1045) || (isec1[39] == 1081))  && 
	       ((isec1[ 5] ==  250) || (isec1[ 5] ==  251)) )
	    gribPrintSec4Wave(isec4);
	}
    }
}
#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif




int gribOpen(const char *filename, const char *mode)
{
  int fileID;

  fileID = fileOpen(filename, mode);

#if defined (__sun)
  if ( fileID != FILE_UNDEFID && tolower(*mode) == 'r' )
    {
      fileSetBufferType(fileID, FILE_BUFTYPE_MMAP);
    }
#endif

  return (fileID);  
}


void gribClose(int fileID)
{
  fileClose(fileID);
}


off_t gribGetPos(int fileID)
{
  return (fileGetPos(fileID));
}


int gribCheckFiletype(int fileID)
{
  static char func[] = "gribCheckFiletype";
  int ierr;
  int found = 0;
  char buffer[4];

  if ( fileRead(fileID, buffer, 4) != 4 ) return(found);

  if ( strncmp(buffer, "GRIB", 4) == 0 )
    {
      found = 1;
      if ( GRB_Debug ) Message(func, "found GRIB file = %s", fileInqName(fileID));
    }
  else
    {
      long offset;

      ierr = gribFileSeek(fileID, &offset);
      fileRewind(fileID);
      if ( !ierr )
	{
	  found = 1;
	  if ( GRB_Debug ) Message(func, "found seek GRIB file = %s", fileInqName(fileID));
	}
    }

  return (found);
}


int gribCheckSeek(int fileID, long *offset, int *version)
{
  int ierr;
  char buffer[4];

  ierr = gribFileSeek(fileID, offset);

  *version = -1;
  if ( !ierr )
    {
      if ( fileRead(fileID, buffer, 4) == 4 )
	*version = buffer[3];
    }

  return (ierr);
}


int gribFileSeekOld(int fileID, long *offset)
{
  /* position file pointer after GRIB */
  static char func[] = "gribFileSeek";
  int ch;
  int buffersize = 4096;
  unsigned char buffer[4096];
  int retry = 4096;
  int i;
  void *fileptr;

  *offset = 0;

  fileptr = filePtr(fileID);

  ch = filePtrGetc(fileptr); if ( ch == EOF ) return (-1); buffer[0] = ch;
  ch = filePtrGetc(fileptr); if ( ch == EOF ) return (-1); buffer[1] = ch;
  ch = filePtrGetc(fileptr); if ( ch == EOF ) return (-1); buffer[2] = ch;
  ch = filePtrGetc(fileptr); if ( ch == EOF ) return (-1); buffer[3] = ch;
  /*
  fileRead(fileID, buffer, 4);
  */

  while ( retry-- )
    {
      for ( i = 0; i < buffersize-4; ++i )
	{
	  if (buffer[i  ] == 'G' && 
	      buffer[i+1] == 'R' &&
	      buffer[i+2] == 'I' &&
	      buffer[i+3] == 'B')
	    {
	      if ( GRB_Debug )
		Message(func, "record offset = %d", (int) *offset);
	      return (0);
	    }
	  else
	    {
	      ch = filePtrGetc(fileptr); if ( ch == EOF ) return (-1); buffer[i+4] = ch;
	      (*offset)++;
	    }
	}
      buffer[0] = buffer[i  ];
      buffer[1] = buffer[i+1];
      buffer[2] = buffer[i+2];
      buffer[3] = buffer[i+3];
    }

  if ( GRB_Debug )
    Message(func, "record offset = %d", (int) *offset);

  return (1);
}


int gribFileSeek(int fileID, long *offset)
{
  /* position file pointer after GRIB */
  static char func[] = "gribFileSeek";
  const long GRIB = 0x47524942;
  long code = 0;
  int ch;
  int retry = 4096*4096;
  void *fileptr;

  *offset = 0;

  fileptr = filePtr(fileID);

  while ( retry-- )
    {
      ch = filePtrGetc(fileptr);
      if ( ch == EOF ) return (-1);
    
      code = ( (code << 8) + ch ) & 0xFFFFFFFF;

      if ( code == GRIB )
	{
	  if ( GRB_Debug )
	    Message(func, "record offset = %d", (int) *offset);
	  return (0);
	}

      (*offset)++;
    }

  if ( GRB_Debug )
    Message(func, "record offset = %d", (int) *offset);

  return (1);
}


int gribFileSeekTest(int fileID, long *offset)
{
  /* position file pointer after GRIB */
  static char func[] = "gribFileSeek";
  const long GRIB = 0x47524942;
  long code = 0;
  int ch;
  int i = 0;
  const int buffersize = 8;
  unsigned char buffer[8];
  int retry = 4096*4096;
  void *fileptr;
  int nread = 0;

  *offset = 0;

  fileptr = filePtr(fileID);

  while ( retry-- )
    {
      if ( i >= nread )
	{
	  nread = (int) filePtrRead(fileptr, buffer, buffersize);
	  if ( nread == 0 ) return (-1);
	  i = 0;
	}

      ch = buffer[i++];
      code = ( (code << 8) + ch ) & 0xFFFFFFFF;

      if ( code == GRIB )
	{
	  /* printf("end: %d %d\n", nread, i); */
	  if ( GRB_Debug )
	    Message(func, "record offset = %d", (int) *offset);

	  if ( i != nread ) fileSetPos(fileID, (off_t) i-nread, SEEK_CUR);

	  return (0);
	}

      (*offset)++;
    }

  if ( GRB_Debug )
    Message(func, "record offset = %d", (int) *offset);

  return (1);
}


int gribReadSize(int fileID)
{
  static char func[] = "gribReadSize";
  int gribversion, gribsize;
  off_t pos;
  void *fileptr;
  /*
  const int buffersize = 4;
  unsigned char buffer[4];
  */
  fileptr = filePtr(fileID);

  pos = fileGetPos(fileID); 

  gribsize = (filePtrGetc(fileptr) << 16) + (filePtrGetc(fileptr) << 8) + filePtrGetc(fileptr);

  gribversion = filePtrGetc(fileptr);
  /*
  filePtrRead(fileptr, buffer, buffersize);

  gribsize = (buffer[0] << 16) + (buffer[1] << 8) + buffer[2];

  gribversion = buffer[3];
  */
  if ( gribsize == 24 )
    {
      if ( gribversion != 1 && gribversion != 2 ) gribversion = 0;
    }

  if ( GRB_Debug )
    Message(func, "gribversion = %d", gribversion);

  if ( gribversion == 0 )
    {
      int pdssize = 0, gdssize = 0, bmssize = 0, bdssize = 0;
      int issize = 4, essize = 4;
      int flag;

      pdssize = gribsize;
      fileSetPos(fileID, (off_t) 3, SEEK_CUR);
      if ( GRB_Debug )
	Message(func, "pdssize     = %d", pdssize);
      flag = filePtrGetc(fileptr);
      if ( GRB_Debug )
	Message(func, "flag        = %d", flag);
  
      fileSetPos(fileID, (off_t) pdssize-8, SEEK_CUR);

      if ( flag & 128 )
	{
	  gdssize = (filePtrGetc(fileptr) << 16) + (filePtrGetc(fileptr) << 8) + filePtrGetc(fileptr);
	  fileSetPos(fileID, (off_t) gdssize-3, SEEK_CUR);
	}
      if ( GRB_Debug )
	Message(func, "gdssize     = %d", gdssize);

      if ( flag & 64 )
	{
	  bmssize = (filePtrGetc(fileptr) << 16) + (filePtrGetc(fileptr) << 8) + filePtrGetc(fileptr);
	  fileSetPos(fileID, (off_t) bmssize-3, SEEK_CUR);
	}
      if ( GRB_Debug )
	Message(func, "bmssize     = %d", bmssize);

      bdssize = (filePtrGetc(fileptr) << 16) + (filePtrGetc(fileptr) << 8) + filePtrGetc(fileptr);

      if ( GRB_Debug )
	Message(func, "bdssize     = %d", bdssize);

      gribsize = issize + pdssize + gdssize + bmssize + bdssize + essize;
    }
  else if ( gribversion == 2 )
    {
      int i;
      /* we set gribsize the following way because it doesn't matter then
	 whether int is 4 or 8 bytes long - we don't have to care if the size
	 really fits: if it does not, the record can not be read at all */

      gribsize = 0;
      for ( i = 0; i < 8; i++ ) gribsize = (gribsize << 8) | filePtrGetc(fileptr);
    }
  else if ( gribversion != 1 )
    {
      gribsize = 0;
      Warning(func, "GRIB version %d unsupported!", gribversion);
    }

  if ( filePtrEOF(fileptr) ) gribsize = 0;

  if ( GRB_Debug )
    Message(func, "gribsize    = %d", gribsize);

  fileSetPos(fileID, pos, SEEK_SET);

  return (gribsize);
}


int gribGetSize(int fileID)
{
  static char func[] = "gribGetSize";
  int recsize;
  long offset;
  int ierr;

  ierr = gribFileSeek(fileID, &offset); /* position file pointer after GRIB */
  if ( ierr > 0 )
    {
      Warning(func, "GRIB record not found!");
      return (0);
    }

  if ( ierr == -1 )
    return (0);
  else if ( ierr == 1 )
    return (0);

  recsize = gribReadSize(fileID);

  if ( GRB_Debug ) Message(func, "recsize = %d", recsize);

  fileSetPos(fileID, (off_t) -4, SEEK_CUR);

  return (recsize);
}


int gribRead(int fileID, unsigned char *buffer, size_t *buffersize)
{
  static char func[] = "gribRead";
  long offset;
  int ierr = 0;
  size_t nread, recsize, recsize0;

  ierr = gribFileSeek(fileID, &offset); /* position file pointer after GRIB */
  if ( ierr > 0 )
    {
      Warning(func, "GRIB record not found!");
      return (-2);
    }

  if ( ierr == -1 )
    {
      *buffersize = 0;
      return (-1);
    }
  else if ( ierr == 1 )
    {
      *buffersize = 0;
      return (-2);
    }

  recsize = gribReadSize(fileID);

  buffer[0] = 'G';
  buffer[1] = 'R';
  buffer[2] = 'I';
  buffer[3] = 'B';

  recsize0 = recsize;

  if ( recsize > *buffersize )
    {
      recsize = *buffersize;
      ierr = -3;
    }

  *buffersize = recsize0;

  nread = fileRead(fileID, &buffer[4], recsize-4);

  if ( nread != recsize-4 ) ierr = 1;

  return (ierr);
}


int gribWrite(int fileID, unsigned char *buffer, size_t buffersize)
{
  static char func[] = "gribWrite";
  int  nwrite = 0;

  if( (nwrite = fileWrite(fileID, buffer, buffersize)) != (int) buffersize )
    {
      perror(func);
      nwrite = -1;
    }

  return ((int) nwrite);
}


/* ============ */
/* scaleComplex */
/* ============ */

void scaleComplex(double *fpdata, int pcStart, int pcScale, int truncation)
{
  static char func[] = "scaleComplex";
  double power;
  double *scale = (double *) malloc((truncation+1)*sizeof(double));
  int  n, m;
  int  index;

  if ( scale == NULL ) SysError(func, "No Memory!");

  if ( pcScale < -10000 || pcScale > 10000 )
    {
      fprintf(stderr, " scaleComplex: Invalid power given %6d\n", pcScale);
      return;
   }

  /* Setup scaling factors = n(n+1)^^p for n = 1 to truncation */

  if ( pcScale == 0 ) return;

  power = (double) pcScale / 1000.;
  scale[0] = 1.0;

  for ( n = 1; n <= truncation; n++ )
    {
      if (pcScale != 1000)
         scale[n] = 1.0 / pow((double) (n*(n+1)), power);
      else
         scale[n] = 1.0 /     (double) (n*(n+1));
    }

  /* Scale the values */

  index = 0;

  for (m = 0; m < pcStart;     m++)
  for (n = m; n <= truncation; n++)
    {
      if ( n >= pcStart )
	{
          fpdata[index  ] *= scale[n];
          fpdata[index+1] *= scale[n];
	}
      index += 2;
    }

  for (m = pcStart; m <= truncation; m++)
  for (n = m;       n <= truncation; n++)
    {
      fpdata[index  ] *= scale[n];
      fpdata[index+1] *= scale[n];
      index += 2;
    }

  free(scale);
}

/* ============== */
/* ScatterComplex */
/* ============== */

void scatterComplex(double *fpdata, int pcStart, int truncation, int dimSP)
{
  static char func[] = "scatterComplex";
  double *fphelp = (double *) malloc(dimSP*sizeof(double));
  int  m, n;
  int  index, inext;

  if ( fphelp == NULL ) SysError(func, "No Memory!");

  index = inext = 0;

  for (m = 0; m <= pcStart;    m++)
  for (n = m; n <= truncation; n++)
    {
      if ( pcStart >= n )
	{
          fphelp[index  ] = fpdata[inext++];
          fphelp[index+1] = fpdata[inext++];
	}
      index += 2;
    }
  index = 0;
  for (m = 0; m <= truncation; m++)
  for (n = m; n <= truncation; n++)
    {
      if ( n > pcStart )
	{
	  fphelp[index  ] = fpdata[inext++];
	  fphelp[index+1] = fpdata[inext++];
	}
      index += 2;
    }
  for (m = 0; m < dimSP; m++) fpdata[m] = fphelp[m];

  free(fphelp);
}

void scm0(double *pdl, double *pdr, double *pfl, double *pfr, int klg)
{
  /* System generated locals */
  double r_1;

  /* Local variables */
  double zfac, zeps, zbeta;
  int jl;
  double zalpha;

  /* **** SCM0   - Apply SCM0 limiter to derivative estimates. */
  /* output: */
  /*   pdl   = the limited derivative at the left edge of the interval */
  /*   pdr   = the limited derivative at the right edge of the interval */
  /* inputs */
  /*   pdl   = the original derivative at the left edge */
  /*   pdr   = the original derivative at the right edge */
  /*   pfl   = function value at the left edge of the interval */
  /*   pfr   = function value at the right edge of the interval */
  /*   klg   = number of intervals where the derivatives are limited */

  /*  define constants */

  zeps = 1.0e-12;
  zfac = (1.0 - zeps) * 3.0;

  for ( jl = 0; jl < klg; ++jl )
    {
      if ( (r_1 = pfr[jl] - pfl[jl], fabs(r_1)) > zeps )
	{
	  zalpha = pdl[jl] / (pfr[jl] - pfl[jl]);
	  zbeta  = pdr[jl] / (pfr[jl] - pfl[jl]);
	  if ( zalpha <= 0.0 ) pdl[jl] = 0.0;
	  if ( zbeta  <= 0.0 ) pdr[jl] = 0.0;
	  if ( zalpha > zfac ) pdl[jl] = zfac * (pfr[jl] - pfl[jl]);
	  if ( zbeta  > zfac ) pdr[jl] = zfac * (pfr[jl] - pfl[jl]);
	}
      else
	{
	  pdl[jl] = 0.0;
	  pdr[jl] = 0.0;
	}
    }
} /* scm0 */


int rowina2(double *p, int ko, int ki, double *pw,
	    int kcode, double msval, int *kret)
{
  /* System generated locals */
  int pw_dim1, pw_offset, i_1;

  /* Local variables */
  double zwt1, zrdi, zpos;
  int jl, ip;
  double zdo, zwt;

  /* Parameter adjustments */
  --p;
  pw_dim1 = ko + 3;
  pw_offset = pw_dim1;
  pw -= pw_offset;

  /* **** ROWINA2 - Interpolation of row of values. */
  /*     Input Parameters. */
  /*     ----------------- */
  /*     P      - Row of values to be interpolated. */
  /*              Dimension must be at least KO. */
  /*     KO     - Number of values required. */
  /*     KI     - Number of values in P on input. */
  /*     PW     - Working array. */
  /*              Dimension must be at least (0:KO+2,3). */
  /*     KCODE  - Interpolation required. */
  /*              1 , linear. */
  /*              3 , cubic. */
  /*     PMSVAL - Value used for missing data indicator. */

  /*     Output Parameters. */
  /*     ------------------ */
  /*     P     - Now contains KO values. */
  /*     KRET  - Return code */
  /*             0, OK */
  /*             Non-zero, error */

  /*     Author. */
  /*     ------- */
  /*     J.D.Chambers    ECMWF     22.07.94 */

  /*     ********************************    */
  /*     Section 1.  Linear interpolation .. */
  /*     ********************************    */

  *kret = 0;

  if ( kcode == 1 )
    {
      /*    Move input values to work array */
      for ( jl = 1; jl <= ki; ++jl )
	pw[jl + pw_dim1] = p[jl];

      /*    Arrange wrap-around value in work array */
      pw[ki + 1 + pw_dim1] = p[1];

      /*    Set up constants to be used to figure out weighting for */
      /*    values in interpolation. */
      zrdi = (double) ki;
      zdo = 1.0 / (double) ko;

      /*    Loop through the output points */
      for ( jl = 1; jl <= ko; ++jl )
	{

	  /*    Calculate weight from the start of row */
	  zpos = (jl - 1) * zdo;
	  zwt = zpos * zrdi;

	  /*    Get the current array position(minus 1) from the weight - */
	  /*    note the implicit truncation. */
	  ip = (int) zwt;

	  /*    If the left value is missing, use the right value */
	  if ( DBL_IS_EQUAL(pw[ip + 1 + pw_dim1], msval) )
	    {
	      p[jl] = pw[ip + 2 + pw_dim1];
	    }
	  /*    If the right value is missing, use the left value */
	  else if ( DBL_IS_EQUAL(pw[ip + 2 + pw_dim1], msval) )
	    {
	      p[jl] = pw[ip + 1 + pw_dim1];
	    }
	  /*    If neither missing, interpolate ... */
	  else
	    {

	      /*       Adjust the weight to range (0.0 to 1.0) */
	      zwt -= ip;

	      /*       Interpolate using the weighted values on either side */
	      /*       of the output point position */
	      p[jl] = (1.0 - zwt) * pw[ip + 1 + pw_dim1] +
		zwt * pw[ip + 2 + pw_dim1];
	    }
	}

      /*     *******************************    */
      /*     Section 2.  Cubic interpolation .. */
      /*     *******************************    */

    }
  else if ( kcode == 3 )
    {
      i_1 = ki;
      for ( jl = 1; jl <= i_1; ++jl )
	{
          if ( DBL_IS_EQUAL(p[jl], msval) )
	    {
	      fprintf(stderr," ROWINA2: ");
	      fprintf(stderr," Cubic interpolation not supported");
	      fprintf(stderr," for fields containing missing data.\n");
	      *kret = 1;
	      goto L900;
	    }
          pw[jl + pw_dim1] = p[jl];
	}
      pw[pw_dim1] = p[ki];
      pw[ki + 1 + pw_dim1] = p[1];
      pw[ki + 2 + pw_dim1] = p[2];
      i_1 = ki;
      for ( jl = 1; jl <= i_1; ++jl )
	{
          pw[jl + (pw_dim1 << 1)] =
	        - pw[jl - 1 + pw_dim1] / 3.0 -
	          pw[jl     + pw_dim1] * 0.5 +
	          pw[jl + 1 + pw_dim1] - pw[jl + 2 + pw_dim1] / 6.0;
          pw[jl + 1 + pw_dim1 * 3] =
                  pw[jl - 1 + pw_dim1] / 6.0 -
                  pw[jl     + pw_dim1] +
                  pw[jl + 1 + pw_dim1] * 0.5 +
                  pw[jl + 2 + pw_dim1] / 3.0;
	}

      scm0(&pw[(pw_dim1 << 1) + 1], &pw[pw_dim1 * 3 + 2],
	   &pw[pw_dim1 + 1], &pw[pw_dim1 + 2], ki);

      zrdi = (double) ki;
      zdo = 1.0 / (double) ko;
      for ( jl = 1; jl <= ko; ++jl )
	{
          zpos = (jl - 1) * zdo;
          zwt = zpos * zrdi;
          ip = (int) zwt + 1;
          zwt = zwt + 1.0 - ip;
          zwt1 = 1.0 - zwt;
          p[jl] = ((3.0 - zwt1 * 2.0) * pw[ip + pw_dim1] +
                  zwt * pw[ip + (pw_dim1 << 1)]) * zwt1 * zwt1 +
                  ((3.0 - zwt * 2.0) * pw[ip + 1 + pw_dim1] -
                  zwt1 * pw[ip + 1 + pw_dim1 * 3]) * zwt * zwt;
	}

    }
  else
    {
      /*    **************************************    */
      /*    Section 3.  Invalid interpolation code .. */
      /*    **************************************    */
      fprintf(stderr," ROWINA2:");
      fprintf(stderr," Invalid interpolation code = %2d\n",kcode);
      *kret = 2;
    }

L900:
    return 0;
} /* rowina2 */


int rowina3(double *p, int ko, int ki, double *pw,
	    int kcode, double msval, int *kret, int omisng, int operio, int oveggy)
{
  /*
C---->
C**** ROWINA3 - Interpolation of row of values.
C
C     Purpose.
C     --------
C
C     Interpolate a row of values.
C
C
C**   Interface.
C     ----------
C
C     CALL ROWINA3( P, KO, KI, PW, KCODE, PMSVAL, KRET, OMISNG, OPERIO)
C
C
C     Input Parameters.
C     -----------------
C
C     P      - Row of values to be interpolated.
C              Dimension must be at least KO.
C
C     KO     - Number of values required.
C
C     KI     - Number of values in P on input.
C
C     PW     - Working array.
C              Dimension must be at least (0:KO+2,3).
C
C     KCODE  - Interpolation required.
C              1 , linear.
C              3 , cubic.
C
C     PMSVAL - Value used for missing data indicator.
C
C     OMISNG - True if missing values are present in field.
C
C     OPERIO - True if input field is periodic.
C
C     OVEGGY     - True if 'nearest neighbour' processing must be used
C                  for interpolation
C
C     Output Parameters.
C     ------------------
C
C     P     - Now contains KO values.
C     KRET  - Return code
C             0, OK
C             Non-zero, error
C
C
C     Method.
C     -------
C
C     Linear or cubic interpolation performed as required.
C
C     Comments.
C     ---------
C
C     This is a version of ROWINA which allows for missing data
C     values and hence for bitmapped fields.
C
C
C     Author.
C     -------
C
C     J.D.Chambers    ECMWF     22.07.94
C
C
C     Modifications.
C     --------------
C
C     J.D.Chambers    ECMWF     13.09.94
C     Add return code KRET and remove calls to ABORT.
C
C     J. Clochard, Meteo France, for ECMWF - January 1998.
C     Addition of OMISNG and OPERIO arguments.
C
C
C     -----------------------------------------------------------------
*/
  /* System generated locals */
  int pw_dim1, pw_offset, i_1;

  /* Local variables */
  double zwt1, zrdi, zpos;
  int jl, ip;
  double zdo, zwt;

  /* Parameter adjustments */
  --p;
  pw_dim1 = ko + 3;
  pw_offset = pw_dim1;
  pw -= pw_offset;

  *kret = 0;

  if ( kcode == 1 )
    {
      /*    Move input values to work array */
      for ( jl = 1; jl <= ki; ++jl )
	pw[jl + pw_dim1] = p[jl];

      /* Arrange wrap-around value in work array */
      pw[ki + 1 + pw_dim1] = p[1];

      /* Set up constants to be used to figure out weighting for */
      /* values in interpolation. */
      zrdi = (double) ki;
      zdo = 1.0 / (double) ko;

      /*    Loop through the output points */
      for ( jl = 1; jl <= ko; ++jl )
	{

	  /* Calculate weight from the start of row */
	  zpos = (jl - 1) * zdo;
	  zwt = zpos * zrdi;

	  /* Get the current array position(minus 1) from the weight - */
	  /* note the implicit truncation. */
	  ip = (int) zwt;
		  
	  /* Adjust the weight to range (0.0 to 1.0) */
	  zwt -= ip;

          /* If 'nearest neighbour' processing must be used */
	  if ( oveggy )
	    {
              if ( zwt < 0.5 )
                p[jl] = pw[ip + 1 + pw_dim1];
	      else
		p[jl] = pw[ip + 2 + pw_dim1];
	    }
	  else
	    {
	      /*    If the left value is missing, use the right value */
	      if ( DBL_IS_EQUAL(pw[ip + 1 + pw_dim1], msval) )
		{
		  p[jl] = pw[ip + 2 + pw_dim1];
		}
	      /*    If the right value is missing, use the left value */
	      else if ( DBL_IS_EQUAL(pw[ip + 2 + pw_dim1], msval) )
		{
		  p[jl] = pw[ip + 1 + pw_dim1];
		}
	      /*    If neither missing, interpolate ... */
	      else
		{
		  /*       Interpolate using the weighted values on either side */
		  /*       of the output point position */
		  p[jl] = (1.0 - zwt) * pw[ip + 1 + pw_dim1] +
		    zwt * pw[ip + 2 + pw_dim1];
		}
	    }
	}

      /*     *******************************    */
      /*     Section 2.  Cubic interpolation .. */
      /*     *******************************    */

    }
  else if ( kcode == 3 )
    {
      i_1 = ki;
      for ( jl = 1; jl <= i_1; ++jl )
	{
          if ( DBL_IS_EQUAL(p[jl], msval) )
	    {
	      fprintf(stderr," ROWINA2: ");
	      fprintf(stderr," Cubic interpolation not supported");
	      fprintf(stderr," for fields containing missing data.\n");
	      *kret = 1;
	      goto L900;
	    }
          pw[jl + pw_dim1] = p[jl];
	}
      pw[pw_dim1] = p[ki];
      pw[ki + 1 + pw_dim1] = p[1];
      pw[ki + 2 + pw_dim1] = p[2];
      i_1 = ki;
      for ( jl = 1; jl <= i_1; ++jl )
	{
          pw[jl + (pw_dim1 << 1)] =
	        - pw[jl - 1 + pw_dim1] / 3.0 -
	          pw[jl     + pw_dim1] * 0.5 +
	          pw[jl + 1 + pw_dim1] - pw[jl + 2 + pw_dim1] / 6.0;
          pw[jl + 1 + pw_dim1 * 3] =
                  pw[jl - 1 + pw_dim1] / 6.0 -
                  pw[jl     + pw_dim1] +
                  pw[jl + 1 + pw_dim1] * 0.5 +
                  pw[jl + 2 + pw_dim1] / 3.0;
	}

      scm0(&pw[(pw_dim1 << 1) + 1], &pw[pw_dim1 * 3 + 2],
	   &pw[pw_dim1 + 1], &pw[pw_dim1 + 2], ki);

      zrdi = (double) ki;
      zdo = 1.0 / (double) ko;
      for ( jl = 1; jl <= ko; ++jl )
	{
          zpos = (jl - 1) * zdo;
          zwt = zpos * zrdi;
          ip = (int) zwt + 1;
          zwt = zwt + 1.0 - ip;
          zwt1 = 1.0 - zwt;
          p[jl] = ((3.0 - zwt1 * 2.0) * pw[ip + pw_dim1] +
                  zwt * pw[ip + (pw_dim1 << 1)]) * zwt1 * zwt1 +
                  ((3.0 - zwt * 2.0) * pw[ip + 1 + pw_dim1] -
                  zwt1 * pw[ip + 1 + pw_dim1 * 3]) * zwt * zwt;
	}

    }
  else
    {
      /*    **************************************    */
      /*    Section 3.  Invalid interpolation code .. */
      /*    **************************************    */
      fprintf(stderr," ROWINA2:");
      fprintf(stderr," Invalid interpolation code = %2d\n",kcode);
      *kret = 2;
    }

L900:
    return 0;
} /* rowina3 */


int qu2reg2(double *pfield, int *kpoint, int klat, int klon,
	    double *ztemp, double msval, int *kret)
{
   static char func[] = "qu2reg2";

   /* System generated locals */
   int i_1, i_2;
   int kcode = 1;

   /* Local variables */
   int ilii, ilio, icode;
   double *zline = NULL;
   double *zwork = NULL;
   int iregno, iquano, j210, j220, j230, j240, j225;


   zline = (double *) malloc(2*klon*sizeof(double));
   if ( zline == NULL ) SysError(func, "No Memory!");

   zwork = (double *) malloc(3*(2*klon+3)*sizeof(double));
   if ( zwork == NULL ) SysError(func, "No Memory!");

   /* Parameter adjustments */
   --pfield;
   --kpoint;

/* **** QU2REG - Convert quasi-regular grid data to regular. */
/*     Input Parameters. */
/*     ----------------- */
/*     PFIELD     - Array containing quasi-regular grid */
/*                  data. */
/*     KPOINT     - Array containing list of the number of */
/*                  points on each latitude (or longitude) of */
/*                  the quasi-regular grid. */
/*     KLAT       - Number of latitude lines */
/*     KLON       - Number of longitude lines */
/*     KCODE      - Interpolation required. */
/*                  1 , linear - data quasi-regular on */
/*                               latitude lines. */
/*                  3 , cubic -  data quasi-regular on */
/*                               latitude lines. */
/*                  11, linear - data quasi-regular on */
/*                               longitude lines. */
/*                  13, cubic -  data quasi-regular on */
/*                               longitude lines. */
/*     PMSVAL     - Value used for missing data indicator. */
/*     Output Parameters. */
/*     ------------------ */
/*     KRET       - return code */
/*                  0 = OK */
/*                  non-zero indicates fatal error */
/*     PFIELD     - Array containing regular grid data. */
/*     Author. */
/*     ------- */
/*     J.D.Chambers     ECMWF      22.07.94 */
/*     J.D.Chambers     ECMWF      13.09.94 */
/*     Add return code KRET and remove calls to ABORT. */


/* ------------------------------ */
/* Section 1. Set initial values. */
/* ------------------------------ */

   *kret = 0;

/* Check input parameters. */

   if (kcode != 1 && kcode != 3 && kcode != 11 && kcode != 13) {
      fprintf(stderr," QU2REG :");
      fprintf(stderr," Invalid interpolation type code = %2d\n",kcode);
      *kret = 1;
      goto L900;
   }

/* Set array indices to 0. */

   ilii = 0;
   ilio = 0;

/* Establish values of loop parameters. */

   if (kcode > 10) {

/*    Quasi-regular along longitude lines. */

      iquano = klon;
      iregno = klat;
      icode = kcode - 10;
   } else {

/*    Quasi-regular along latitude lines. */

      iquano = klat;
      iregno = klon;
      icode = kcode;
   }

/*     -------------------------------------------------------- */
/**    Section 2. Interpolate field from quasi to regular grid. */
/*     -------------------------------------------------------- */

   i_1 = iquano;
   for (j230 = 1; j230 <= i_1; ++j230) {

      if (iregno != kpoint[j230]) {

/*       Line contains less values than required,so */
/*       extract quasi-regular grid values for a line */

         i_2 = kpoint[j230];
         for (j210 = 1; j210 <= i_2; ++j210) {
            ++ilii;
            zline[j210 - 1] = pfield[ilii];
         }

/*       and interpolate this line. */

         rowina2(zline, iregno, kpoint[j230], zwork, icode, msval, kret);
         if (*kret != 0) goto L900;

/*       Add regular grid values for this line to the
         temporary array. */

         i_2 = iregno;
         for (j220 = 1; j220 <= i_2; ++j220) {
            ++ilio;
            ztemp[ilio - 1] = zline[j220 - 1];
         }

      } else {

/*       Line contains the required number of values, so add */
/*       this line to the temporary array. */

         i_2 = iregno;
         for (j225 = 1; j225 <= i_2; ++j225) {
            ++ilio;
            ++ilii;
            ztemp[ilio - 1] = pfield[ilii];
         }
      }
   }

/* Copy temporary array to user array. */

   i_1 = klon * klat;
   for (j240 = 1; j240 <= i_1; ++j240) {
      pfield[j240] = ztemp[j240 - 1];
   }

/* -------------------------------------------------------- */
/* Section 9. Return to calling routine. Format statements. */
/* -------------------------------------------------------- */

L900:

   free(zline);
   free(zwork);

   return 0;
} /* qu2reg2 */


int qu2reg3(double *pfield, int *kpoint, int klat, int klon,
	    double msval, int *kret, int omisng, int operio, int oveggy)
{
  /*
C**** QU2REG3 - Convert quasi-regular grid data to regular.
C
C     Purpose.
C     --------
C
C     Convert quasi-regular grid data to regular,
C     using either a linear or cubic interpolation.
C
C
C**   Interface.
C     ----------
C
C     CALL QU2REG3(PFIELD,KPOINT,KLAT,KLON,KCODE,PMSVAL,OMISNG,OPERIO,
C    X            OVEGGY)
C
C
C     Input Parameters.
C     -----------------
C
C     PFIELD     - Array containing quasi-regular grid data.
C
C     KPOINT     - Array containing list of the number of
C                  points on each latitude (or longitude) of
C                  the quasi-regular grid.
C
C     KLAT       - Number of latitude lines
C
C     KLON       - Number of longitude lines
C
C     KCODE      - Interpolation required.
C                  1 , linear - data quasi-regular on latitude lines.
C                  3 , cubic -  data quasi-regular on latitude lines.
C                  11, linear - data quasi-regular on longitude lines.
C                  13, cubic -  data quasi-regular on longitude lines.
C
C     PMSVAL     - Value used for missing data indicator.
C
C     OMISNG     - True if missing values are present in field.
C
C     OPERIO     - True if input field is periodic.
C
C     OVEGGY     - True if 'nearest neighbour' processing must be used
C                  for interpolation
C
C
C     Output Parameters.
C     ------------------
C
C     KRET       - return code
C                  0 = OK
C                  non-zero indicates fatal error
C
C
C     Output Parameters.
C     ------------------
C
C     PFIELD     - Array containing regular grid data.
C
C
C     Method.
C     -------
C
C     Data is interpolated and expanded into a temporary array,
C     which is then copied back into the user's array.
C     Returns an error code if an invalid interpolation is requested
C     or field size exceeds array dimensions.
C
C     Comments.
C     ---------
C
C     This routine is an adaptation of QU2REG to allow missing data
C     values, and hence bit mapped fields.
C
C
C     Author.
C     -------
C
C     J.D.Chambers     ECMWF      22.07.94
C
C
C     Modifications.
C     --------------
C
C     J.D.Chambers     ECMWF      13.09.94
C     Add return code KRET and remove calls to ABORT.
C
C     J.D.Chambers     ECMWF        Feb 1997
C     Allow for 64-bit pointers
C
C     J. Clochard, Meteo France, for ECMWF - January 1998.
C     Addition of OMISNG and OPERIO arguments.
C     Fix message for longitude number out of bounds, and routine
C     name in title and formats.
C
*/
   static char func[] = "qu2reg2";

   /* System generated locals */
   int i_1, i_2;
   int kcode = 1;

   /* Local variables */
   int ilii, ilio, icode;
   double *ztemp = NULL;
   double *zline = NULL;
   double *zwork = NULL;
   int iregno, iquano, j210, j220, j230, j240, j225;

   ztemp = (double *) malloc(klon*klat*sizeof(double));
   if ( ztemp == NULL ) SysError(func, "No Memory!");

   zline = (double *) malloc(2*klon*sizeof(double));
   if ( zline == NULL ) SysError(func, "No Memory!");

   zwork = (double *) malloc(3*(2*klon+3)*sizeof(double));
   if ( zwork == NULL ) SysError(func, "No Memory!");

   /* Parameter adjustments */
   --pfield;
   --kpoint;

/* ------------------------------ */
/* Section 1. Set initial values. */
/* ------------------------------ */

   *kret = 0;

/* Check input parameters. */

   if (kcode != 1 && kcode != 3 && kcode != 11 && kcode != 13) {
      fprintf(stderr," QU2REG :");
      fprintf(stderr," Invalid interpolation type code = %2d\n",kcode);
      *kret = 1;
      goto L900;
   }

/* Set array indices to 0. */

   ilii = 0;
   ilio = 0;

/* Establish values of loop parameters. */

   if (kcode > 10) {

/*    Quasi-regular along longitude lines. */

      iquano = klon;
      iregno = klat;
      icode = kcode - 10;
   } else {

/*    Quasi-regular along latitude lines. */

      iquano = klat;
      iregno = klon;
      icode = kcode;
   }

/*     -------------------------------------------------------- */
/**    Section 2. Interpolate field from quasi to regular grid. */
/*     -------------------------------------------------------- */

   i_1 = iquano;
   for (j230 = 1; j230 <= i_1; ++j230) {

      if (iregno != kpoint[j230]) {

/*       Line contains less values than required,so */
/*       extract quasi-regular grid values for a line */

         i_2 = kpoint[j230];
         for (j210 = 1; j210 <= i_2; ++j210) {
            ++ilii;
            zline[j210 - 1] = pfield[ilii];
         }

/*       and interpolate this line. */

         rowina3(zline, iregno, kpoint[j230], zwork, icode, msval, kret, omisng, operio , oveggy);
         if (*kret != 0) goto L900;

/*       Add regular grid values for this line to the
         temporary array. */

         i_2 = iregno;
         for (j220 = 1; j220 <= i_2; ++j220) {
            ++ilio;
            ztemp[ilio - 1] = zline[j220 - 1];
         }

      } else {

/*       Line contains the required number of values, so add */
/*       this line to the temporary array. */

         i_2 = iregno;
         for (j225 = 1; j225 <= i_2; ++j225) {
            ++ilio;
            ++ilii;
            ztemp[ilio - 1] = pfield[ilii];
         }
      }
   }

/* Copy temporary array to user array. */

   i_1 = klon * klat;
   for (j240 = 1; j240 <= i_1; ++j240) {
      pfield[j240] = ztemp[j240 - 1];
   }

/* -------------------------------------------------------- */
/* Section 9. Return to calling routine. Format statements. */
/* -------------------------------------------------------- */

L900:

   free(zwork);
   free(zline);
   free(ztemp);

   return 0;
} /* qu2reg3 */


FILE *grprsm = NULL;
double fref;
double fmaxval;
int nfref;
int nfmaxval;
int nrnd;
int ndbg;
int nvck;
int nonoff;
int noabort;
int num2ok;
int next2o;
int nloc2o;
int nsubce;


void grsdef(void)
{
  /*
C---->
C**** GRSDEF - Initial (default) setting of common area variables
C              for GRIBEX package.
C
C     Purpose.
C     --------
C
C     Sets initial values for common area variables for all
C     routines of GRIBEX package, if not already done.
C
C**   Interface.
C     ----------
C
C     CALL GRSDEF
C
C     Input Parameters.
C     -----------------
C
C     None.
C
C     Output Parameters.
C     ------------------
C
C     None.
C
C     Method.
C     -------
C
C     Self-explanatory.
C
C     Externals.
C     ----------
C
C     None.
C
C     Reference.
C     ----------
C
C     See subroutine GRIBEX.
C
C     Comments.
C     ---------
C
C     None
C
C     Author.
C     -------
C
C     J. Clochard, Meteo France, for ECMWF - March 1998.
C
C     Modifications.
C     --------------
C
C     J. Clochard, Meteo France, for ECMWF - June 1999.
C     Add variable NSUBCE.
C     Use a static variable to determine if initialisation has already
C     been done. NUSER removed .
C     Reverse defaults for NEXT2O and NLOC2O, for consistency with
C     version 13.023 of software .
C
  */
  static char func[] = "grsdef";
  /*
C     ----------------------------------------------------------------
C*    Section 0 . Definition of variables.
C     ----------------------------------------------------------------
  */
  char *hndbg, *hnvck;
  char *env_stream;
  static int lfirst = TRUE;

  if ( ! lfirst ) return;

  /*
    ----------------------------------------------------------------
    Section 1 . Set values, conditionally.
    ----------------------------------------------------------------
  */
  /*
    Common area variables have not been set. Set them.
    
    User supplied reference value.
  */
  fref   = 0.0;
  /*
    Reference value supplied by user flag. Set to off.
  */
  nfref  = 0;
  /*
    User supplied maximum value.
  */
  fmaxval   = 0.0;
  /*
    Maximum value supplied by user flag. Set to off.
  */
  nfmaxval  = 0;
  /*
    Set rounding to 120 bytes on.
  */
  nrnd   = 1;
  /*
    Set debug print off.
  */
  ndbg   = 0;
  
  hndbg = getenv("GRIBEX_DEBUG");
  if ( hndbg != NULL )
    {
      if ( !strncmp(hndbg, "ON", 2) )
        ndbg = 1;
      else if( *hndbg == '1')
        ndbg = 1;
      else if( *hndbg == '2')
        ndbg = 2;
      else
        ndbg = 0;
    }
  /*
    Set GRIB value checking on.
  */
  nvck   = 1;
  
  hnvck = getenv("GRIBEX_CHECK");
  if ( hnvck )
    {
      if ( !strncmp(hnvck, "OFF", 3) )
        nvck = 0;
      else
        nvck = 1;
    }
  /*
    See if output stream needs changing
  */
  grprsm = stdout;
  env_stream = getenv("GRPRS_STREAM");
  if ( env_stream )
    {
      if ( isdigit((int) env_stream[0]) )
	{
	  int unit;
	  unit = atoi(env_stream);
	  if ( unit < 1 || unit > 99 )
	    Warning(func, "Invalid number for GRPRS_STREAM: %d\n", unit);
	  else if ( unit == 2 )
	    grprsm = stderr;
	  else if ( unit == 6 )
	    grprsm = stdout;
	  else
	    {
	      char filename[] = "unit.00";
	      sprintf(filename, "%2.2d", unit);
	      grprsm = fopen(filename, "w");
	      if ( ! grprsm )
		SysError(func, "GRPRS_STREAM = %d", unit);
	    }
	}
      else
	{
	  if ( env_stream[0] )
	    {
	      grprsm = fopen(env_stream, "w");
	      if ( ! grprsm )
		SysError(func, "GRPRS_STREAM = %s", env_stream);
	    }
	}
    }
  /*
    Set P factor switch to default, user supplies the P factor.
  */
  nonoff = 0;
  /*
    Set abort flag to NO abort
  */
  noabort = 1;
  /*
    Mark common area values set by user.
  */
  lfirst = FALSE;
  /*
    Exhaustive use of all possible second-order packing methods
    for HOPER='K'. Set to off.
  */
  num2ok  = 0;
  /*
    Use of extended second-order packing methods for grid-point
    encoding (HOPER='C' and 'K'). Set to on.
  */
  next2o  = 1;
  /*
    Use of non-local second-order packing methods for grid-point
    encoding (HOPER='C' and 'K'). Set to on.
  */
  nloc2o  = 1;
  /*
    Use of (all valid) sub-centre values for ECMWF fields encoding .
    encoding. Set to off.
  */
  nsubce  = 0;
}

#undef  IsBigendian
#define IsBigendian()  ( u_byteorder.c[sizeof(long) - 1] )

/* pack 8-bit bytes from 64-bit words to a packed buffer */
/* same as : for ( int i = 0; i < bc; ++i ) cp[i] = (char) up[i]; */

long packInt64(unsigned INT64 *up, char *cp, long bc, long tc)
{
#if defined (CRAY)
  (void) _pack(up, cp, bc, tc);
#else
  static union {unsigned long l; unsigned char c[sizeof(long)];} u_byteorder = {1};
  char *cp0;
  unsigned INT64 upi, *up0, *ip0, *ip1, *ip2, *ip3, *ip4, *ip5, *ip6, *ip7;
  long head, trail, inner, i, j;
  int ipack = sizeof(INT64);
  
  /* Bytes until first word boundary in destination buffer */

  head = ( (long) cp ) & (ipack-1);
  if ( head != 0 ) head = ipack - head;

  inner = bc - head;

  /* Trailing bytes which do not make a full word */

  trail = inner & (ipack-1);

  /* Number of bytes/words to be processed in fast loop */

  inner -= trail;
  inner /= ipack;

  ip0 = up + head;
  ip1 = ip0 + 1;
  ip2 = ip0 + 2;
  ip3 = ip0 + 3;
  ip4 = ip0 + 4;
  ip5 = ip0 + 5;
  ip6 = ip0 + 6;
  ip7 = ip0 + 7;

  up0 = (unsigned INT64 *) (cp + head);

  /* Here we should process any bytes until the first word boundary 
   * of our destination buffer 
   * That code is missing so far  because our output buffer is 
   * word aligned by FORTRAN 
   */

  j = 0;

  if ( IsBigendian() )
    {
#if defined (CRAY)
#pragma _CRI ivdep
#endif
#if defined (SX)
#pragma vdir nodep
#endif
#ifdef __uxpch__
#pragma loop novrec
#endif
      for ( i = 0 ; i < inner ; i++ )
	{
	  upi =             (   ip0[j]         << 56 ) 
	                 |  ( ( ip1[j] & 255 ) << 48 )
	                 |  ( ( ip2[j] & 255 ) << 40 )
	                 |  ( ( ip3[j] & 255 ) << 32 )
	                 |  ( ( ip4[j] & 255 ) << 24 ) ;
	  up0[i] = upi   |  ( ( ip5[j] & 255 ) << 16 )
	                 |  ( ( ip6[j] & 255 ) <<  8 )
	                 |    ( ip7[j] & 255 ) ;
	  j += ipack;
	}
    }
  else
    {
      for ( i = 0 ; i < inner ; i++ )
	{
	  upi =             (   ip7[j]         << 56 ) 
	                 |  ( ( ip6[j] & 255 ) << 48 )
                         |  ( ( ip5[j] & 255 ) << 40 )
                         |  ( ( ip4[j] & 255 ) << 32 )
                         |  ( ( ip3[j] & 255 ) << 24 ) ;
	  up0[i] = upi   |  ( ( ip2[j] & 255 ) << 16 )
                         |  ( ( ip1[j] & 255 ) <<  8 )
                         |    ( ip0[j] & 255 ) ;
	  j += ipack;
	}
    }

  cp0 = (char *) ( up0 + inner );
  if ( trail > 0 )
    {
      up0[inner] = 0;
      for ( i = 0 ; i < trail ; i ++ )
	{
	  *cp0 = (char ) ip0[ipack*inner+i];
	  cp0++;
	}
    }

  if ( tc != -1 )
    {
      bc++;
      *cp0 = (char) tc;
    }
#endif
  return (bc);
}

/* unpack 8-bit bytes from a packed buffer with 64-bit words */
/* same as : for ( int i = 0; i < bc; ++i ) up[i] = (INT64) cp[i]; */

long unpackInt64(unsigned char *cp, unsigned INT64 *up, long bc, long tc)
{
  static union {unsigned long l; unsigned char c[sizeof(long)];} u_byteorder = {1};
  unsigned char *cp0;
  unsigned INT64 *up0;
  unsigned INT64 *ip0, *ip1, *ip2, *ip3, *ip4, *ip5, *ip6, *ip7;
  long head, trail, inner, i, j;
  long offset;
  int ipack = sizeof(INT64);

  /* Bytes until first word boundary in source buffer */

  head = ( (long) cp ) & (ipack-1);
  if ( head != 0 ) head = ipack - head;
  if ( head > bc ) head = bc;

  inner = bc - head;

  /* Trailing bytes which do not make a full word */
 
  trail = inner & (ipack-1);
 
  /* Number of bytes/words to be processed in fast loop */

  inner -= trail;
  inner /= ipack;

  ip0 = up + head;
  ip1 = ip0 + 1;
  ip2 = ip0 + 2;
  ip3 = ip0 + 3;
  ip4 = ip0 + 4;
  ip5 = ip0 + 5;
  ip6 = ip0 + 6;
  ip7 = ip0 + 7;

  up0 = (unsigned INT64 *) (cp + head);

  /* Process any bytes until the first word boundary 
   * of our source buffer 
   */
  for ( i = 0 ; i < head ; i++ ) up[i] = (unsigned INT64) cp[i];

  j = 0;

  if ( IsBigendian() )
    {
#if defined (CRAY)
#pragma _CRI ivdep
#endif
#if defined (SX)
#pragma vdir nodep
#endif
#ifdef __uxpch__
#pragma loop novrec
#endif
      for ( i = 0 ; i < inner ; i++ )
	{
	  ip0[j] = (up0[i] >> 56) & 255;
	  ip1[j] = (up0[i] >> 48) & 255;
	  ip2[j] = (up0[i] >> 40) & 255;
	  ip3[j] = (up0[i] >> 32) & 255;
	  ip4[j] = (up0[i] >> 24) & 255;
	  ip5[j] = (up0[i] >> 16) & 255;
	  ip6[j] = (up0[i] >>  8) & 255;
	  ip7[j] = (up0[i])       & 255;

	  j += ipack;
	}
    }
  else
    {
      for ( i = 0 ; i < inner ; i++ )
	{
	  ip7[j] = (up0[i] >> 56) & 255;
	  ip6[j] = (up0[i] >> 48) & 255;
	  ip5[j] = (up0[i] >> 40) & 255;
	  ip4[j] = (up0[i] >> 32) & 255;
	  ip3[j] = (up0[i] >> 24) & 255;
	  ip2[j] = (up0[i] >> 16) & 255;
	  ip1[j] = (up0[i] >>  8) & 255;
	  ip0[j] = (up0[i])       & 255;

	  j += ipack;
	}
    }

  if ( trail > 0 )
    {
      offset = head + ipack*inner;
      cp0 = cp + offset;
      for ( i = 0 ; i < trail ; i++ ) up[i+offset] = (unsigned INT64) cp0[i];
    }
  /*
  if ( tc != -1 ) {
    bc++;
    *cp0 = (char) tc;
  }
  */
  return (bc);
}

/* pack 8-bit bytes from 32-bit words to a packed buffer */
/* same as : for ( int i = 0; i < bc; ++i ) cp[i] = (char) up[i]; */

#if  defined  (INT32)
long packInt32(unsigned INT32 *up, char *cp, long bc, long tc)
{
  static union {unsigned long l; unsigned char c[sizeof(long)];} u_byteorder = {1};
  char *cp0;
  unsigned INT32 *up0, *ip0, *ip1, *ip2, *ip3;
  long head, trail, inner, i, j;
  int ipack = sizeof(INT32);
  
  /* Bytes until first word boundary in destination buffer */

  head = ( (long) cp ) & (ipack-1);
  if ( head != 0 ) head = ipack - head;

  inner = bc - head;

  /* Trailing bytes which do not make a full word */

  trail = inner & (ipack-1);

  /* Number of bytes/words to be processed in fast loop */

  inner -= trail;
  inner /= ipack;

  ip0 = up + head;
  ip1 = ip0 + 1;
  ip2 = ip0 + 2;
  ip3 = ip0 + 3;

  up0 = (unsigned INT32 *) (cp + head);

  /* Here we should process any bytes until the first word boundary 
   * of our destination buffer 
   * That code is missing so far  because our output buffer is 
   * word aligned by FORTRAN 
   */

  j = 0;

  if ( IsBigendian() )
    {
#if defined (CRAY)
#pragma _CRI ivdep
#endif
#if defined (SX)
#pragma vdir nodep
#endif
#ifdef __uxpch__
#pragma loop novrec
#endif
      for ( i = 0 ; i < inner ; i++ )
	{
	  up0[i] =          (   ip0[j]         << 24 ) 
	                 |  ( ( ip1[j] & 255 ) << 16 )
	                 |  ( ( ip2[j] & 255 ) <<  8 )
	                 |    ( ip3[j] & 255 ) ;
	  j += ipack;
	}
    }
  else
    {
      for ( i = 0 ; i < inner ; i++ )
	{
	  up0[i] =          (   ip3[j]         << 24 ) 
	                 |  ( ( ip2[j] & 255 ) << 16 )
                         |  ( ( ip1[j] & 255 ) <<  8 )
                         |    ( ip0[j] & 255 ) ;
	  j += ipack;
	}
    }

  cp0 = (char *) ( up0 + inner );
  if ( trail > 0 )
    {
      up0[inner] = 0;
      for ( i = 0 ; i < trail ; i ++ )
	{
	  *cp0 = (char ) ip0[ipack*inner+i];
	  cp0++;
	}
    }

  if ( tc != -1 )
    {
      bc++;
      *cp0 = (char) tc;
    }

  return (bc);
}
#endif

/* unpack 8-bit bytes from a packed buffer with 32-bit words */
/* same as : for ( int i = 0; i < bc; ++i ) up[i] = (INT32) cp[i]; */

#if  defined  (INT32)
long unpackInt32(unsigned char *cp, unsigned INT32 *up, long bc, long tc)
{
  static union {unsigned long l; unsigned char c[sizeof(long)];} u_byteorder = {1};
  unsigned char *cp0;
  unsigned INT32 *up0;
  unsigned INT32 *ip0, *ip1, *ip2, *ip3;
  long head, trail, inner, i, j;
  long offset;
  int ipack = sizeof(INT32);

  /* Bytes until first word boundary in source buffer */

  head = ( (long) cp ) & (ipack-1);
  if ( head != 0 ) head = ipack - head;
  if ( head > bc ) head = bc;

  inner = bc - head;

  /* Trailing bytes which do not make a full word */
 
  trail = inner & (ipack-1);
 
  /* Number of bytes/words to be processed in fast loop */

  inner -= trail;
  inner /= ipack;

  ip0 = up + head;
  ip1 = ip0 + 1;
  ip2 = ip0 + 2;
  ip3 = ip0 + 3;

  up0 = (unsigned INT32 *) (cp + head);

  /* Process any bytes until the first word boundary 
   * of our source buffer 
   */
  for ( i = 0 ; i < head ; i++ ) up[i] = (unsigned INT32) cp[i];

  j = 0;

  if ( IsBigendian() )
    {
#if defined (CRAY)
#pragma _CRI ivdep
#endif
#if defined (SX)
#pragma vdir nodep
#endif
#ifdef __uxpch__
#pragma loop novrec
#endif
      for ( i = 0 ; i < inner ; i++ )
	{
	  ip0[j] = (up0[i] >> 24) & 255;
	  ip1[j] = (up0[i] >> 16) & 255;
	  ip2[j] = (up0[i] >>  8) & 255;
	  ip3[j] = (up0[i])       & 255;

	  j += ipack;
	}
    }
  else
    {
      for ( i = 0 ; i < inner ; i++ )
	{
	  ip3[j] = (up0[i] >> 24) & 255;
	  ip2[j] = (up0[i] >> 16) & 255;
	  ip1[j] = (up0[i] >>  8) & 255;
	  ip0[j] = (up0[i])       & 255;

	  j += ipack;
	}
    }

  if ( trail > 0 )
    {
      offset = head + ipack*inner;
      cp0 = cp + offset;
      for ( i = 0 ; i < trail ; i++ ) up[i+offset] = (unsigned INT32) cp0[i];
    }
  /*
  if ( tc != -1 ) {
    bc++;
    *cp0 = (char) tc;
  }
  */
  return (bc);
}
#endif

void prtbin(int kin, int knbit, int *kout, int *kerr)
{
  /*

    Produces a decimal number with ones and zeroes
    corresponding to the ones and zeroes of the input
    binary number.
    eg input number 1011 binary, output number 1011 decimal.


    Input Parameters:
    
       kin   - Integer variable containing binary number.

       knbit - Number of bits in binary number.

    Output Parameters:

       kout  - Integer variable containing decimal value
               with ones and zeroes corresponding to those of
	       the input binary number.

       kerr  - 0, If no error.
               1, Number of bits in binary number exceeds
	          maximum allowed or is less than 1.


    Converted from EMOS routine PRTBIN.

       Uwe Schulzweida   MPIfM   01/04/2001

  */
  int idec;
  int ik;
  int itemp;
  int j;

  /*
    Check length of binary number to ensure decimal number
    generated will fit in the computer word - in this case will
    it fit in a Cray 48 bit integer?
  */
  if ( knbit < 1 || knbit > 14 )
    {
      *kerr = 1;
      printf(" prtbin : Error in binary number length - %3d bits.\n", knbit);
      return;
    }
  else
    *kerr = 0;
  /*
    -----------------------------------------------------------------
    Section 1. Generate required number.
    -----------------------------------------------------------------
  */
  *kout = 0;
  ik    = kin;
  idec  = 1;

  for ( j = 0; j < knbit; j++ )
    {
      itemp = ik - ( (ik/2)*2 );
      *kout = (*kout) + itemp * idec;
      ik    = ik / 2;
      idec  = idec * 10;
    }

  return;
}


void ref2ibm(double *pref, int kbits)
{
  /*

    Purpose:
    --------

    Code and check reference value in IBM format

    Input Parameters:
    -----------------

    pref       - Reference value
    kbits      - Number of bits per computer word.

    Output Parameters:
    ------------------

    pref       - Reference value

    Method:
    -------

    Codes in IBM format, then decides to ensure that reference 
    value used for packing is not different from that stored
    because of packing differences.

    Externals.
    ----------

    confp3    - Encode into IBM floating point format.
    decfp2    - Decode from IBM floating point format.

    Reference:
    ----------

    None.

    Comments:
    --------

    None.

    Author:
    -------

    J.D.Chambers     ECMWF      17:05:94

    Modifications:
    --------------

    Uwe Schulzweida   MPIfM   01/04/2001

    Convert to C from EMOS library version 130

  */

  static char func[] = "ref2ibm";
  static int itrnd;
  static int kexp, kmant;
  static double ztemp, zdumm;
  extern int GRB_Debug;

  /* ----------------------------------------------------------------- */
  /*   Section 1. Convert to and from IBM format.                      */
  /* ----------------------------------------------------------------- */

  /*  Convert floating point reference value to IBM representation. */

  itrnd = 1;
  zdumm = ztemp = *pref;
  confp3(zdumm, &kexp, &kmant, kbits, itrnd);

  if ( kexp == 0 && kmant == 0 ) return;

  /*  Set reference value to that actually stored in the GRIB code. */

  *pref = decfp2(kexp, kmant);

  /*  If the nearest number which can be represented in */
  /*  GRIB format is greater than the reference value,  */
  /*  find the nearest number in GRIB format lower      */
  /*  than the reference value.                         */

  if ( ztemp < *pref )
    {
      /*  Convert floating point to GRIB representation */
      /*  using truncation to ensure that the converted */
      /*  number is smaller than the original one.      */

      itrnd = 0;
      zdumm = *pref = ztemp;
      confp3(zdumm, &kexp, &kmant, kbits, itrnd);

      /*  Set reference value to that stored in the GRIB code. */

      *pref = decfp2(kexp, kmant);

      if ( ztemp < *pref )
	{
	  if ( GRB_Debug )
	    {
	      Message(func, "Reference value error.");
	      Message(func, "Notify Met.Applications Section.");
	      Message(func, "ZTEMP = ", ztemp);
	      Message(func, "PREF = ", pref);
	    }
	  *pref = ztemp;
	}
    }

  return;
} /* ref2ibm */


int gribSections(unsigned char *gribbuffer, long recsize, unsigned char **pdsp,
		 unsigned char **gdsp, unsigned char **bmsp, unsigned char **bdsp)
{
  unsigned char *pds, *gds, *bms, *bds;
  unsigned char *bufpointer, *is;
  int gribversion, grib1offset;
  long gribsize = 0;

  is = gribbuffer;
  if ( is[0] != 'G' || is[1] != 'R' || is[2] != 'I' || is[3] != 'B' )
    {
      fprintf(stderr, "wrong indicator section >%c%c%c%c<\n",
		 is[0], is[1], is[2], is[3]);
      return (-1);
    }

  gribversion = IS_GRIB_Version;
  if ( IS_GRIB_Len == 24 && gribversion == 0 ) gribversion = 0;

  if ( gribversion == 1 )
    grib1offset = 4;
  else
    grib1offset = 0;

  pds = is + 4 + grib1offset;
  bufpointer = pds + PDS_Len;
  gribsize += 4 + grib1offset + PDS_Len;

  if ( PDS_HAS_GDS )
    {
      gds = bufpointer;
      bufpointer += GDS_Len;
      gribsize += GDS_Len;
    }
  else
    {
      gds = NULL;
    }

  if ( PDS_HAS_BMS )
    {
      bms = bufpointer;
      bufpointer += BMS_Len;
      gribsize += BMS_Len;
    }
  else
    {
      bms = NULL;
    }

  bds = bufpointer;
  bufpointer += BDS_Len;
  gribsize += BDS_Len;
  gribsize += 4;
  if ( gribsize > recsize )
    {
      fprintf(stderr, "GRIB buffer size %ld too small! Min size = %ld\n", recsize, gribsize);
      return (1);
    }

  *pdsp = pds;
  *gdsp = gds;
  *bmsp = bms;
  *bdsp = bds;

  /* end section - "7777" in ascii */
  if ( bufpointer[0] != 0x37 || bufpointer[1] != 0x37 ||
       bufpointer[2] != 0x37 || bufpointer[3] != 0x37 )
    {
      fprintf(stderr, "Missing end section >%2x %2x %2x %2x<\n",
	      bufpointer[0], bufpointer[1], bufpointer[2], bufpointer[3]);
    }

  return (0);
}


int gribGinfo(long recpos, long recsize, unsigned char *gribbuffer,
	      int *intnum, float *fltnum)
{
  unsigned char *pds, *gds, *bms, *bds;
  unsigned char *bufpointer, *is;
  int gribversion, grib1offset;
  long gribsize = 0;
  int dpos, bpos = 0;
  float bsf;

  is = gribbuffer;
  if ( is[0] != 'G' || is[1] != 'R' || is[2] != 'I' || is[3] != 'B' )
    {
      fprintf(stderr, "wrong indicator section >%c%c%c%c<\n",
		 is[0], is[1], is[2], is[3]);
      return (-1);
    }

  gribversion = IS_GRIB_Version;
  if ( IS_GRIB_Len == 24 && gribversion == 0 ) gribversion = 0;

  if ( gribversion == 1 )
    grib1offset = 4;
  else
    grib1offset = 0;

  pds = is + 4 + grib1offset;
  bufpointer = pds + PDS_Len;
  gribsize += 4 + grib1offset + PDS_Len;

  if ( PDS_HAS_GDS )
    {
      gds = bufpointer;
      bufpointer += GDS_Len;
      gribsize += GDS_Len;
    }
  else
    {
      gds = NULL;
    }

  if ( PDS_HAS_BMS )
    {
      bms = bufpointer;
      bufpointer += BMS_Len;

      bpos = recpos + gribsize + 6;

      gribsize += BMS_Len;
    }
  else
    {
      bms = NULL;
    }

  bds = bufpointer;
  bufpointer += BDS_Len;

  dpos = recpos + gribsize + 11;

  gribsize += BDS_Len;
  gribsize += 4;
  if ( gribsize > recsize )
    {
      fprintf(stderr, "GRIB buffer size %ld too small! Min size = %ld\n", recsize, gribsize);
      return (1);
    }

  /* end section - "7777" in ascii */
  if ( bufpointer[0] != 0x37 || bufpointer[1] != 0x37 ||
       bufpointer[2] != 0x37 || bufpointer[3] != 0x37 )
    {
      fprintf(stderr, "Missing end section >%2x %2x %2x %2x<\n",
	      bufpointer[0], bufpointer[1], bufpointer[2], bufpointer[3]);
    }

  bsf = BDS_BinScale;
  if ( bsf > 32767 ) bsf = 32768-bsf;
  bsf = pow(2.0,(double)bsf);

  intnum[0] = dpos;
  if ( bms ) intnum[1] = bpos;
  else       intnum[1] = -999;
  intnum[2] = BDS_NumBits;

  /*  fltnum[0] = 1.0; */
  fltnum[0] = pow(10.0, (double)PDS_DecimalScale);
  fltnum[1] = bsf;
  fltnum[2] = BDS_RefValue;
  /*
  printf("intnum %d %d %d\n", intnum[0], intnum[1], intnum[2]);
  printf("fltnum %g %g %g\n", fltnum[0], fltnum[1], fltnum[2]);
  */
  return (0);
}


void gribPrintALL(int nrec, long offset, long recpos, long recsize, unsigned char *gribbuffer)
{
  static int header = 1;
  int GridType, level, nerr;
  unsigned char *is = NULL, *pds = NULL, *gds = NULL, *bms = NULL, *bds = NULL;
  double cr = 1;

  if ( header )
    {
      fprintf(stdout, 
      "  Rec : Off Position   Size : V PDS  GDS    BMS    BDS : Code Level :  LType GType: CR\n");
/*     ----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+ */
      header = 0;
    }

  is = gribbuffer;

  nerr = gribSections(gribbuffer, recsize, &pds, &gds, &bms, &bds);

  if ( nerr )
    {
      fprintf(stdout, "%5d :%4ld %8ld %6ld : error\n", nrec, offset, recpos, recsize);
      return;
    }

  if ( gds == NULL )
    GridType = -1;
  else
    GridType = GDS_GridType;

  if ( PDS_LevelType == 100 )
    level = PDS_Level * 100;
  else if ( PDS_LevelType == 99 )
    level = PDS_Level;
  else if ( PDS_LevelType == 109 )
    level = PDS_Level;
  else
    level = PDS_Level1;

  if ( (BDS_Flag >> 4)&1 && BDS_Z == 128 )
    {
      int s1, s2;
      s1 = ((int) ((bds[14]<<16)+(bds[15]<<8)+bds[16]));
      s2 = ((int) ((gribbuffer[4]<<16)+(gribbuffer[5]<<8)+gribbuffer[6]));
      cr = ((double)s1)/s2;
    }

  fprintf(stdout, "%5d :%4ld %8ld %6ld :%2d%4d%5d%7d%7d : %3d%7d : %5d %5d %6.4g\n",
	  nrec, offset, recpos, recsize, IS_GRIB_Version,
	  PDS_Len, GDS_Len, BMS_Len, BDS_Len,
	  PDS_Parameter, level, PDS_LevelType, GridType, cr);
}


void gribPrintPDS(int nrec, long recpos, long recsize, unsigned char *gribbuffer)
{
  static int header = 1;
  unsigned char *is = NULL, *pds = NULL, *gds = NULL, *bms = NULL, *bds = NULL;
  int century, subcenter, decimalscale, nerr;
  int fc_num = 0;
  int year = 0, date;

  if ( header )
    {
      fprintf(stdout, 
      "  Rec : PDS Tab Cen Sub Ver Grid Code LTyp Level1 Level2    Date  Time P1 P2 TU TR NAVE Scale FCnum CT\n");
/*     ----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+ */
      header = 0;
    }

  is = gribbuffer;

  nerr = gribSections(gribbuffer, recsize, &pds, &gds, &bms, &bds);

  if ( nerr )
    {
      fprintf(stdout, "%5d : error\n", nrec);
      return;
    }

  switch(IS_GRIB_Version)
    {   
    case 0:
      year                = GET_UINT1(pds[12]);
      century             = 1;
      subcenter           = 0;
      decimalscale        = 0;
      break;
    case 1:
      year                = PDS_Year;
      century             = PDS_Century;
      subcenter           = PDS_Subcenter;
      decimalscale        = PDS_DecimalScale;
      break;
    default:
      fprintf(stderr, "Grib version %d not supported!", IS_GRIB_Version);
      exit(EXIT_FAILURE);
    }

  if ( PDS_Len > 28 )
    if ( PDS_CenterID    == 98 || PDS_Subcenter == 98 ||
	(PDS_CenterID    ==  7 && PDS_Subcenter == 98) )
      if ( pds[40] == 1 )
	fc_num = GET_UINT1(pds[49]);

  if ( year < 0 )
    {
      date = (-year)*10000+PDS_Month*100+PDS_Day;
      century = -century;
    }
  else
    {
      date =    year*10000+PDS_Month*100+PDS_Day;
    }
      
  fprintf(stdout, "%5d :%4d%4d%4d%4d%4d %4d %4d%4d%7d%7d %8d%6d%3d%3d%3d%3d%5d%6d%5d%4d\n", nrec,
	  PDS_Len,  PDS_CodeTable,   PDS_CenterID, subcenter, PDS_ModelID,
	  PDS_GridDefinition, PDS_Parameter, PDS_LevelType, PDS_Level1, PDS_Level2,
	  date, PDS_Time, PDS_TimePeriod1, PDS_TimePeriod2, PDS_TimeUnit, PDS_TimeRange,
	  PDS_AvgNum, decimalscale, fc_num, century);
}


void gribPrintGDS(int nrec, long recpos, long recsize, unsigned char *gribbuffer)
{
  static int header = 1;
  int nerr;
  unsigned char *pds = NULL, *gds = NULL, *bms = NULL, *bds = NULL;

  if ( header )
    {
      fprintf(stdout, 
      "  Rec : GDS  NV PVPL Typ : xsize ysize   Lat1   Lon1   Lat2   Lon2    dx    dy\n");
/*     ----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+ */
      header = 0;
    }

  nerr = gribSections(gribbuffer, recsize, &pds, &gds, &bms, &bds);

  if ( nerr )
    {
      fprintf(stdout, "%5d : error\n", nrec);
      return;
    }

  if ( gds )
    fprintf(stdout, "%5d :%4d%4d%4d %4d :%6d%6d%7d%7d%7d%7d%6d%6d\n", nrec,
	    GDS_Len,  GDS_NV,   GDS_PVPL, GDS_GridType,
	    GDS_NumLon,   GDS_NumLat,
	    GDS_FirstLat, GDS_FirstLon,
	    GDS_LastLat,  GDS_LastLon,
	    GDS_LonIncr,  GDS_LatIncr);
  else
    fprintf(stdout, "%5d : Grid Description Section not defined\n", nrec);
}


void gribPrintBMS(int nrec, long recpos, long recsize, unsigned char *gribbuffer)
{
  static int header = 1;
  int level, nerr;
  unsigned char *pds = NULL, *gds = NULL, *bms = NULL, *bds = NULL;

  if ( header )
    {
      fprintf(stdout, 
      "  Rec : Code Level     BMS    Size\n");
/*     ----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+ */
      header = 0;
    }

  nerr = gribSections(gribbuffer, recsize, &pds, &gds, &bms, &bds);

  if ( nerr )
    {
      fprintf(stdout, "%5d : error\n", nrec);
      return;
    }

  if ( PDS_LevelType == 100 )
    level = PDS_Level * 100;
  else if ( PDS_LevelType == 99 )
    level = PDS_Level;
  else
    level = PDS_Level1;

  if ( bms )
    fprintf(stdout, "%5d :%4d%7d %7d %7d\n", nrec,
	    PDS_Parameter, level,
	    BMS_Len, BMS_BitmapSize);
  else
    fprintf(stdout, "%5d :%4d%7d Bit Map Section not defined\n", nrec,
	    PDS_Parameter, level);
}


void gribPrintBDS(int nrec, long recpos, long recsize, unsigned char *gribbuffer)
{
  static int header = 1;
  int level, nerr;
  unsigned char *pds = NULL, *gds = NULL, *bms = NULL, *bds = NULL;
  double cr = 1;
  double refval, scale;

  if ( header )
    {
      fprintf(stdout, 
      "  Rec : Code Level     BDS Flag     Scale   RefValue Bits  CR\n");
/*     ----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+ */
      header = 0;
    }

  nerr = gribSections(gribbuffer, recsize, &pds, &gds, &bms, &bds);

  if ( nerr )
    {
      fprintf(stdout, "%5d : error\n", nrec);
      return;
    }

  if ( PDS_LevelType == 100 )
    level = PDS_Level * 100;
  else if ( PDS_LevelType == 99 )
    level = PDS_Level;
  else
    level = PDS_Level1;

  if ( (BDS_Flag >> 4)&1 && BDS_Z == 128 )
    {
      int s1, s2;
      s1 = ((int) ((bds[17]<<16)+(bds[18]<<8)+bds[19]));
      s2 = ((int) ((bds[20]<<16)+(bds[21]<<8)+bds[22]));
      cr = ((double)s1)/s2;
    }

  refval = BDS_RefValue;

  if ( BDS_BinScale < 0 )
    scale = 1.0/pow(2.0, (double) -BDS_BinScale);
  else
    scale = pow(2.0, (double) BDS_BinScale);

  if ( PDS_DecimalScale )
    {
      double decscale;
      decscale = pow(10.0, (double)-PDS_DecimalScale);
      refval *= decscale;
      scale  *= decscale;
    }

  fprintf(stdout, "%5d :%4d%7d %7d %4d %8.5g %11.5g%4d %6.4g\n", nrec,
	  PDS_Parameter, level,
	  BDS_Len, BDS_Flag, scale, refval, BDS_NumBits, cr);
}

#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

/* #define SZTEST */

#if  defined  (HAVE_LIBSZ)
#if defined(__cplusplus)
extern "C" {
#endif
#  include "szlib.h"
#if defined (__cplusplus)
}
#endif

#  define OPTIONS_MASK        (SZ_RAW_OPTION_MASK | SZ_MSB_OPTION_MASK | SZ_NN_OPTION_MASK)
#  define PIXELS_PER_BLOCK    (8)
#  define PIXELS_PER_SCANLINE (PIXELS_PER_BLOCK*128)

#  define MIN_COMPRESS        (0.95)
#  define MIN_SIZE            (256)
#endif


#if  defined  (HAVE_LIBBZ)
#  include "bzlib.h"

#  define MIN_COMPRESS        (0.95)
#  define MIN_SIZE            (256)
#endif


int gribGetZip(long recsize, unsigned char *gribbuffer, long *urecsize)
{
  /* urecsize : uncompressed record size  */
  int compress = 0;
  int nerr;
  int bds_len, bds_nbits, bds_flag, bds_aflag, bds_rep, bds_cplx;
  long gribsize = 0;
  unsigned char *pds = NULL, *gds = NULL, *bms = NULL, *bds = NULL;

  nerr = gribSections(gribbuffer, recsize, &pds, &gds, &bms, &bds);
  if ( nerr )
    {
      fprintf(stdout, "gribSections error\n");
      return (compress);
    }

  bds_len   = BDS_Len;
  bds_nbits = BDS_NumBits;
  bds_flag  = BDS_Flag;
  bds_rep   =  bds_flag >> 7;
  bds_cplx  = (bds_flag >> 6)&1;
  bds_aflag = (bds_flag >> 4)&1;

  *urecsize = 0;
  if ( bds_aflag )
    {
      compress = BDS_Z;
      if ( compress == 128 )
	{
	  gribsize = ((int) ((bds[14]<<16)+(bds[15]<<8)+bds[16]));
	}
    }

  *urecsize = gribsize;

  return (compress);
}


int  gribBzip(unsigned char *dbuf, long dbufsize, unsigned char *sbuf, long sbufsize)
{
  static char func[] = "gribBzip";
  int nerr;
  int gribLen;
  int rec_len;
  static int libszwarn = 1;
  unsigned char *pds = NULL, *gds = NULL, *bms = NULL, *bds = NULL;

  gribLen = ((int) ((dbuf[4]<<16)+(dbuf[5]<<8)+dbuf[6]));
  rec_len = gribLen;

  nerr = gribSections(dbuf, sbufsize, &pds, &gds, &bms, &bds);
  if ( nerr )
    {
      fprintf(stdout, "gribSections error\n");
      return (rec_len);
    }

#if  defined  (HAVE_LIBBZ)

  {
    int bdsLen;
    int gribLenOld = 0;
    int status;
    int datstart, datsize;
    char *dest, *source;
    int destLen, sourceLen;
    int bds_len, bds_nbits, bds_flag, bds_rep, bds_cplx, bds_aflag;
    int bds_ubits;
    char tmpbuffer[1000000];
    char tmpbuffer2[1000000];

    bds_len   = BDS_Len;
    bds_nbits = BDS_NumBits;
    bds_flag  = BDS_Flag;
    bds_rep   =  bds_flag >> 7;
    bds_cplx  = (bds_flag >> 6)&1;
    bds_aflag = (bds_flag >> 4)&1;
    
    if ( bds_nbits != 8 && bds_nbits != 16 && bds_nbits != 24 && bds_nbits != 32 ) return (rec_len);
    
    if ( bds_rep == 0 )
      {
	datstart = 11;
      }
    else if ( bds_rep == 1 && bds_cplx == 0 )
      {
	datstart = 15;
      }
    else
      {
	fprintf(stderr, "compression of complex packed spectral data unsupported!\n");
	return (rec_len);
      }

    datsize = ((((bds_len - datstart)*8)/bds_nbits)*bds_nbits)/8;

    if ( datsize < MIN_SIZE ) return (rec_len);
    /*
    fprintf(stderr, "%d %d %d %d\n", bds_len, datstart, bds_len - datstart, datsize);
    */
    sourceLen = datsize;
    destLen   = dbufsize;
    
    source = bds + datstart;
    dest = sbuf;

    {
      int nbytes = bds_nbits/8;
      int i;

      if ( nbytes == 2 )
	{
	  /*
	  if ( datstart == 15 )
	    {
	      tmpbuffer2[0] = source[0];
	      tmpbuffer2[1] = source[1];
	      for ( i = 0; i < sourceLen/4; i++ )
		{
		  
		  tmpbuffer2[(sourceLen/4)*2+2+2*i]   = source[2+4*i];
		  tmpbuffer2[(sourceLen/4)*2+2+2*i+1] = source[2+4*i+1];
		  
		  tmpbuffer2[2+2*i]   = source[2+4*i+2];
		  tmpbuffer2[2+2*i+1]   = source[2+4*i+3];
		  
		}

	      for ( i = 0; i < sourceLen/2; i++ )
		{
		  tmpbuffer[i] = tmpbuffer2[2*i];
		  tmpbuffer[sourceLen/2+i] = tmpbuffer2[2*i+1];
		}
	    }
	  else
	  */
	    {
	      for ( i = 0; i < sourceLen/2; i++ )
		{
		  tmpbuffer[i] = source[2*i];
		  tmpbuffer[sourceLen/2+i] = source[2*i+1];
		}
	    }
	}
      /*
      else if ( nbytes == 3 )
	{
	  for ( i = 0; i < sourceLen/3; i++ )
	    {
	      tmpbuffer[i] = source[3*i];
	      tmpbuffer[sourceLen/3+i] = source[3*i+1];
	      tmpbuffer[2*sourceLen/3+i] = source[3*i+2];
	    }
	}
      */
      else
	memcpy(tmpbuffer, source, (size_t) sourceLen);
    }
    
    status= BZ2_bzBuffToBuffCompress(dest, &destLen, tmpbuffer, sourceLen, 9, 0, 0);
    /*   status= BZ2_bzBuffToBuffCompress(dest, &destLen, source, sourceLen, 9, 0, 0); */
    if ( status != BZ_OK )
      Warning(func, "SZ ERROR: %d", status);
    /*
      {
	if ( status == SZ_NO_ENCODER_ERROR )
	  Warning(func, "SZ_NO_ENCODER_ERROR");
	else if ( status == SZ_PARAM_ERROR )
	  Warning(func, "SZ_PARAM_ERROR");
	else if ( status == SZ_MEM_ERROR )
	  Warning(func, "SZ_MEM_ERROR");
	else if ( status == SZ_OUTBUFF_FULL )
	  Warning(func, "SZ_OUTBUFF_FULL");
	else
	  Warning(func, "SZ ERROR: %d", status);
      }
    */
    /*
    fprintf(stderr, "sourceLen, destLen %d %d\n", sourceLen, destLen);
    */
    /*
    fprintf(stderr, "s>>> %d %d %d %d <<<\n", (int) source[0], (int)source[1], (int)source[2], (int)source[3]);
    fprintf(stderr, "d>>> %d %d %d %d <<<\n", (int) dest[0], (int)dest[1], (int)dest[2], (int)dest[3]);
    */
    
    if ( destLen < MIN_COMPRESS*sourceLen )
      {
	int zz;
	/*
	if ( sourceLen < 1000000 )
	{
	  char buffer[1000000];
	  size_t buffersize = 1000000;
	  status = SZ_BufftoBuffDecompress(buffer, &buffersize, dest, destLen, &sz_param);
	  if ( status != SZ_OK )
	    Warning(func, "SZ ERROR: %d", status);

	  if ( memcmp(buffer, source, sourceLen) != 0 )
	    {
	      Warning(func, "szip/sunzip error: code %3d level %3d  ibuflen %d obuflen %d\n",
		      PDS_Parameter, PDS_Level2, sourceLen, destLen);

	      return (rec_len);
	    }
	}
	*/
	source =  bds + datstart + 12;
	memcpy(source, dest, destLen);
	
	/* ----++++ number of unused bits at end of section) */

	bds_ubits  = bds_flag & 15;
	BDS_Flag -= bds_ubits;
    
	gribLenOld = ((int) ((dbuf[4]<<16)+(dbuf[5]<<8)+dbuf[6]));

	if ( datstart == 15 )
	  {
	    bds[11+12] = bds[11];
	    bds[12+12] = bds[12];
	    bds[13+12] = bds[13];
	    bds[14+12] = bds[14];
	  }

	/*
	fprintf(stderr, "destLen, datsize, datstart %d %d %d\n", destLen, datsize, datstart);
	*/
	/*	memcpy(bds + datstart + 12, source, destLen); */
	/*
    fprintf(stderr, "z>>> %d %d %d %d <<<\n", (int) bds[0+datstart+12],
	    (int)bds[1+datstart+12], (int)bds[2+datstart+12], (int)bds[3+datstart+12]);
	*/
	bds[14] = 255 & (gribLenOld >> 16);
	bds[15] = 255 & (gribLenOld >>  8);
	bds[16] = 255 & (gribLenOld);

	bds[17] = 255 & (sourceLen >> 16);
	bds[18] = 255 & (sourceLen >>  8);
	bds[19] = 255 & (sourceLen);

	bds[20] = 255 & (destLen >> 16);
	bds[21] = 255 & (destLen >>  8);
	bds[22] = 255 & (destLen);

	bdsLen = datstart + 12 + destLen;

	bds[11] = 0;
	bds[12] = 0;
	BDS_Z   = 128;

	BDS_Flag += 16;
	if ( (bdsLen%2) == 1 )
	  {
	    BDS_Flag += 8;
	    bds[bdsLen++] = 0;
	  }

	bds[0] = 255 & (bdsLen >> 16);
	bds[1] = 255 & (bdsLen >>  8);
	bds[2] = 255 & (bdsLen);

	gribLen = (bds - dbuf) + bdsLen;

	dbuf[gribLen++] = '7';
	dbuf[gribLen++] = '7';
	dbuf[gribLen++] = '7';
	dbuf[gribLen++] = '7';

	dbuf[4] = 255 & (gribLen >> 16);
	dbuf[5] = 255 & (gribLen >>  8);
	dbuf[6] = 255 & (gribLen);

	zz = gribLen;
	while ( zz & 7 ) dbuf[zz++] = 0;
      }
    
    fprintf(stderr, "%3d %3d griblen in %6d  out %6d  CR %g   slen %6d dlen %6d  CR %g\n",
	    PDS_Parameter, PDS_Level1, gribLenOld, gribLen,
	    ((double)gribLenOld)/gribLen, sourceLen, destLen,
	    ((double)sourceLen)/destLen);
    
  }

#else
  if ( libszwarn )
    {
      Warning(func, "Compression disabled, bzlib not available!");
      libszwarn = 0;
    }
#endif

  rec_len = gribLen;

  return (rec_len);
}


int  gribZip(unsigned char *dbuf, long dbufsize, unsigned char *sbuf, long sbufsize)
{
  static char func[] = "gribZip";
  int nerr;
  int gribLen;
  int rec_len;
#if ! defined  (HAVE_LIBSZ)
  static int libszwarn = 1;
#endif
  unsigned char *pds = NULL, *gds = NULL, *bms = NULL, *bds = NULL;
#if defined (SZTEST)
    char tmpbuffer[1000000];
    char tmpbuffer2[1000000];
#endif

  gribLen = ((int) ((dbuf[4]<<16)+(dbuf[5]<<8)+dbuf[6]));
  rec_len = gribLen;

  nerr = gribSections(dbuf, sbufsize, &pds, &gds, &bms, &bds);
  if ( nerr )
    {
      fprintf(stdout, "gribSections error\n");
      return (rec_len);
    }

#if  defined  (HAVE_LIBSZ)

  {
    int bdsLen;
    int gribLenOld = 0;
    int status;
    int datstart, datsize;
    SZ_com_t sz_param;          /* szip parameter block */
    unsigned char *dest, *source;
    size_t destLen, sourceLen;
    int bds_len, bds_nbits, bds_flag, bds_rep, bds_cplx, bds_aflag, bds_ubits;

    bds_len   = BDS_Len;
    bds_nbits = BDS_NumBits;
    bds_flag  = BDS_Flag;
    bds_ubits = bds_flag & 15;
    bds_rep   =  bds_flag >> 7;
    bds_cplx  = (bds_flag >> 6)&1;
    bds_aflag = (bds_flag >> 4)&1;
    
    if ( bds_nbits != 8 && bds_nbits != 16 && bds_nbits != 24 && bds_nbits != 32 )
      {
	static int linfo = 1;
	if ( linfo )
	  {
	    linfo = 0;
	    fprintf(stderr, "GRIB zip is only available for 8, 16, 24 and 32 bit data!\n");
	  }
	return (rec_len);
      }
    
    sz_param.options_mask        = OPTIONS_MASK;
#if defined (SZTEST)
    sz_param.bits_per_pixel      = 8;
    sz_param.pixels_per_block    = 8;
    sz_param.pixels_per_scanline = 1024;
#else
    if ( bds_nbits == 24 )
      sz_param.bits_per_pixel    = 8;
    else
      sz_param.bits_per_pixel    = bds_nbits;

    sz_param.pixels_per_block    = PIXELS_PER_BLOCK;
    sz_param.pixels_per_scanline = PIXELS_PER_SCANLINE;
#endif

    if ( bds_rep == 0 )
      {
	datstart = 11;
      }
    else if ( bds_rep == 1 && bds_cplx == 0 )
      {
	datstart = 15;
      }
    else
      {
	fprintf(stderr, "Compression of complex packed spectral data unsupported!\n");
	return (rec_len);
      }

    datsize = ((((bds_len - datstart)*8-bds_ubits)/bds_nbits)*bds_nbits)/8;

    if ( datsize < MIN_SIZE ) return (rec_len);
    /*
    fprintf(stderr, "%d %d %d %d\n", bds_len, datstart, bds_len - datstart, datsize);
    */
    sourceLen = datsize;
    destLen   = dbufsize;
    
    source = bds + datstart;
    dest = sbuf;

#if ! defined (SZTEST)
    if ( bds_nbits == 24 )
      {
	int i, nelem;
        char *pbuf;
	nelem = sourceLen/3;
	pbuf = (char *) malloc(sourceLen);
	for ( i = 0; i < nelem; i++ )
	  {
	    pbuf[        i] = source[3*i  ];
	    pbuf[  nelem+i] = source[3*i+1];
	    pbuf[2*nelem+i] = source[3*i+2];
	  }
	memcpy(source, pbuf, sourceLen);
	free(pbuf);
      }
#endif

#if defined (SZTEST)
    {
      int i;
      if ( bds_nbits == 16 )
	{
	  if ( datstart == 15 )
	    {
	      tmpbuffer2[0] = source[0];
	      tmpbuffer2[1] = source[1];
	      for ( i = 0; i < sourceLen/4; i++ )
		{
		  tmpbuffer2[(sourceLen/4)*2+2+2*i]   = source[2+4*i];
		  tmpbuffer2[(sourceLen/4)*2+2+2*i+1] = source[2+4*i+1];
		  
		  tmpbuffer2[2+2*i]   = source[2+4*i+2];
		  tmpbuffer2[2+2*i+1] = source[2+4*i+3];
		}

	      for ( i = 0; i < sourceLen/2; i++ )
		{
		  tmpbuffer[            i] = tmpbuffer2[2*i];
		  tmpbuffer[sourceLen/2+i] = tmpbuffer2[2*i+1];
		}
	    }
	  else
	    {
	      for ( i = 0; i < sourceLen/2; i++ )
		{
		  tmpbuffer[            i] = source[2*i];
		  tmpbuffer[sourceLen/2+i] = source[2*i+1];
		}
	    }
	}
      else if ( bds_nbits == 24 )
	{
	  for ( i = 0; i < sourceLen/3; i++ )
	    {
	      tmpbuffer[              i] = source[3*i];
	      tmpbuffer[  sourceLen/3+i] = source[3*i+1];
	      tmpbuffer[2*sourceLen/3+i] = source[3*i+2];
	    }
	}
      else
	memcpy(tmpbuffer, source, (size_t) sourceLen);

      source = tmpbuffer;
    }
#endif

    status = SZ_BufftoBuffCompress(dest, &destLen, source, sourceLen, &sz_param);
    if ( status != SZ_OK )
      {
	if ( status == SZ_NO_ENCODER_ERROR )
	  Warning(func, "SZ_NO_ENCODER_ERROR code %3d level %3d", PDS_Parameter, PDS_Level2);
	else if ( status == SZ_PARAM_ERROR )
	  Warning(func, "SZ_PARAM_ERROR code %3d level %3d", PDS_Parameter, PDS_Level2);
	else if ( status == SZ_MEM_ERROR )
	  Warning(func, "SZ_MEM_ERROR code %3d level %3d", PDS_Parameter, PDS_Level2);
	else if ( status == SZ_OUTBUFF_FULL )
	  Warning(func, "SZ_OUTBUFF_FULL code %3d level %3d", PDS_Parameter, PDS_Level2);
	else
	  Warning(func, "SZ ERROR: %d code %3d level %3d", status, PDS_Parameter, PDS_Level2);
      }
    /*
    fprintf(stderr, "sourceLen, destLen %d %d\n", sourceLen, destLen);
    */
    
    if ( destLen < MIN_COMPRESS*sourceLen )
      {
	/*
	if ( sourceLen < 1000000 )
	{
	  char buffer[1000000];
	  size_t buffersize = 1000000;
	  status = SZ_BufftoBuffDecompress(buffer, &buffersize, dest, destLen, &sz_param);
	  if ( status != SZ_OK )
	    Warning(func, "SZ ERROR: %d", status);

	  if ( memcmp(buffer, source, sourceLen) != 0 )
	    {
	      Warning(func, "szip/sunzip error: code %3d level %3d  ibuflen %d obuflen %d\n",
		      PDS_Parameter, PDS_Level2, sourceLen, destLen);

	      return (rec_len);
	    }
	}
	*/
	source =  bds + datstart + 12;
	memcpy(source, dest, destLen);
	
	/* ----++++ number of unused bits at end of section) */

	BDS_Flag -= bds_ubits;
    
	gribLenOld = ((int) ((dbuf[4]<<16)+(dbuf[5]<<8)+dbuf[6]));

	if ( datstart == 15 )
	  {
	    bds[11+12] = bds[11];
	    bds[12+12] = bds[12];
	    bds[13+12] = bds[13];
	    bds[14+12] = bds[14];
	  }

	/*
	fprintf(stderr, "destLen, datsize, datstart %d %d %d\n", destLen, datsize, datstart);
	*/
	/*	memcpy(bds + datstart + 12, source, destLen); */
	/*
    fprintf(stderr, "z>>> %d %d %d %d <<<\n", (int) bds[0+datstart+12],
	    (int)bds[1+datstart+12], (int)bds[2+datstart+12], (int)bds[3+datstart+12]);
	*/
	bds[14] = 255 & (gribLenOld >> 16);
	bds[15] = 255 & (gribLenOld >>  8);
	bds[16] = 255 & (gribLenOld);

	bds[17] = 255 & (sourceLen >> 16);
	bds[18] = 255 & (sourceLen >>  8);
	bds[19] = 255 & (sourceLen);

	bds[20] = 255 & (destLen >> 16);
	bds[21] = 255 & (destLen >>  8);
	bds[22] = 255 & (destLen);

	bdsLen = datstart + 12 + destLen;

	bds[11] = 0;
	bds[12] = 0;
	BDS_Z   = 128;

	BDS_Flag += 16;
	if ( (bdsLen%2) == 1 )
	  {
	    BDS_Flag += 8;
	    bds[bdsLen++] = 0;
	  }

	bds[0] = 255 & (bdsLen >> 16);
	bds[1] = 255 & (bdsLen >>  8);
	bds[2] = 255 & (bdsLen);

	gribLen = (bds - dbuf) + bdsLen;

	dbuf[gribLen++] = '7';
	dbuf[gribLen++] = '7';
	dbuf[gribLen++] = '7';
	dbuf[gribLen++] = '7';

	dbuf[4] = 255 & (gribLen >> 16);
	dbuf[5] = 255 & (gribLen >>  8);
	dbuf[6] = 255 & (gribLen);
      }
    /*
    fprintf(stderr, "%3d %3d griblen in %6d  out %6d  CR %g   slen %6d dlen %6d  CR %g\n",
	    PDS_Parameter, PDS_Level1, gribLenOld, gribLen,
	    ((double)gribLenOld)/gribLen, sourceLen, destLen,
	    ((double)sourceLen)/destLen);
    */
  }

#else
  if ( libszwarn )
    {
      Warning(func, "Compression disabled, szlib not available!");
      libszwarn = 0;
    }
#endif

  while ( gribLen & 7 ) dbuf[gribLen++] = 0;

  rec_len = gribLen;

  return (rec_len);
}


int  gribUnzip(unsigned char *dbuf, long dbufsize, unsigned char *sbuf, long sbufsize)
{
  static char func[] = "gribUnzip";
#if ! defined  (HAVE_LIBSZ)
  static int libszwarn = 1;
#endif
  int nerr;
  unsigned char *pds = NULL, *gds = NULL, *bms = NULL, *bds = NULL;
  int bdsLen, recLen, gribLen = 0;
  char *dest, *source;
  size_t destLen, sourceLen;
  int bds_len, bds_nbits, bds_flag, bds_rep, bds_cplx, bds_aflag;
  int datstart = 0;

  nerr = gribSections(sbuf, sbufsize, &pds, &gds, &bms, &bds);
  if ( nerr )
    {
      fprintf(stdout, "gribSections error\n");
      return (0);
    }

  recLen = ((int) ((bds[14]<<16)+(bds[15]<<8)+bds[16]));

  bds_len   = BDS_Len;
  bds_nbits = BDS_NumBits;
  bds_flag  = BDS_Flag;
  bds_rep   =  bds_flag >> 7;
  bds_cplx  = (bds_flag >> 6)&1;
  bds_aflag = (bds_flag >> 4)&1;

  if ( bds_rep == 0 )
    {
      datstart = 11;
    }
  else if ( bds_rep == 1 && bds_cplx == 0 )
    {
      datstart = 15;
    }
  else
    {
      fprintf(stderr, "compression of complex packed spectral data unsupported!\n");
      return (0);
    }

  source = (char *) bds + datstart + 12;
  sourceLen = ((int) ((bds[20]<<16)+(bds[21]<<8)+bds[22]));

  nerr = gribSections(dbuf, sbufsize, &pds, &gds, &bms, &bds);
  if ( nerr )
    {
      fprintf(stdout, "gribSections error\n");
      return (0);
    }

  dest = (char *) bds + datstart;
  destLen = ((int) ((bds[17]<<16)+(bds[18]<<8)+bds[19]));

  BDS_Flag -= 16;

  bdsLen = datstart + destLen;

#if  defined  (HAVE_LIBSZ)
  {
    int status;
    size_t tmpLen;
    int bds_ubits;
    SZ_com_t sz_param;          /* szip parameter block */

    sz_param.options_mask        = OPTIONS_MASK;

    if ( bds_nbits == 24 )
      sz_param.bits_per_pixel    = 8;
    else
      sz_param.bits_per_pixel    = bds_nbits;

    sz_param.pixels_per_block    = PIXELS_PER_BLOCK;
    sz_param.pixels_per_scanline = PIXELS_PER_SCANLINE;

    if ( datstart == 15 )
      {
	bds[11] = bds[11+12];
	bds[12] = bds[12+12];
	bds[13] = bds[13+12];
	bds[14] = bds[14+12];
      }

    /*
    fprintf(stderr, "gribUnzip: sourceLen %ld; destLen %ld\n", (long)sourceLen, (long)destLen);
    fprintf(stderr, "gribUnzip: sourceOff %d; destOff %d\n", bds[12], bds[11]);
    fprintf(stderr, "gribUnzip: reclen %d; bdslen %d\n", recLen, bdsLen);
    */

    tmpLen = destLen;
    status = SZ_BufftoBuffDecompress(dest, &tmpLen, source, sourceLen, &sz_param);
    if ( status != SZ_OK )
      {
	if ( status == SZ_NO_ENCODER_ERROR )
	  Warning(func, "SZ_NO_ENCODER_ERROR code %3d level %3d", PDS_Parameter, PDS_Level2);
	else if ( status == SZ_PARAM_ERROR )
	  Warning(func, "SZ_PARAM_ERROR code %3d level %3d", PDS_Parameter, PDS_Level2);
	else if ( status == SZ_MEM_ERROR )
	  Warning(func, "SZ_MEM_ERROR code %3d level %3d", PDS_Parameter, PDS_Level2);
	else if ( status == SZ_OUTBUFF_FULL )
	  Warning(func, "SZ_OUTBUFF_FULL code %3d level %3d", PDS_Parameter, PDS_Level2);
	else
	  Warning(func, "SZ ERROR: %d code %3d level %3d", status, PDS_Parameter, PDS_Level2);
      }
    /*
    fprintf(stderr, "gribUnzip: sl = %ld  dl = %ld   tl = %ld\n",
	    (long)sourceLen, (long)destLen,(long) tmpLen);
    */
    if ( tmpLen != destLen )
      Warning(func, "unzip size differ: code %3d level %3d  ibuflen %ld ubuflen %ld\n",
	      PDS_Parameter, PDS_Level2, (long) destLen, (long) tmpLen);
 
    if ( bds_nbits == 24 )
      {
	int i, nelem;
	char *pbuf;
	nelem = tmpLen/3;
	pbuf = (char *) malloc(tmpLen);
	for ( i = 0; i < nelem; i++ )
	  {
	    pbuf[3*i  ] = dest[        i];
	    pbuf[3*i+1] = dest[  nelem+i];
	    pbuf[3*i+2] = dest[2*nelem+i];
	  }
	memcpy(dest, pbuf, tmpLen);
	free(pbuf);
      }
  
    bds_ubits  = BDS_Flag & 15;
    BDS_Flag -= bds_ubits;

    if ( (bdsLen%2) == 1 )
      {
	BDS_Flag += 8;
	bds[bdsLen++] = 0;
      }

    bds[0] = 255 & (bdsLen >> 16);
    bds[1] = 255 & (bdsLen >>  8);
    bds[2] = 255 & (bdsLen);

    gribLen = (bds - dbuf) + bdsLen;
    
    dbuf[gribLen++] = '7';
    dbuf[gribLen++] = '7';
    dbuf[gribLen++] = '7';
    dbuf[gribLen++] = '7';

    dbuf[4] = 255 & (recLen >> 16);
    dbuf[5] = 255 & (recLen >>  8);
    dbuf[6] = 255 & (recLen);
    /*
    fprintf(stderr, "recLen, gribLen, bdsLen %d %d %d\n", recLen, gribLen, bdsLen);
    */
    while ( gribLen & 7 ) dbuf[gribLen++] = 0;
    /*
    fprintf(stderr, "recLen, gribLen, bdsLen %d %d %d\n", recLen, gribLen, bdsLen);
    */
  }
#else
  if ( libszwarn )
    {
      Warning(func, "Decompression disabled, szlib not available!");
      libszwarn = 0;
    }
#endif

  return (gribLen);
}
static const char grb_libvers[] = "1.2.0" " of ""Jul 23 2008"" ""15:50:19";
const char *
gribLibraryVersion(void)
{
  return (grb_libvers);
}
