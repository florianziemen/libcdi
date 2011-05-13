#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include "dmemory.h"
#include "cdi.h"
#include "stream_int.h"
#include "resource_handle.h"
#include "pio_util.h"
#include "resource_handle.h"

#undef  UNDEFID
#define UNDEFID  -1

int ECMWF  = UNDEFID;
int MPIMET = UNDEFID;
int MCH    = UNDEFID;

typedef struct
{
  int    self;
  int    used;  
  int    center;  
  int    subcenter;
  char  *name;
  char  *longname;
}
institute_t;


static int    instituteCompareP ( void * instituteptr1, void * instituteptr2 );
static void   instituteDestroyP ( void * instituteptr );
static void   institutePrintP   ( void * instituteptr );
static size_t instituteGetSizeP ( void * instituteptr );
static void   institutePackP    ( void * instituteptr, void * position, void * buff );

resOps instituteOps = { instituteCompareP, instituteDestroyP, institutePrintP, \
			instituteGetSizeP, institutePackP };


static
void instituteDefaultValue ( institute_t * instituteptr )
{
  instituteptr->self       = UNDEFID;
  instituteptr->used       = 0;
  instituteptr->center     = UNDEFID;
  instituteptr->subcenter  = UNDEFID;
  instituteptr->name       = NULL;
  instituteptr->longname   = NULL;
}


static
institute_t * instituteNewEntry ( void )
{
  institute_t *instituteptr;

  instituteptr = ( institute_t * ) xmalloc ( sizeof ( institute_t ));
 
  instituteDefaultValue ( instituteptr );
  instituteptr->self = reshPut (( void * ) instituteptr, &instituteOps );
  instituteptr->used = 1;

  return  instituteptr;
}


static
void instituteDefaultEntries ( void )
{
  ECMWF   = institutDef( 98,   0, "ECMWF",     "European Centre for Medium-Range Weather Forecasts");
  MPIMET  = institutDef( 98, 232, "MPIMET",    "Max-Planck-Institute for Meteorology");
     (void) institutDef( 98, 255, "MPIMET",    "Max-Planck-Institute for Meteorology");
     (void) institutDef( 98, 232, "MPIMET",    "Max-Planck Institute for Meteorology");
     (void) institutDef( 78, 255, "DWD",       "Deutscher Wetterdienst");
  MCH     = institutDef(215, 255, "MCH",       "MeteoSwiss");
     (void) institutDef(  7,   0, "NCEP",      "National Centers for Environmental Prediction");
     (void) institutDef(  7,   1, "NCEP",      "National Centers for Environmental Prediction");
     (void) institutDef( 60,   0, "NCAR",      "National Center for Atmospheric Research");
     (void) institutDef( 74,   0, "METOFFICE", "U.K. Met Office");
     (void) institutDef( 97,   0, "ESA",       "European Space Agency ");
     (void) institutDef( 99,   0, "KNMI",      "Royal Netherlands Meteorological Institute");
     /*     (void) institutDef(  0,   0, "IPSL",      "IPSL (Institut Pierre Simon Laplace, Paris, France)"); */
}


static
void instituteInit (void)
{
  static int instituteInitialized = 0;

  if ( instituteInitialized ) return;

  instituteInitialized = 1; 

  instituteDefaultEntries();
}


int instituteCount ( void )
{ 
  return reshCountType ( &instituteOps );
}


int instituteCompareKernel ( institute_t *  ip1, institute_t * ip2 )
{
  int differ = 0;
  size_t len;

  if ( ip1->name )
    {
      if ( ip1->center    > 0 && ip2->center    != ip1->center )    differ = 1;
      if ( ip1->subcenter > 0 && ip2->subcenter != ip1->subcenter ) differ = 1;
      
      if ( !differ )
	{
	  if ( ip2->name )
	    {
	      len = strlen(ip2->name);
	      if ( memcmp(ip2->name, ip1->name, len)) differ = 1;
	    }
	}
    }
  else if ( ip1->longname )
    {
      if ( ip2->longname )
	{
	  len = strlen(ip2->longname);
	  if ( memcmp(ip2->longname, ip1->longname, len)) differ = 1;
	}
    }
  else
    {
      if ( !( ip2->center    == ip1->center &&
	      ip2->subcenter == ip1->subcenter )) differ = 1;
    }

  return differ;
}


int instituteCompareP ( void *  instituteptr1, void * instituteptr2 )
{
  institute_t * i1, * i2;

  i1 = ( institute_t * ) instituteptr1;
  i2 = ( institute_t * ) instituteptr2;

  assert ( i1 );
  assert ( i2 );

  return instituteCompareKernel ( i1, i2 );
}


int institutInq(int center, int subcenter, const char *name, const char *longname)
{
  int instID, instCount, * instIndice, i;
  institute_t * ip1;
  institute_t * ip2;

  instituteInit ();

  ip1 = xmalloc ( sizeof ( institute_t ));

  ip1->self       = UNDEFID;
  ip1->used       = 0;
  ip1->center     = center;
  ip1->subcenter  = subcenter;
  ip1->name       = ( char * ) name;
  ip1->longname   = ( char * ) longname;

  instCount = instituteCount ();
  instIndice = xmalloc ( instCount * sizeof ( int ));
  reshGetIndiceArrayOfType ( instCount, instIndice, &instituteOps );

  for( i = 0; i < instCount; i++ )
    {
      instID = instIndice[i];
      ip2 = ( institute_t * ) reshGetVal ( instID, &instituteOps );
      assert ( ip2 );
      
      if ( ip2->used && ! instituteCompareKernel ( ip1, ip2 ))
	{
	  instID = ip2->self;
	  break;
	}
    }

  if ( instID == instCount ) instID = UNDEFID;

  free ( instIndice );
  free ( ip1 );

  return  (instID);
}


int institutDef(int center, int subcenter, const char *name, const char *longname)
{
  institute_t * instituteptr;

  instituteInit ();

  instituteptr = instituteNewEntry();
  
  instituteptr->center    = center;
  instituteptr->subcenter = subcenter;
  if ( name )     instituteptr->name     = strdupx(name);
  if ( longname ) instituteptr->longname = strdupx(longname);

  return instituteptr->self;
}


int institutInqCenter(int instID)
{
  institute_t * instituteptr;

  instituteInit ();

  instituteptr = ( institute_t * ) reshGetVal ( instID, &instituteOps );

  return  instituteptr ? instituteptr->center : UNDEFID;
}


int institutInqSubcenter(int instID)
{
  institute_t * instituteptr;

  instituteInit ();

  instituteptr = ( institute_t * ) reshGetVal ( instID, &instituteOps );

  return instituteptr ? instituteptr->center: UNDEFID;
}


char *institutInqNamePtr(int instID)
{
  institute_t * instituteptr;

  instituteInit ();

  instituteptr = ( institute_t * ) reshGetVal ( instID, &instituteOps );

  return instituteptr ? instituteptr->name : NULL;
}


char *institutInqLongnamePtr(int instID)
{
  institute_t * instituteptr;

  instituteInit ();

  instituteptr = ( institute_t * ) reshGetVal ( instID, &instituteOps );

  return instituteptr ? instituteptr->longname : NULL;
}


int institutInqNumber(void)
{
  int i, instCount, * instIndice, instNum;
  institute_t * instituteptr;

  instituteInit ();

  instCount = reshCountType ( &instituteOps );
  instIndice = xmalloc ( instCount * sizeof ( int ));
  reshGetIndiceArrayOfType ( instCount, instIndice, &instituteOps );
  
  for ( i = 0; i < instCount; i++ )
    {
      instituteptr = ( institute_t * ) reshGetVal ( instIndice[i], &instituteOps );
      if ( instituteptr && instituteptr->used )
	instNum++;
    }

  if ( instIndice ) free ( instIndice );
    
  return instNum;
}


void instituteDestroyP ( void * instituteptr )
{
  int id;
  institute_t * i1 = ( institute_t * ) instituteptr;

  assert ( i1 );

  id = i1->self;

  if ( instituteptr ) free ( instituteptr );

  reshRemove ( id, &instituteOps );
}


void institutePrintP   ( void * instituteptr )
{
  FILE *fp = stdout;
  institute_t * ip = ( institute_t * ) instituteptr;

  if ( !ip ) return;

  fprintf ( fp, "#\n");
  fprintf ( fp, "# instituteID %d\n", ip->self);
  fprintf ( fp, "#\n"); 
  fprintf ( fp, "self          = %d\n", ip->self );
  fprintf ( fp, "used          = %d\n", ip->used );
  fprintf ( fp, "center        = %d\n", ip->center );
  fprintf ( fp, "subcenter     = %d\n", ip->subcenter );
  fprintf ( fp, "name          = %s\n", ip->name ? ip->name : "NN" );
  fprintf ( fp, "longname      = %s\n", ip->longname ? ip->longname : "NN" );
}



static 
size_t instituteGetSizeP ( void * instituteptr )
{
  xdebug ();
  return 0; 
}


static 
void institutePackP ( void * instituteptr, void * position, void * buff )
{
  xdebug ();
  return;
}
