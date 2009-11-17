import os, string

INCFLAGS = []
CFLAGS   = []
LDFLAGS  = []
LIBS     = []
if os.environ.has_key('INCFLAGS'):
   INCFLAGS = string.split(os.environ['INCFLAGS'])
if os.environ.has_key('CFLAGS'):
   CFLAGS   = string.split(os.environ['CFLAGS'])
if os.environ.has_key('LDFLAGS'):
  LDFLAGS   = string.split(os.environ['LDFLAGS'])
if os.environ.has_key('LIBS'):
  LIBS      = string.split(os.environ['LIBS'])
