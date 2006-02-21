#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include "dmemory.h"
#include "cdi.h"
#include "stream_int.h"

#undef  UNDEFID
#define UNDEFID -1

int ECHAM4 = UNDEFID;
int ECHAM5 = UNDEFID;

typedef struct
{
  int      used;  
  int      instID;  
  int      modelgribID;
  char    *name;
}
Model;

Model *models;

static int modelsSize = 0;

static int ModelsInit = 0;

void modelsInit(void);

int modelDef(int instID, int modelgribID, const char *name);

void modelsInitEntry(int modelID)
{
  static char func[] = "modelsInitEntry";

  if ( modelID < 0 || modelID >= modelsSize )
    Error(func, "modelID %d undefined!", modelID);

  models[modelID].used        = 0;
  models[modelID].instID      = UNDEFID;
  models[modelID].modelgribID = UNDEFID;
  models[modelID].name        = NULL;
}

int modelsNewEntry(void)
{
  static char func[] = "modelsNewEntry";
  int modelID = 0;

  /*
    Look for a free slot in models.
    (Create the table the first time through).
  */
  if ( !modelsSize )
    {
      int i;

      modelsSize = 2;
      models = (Model *) malloc(modelsSize*sizeof(Model));
      if( models == NULL )
	{
          Message(func, "modelsSize = %d", modelsSize);
	  SysError(func, "Allocation of Model failed");
	}

      for( i = 0; i < modelsSize; i++ )
	modelsInitEntry(i);
    }
  else
    {
      while( modelID < modelsSize )
	{
	  if ( models[modelID].used == 0 ) break;
	  modelID++;
	}
    }
  /*
    If the table overflows, double its size.
  */
  if ( modelID == modelsSize )
    {
      int i;

      modelsSize = 2*modelsSize;
      models = (Model *) realloc(models, modelsSize*sizeof(Model));
      if( models == NULL )
	{
          Message(func, "modelsSize = %d", modelsSize);
	  SysError(func, "Reallocation of Model failed");
	}

      for( i = modelID; i < modelsSize; i++ )
	modelsInitEntry(i);
    }

  models[modelID].used = 1;

  return (modelID);
}

void modelsDefault(void)
{
  int instID;

  instID  = institutInq(  0,   0, "ECMWF", NULL);
  (void)    modelDef(instID, 131, "ERA15");
  (void)    modelDef(instID, 199, "ERA40");

  instID  = institutInq(  0,   0, "MPIMET", NULL);
  ECHAM5  = modelDef(instID,  63, "ECHAM5.3");
  (void)    modelDef(instID,  62, "ECHAM5.2");
  (void)    modelDef(instID,  61, "ECHAM5.1");

  instID  = institutInq( 98, 255, "MPIMET", NULL);
  (void)    modelDef(instID,  60, "ECHAM5.0");
  ECHAM4  = modelDef(instID,  50, "ECHAM4");
  (void)    modelDef(instID, 110, "MPIOM1");

  instID  = institutInq(  0,   0, "DWD", NULL);
  (void)    modelDef(instID, 149, "GME");

  instID  = institutInq(  0,   1, "NCEP", NULL);
  (void)    modelDef(instID,  80, "T62L28MRF");
}

void modelsInit(void)
{
  ModelsInit = 1;

  modelsDefault();
}

int modelInq(int instID, int modelgribID, char *name)
{
  int modelID = UNDEFID;
  size_t len;
  int found;

  if ( ! ModelsInit ) modelsInit();

  for( modelID = 0; modelID < modelsSize; modelID++ )
    {
      if ( models[modelID].used )
	{
	  if ( name )
	    {
	      found = 1;
	      if ( instID      != -1 && models[modelID].instID      != instID )      found = 0;
	      if ( modelgribID !=  0 && models[modelID].modelgribID != modelgribID ) found = 0;

	      if ( found )
		{
		  if ( models[modelID].name )
		    {
		      len = strlen(models[modelID].name);
		      if ( strncmp(models[modelID].name, name, len) == 0 ) break;
		      len = strlen(name);
		      if ( strncmp(models[modelID].name, name, len) == 0 ) break;
		    }
		}
	    }
	  else
	    {
	      if ( models[modelID].instID      == instID &&
		   models[modelID].modelgribID == modelgribID ) break;
	    }
	}
    }

  if ( modelID == modelsSize ) modelID = UNDEFID;

  return (modelID);
}

int modelDef(int instID, int modelgribID, const char *name)
{
  static char func[] = "modelDef";
  int modelID = UNDEFID;

  if ( ! ModelsInit ) modelsInit();
  /*
  modelID = modelInq(instID, modelgribID, name);
  */
  if ( modelID == UNDEFID )
    {
      modelID = modelsNewEntry();

      models[modelID].instID      = instID;
      models[modelID].modelgribID = modelgribID;

      if ( name ) models[modelID].name = strdupx(name);
    }

  return (modelID);
}

void modelCheckID(char *func, int modelID)
{
  if ( modelID < 0 || modelID >= modelsSize )
    Error(func, "modelID %d undefined!", modelID);

  if ( ! models[modelID].used )
    Error(func, "modelID %d undefined!", modelID);
}

int modelInqInstitut(int modelID)
{
  static char func[] = "modelInqInstitut";
  int instID = UNDEFID;

  if ( ! ModelsInit ) modelsInit();

  if ( modelID != UNDEFID )
    {
      modelCheckID(func, modelID);
  
      instID = models[modelID].instID;
    }

  return (instID);
}

int modelInqGribID(int modelID)
{
  static char func[] = "modelInqGribID";
  int modelgribID = UNDEFID;

  if ( ! ModelsInit ) modelsInit();

  if ( modelID != UNDEFID )
    {
      modelCheckID(func, modelID);

      modelgribID = models[modelID].modelgribID;
    }

  return (modelgribID);
}

char *modelInqNamePtr(int modelID)
{
  static char func[] = "modelInqNamePtr";
  char *name = NULL;

  if ( ! ModelsInit ) modelsInit();

  if ( modelID != UNDEFID )
    {
      modelCheckID(func, modelID);

      if ( models[modelID].name )
	name = models[modelID].name;
    }

  return (name);
}
