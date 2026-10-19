# Compiler- und System-Checks für src/cdi_config.h.in
include(CheckCSourceCompiles)
include(CheckIncludeFile)
include(CheckStructHasMember)
include(CheckSymbolExists)

check_include_file(malloc.h HAVE_MALLOC_H)
check_include_file(sys/time.h HAVE_SYS_TIME_H)

check_symbol_exists(mallinfo "malloc.h" HAVE_MALLINFO)
check_symbol_exists(mmap "sys/mman.h" HAVE_MMAP)
check_symbol_exists(isnan "math.h" HAVE_DECL_ISNAN)

check_symbol_exists(_SC_LARGE_PAGESIZE "unistd.h" HAVE_DECL__SC_LARGE_PAGESIZE)
check_symbol_exists(_SC_PAGE_SIZE "unistd.h" HAVE_DECL__SC_PAGE_SIZE)
check_symbol_exists(_SC_PAGESIZE "unistd.h" HAVE_DECL__SC_PAGESIZE)
check_symbol_exists(PAGE_SIZE "unistd.h;sys/param.h" HAVE_DECL_PAGE_SIZE)
check_symbol_exists(PAGESIZE "unistd.h;sys/param.h" HAVE_DECL_PAGESIZE)

check_struct_has_member("struct stat" st_blksize "sys/stat.h" HAVE_STRUCT_STAT_ST_BLKSIZE)

check_c_source_compiles("int main(void) { _Float16 x = 1; return (int) x; }" HAVE__FLOAT16)
