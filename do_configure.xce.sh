module swap grib_api grib_api/1.13.0-prerelease

CC="cc" \
LDFLAGS="-L$NETCDF_DIR/lib $GRIB_API_LIB"  \
CFLAGS="$GRIB_API_INCLUDE -I$NETCDF_DIR/include " \
 ./configure --prefix=$HOME/CDI/branches/cdi-tiles/build \
--with-netcdf=yes --with-grib-api=yes --disable-shared
