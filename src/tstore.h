#ifndef TSTORE_H
#define TSTORE_H

// clang-format off

#ifdef __cplusplus
extern "C" {
#endif

const char *ts_lib_version(void);

int ts_open(const char *fileName, const char *mode, int fileType);

#ifdef __cplusplus
}
#endif

// clang-format on

#endif
