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
#define UNDEFID -1

int ECHAM4 = UNDEFID;
int ECHAM5 = UNDEFID;
int COSMO  = UNDEFID;

typedef struct
{
  int      self;
  int      used;
  int      instID;
  int      modelgribID;
  char    *name;
}
model_t;


static int  MODEL_Debug = 0;   /* If set to 1, debugging */

static void modelInit(void);


static int    modelCompareP ( void * modelptr1, void * modelptr2 );
static void   modelDestroyP ( void * modelptr );
static void   modelPrintP   ( void * modelptr );
#ifdef USE_MPI
static int    modelGetSizeP ( void * modelptr );
static void   modelPackP    ( void * modelptr, void * buff, int size,
                              int *position, MPI_Comm comm);
#endif

resOps modelOps = { modelCompareP, modelDestroyP, modelPrintP
#ifdef USE_MPI
                    , modelGetSizeP, modelPackP
#endif
};

static
void modelDefaultValue ( model_t *modelptr )
{
  modelptr->self        = UNDEFID;
  modelptr->used        = 0;
  modelptr->instID      = UNDEFID;
  modelptr->modelgribID = UNDEFID;
  modelptr->name        = NULL;
}

static
model_t *modelNewEntry ( void )
{
  model_t *modelptr;

  modelptr = (model_t *) xmalloc(sizeof(model_t));
  modelDefaultValue ( modelptr );
  modelptr->self = reshPut (( void * ) modelptr, &modelOps );
  modelptr->used        = 1;

  return (modelptr);
}


int modelDef(int instID, int modelgribID, const char *name);

static
void modelDefaultEntries ( void )
{
  int instID;

  instID  = institutInq(  0,   0, "ECMWF", NULL);
  /* (void)    modelDef(instID, 131, "ERA15"); */
  /* (void)    modelDef(instID, 199, "ERA40"); */

  instID  = institutInq(  0,   0, "MPIMET", NULL);
  ECHAM5  = modelDef(instID,  64, "ECHAM5.4");
  (void)    modelDef(instID,  63, "ECHAM5.3");
  (void)    modelDef(instID,  62, "ECHAM5.2");
  (void)    modelDef(instID,  61, "ECHAM5.1");

  instID  = institutInq( 98, 255, "MPIMET", NULL);
  (void)    modelDef(instID,  60, "ECHAM5.0");
  ECHAM4  = modelDef(instID,  50, "ECHAM4");
  (void)    modelDef(instID, 110, "MPIOM1");

  instID  = institutInq(  0,   0, "DWD", NULL);
  (void)    modelDef(instID, 149, "GME");

  instID  = institutInq(  0,   0, "MCH", NULL);
  //(void)  = modelDef(instID, 137, "COSMO");
  COSMO   = modelDef(instID, 255, "COSMO");

  instID  = institutInq(  0,   1, "NCEP", NULL);
  (void)    modelDef(instID,  80, "T62L28MRF");
}

static
void modelInit(void)
{
  static int modelInitialized = 0;
  char *env;

  if ( modelInitialized ) return;

  modelInitialized = 1;

  env = getenv("MODEL_DEBUG");
  if ( env ) MODEL_Debug = atoi(env);

  modelDefaultEntries ();
}

int modelSize ( void )
{
  return reshCountType ( &modelOps );
}


int modelInq(int instID, int modelgribID, char *name)
{
  int modelID = UNDEFID;
  size_t len;
  int found;
  int modelCount;
  model_t *modelptr;
  int i, * modelIndice;

  modelInit ();

  modelCount = modelSize();
  modelIndice = xmalloc ( modelCount * sizeof ( int ));
  reshGetIndiceArrayOfType ( modelCount, modelIndice, &modelOps );

  for( i = 0; i < modelCount; i++ )
    {
      modelID = modelIndice[i];
      modelptr = ( model_t * ) reshGetVal ( modelID, &modelOps );

      if ( modelptr->used )
        {
          if ( name )
            {
              found = 1;
              if ( instID      != -1 && modelptr->instID      != instID )      found = 0;
              if ( modelgribID !=  0 && modelptr->modelgribID != modelgribID ) found = 0;

              if ( found )
                {
                  if ( modelptr->name )
                    {
                      len = strlen(modelptr->name);
                      if ( memcmp(modelptr->name, name, len) == 0 ) break;
                      len = strlen(name);
                      if ( memcmp(modelptr->name, name, len) == 0 ) break;
                    }
                }
            }
          else
            {
              if ( modelptr->instID      == instID &&
                   modelptr->modelgribID == modelgribID ) break;
            }
        }
    }

  if ( modelID == modelCount ) modelID = UNDEFID;

  if ( modelIndice ) free ( modelIndice );

  return (modelID);
}


int modelDef(int instID, int modelgribID, const char *name)
{
  model_t *modelptr;

  modelInit ();

  modelptr = modelNewEntry();

  modelptr->instID      = instID;
  modelptr->modelgribID = modelgribID;
  if ( name ) modelptr->name = strdupx(name);

  return modelptr->self;
}


int modelInqInstitut(int modelID)
{
  model_t *modelptr;

  modelInit ();

  modelptr = ( model_t * ) reshGetVal ( modelID, &modelOps );

  return modelptr ? modelptr->instID : UNDEFID;
}


int modelInqGribID(int modelID)
{
  model_t *modelptr;

  modelInit ();

  modelptr = ( model_t * ) reshGetVal ( modelID, &modelOps );

  return modelptr ? modelptr->modelgribID : UNDEFID;
}


char *modelInqNamePtr(int modelID)
{
  model_t *modelptr;

  modelInit ();

  modelptr = ( model_t * ) reshGetVal ( modelID, &modelOps );

  return modelptr ? modelptr->name : NULL;
}


int  modelCompareP ( void * modelptr1, void * modelptr2 )
{
  return 0;
}


void modelDestroyP ( void * modelptr )
{
}


void modelPrintP   ( void * modelptr )
{
  FILE *fp = stdout;
  model_t * mp = ( model_t * ) modelptr;

  if ( !mp ) return;

  fprintf ( fp, "#\n");
  fprintf ( fp, "# modelID %d\n", mp->self);
  fprintf ( fp, "#\n");
  fprintf ( fp, "self          = %d\n", mp->self );
  fprintf ( fp, "used          = %d\n", mp->used );
  fprintf ( fp, "instID        = %d\n", mp->instID );
  fprintf ( fp, "modelgribID   = %d\n", mp->modelgribID );
  fprintf ( fp, "name          = %s\n", mp->name ? mp->name : "NN" );
}


#ifdef USE_MPI
static
int modelGetSizeP ( void * modelptr )
{
  xdebug ();
  return 0;
}


static
void modelPackP ( void * modelptr, void * buff, int size,
                  int *position, MPI_Comm comm )
{
  xdebug ();
  return;
}

#endif
