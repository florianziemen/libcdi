#ifndef _GRIBAPI_H
#define _GRIBAPI_H

#define  GRIBAPI_MISSVAL  -9.E33

/* Level Types */
#define  GRIBAPI_LTYPE_SURFACE               1
#define  GRIBAPI_LTYPE_ISOBARIC            100
#define  GRIBAPI_LTYPE_MEANSEA             101
#define  GRIBAPI_LTYPE_ALTITUDE            102
#define  GRIBAPI_LTYPE_HEIGHT              103
#define  GRIBAPI_LTYPE_SIGMA               104
#define  GRIBAPI_LTYPE_HYBRID              105
#define  GRIBAPI_LTYPE_LANDDEPTH           106
#define  GRIBAPI_LTYPE_ISENTROPIC          107
#define  GRIBAPI_LTYPE_SEADEPTH            160

/*
 *  Data representation type (Grid Type)
 */
#define  GRIBAPI_GTYPE_LATLON                0  /*  latitude/longitude                       */
#define  GRIBAPI_GTYPE_LATLON_ROT            1  /*  rotated latitude/longitude               */
#define  GRIBAPI_GTYPE_LATLON_STR            2  /*  stretched latitude/longitude             */
#define  GRIBAPI_GTYPE_LATLON_ROTSTR         3  /*  rotated and stretched latitude/longitude */
#define  GRIBAPI_GTYPE_GAUSSIAN             40  /*  gaussian grid                            */
#define  GRIBAPI_GTYPE_GAUSSIAN_ROT         41  /*  rotated gaussian grid                    */
#define  GRIBAPI_GTYPE_GAUSSIAN_STR         42  /*  stretched gaussian grid                  */
#define  GRIBAPI_GTYPE_GAUSSIAN_ROTSTR      43  /*  rotated and stretched gaussian grid      */
#define  GRIBAPI_GTYPE_LCC                  30  /*  Lambert conformal                        */
#define  GRIBAPI_GTYPE_SPECTRAL             50  /*  spherical harmonics                      */
#define  GRIBAPI_GTYPE_GME                 192  /*  hexagonal GME grid                       */

const char *gribapiLibraryVersion(void);
void gribapiDefHandles(int streamID);
void *gribHandleNew();
void gribHandleDelete(void *gh);

#endif  /* _GRIBAPI_H */
