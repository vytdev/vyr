/*==============================================================================
 * vyr.h — VYT Archive Header
 *==============================================================================
 */
#ifndef __VYT_ARCHIVE_H
#define __VYT_ARCHIVE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef vyr_FORMAT_VERSION
#undef vyr_FORMAT_VERSION
#endif
/**
 * the format version of this implementation
 */
#define vyr_FORMAT_VERSION 1

/**
 * the header
 */
extern uint8_t vyr_header[4];

/**
 * vyr file item
 */
typedef struct __vyr_File_s {
	/**
	 * name of the file (within the archive)
	 */
	char* filename;
	/**
	 * content of file
	 */
	uint8_t* content;
	/**
	 * size of file
	 */
	uint32_t size;
} vyr_File;

/**
 * vyr package stream
 */
typedef struct __vyr_PackageStream_s {
	size_t __files_bufferSize;
	/**
	 * list of files in this stream
	 */
	vyr_File** files;
	/**
	 * how many files are there
	 */
	uint32_t file_count;
	/**
	 * the format version of source archive
	 */
	uint8_t format;
} vyr_PackageStream;

/**
 * initialize package stream
 */
bool vyr_openStream(vyr_PackageStream* stream);

/**
 * add a file to a stream
 */
bool vyr_addFile(vyr_PackageStream* stream, vyr_File* file);

/**
 * remove a file from the stream,
 * this will not free the file name and content
 */
bool vyr_removeFile(vyr_PackageStream* stream, char* filename);

/**
 * find a file in the package stream and return the pointer to vyr_File
 * struct item of it
 */
vyr_File* vyr_getFile(vyr_PackageStream* stream, char* filename);

/**
 * find a file in an input i/o stream of an archive and return it as vyr_File
 */
vyr_File* vyr_readFile(FILE* fd, char* filename);

/**
 * estimate the exact output size (in bytes) of given stream
 */
uint32_t vyr_estimateSize(vyr_PackageStream* stream);

/**
 * load archive from given binary to the stream
 */
bool vyr_loadArchive(vyr_PackageStream* stream, uint8_t* input, size_t len);

/**
 * save archive into a file
 */
bool vyr_packArchive(vyr_PackageStream* stream, FILE* out);

/**
 * function to close a vyr_PackageStream
 */
void vyr_closeStream(vyr_PackageStream* stream);

/**
 * utility function to read unsigned 32-bit integer from first 4 bytes of the
 * given array (of uint8_t), in least significant byte first manner
 */
uint32_t __vyr_util_read32BitIntLSB(uint8_t* bytes);
uint32_t __vyr_util_fread32BitIntLSB(FILE* file);

/**
 * the same with __vyr_util_read32BitIntLSB, but unsigned 16-bit number
 * instead
 */
uint16_t __vyr_util_read16BitIntLSB(uint8_t* bytes);
uint16_t __vyr_util_fread16BitIntLSB(FILE* file);

/**
 * utility function to write unsigned 32-bit integer onto given array pointer,
 * in little-endian
 */
void __vyr_util_write32BitIntLSB(uint32_t num, FILE* file);

/**
 * also same as __vyr_util_write32BitIntLSB, but write 16-bit number instead
 */
void __vyr_util_write16BitIntLSB(uint16_t num, FILE* file);

#endif // __VYT_ARCHIVE_H
