#define TFS_NO_OVERRIDE
#include "tfs.h"

#define CTAR_IMPLEMENTATION
#include "ctar.h"

#include "errno.h"
#include "string.h"
#include "stdlib.h"

struct ctar_t* tfs_rootentry = NULL;
FILE* tfs_currfile = NULL;

#define TFS_SETERRNO(no) errno = no
#define TFS_STREAM_SETERRNO(no) stream->_errno = TFS_SETERRNO(no)

/* returns the next name ptr */
char* tfs_namepath(char* pathname){
	if(!pathname || *pathname == '\0') return NULL;
	char* ptr = pathname;
	while(*ptr != '/' && *ptr != '\0') ++ptr;
	if(*ptr == '\0') return NULL;
	*ptr = '\0';
	return ptr + 1;
}

struct ctar_t* tfs_query_path(struct ctar_t* archive, const char* pathname){
	if(!archive || !pathname || *pathname != '/') return NULL;
	++pathname;
	char* const path = strdup(pathname);
	char* name = path;
	for(char* name_next = tfs_namepath(name);
		name_next;
		name = name_next, name_next = tfs_namepath(name_next)){

		archive = ctar_exists(archive, name, 0);
		if(!archive) break;
	}
	free(path);
	return archive;
}

// void tfs_inittar(const char* buffer){
	// 
// }

void tfs_inittarfile(const char* pathname){
	FILE* fp = fopen(pathname, "rb");
	// TODO error handling
	if(!fp) return;
	if(!ctar_istarfile(fp)){
		fclose(fp);
		return;
	}
	ctar_read(fp, &tfs_rootentry, 0);
	tfs_currfile = fp;
}

void tfs_deinit(){
	if(tfs_currfile){
		ctar_free(tfs_rootentry);
		tfs_currfile = NULL;
	}
}


/* generic */

FILE* tfs_fopen(const char* pathname, const char* mode){
	if(!pathname || *pathname == '\0') return NULL;
	if(*pathname == TFS_PATH_PREFIX){
		if(pathname[1] != '/'){
			TFS_SETERRNO(ENOENT);
			return NULL;
		}
		// tfs
		TFS_FILE* tfp = (TFS_FILE*) calloc(sizeof(TFS_FILE), 1);
		if(!tfp) return NULL;
		if(!tfs_rootentry){
			TFS_SETERRNO(ENOMEM);
			free(tfp);
			return NULL;
		}
		// struct ctar_t* entry = tfs_query_path(tfs_rootentry, pathname + 1);
		struct ctar_t* entry = ctar_exists(tfs_rootentry, pathname + 2, 0);
		if(!entry){
			TFS_SETERRNO(ENOENT);
			free(tfp);
			return NULL;
		};
		if((entry->type == REGULAR) || (entry->type == NORMAL) || (entry->type == CONTIGUOUS)){
			tfp->magic = TFS_MAGIC;
			tfp->base = tfs_currfile;
			tfp->data_begin = entry->begin + 512;
			tfp->data_len = ctar_getsize(entry);
			return (FILE*) tfp;
		}
		free(tfp);
		return NULL;
	}else return fopen(pathname, mode);
}

size_t tfs_fread(void* ptr, size_t size, size_t nmemb, FILE* _stream){
	if(IS_TFS_FILE(_stream)){
		// tfs
		if(!ptr){
			TFS_SETERRNO(ENOSR);
			return 0;
		}
		TFS_FILE* stream = (TFS_FILE*) _stream;
		size_t off = 0;
		// TODO optimize
		int stat = fseek(stream->base, stream->data_begin + stream->now_pos, SEEK_SET);
		if(stat != 0) return 0;
		int count = 0;
		while(stream->now_pos < stream->data_len){
			size_t remain_size = stream->data_len - stream->now_pos;
			size_t size_to_read = size <= remain_size? size: remain_size;
			size_t got = fread(ptr + off, 1, size_to_read, stream->base);
			if(got<=0) break;
			++count;
			off += got;
			stream->now_pos += got;
		}
		return count;
	}
	else return fread(ptr, size, nmemb, _stream);
}

int tfs_fseek(FILE* _stream, long offset, int whence){
	if(IS_TFS_FILE(_stream)){
		TFS_FILE* stream = (TFS_FILE*) _stream;
		switch(whence){
			default: TFS_SETERRNO(ESPIPE); return -1;
			case SEEK_SET:
				if(offset < 0 || offset > (long) stream->data_len){
					TFS_STREAM_SETERRNO(ESPIPE);
					return -1;
				}
				break;
			case SEEK_CUR:
				offset += stream->now_pos;
				if(offset < 0 || offset > (long) stream->data_len){
					TFS_STREAM_SETERRNO(ESPIPE);
					return -1;
				}
				break;
			case SEEK_END:
				if(-offset > (long) stream->data_len){
					TFS_STREAM_SETERRNO(ESPIPE);
					return -1;
				}
				offset += stream->data_len;
				offset %= (stream->data_len + 1);
				break;
		}
		stream->now_pos = offset;
		return 0;
	}else return fseek(_stream, offset, whence);
}

long tfs_ftell(FILE* _stream){
	if(IS_TFS_FILE(_stream)){
		TFS_FILE* stream = (TFS_FILE*) _stream;
		return stream->now_pos;
	}else return ftell(_stream);
}

size_t tfs_fwrite(const void* ptr, size_t size, size_t nmemb, FILE* _stream){
	if(IS_TFS_FILE(_stream)){
		TFS_FILE* stream = (TFS_FILE*) _stream;
		TFS_STREAM_SETERRNO(EROFS);
		return -1;
	}
	else return fwrite(ptr, size, nmemb, _stream);
}

int tfs_fclose(FILE* _stream){
	if(IS_TFS_FILE(_stream)){
		// tfs
		TFS_FILE* stream = (TFS_FILE*) _stream;
		free(stream);
		return 0;
	}else{
		return fclose(_stream);
	}
}

/* error handling */

void tfs_clearerr(FILE* _stream){
	if(IS_TFS_FILE(_stream)){
		TFS_FILE* stream = (TFS_FILE*) _stream;
		stream->_errno = 0;
	}else clearerr(_stream);
}

int tfs_ferror(FILE* _stream){
	if(IS_TFS_FILE(_stream)){
		TFS_FILE* stream = (TFS_FILE*) _stream;
		return stream->_errno;
	}else return ferror(_stream);
}
