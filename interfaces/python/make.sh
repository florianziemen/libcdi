#!/bin/sh
OBJECTS='../../src/.libs/libcdo.so'
swig -python -c++  -o cdiobj_wrap.cpp ../cdiobj.i
g++ -pthread -fno-strict-aliasing -DNDEBUG -g -fwrapv -O2 -Wall -fPIC -I../ -I/usr/include -I/usr/local/include -I/usr/include/python2.6 -c cdiobj_wrap.cpp -o build/temp.linux-i686-2.6/cdiobj_wrap.o   
g++ -pthread -shared -Wl,-O1 -Wl,-Bsymbolic-functions build/temp.linux-i686-2.6/cdiobj_wrap.o $OBJECTS -L/usr/lib -L/usr/local/lib -lnetcdf -lhdf5_hl -lhdf5 -lsz -lz -lm -o /home/ram/fromHome/cdi/interfaces/python/_CdiObj.so -ldl -lutil -lm -lpython2.6

