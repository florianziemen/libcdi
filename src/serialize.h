#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifndef SERIALIZE_H
#define SERIALIZE_H

/*
 * Generic interfaces for (de-)marshalling
 */
int serializeGetSize(int count, int datatype, void *context);
void serializePack(const void *data, int count, int datatype,
                   void *buf, int buf_size, int *position, void *context);
void serializeUnpack(const void *buf, int buf_size, int *position,
                     void *data, int count, int datatype, void *context);

/*
 * top-level de-marshalling function
 */

/*
 * Interfaces for marshalling within a single memory domain
 */
int serializeGetSizeInCore(int count, int datatype, void *context);
void serializePackInCore(const void *data, int count, int datatype,
                          void *buf, int buf_size, int *position, void *context);
void serializeUnpackInCore(const void *buf, int buf_size, int *position,
                           void *data, int count, int datatype, void *context);

#endif
/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
