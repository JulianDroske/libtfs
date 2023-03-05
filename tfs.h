/*
	replace all lines including stdio.h with this header
	or insert it into the first line of code
	or add "-include path/to/tfs.h" to compiler params to take effect
*/

#ifndef __TFS_H__
#define __TFS_H__

#include "stdio.h"
#include "stdint.h"

#define TFS_PATH_PREFIX '@'

#define TFS_MAGIC_T int
#define TFS_MAGIC ((TFS_MAGIC_T)0xf7f77f7f)

#define IS_TFS_FILE(stream) (*(TFS_MAGIC_T*)stream == TFS_MAGIC)

typedef struct {
	/* to be compatible with std */
	// FILE fp;
	TFS_MAGIC_T magic;
	FILE* base;
	size_t data_begin;
	size_t data_len;
	size_t now_pos;
	int _errno;
} TFS_FILE;

// void tfs_inittar(const char* buffer);
void tfs_inittarfile(const char* pathname);
void tfs_deinit();

/* generic */
FILE* tfs_fopen(const char* pathname, const char* mode);
size_t tfs_fread(void* ptr, size_t size, size_t nmemb, FILE* stream);
int tfs_fseek(FILE* stream, long offset, int whence);
long tfs_ftell(FILE* stream);
size_t tfs_fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream);
int tfs_fclose(FILE* stream);

/* error handling */
void tfs_clearerr(FILE* stream);
int tfs_ferror(FILE* stream);

#ifndef TFS_NO_OVERRIDE
	#define fopen(pathname, mode) tfs_fopen(pathname, mode)
	#define fread(ptr, size, nmemb, stream) tfs_fread(ptr, size, nmemb, stream)
	#define fseek(stream, offset, whence) tfs_fseek(stream, offset, whence)
	#define ftell(stream) tfs_ftell(stream)
	#define fwrite(ptr, size, nmemb, stream) tfs_fwrite(ptr, size, nmemb, stream)
	#define fclose(stream) tfs_fclose(stream)

	#define clearerr(stream) tfs_clearerr(stream)
	#define ferror(stream) tfs_ferror(stream)
#endif

#endif // __TFS_H__
