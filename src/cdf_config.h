#ifndef CDF_CONFIG_H_
#define CDF_CONFIG_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_LIBNETCDF

#include <netcdf.h>

#ifdef NC_FORMAT_64BIT_DATA
#define HAVE_NETCDF5 1
#endif

#if defined HAVE_NETCDF4 && HAVE_DECL_NC_INQ_VAR_FILTER_IDS && HAVE_DECL_NCAUX_H5FILTERSPEC_PARSELIST
#define HAVE_NC4FILTER 1
#endif

#endif

#endif
