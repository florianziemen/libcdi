#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "cdi.h"


int getopt(int argc, char *const argv[], const char *optstring);

extern char *optarg;
extern int optind, opterr, optopt;


char *Progname;

int DefaultFileType = CDI_UNDEFID;
int DefaultDataType = CDI_UNDEFID;


static void version(void)
{
  fprintf(stderr, "CDI version 1.5.0\n");
  cdiPrintVersion();
  fprintf(stderr, "\n");
/*
  1.0.0   6 Feb 2001 : initial version
  1.1.0  30 Jul 2003 : missing values implemented
  1.2.0   8 Aug 2003 : changes for CDI library version 0.7.0
  1.3.0  10 Feb 2004 : changes for CDI library version 0.7.9
  1.4.0   5 May 2004 : changes for CDI library version 0.8.1 (error handling)
  1.4.1  18 Sep 2004 : netCDF 2 support
  1.4.2  22 Mar 2005 : change level from int to double
  1.4.3  11 Apr 2005 : change date and time format to ISO
  1.5.0  22 Nov 2005 : IEG support
*/
}

static void usage(void)
{
  char *name;
  int id;

  fprintf(stderr, "usage : %s  [Option]  [ifile]  [ofile]\n", Progname);

  fprintf(stderr, "\n");
  fprintf(stderr, "  Options:\n");
  fprintf(stderr, "    -d             Print debugging information.\n");
  fprintf(stderr, "    -f <format>    Format of the output file. (grb, nc, nc2, hdf5, src or ext).\n");
  fprintf(stderr, "                   (e.g hdf5_8z  --> hdf5 format with compressed 8 byte reals)\n");
  fprintf(stderr, "    -t <table>     Parameter table name/file.\n");
  fprintf(stderr, "                   Predefined tables: ");
  for ( id = 0; id < tableInqNumber(); id++ )
    if ( (name = tableInqNamePtr(id)) )
      fprintf(stderr, " %s", name);
  fprintf(stderr, "\n");

  fprintf(stderr, "    -V             Print version number\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "  Report bugs to <schulzweida@dkrz.de>.\n");
}


static void decodeDate(int date, int *year, int *month, int *day)
{
  *year   =  date / 10000;
  *month  = (date - *year*10000) / 100;
  *day    =  date - *year*10000 - *month*100;
}


static void decodeTime(int time, int *hour, int *minute)
{
  *hour   =  time / 100;
  *minute =  time - *hour*100;
}


static void printInfo(int gridtype, int date, int time, int code, double level,
		      int datasize, int nmiss, double missval, const double *data)
{
  static int rec = 0;
  int i, ivals = 0, imiss = 0;
  int year, month, day, hour, minute;
  double arrmean, arrmin, arrmax;
 
  if ( ! rec )
  {
    fprintf(stdout, 
    "   Rec :       Date  Time Code  Level    Size    Miss :     Minimum        Mean     Maximum\n");
/*   ----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+ */
  }

  decodeDate(date, &year, &month, &day);
  decodeTime(time, &hour, &minute);

  fprintf(stdout, "%6d : %4.4d-%2.2d-%2.2d %2.2d:%2.2d%4d %7g ",
	  ++rec, year, month, day, hour, minute, code, level);

  fprintf(stdout, "%7d ", datasize);

  fprintf(stdout, "%7d :", nmiss);

  if ( gridtype == GRID_SPECTRAL )
    {
      fprintf(stdout, "            %#12.5g\n", data[0]);
    }
  else
    {
      if ( nmiss > 0 )
	{
	  arrmean = 0;
	  arrmin  =  1e50;
	  arrmax  = -1e50;
	  for ( i = 0; i < datasize; i++ )
	    {
	      if ( data[i] != missval )
		{
		  if ( data[i] < arrmin ) arrmin = data[i];
		  if ( data[i] > arrmax ) arrmax = data[i];
		  arrmean += data[i];
		  ivals++;
		}
	    }
	  imiss = datasize - ivals;
	  datasize = ivals;
	}
      else
	{
	  arrmean = data[0];
	  arrmin  = data[0];
	  arrmax  = data[0];
	  for ( i = 1; i < datasize; i++ )
	    {
	      if ( data[i] < arrmin ) arrmin = data[i];
	      if ( data[i] > arrmax ) arrmax = data[i];
	      arrmean += data[i];
	    }
	}

      if ( datasize > 0 ) arrmean /= datasize;

      fprintf(stdout, "%#12.5g%#12.5g%#12.5g\n", arrmin, arrmean, arrmax);

      if ( imiss != nmiss && nmiss > 0 )
	fprintf(stdout, "Found %d of %d missing values!\n", imiss, nmiss);
    }
}


static void setDefaultDataType(char *datatypestr)
{
  int datatype = -1;

  if ( isdigit((int) *datatypestr) )
    {
      datatype = atoi(datatypestr);
      datatypestr++;

      if      ( datatype == 1 ) DefaultDataType = DATATYPE_PACK1;
      else if ( datatype == 2 ) DefaultDataType = DATATYPE_PACK2;
      else if ( datatype == 3 ) DefaultDataType = DATATYPE_PACK3;
      else if ( datatype == 4 ) DefaultDataType = DATATYPE_REAL4;
      else if ( datatype == 8 ) DefaultDataType = DATATYPE_REAL8;
      else
	{
	  fprintf(stderr, "Unsupported datatype %d!\n", datatype);
	  fprintf(stderr, "Use 4/8 for filetype nc, hdf5, srv, ext, ieg and 1/2/3 for grb.\n");
	  exit(-1);
	}
    }

  if ( *datatypestr == 'z' || *datatypestr == 'Z' )
    {
      int level = 6;
      datatypestr++;
      if ( isdigit((int) *datatypestr) )
	{
	  level = atoi(datatypestr);
	  datatypestr++;	      
	}
      cdiDefCompress(level);
    }
}


static void setDefaultFileType(char *filetypestr)
{
  if ( filetypestr )
    {
      if      ( strncmp(filetypestr, "grb", 3)  == 0 ) DefaultFileType = FILETYPE_GRB;
      else if ( strncmp(filetypestr, "nc2", 3)  == 0 ) DefaultFileType = FILETYPE_NC2;
      else if ( strncmp(filetypestr, "nc",  2)  == 0 ) DefaultFileType = FILETYPE_NC;
      else if ( strncmp(filetypestr, "srv", 3)  == 0 ) DefaultFileType = FILETYPE_SRV;
      else if ( strncmp(filetypestr, "ext", 3)  == 0 ) DefaultFileType = FILETYPE_EXT;
      else if ( strncmp(filetypestr, "ieg", 3)  == 0 ) DefaultFileType = FILETYPE_IEG;
      else if ( strncmp(filetypestr, "h5",  2)  == 0 ) DefaultFileType = FILETYPE_H5;
      else if ( strncmp(filetypestr, "hdf5", 4) == 0 ) DefaultFileType = FILETYPE_H5;
      else
	{
	  fprintf(stderr, "Unsupported filetype %s!\n", filetypestr);
	  fprintf(stderr, "Available filetypes are grb, nc, hdf5, srv, ext, ieg.\n");
	  exit(-1);
	}

      if ( DefaultFileType != CDI_UNDEFID )
	{
	  if ( (filetypestr = strchr(filetypestr, '_' )) )
	    {
	      filetypestr++;

	      setDefaultDataType(filetypestr);
	    }
	}
    }
}


int handle_error(int cdiErrno, const char *fmt, ...)
{
  va_list args;
	
  va_start(args, fmt);

  printf("\n");
  vfprintf(stderr, fmt, args);
   fprintf(stderr, "\n");

  va_end(args);

  fprintf(stderr, "%s\n", cdiStringError(cdiErrno));

  return (cdiErrno);
}


int main(int argc, char *argv[])
{
  int c;
  char *fname1 = NULL;
  char *fname2 = NULL;
  char *rTable = NULL;
  char *wTable = NULL;
  int Move = 0;
  int Record = 0;
  int Debug = 0;
  int Quiet  = 0;
  int Verbose = 0;
  int Version = 0;
  int Longinfo = 0;
  int varID;
  int itableID = CDI_UNDEFID, otableID = CDI_UNDEFID;
  int linfo = 1;

  Progname = strrchr(argv[0], '/');
  if (Progname == 0) Progname = argv[0];
  else               Progname++;

  while ( (c = getopt(argc, argv, "f:t:w:cdhlmqrvVz")) != EOF )
    {
      switch (c)
	{
	case 'd':
	  Debug = 1;
	  break;
	case 'f':
	  setDefaultFileType(optarg);
	  break;
	case 'r':
	  Record = 1;
	  break;
	case 'l':
	  Longinfo = 1;
	  break;
	case 'm':
	  Move = 1;
	  break;
	case 'v':
	  Verbose = 1;
	  break;
	case 'V':
	  Version = 1;
	  break;
	case 'q':
	  Quiet = 1;
	  break;
	case 't':
	  rTable = optarg;
	  break;
	case 'w':
	  wTable = optarg;
	  break;
	case 'z':
	  cdiDefCompress(6);
	  break;
	case 'h':
	  usage();
	  exit (0);
	}
    }

  if ( optind < argc ) fname1 = argv[optind++];
  if ( optind < argc ) fname2 = argv[optind++];
  if ( optind < argc ) 
    {
       fprintf(stderr, "optind: %d argc: %d\n", optind, argc);
    }

  if ( Debug || Version ) version();

  if ( Verbose )
    {
      fprintf(stdout, "\n");
      if ( fname1 ) fprintf(stdout, "  < %s  \n", fname1);
      if ( fname2 ) fprintf(stdout, "  > %s\n\n", fname2);
    }


  if ( Debug ) cdiDebug(Debug);

  if ( rTable )
    {
      itableID = tableInq(-1, 0, rTable);
      otableID = itableID;
    }

  if ( fname1 == NULL && ! (Debug || Version) )
    {
      usage();
      exit (0);
    }

  if ( fname1 )
    {
      double *data = NULL;
      double missval;
      double level;
      int nmiss;
      int datasize = 0;
      int streamID1 = CDI_UNDEFID;
      int streamID2 = CDI_UNDEFID;
      int filetype;
      int gridID, zaxisID, code;
      int vdate, vtime;
      int nrecs, nvars;
      int levelID, levelsize;
      int nts = 0;
      int gridsize = 0;
      int recID;
      int tsID;
      int ntsteps = 0;
      int taxisID;
      int gridtype;
      int vlistID1, vlistID2 = CDI_UNDEFID;

      streamID1 = streamOpenRead(fname1);
      if ( streamID1 < 0 )
	return (handle_error(streamID1, "Open failed on %s", fname1));

      vlistID1 = streamInqVlist(streamID1);

      if ( Longinfo )
	{
	  int ngrids, nzaxis;
	  vlistPrint(vlistID1);
	  ngrids = vlistNgrids(vlistID1);
	  nzaxis = vlistNzaxis(vlistID1);
	  for ( gridID = 0; gridID < ngrids; gridID++ ) gridPrint(gridID);
	  for ( zaxisID = 0; zaxisID < nzaxis; zaxisID++ ) zaxisPrint(zaxisID);
	}

      nvars   = vlistNvars(vlistID1);
      taxisID = vlistInqTaxis(vlistID1);
      ntsteps = vlistNtsteps(vlistID1);

      if ( Debug ) fprintf(stderr, "nvars = %d\n", nvars);
      if ( Debug ) fprintf(stderr, "ntsteps = %d\n", ntsteps);

      if ( fname2 ) vlistID2 = vlistDuplicate(vlistID1);
	  
      for ( varID = 0; varID < nvars; varID++)
	{
	  gridID   = vlistInqVarGrid(vlistID1, varID);
	  gridsize = gridInqSize(gridID);
	  if ( gridsize > datasize ) datasize = gridsize;
	  if ( fname2 )
	    {
	      if ( DefaultDataType != CDI_UNDEFID )
		vlistDefVarDatatype(vlistID2, varID, DefaultDataType);
	    }
	}

      if ( fname2 )
	{
	  linfo = 0;
	  filetype = streamInqFiletype(streamID1);

	  if ( DefaultFileType != CDI_UNDEFID )
	    filetype = DefaultFileType;

	  streamID2 = streamOpenWrite(fname2, filetype);
	  if ( streamID2 < 0 )
	    return (handle_error(streamID2, "Open failed on %s", fname2));

	  streamDefVlist(streamID2, vlistID2);

	  if ( otableID == CDI_UNDEFID ) otableID = itableID;
	}

      data = (double *) malloc(datasize*sizeof(double));

      /*
	nts = cdiInqTimeSize(streamID1);
      */
      if ( Debug )
	printf("nts = %d\n", nts);

      if ( Debug )
	printf("streamID1 = %d, streamID2 = %d\n", streamID1, streamID2);

      tsID = 0;
      while ( (nrecs = streamInqTimestep(streamID1, tsID)) > 0 )
	{
	  if ( fname2 && ntsteps != 0 )
	    streamDefTimestep(streamID2, tsID);

	  vdate = taxisInqVdate(taxisID);
	  vtime = taxisInqVtime(taxisID);

	  if ( Debug )
	    fprintf(stdout, "tsID = %d nrecs = %d date = %d time = %d\n", tsID, nrecs, vdate, vtime);

	  if ( Record )
	    {
	      for ( recID = 0; recID < nrecs; recID++ )
		{
		  streamInqRecord(streamID1, &varID, &levelID);
		  streamReadRecord(streamID1, data, &nmiss);

		  gridID  = vlistInqVarGrid(vlistID1, varID);
		  zaxisID = vlistInqVarZaxis(vlistID1, varID);
		  code    = vlistInqVarCode(vlistID1, varID);
		  /*
		  printf("varID=%d, code=%d, gridID=%d, zaxisID=%d levelID=%d\n",
			 varID, code, gridID, zaxisID, levelID);
		  */
		  gridtype = gridInqType(gridID);
		  gridsize = gridInqSize(gridID);
		  level    = zaxisInqLevel(zaxisID, levelID);
		  missval  = vlistInqVarMissval(vlistID1, varID);
		  
		  if ( linfo )
		    printInfo(gridtype, vdate, vtime, code, level, gridsize, nmiss, missval, data);

		  if ( fname2 )
		    {
		      streamDefRecord(streamID2, varID, levelID);
		      if ( Move )
			streamCopyRecord(streamID2, streamID1);
		      else
			streamWriteRecord(streamID2, data, nmiss);
		    }
	      	}
	    }
	  else
	    {
	      for ( varID = 0; varID < nvars; varID++ )
		{
		  if ( vlistInqVarTime(vlistID1, varID) == TIME_CONSTANT && tsID > 0 ) continue;

		  gridID  = vlistInqVarGrid(vlistID1, varID);
		  zaxisID = vlistInqVarZaxis(vlistID1, varID);
		  code    = vlistInqVarCode(vlistID1, varID);
		  if ( Debug )
		    fprintf(stdout, "varID = %d code = %d gridID = %d zaxisID = %d\n",
			    varID, code, gridID, zaxisID);

		  gridtype = gridInqType(gridID);
		  gridsize = gridInqSize(gridID);
		  missval  = vlistInqVarMissval(vlistID1, varID);

		  levelsize = zaxisInqSize(zaxisID);
		  for ( levelID = 0; levelID < levelsize; levelID++ )
		    {
		      level = zaxisInqLevel(zaxisID, levelID);
		      streamReadVarSlice(streamID1, varID, levelID, data, &nmiss);

		      if ( linfo )
			printInfo(gridtype, vdate, vtime, code, level, gridsize, nmiss, missval, data);

		      if ( fname2 )
			streamWriteVarSlice(streamID2, varID, levelID, data, nmiss);
		    }
		}
	    }
	  tsID++;
        }

      free(data);

      if ( fname2 ) streamClose(streamID2);
      streamClose(streamID1);
    }

  if ( wTable )
    tableWrite(wTable, itableID);

  return (0);
}
