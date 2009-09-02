#ifndef _GRIBAPI_H
#define _GRIBAPI_H

#define  GRIBAPI_MISSVAL  -9.E33

/* Level Types */
#define  GRIBAPI_LTYPE_SURFACE               1
#define  GRIBAPI_LTYPE_99                   99
#define  GRIBAPI_LTYPE_ISOBARIC            100
#define  GRIBAPI_LTYPE_MEANSEA             102
#define  GRIBAPI_LTYPE_ALTITUDE            103
#define  GRIBAPI_LTYPE_HEIGHT              105
#define  GRIBAPI_LTYPE_SIGMA               107
#define  GRIBAPI_LTYPE_HYBRID              109
#define  GRIBAPI_LTYPE_HYBRID_LAYER        110
#define  GRIBAPI_LTYPE_LANDDEPTH           111
#define  GRIBAPI_LTYPE_LANDDEPTH_LAYER     112
#define  GRIBAPI_LTYPE_ISENTROPIC          113
#define  GRIBAPI_LTYPE_SEADEPTH            160
#define  GRIBAPI_LTYPE_99_MARGIN          1000

/*
 *  Data representation type (Grid Type)
 */
#define  GRIBAPI_GTYPE_LATLON                0  /*  latitude/longitude                       */
#define  GRIBAPI_GTYPE_LATLON_ROT           10  /*  rotated latitude/longitude               */
#define  GRIBAPI_GTYPE_LATLON_STR           20  /*  stretched latitude/longitude             */
#define  GRIBAPI_GTYPE_LATLON_ROTSTR        30  /*  rotated and stretched latitude/longitude */
#define  GRIBAPI_GTYPE_GAUSSIAN             40  /*  gaussian grid                            */
#define  GRIBAPI_GTYPE_GAUSSIAN_ROT         14  /*  rotated gaussian grid                    */
#define  GRIBAPI_GTYPE_GAUSSIAN_STR         24  /*  stretched gaussian grid                  */
#define  GRIBAPI_GTYPE_GAUSSIAN_ROTSTR      34  /*  rotated and stretched gaussian grid      */
#define  GRIBAPI_GTYPE_LCC                   3  /*  Lambert conformal                        */
#define  GRIBAPI_GTYPE_SPECTRAL             50  /*  spherical harmonics                      */
#define  GRIBAPI_GTYPE_GME                 192  /*  hexagonal GME grid                       */

const char *gribapiLibraryVersion(void);

#endif  /* _GRIBAPI_H */
