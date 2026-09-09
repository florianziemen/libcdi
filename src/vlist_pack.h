#ifndef VLIST_PACK_H
#define VLIST_PACK_H

int vlistGetSizeP(void *vlistP, void *context);
void vlistPackP(void *vlistP, void *buff, int size, int *position, void *context);
int vlistTxCode(void *vlistP);
int vlistUnpack(char *buffer, int bufferSize, int *pos, int originNamespace, void *context, int force_id);

#endif
