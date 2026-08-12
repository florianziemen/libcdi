#ifndef CDI_PROJ_H
#define CDI_PROJ_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C"
{
#endif

  // CDI projection parameter interface

  struct CDI_GridProjParams
  {
    double mv;     // Missing value
    double lon_0;  // The East longitude of the meridian which is parallel to the Y-axis
    double lat_0;  // Latitude of the projection origin
    double lat_1;  // First latitude from the pole at which the secant cone cuts the sphere
    double lat_2;  // Second latitude at which the secant cone cuts the sphere
    // lat_ts = lat_1;
    double a;       // Semi-major axis or earth radius in metres (optional)
    double b;       // Semi-minor axis in metres (optional)
    double rf;      // Inverse flattening (1/f) (optional)
    double xval_0;  // Longitude of the first grid point in degree (optional)
    double yval_0;  // Latitude of the first grid point in degree (optional)
    double x_0;     // False easting (optional)
    double y_0;     // False northing (optional)
    double x_SP;    // Longitude of southern pole
    double y_SP;    // Latitude of southern pole
    int nside;      // HEALPix number of points along a side (number of data points should be = 12 * nside * nside)
    int order;      // HEALPix ordering convention (0:ring; 1:nested)
  };

  void gridProjParamsInit(struct CDI_GridProjParams *gridProjParams);

  // Lambert Conformal Conic grid
  void gridDefParamsLCC(int gridID, struct CDI_GridProjParams gridProjParams);
  int gridInqParamsLCC(int gridID, struct CDI_GridProjParams *gridProjParams);

  // Polar stereographic grid
  void gridDefParamsSTERE(int gridID, struct CDI_GridProjParams gridProjParams);
  int gridInqParamsSTERE(int gridID, struct CDI_GridProjParams *gridProjParams);

#define HAVE_CDI_PROJ_FUNCS 1
  extern int (*proj_lonlat_to_lcc_func)(struct CDI_GridProjParams gpp, size_t, double *, double *);
  extern int (*proj_lcc_to_lonlat_func)(struct CDI_GridProjParams gpp, double, double, size_t, double *, double *);
  extern int (*proj_lonlat_to_stere_func)(struct CDI_GridProjParams gpp, size_t, double *, double *);
  extern int (*proj_stere_to_lonlat_func)(struct CDI_GridProjParams gpp, double, double, size_t, double *, double *);

#ifdef __cplusplus
}
#endif

#endif
