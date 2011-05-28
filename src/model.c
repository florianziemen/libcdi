#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include "dmemory.h"
#include "cdi.h"
#include "stream_int.h"
#include "pio_util.h"
#include "resource_handle.h"
#include "pio_rpc.h"
#include "namespace.h"

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
static int * modelInitializedNsp;

static void modelInit(void);


static int    modelCompareP ( void * modelptr1, void * modelptr2 );
static void   modelDestroyP ( void * modelptr );
static void   modelPrintP   ( void * modelptr, FILE * fp );
#ifdef USE_MPI
static int    modelGetSizeP ( void * modelptr, MPI_Comm comm );
static void   modelPackP    ( void * modelptr, void * buff, int size,
                              int *position, MPI_Comm comm);
static int    modelTxCode   ( void );
#endif

resOps modelOps = { modelCompareP, modelDestroyP, modelPrintP
#ifdef USE_MPI
                    , modelGetSizeP, modelPackP, modelTxCode
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
  int instID, i;
  cdiResH resH[10];

  instID  = institutInq(  0,   0, "ECMWF", NULL);
  /* (void)    modelDef(instID, 131, "ERA15"); */
  /* (void)    modelDef(instID, 199, "ERA40"); */
  instID  = institutInq(  0,   0, "MPIMET", NULL);

  resH[0] = ECHAM5  = modelDef(instID,  64, "ECHAM5.4");
  resH[1] = modelDef(instID,  63, "ECHAM5.3");
  resH[2] = modelDef(instID,  62, "ECHAM5.2");
  resH[3] = modelDef(instID,  61, "ECHAM5.1");

  instID  = institutInq( 98, 255, "MPIMET", NULL);
  resH[4] = modelDef(instID,  60, "ECHAM5.0");
  resH[5] = ECHAM4  = modelDef(instID,  50, "ECHAM4");
  resH[6] = modelDef(instID, 110, "MPIOM1");

  instID  = institutInq(  0,   0, "DWD", NULL);
  resH[7] = modelDef(instID, 149, "GME");

  instID  = institutInq(  0,   0, "MCH", NULL);
  //(void)  = modelDef(instID, 137, "COSMO");
  resH[8] = COSMO   = modelDef(instID, 255, "COSMO");

  instID  = institutInq(  0,   1, "NCEP", NULL);
  resH[9] = modelDef(instID,  80, "T62L28MRF");
  
  if ( ! namespaceHasLocalFile ( namespaceGetActive ()))
    for ( i = 0; i < 10 ; i++ )
      reshSetStatus ( resH[i], &modelOps, SUSPENDED );  
}

static
void modelFinalize ( void )
{
  free (   modelInitializedNsp );
}

static
void modelInit(void)
{
  static int modelInitialized = 0;
  int nsp, nspc;
  char *env;

  nspc = namespaceGetNumber ();
  
  if ( !modelInitialized )
    {
      modelInitialized = 1;
      modelInitializedNsp = xcalloc ( 1, nspc * sizeof ( int ));
      atexit ( modelFinalize );
      env = getenv("MODEL_DEBUG");
      if ( env ) MODEL_Debug = atoi(env);  
    }

  nsp = namespaceGetActive ();
  
  if ( modelInitializedNsp[nsp] ) return;
  
  modelInitializedNsp[nsp] = 1;
  
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
  int i, * modelResHs;

  modelInit ();

  modelCount = modelSize();
  modelResHs = xmalloc ( modelCount * sizeof ( int ));
  reshGetResHArrayOfType ( modelCount, modelResHs, &modelOps );

  for( i = 0; i < modelCount; i++ )
    {
      modelID = modelResHs[i];
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

  if ( modelResHs ) free ( modelResHs );

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


void modelPrintP   ( void * modelptr, FILE * fp )
{
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

static int
modelTxCode ( void )
{
  return MODEL;
}

enum {
  model_nints = 4,
};


static
int modelGetSizeP ( void * modelptr, MPI_Comm comm )
{
  model_t *p = modelptr;
  int txsize = 0, txinc;
  xmpi(MPI_Pack_size(model_nints, MPI_INT, comm, &txsize));
  xmpi(MPI_Pack_size(strlen(p->name) + 1, MPI_CHAR, comm, &txinc));
  txsize += txinc;
  return txsize;
}


static
void modelPackP ( void * modelptr, void * buf, int size,
                  int *position, MPI_Comm comm )
{
  model_t *p = modelptr;
  int tempbuf[model_nints];
  tempbuf[0] = p->self;
  tempbuf[1] = p->instID;
  tempbuf[2] = p->modelgribID;
  tempbuf[3] = (int)strlen(p->name) + 1;
  xmpi(MPI_Pack(tempbuf, model_nints, MPI_INT, buf, size, position, comm));
  xmpi(MPI_Pack(p->name, tempbuf[3], MPI_CHAR, buf, size, position, comm));
}

int
modelUnpack(void *buf, int size, int *position, int nspTarget,
            MPI_Comm comm)
{
  int tempbuf[model_nints];
  int modelID;
  char *name;
  xmpi(MPI_Unpack(buf, size, position, tempbuf, model_nints, MPI_INT, comm));
  name = xmalloc(tempbuf[3]);
  xmpi(MPI_Unpack(buf, size, position, name, tempbuf[3], MPI_CHAR, comm));
  modelID = modelDef( namespaceAdaptKey ( tempbuf[1], nspTarget ), tempbuf[2], name);
  // FIXME: this should work, once all types are transferred
  //assert(modelID == tempbuf[0]);
  return modelID;
}

#endif
