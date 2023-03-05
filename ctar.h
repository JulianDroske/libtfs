/*
	This file is based on https://github.com/calccrypto/tar

	A universal simple tar reader library
*/

/*
tar.h
tar data structure and accompanying functions

Copyright (c) 2015 Jason Lee
Copyright (c) 2023 Julian Droske

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE
*/

#ifndef __CTAR_H__
#define __CTAR_H__

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include "errno.h"
#include "stdarg.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "time.h"

#define BLOCKSIZE       512
#define BLOCKING_FACTOR 20
#define RECORDSIZE      10240

// file type values (1 octet)
#define REGULAR          0
#define NORMAL          '0'
#define HARDLINK        '1'
#define SYMLINK         '2'
#define CHAR            '3'
#define BLOCK           '4'
#define DIRECTORY       '5'
#define FIFO            '6'
#define CONTIGUOUS      '7'

// tar entry metadata structure (singly-linked list)
struct ctar_t {
    char original_name[100];                // original filenme; only availible when writing into a tar
    unsigned int begin;                     // location of data in file (including metadata)
    union {
        union {
            // Pre-POSIX.1-1988 format
            struct {
                char name[100];             // file name
                char mode[8];               // permissions
                char uid[8];                // user id (octal)
                char gid[8];                // group id (octal)
                char size[12];              // size (octal)
                char mtime[12];             // modification time (octal)
                char check[8];              // sum of unsigned characters in block, with spaces in the check field while calculation is done (octal)
                char link;                  // link indicator
                char link_name[100];        // name of linked file
            };

            // UStar format (POSIX IEEE P1003.1)
            struct {
                char old[156];              // first 156 octets of Pre-POSIX.1-1988 format
                char type;                  // file type
                char also_link_name[100];   // name of linked file
                char ustar[8];              // ustar\000
                char owner[32];             // user name (string)
                char group[32];             // group name (string)
                char major[8];              // device major number
                char minor[8];              // device minor number
                char prefix[155];
            };
        };

        char block[512];                    // raw memory (500 octets of actual data, padded to 1 block)
    };

    struct ctar_t * next;
};

// core functions //////////////////////////////////////////////////////////////
// read a tar file
// archive should be address to null pointer
int ctar_read(FILE* fp, struct ctar_t ** archive, const char verbosity);

// determine if a file is a tar file
int ctar_istarfile(FILE* fp);

// determine if a filepath file is a tar file
int ctar_istarfilepath(const char* pathname);

// recursive freeing of entries
void ctar_free(struct ctar_t * archive);
// /////////////////////////////////////////////////////////////////////////////

// utilities ///////////////////////////////////////////////////////////////////
// print contents of archive
// verbosity should be greater than 0
int ctar_ls(FILE * f, struct ctar_t * archive, const size_t filecount, const char * files[], const char verbosity);

// /////////////////////////////////////////////////////////////////////////////

// internal functions; generally don't call from outside ///////////////////////
// print raw data with definitions (meant for debugging)
int print_entry_metadata(FILE * f, struct ctar_t * entry);

// print metadata of entire tar file
int print_ctar_metadata(FILE * f, struct ctar_t * archive);

// check if file with original name/modified name exists
struct ctar_t * ctar_exists(struct ctar_t * archive, const char * filename, const char ori);

// print single entry
// verbosity should be greater than 0
int ls_ctar_entry(FILE * f, struct ctar_t * archive, const size_t filecount, const char * files[], const char verbosity);

// convert octal string to unsigned integer
unsigned int ctar_oct2uint(char * oct, unsigned int size);

#define ctar_getsize(archive) (ctar_oct2uint(archive? archive->size: NULL, 11))

// /////////////////////////////////////////////////////////////////////////////

#ifdef CTAR_IMPLEMENTATION

#ifndef S_IRUSR
# define CTAR_CUSTOM_CHMOD
# define S_IRUSR 0x100
# define S_IWUSR 0x80
# define S_IXUSR 0x40
# define S_IRGRP 0x20
# define S_IWGRP 0x10
# define S_IXGRP 0x8
# define S_IROTH 0x4
# define S_IWOTH 0x2
# define S_IXOTH 0x1
#endif

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
// only print in verbose mode
#define V_PRINT(f, fmt, ...) if (verbosity) { fprintf(f, fmt "\n", ##__VA_ARGS__); }
// generic error
#define ERROR(fmt, ...) fprintf(stderr, "Error: " fmt "\n", ##__VA_ARGS__); return -1;
// capture errno when erroring
#define RC_ERROR(fmt, ...) const int rc = errno; ERROR(fmt, ##__VA_ARGS__); return -1;
#define WRITE_ERROR(fmt, ...) { ERROR(fmt, ##__VA_ARGS__); ctar_free(*archive); *archive = NULL; return -1; }
#define EXIST_ERROR(fmt, ...) const int rc = errno; if (rc != EEXIST) { ERROR(fmt, ##__VA_ARGS__); return -1; }

// force read() to complete
static int ctar_read_size(FILE* fp, char * buf, int size);

// check if a buffer is zeroed
static int ctar_iszeroed(char * buf, size_t size);

// make directory recursively
static int recursive_mkdir(const char * dir, const unsigned int mode, const char verbosity);

int ctar_read(FILE* fp, struct ctar_t ** archive, const char verbosity){
    if (!fp){
        ERROR("Bad FILE pointer");
    }

    if (!archive || *archive){
        ERROR("Bad archive");
    }

    unsigned int offset = 0;
    int count = 0;

    struct ctar_t ** tar = archive;
    char update = 1;

    for(count = 0; ; count++){
        *tar = calloc(1, sizeof(struct ctar_t));
        if (update && (ctar_read_size(fp, (*tar) -> block, 512) != 512)){
            V_PRINT(stderr, "Error: Bad read. Stopping");
            ctar_free(*tar);
            *tar = NULL;
            break;
        }

        update = 1;
        // if current block is all zeros
        if (ctar_iszeroed((*tar) -> block, 512)){
            if (ctar_read_size(fp, (*tar) -> block, 512) != 512){
                V_PRINT(stderr, "Error: Bad read. Stopping");
                ctar_free(*tar);
                *tar = NULL;
                break;
            }

            // check if next block is all zeros as well
            if (ctar_iszeroed((*tar) -> block, 512)){
                ctar_free(*tar);
                *tar = NULL;

                // skip to end of record
                if (fseek(fp, RECORDSIZE - (offset % RECORDSIZE), SEEK_CUR) == (off_t) (-1)){
                    RC_ERROR("Unable to seek file: %s", strerror(rc));
                }

                break;
            }

            update = 0;
        }

        // set current entry's file offset
        (*tar) -> begin = offset;

        // skip over data and unfilled block
        unsigned int jump = ctar_oct2uint((*tar) -> size, 11);
        if (jump % 512){
            jump += 512 - (jump % 512);
        }

        // move file descriptor
        offset += 512 + jump;
        if (fseek(fp, jump, SEEK_CUR) == (off_t) (-1)){
            RC_ERROR("Unable to seek file: %s", strerror(rc));
        }

        // ready next value
        tar = &((*tar) -> next);
    }

    return count;
}

int ctar_istarfile(FILE* fp){
	if(!fp) return 0;
	size_t prev_pos = ftell(fp);
	if(prev_pos < 0) return 0;
	int ok = fseek(fp, -1024, SEEK_END);
	if(ok != 0) return 0;
	static char buf[1024] = {};
	ok = fread(buf, 1024, 1, fp);
	fseek(fp, prev_pos, SEEK_SET);
	if(ok != 1){
		return 0;
	}
	return ctar_iszeroed(buf, 1024);
}

int ctar_istarfilepath(const char* pathname){
	if(!pathname || *pathname == '\0') return 0;
	FILE* fp = fopen(pathname, "rb");
	if(!fp) return 0;
	int res = ctar_istarfile(fp);
	fclose(fp);
	return res;
}

void ctar_free(struct ctar_t * archive){
    while (archive){
        struct ctar_t * next = archive -> next;
        free(archive);
        archive = next;
    }
}

int ctar_ls(FILE * f, struct ctar_t * archive, const size_t filecount, const char * files[], const char verbosity){
    if (!verbosity){
        return 0;
    }

    if (filecount && !files){
        ERROR("Non-zero file count provided, but file list is NULL");
    }

    while (archive){
        if (ls_ctar_entry(f, archive, filecount, files, verbosity) < 0){
            return -1;
        }
        archive = archive -> next;
    }

    return 0;
}

int print_entry_metadata(FILE * f, struct ctar_t * entry){
    if (!entry){
        return -1;
    }

    time_t mtime = ctar_oct2uint(entry -> mtime, 12);
    char mtime_str[32];
    strftime(mtime_str, sizeof(mtime_str), "%c", localtime(&mtime));
    fprintf(f, "File Name: %s\n", entry -> name);
    fprintf(f, "File Mode: %s (%03o)\n", entry -> mode, ctar_oct2uint(entry -> mode, 8));
    fprintf(f, "Owner UID: %s (%d)\n", entry -> uid, ctar_oct2uint(entry -> uid, 12));
    fprintf(f, "Owner GID: %s (%d)\n", entry -> gid, ctar_oct2uint(entry -> gid, 12));
    fprintf(f, "File Size: %s (%d)\n", entry -> size, ctar_oct2uint(entry -> size, 12));
    fprintf(f, "Time     : %s (%s)\n", entry -> mtime, mtime_str);
    fprintf(f, "Checksum : %s\n", entry -> check);
    fprintf(f, "File Type: ");
    switch (entry -> type){
        case REGULAR: case NORMAL:
            fprintf(f, "Normal File");
            break;
        case HARDLINK:
            fprintf(f, "Hard Link");
            break;
        case SYMLINK:
            fprintf(f, "Symbolic Link");
            break;
        case CHAR:
            fprintf(f, "Character Special");
            break;
        case BLOCK:
            fprintf(f, "Block Special");
            break;
        case DIRECTORY:
            fprintf(f, "Directory");
            break;
        case FIFO:
            fprintf(f, "FIFO");
            break;
        case CONTIGUOUS:
            fprintf(f, "Contiguous File");
            break;
    }
    fprintf(f, " (%c)\n", entry -> type?entry -> type:'0');
    fprintf(f, "Link Name: %s\n", entry -> link_name);
    fprintf(f, "Ustar\\000: %c%c%c%c%c\\%2x\\%2x\\%02x\n",   entry -> ustar[0], entry -> ustar[1], entry -> ustar[2], entry -> ustar[3], entry -> ustar[4], entry -> ustar[5], entry -> ustar[6], entry -> ustar[7]);
    fprintf(f, "Username : %s\n", entry -> owner);
    fprintf(f, "Group    : %s\n", entry -> group);
    fprintf(f, "Major    : %s\n", entry -> major);
    fprintf(f, "Minor    : %s\n", entry -> minor);
    fprintf(f, "Prefix   : %s\n", entry -> prefix);
    fprintf(f, "\n");

    return 0;
}

int print_ctar_metadata(FILE * f, struct ctar_t * archive){
    while (archive){
        print_entry_metadata(f, archive);
        archive = archive -> next;
    }
    return 0;
}

struct ctar_t * ctar_exists(struct ctar_t * archive, const char * filename, const char ori){
    while (archive){
        if (ori){
            if (!strncmp(archive -> original_name, filename, MAX(strlen(archive -> original_name), strlen(filename)) + 1)){
                return archive;
            }
        }
        else{
            if (!strncmp(archive -> name, filename, MAX(strlen(archive -> name), strlen(filename)) + 1)){
                return archive;
            }
        }
        archive = archive -> next;
    }
    return NULL;
}

int ls_ctar_entry(FILE * f, struct ctar_t * entry, const size_t filecount, const char * files[], const char verbosity){
    if (!verbosity){
        return 0;
    }

    if (filecount && !files){
        V_PRINT(stderr, "Error: Non-zero file count given but no files given");
        return -1;
    }

    // figure out whether or not to print
    // if no files were specified, print everything
    char print = !filecount;
    // otherwise, search for matching names
    for(size_t i = 0; i < filecount; i++){
        if (strncmp(entry -> name, files[i], MAX(strlen(entry -> name), strlen(files[i])))){
            print = 1;
            break;
        }
    }

    if (print){
        if (verbosity > 1){
            const mode_t mode = ctar_oct2uint(entry -> mode, 7);
            const char mode_str[26] = { ("-hlcbdp-")[entry -> type?entry -> type - '0':0],
                                        mode & S_IRUSR?'r':'-',
                                        mode & S_IWUSR?'w':'-',
                                        mode & S_IXUSR?'x':'-',
                                        mode & S_IRGRP?'r':'-',
                                        mode & S_IWGRP?'w':'-',
                                        mode & S_IXGRP?'x':'-',
                                        mode & S_IROTH?'r':'-',
                                        mode & S_IWOTH?'w':'-',
                                        mode & S_IXOTH?'x':'-',
                                        0};
            fprintf(f, "%s %s/%s ", mode_str, entry -> owner, entry -> group);
            char size_buf[22] = {0};
            int rc = -1;
            switch (entry -> type){
                case REGULAR: case NORMAL: case CONTIGUOUS:
                    rc = sprintf(size_buf, "%u", ctar_oct2uint(entry -> size, 11));
                    break;
                case HARDLINK: case SYMLINK: case DIRECTORY: case FIFO:
                    rc = sprintf(size_buf, "%u", ctar_oct2uint(entry -> size, 11));
                    break;
                case CHAR: case BLOCK:
                    rc = sprintf(size_buf, "%d,%d", ctar_oct2uint(entry -> major, 7), ctar_oct2uint(entry -> minor, 7));
                    break;
            }

            if (rc < 0){
                ERROR("Failed to write length");
            }

            fprintf(f, "%s", size_buf);

            time_t mtime = ctar_oct2uint(entry -> mtime, 11);
            struct tm * time = localtime(&mtime);
            fprintf(f, " %d-%02d-%02d %02d:%02d ", time -> tm_year + 1900, time -> tm_mon + 1, time -> tm_mday, time -> tm_hour, time -> tm_min);
        }

        fprintf(f, "%s", entry -> name);

        if (verbosity > 1){
            switch (entry -> type){
                case HARDLINK:
                    fprintf(f, " link to %s", entry -> link_name);
                    break;
                case SYMLINK:
                    fprintf(f, " -> %s", entry -> link_name);
                    break;
                break;
            }
        }

        fprintf(f, "\n");
    }

    return 0;
}

int ctar_read_size(FILE* fp, char * buf, int size){
    int got = 0, rc;
    while ((got < size) && ((rc = fread(buf + got, 1, size - got, fp)) > 0)){
        got += rc;
    }
    return got;
}

unsigned int ctar_oct2uint(char * oct, unsigned int size){
    unsigned int out = 0;
    int i = 0;
    while ((i < size) && oct[i]){
        out = (out << 3) | (unsigned int) (oct[i++] - '0');
    }
    return out;
}

int ctar_iszeroed(char * buf, size_t size){
    for(size_t i = 0; i < size; buf++, i++){
        if (* (char *) buf){
            return 0;
        }
    }
    return 1;
}

#undef MIN
#undef MAX
#undef V_PRINT
#undef ERROR
#undef RC_ERROR
#undef WRITE_ERROR
#undef EXIST_ERROR

#ifdef CTAR_CUSTOM_CHMOD
# undef S_IRUSR
# undef S_IWUSR
# undef S_IXUSR
# undef S_IRGRP
# undef S_IWGRP
# undef S_IXGRP
# undef S_IROTH
# undef S_IWOTH
# undef S_IXOTH
# undef CTAR_CUSTOM_CHMOD
#endif

#endif // CTAR_IMPLEMENTATION

#endif // __CTAR_H__
