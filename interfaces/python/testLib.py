#! /usr/bin/env python
from __future__ import print_function
import sys, os
for env_var in [ 'abs_srcdir', 'abs_builddir_python',
                 'abs_builddir_python_build',  'abs_builddir' ]:
  v = os.getenv(env_var)
  if v != None:
    sys.path.insert(0, v)

import CdiLib
ifile_name = sys.argv[1] if len(sys.argv) > 1 else "../testdata/mulval.nc"


streamID = CdiLib.streamOpenRead(ifile_name)
if (streamID < 0):
  print('failed to open file', ifile_name, file=sys.stderr)
  sys.exit(1)

vlistID  = CdiLib.streamInqVlist(streamID)
nvars    = CdiLib.vlistNvars(vlistID)
for i in range(0,nvars):
  print(CdiLib.vlistInqVarCode(vlistID, i), end=(" " if i < nvars - 1
                                                 else "\n" ))

CdiLib.streamClose(streamID)
