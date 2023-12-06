#include "vyr.h"

uint8_t vyr_header[4] = { 0x56, 0x59, 0x54, 0x52 };

bool vyr_openStream(vyr_PackageStream* stream) {
	// initialize some variables
	stream->__files_bufferSize = 2;
	stream->file_count = 0;
	// the file stream
	stream->files = (vyr_File**) malloc(sizeof(vyr_File*) * 2);
	if (!stream->files) return false;
	// succeded
	return true;
}

bool vyr_addFile(vyr_PackageStream* stream, vyr_File* file) {
	// file buffer is full
	if (stream->__files_bufferSize <= stream->file_count) {
		stream->__files_bufferSize *= 2;

		// temporary variable to re-allocate the file buffer
		vyr_File** tmp = (vyr_File**) realloc(stream->files,
			sizeof(vyr_File*) * stream->__files_bufferSize);

		// re-allocation failed
		if (!tmp) {
			stream->__files_bufferSize /= 2;
			return false;
		}

		// update the package stream file buffer
		stream->files = tmp;
	}

	// take care of empty spots in list
	int i = 0;
	for ( ; i <= stream->file_count; i++) {
		if (stream->files[i]) continue;
		stream->files[i] = file;
		break;
	}

	// put on the end, increment file count
	if (i == stream->file_count) stream->file_count++;

	// add succeded
	return true;
}

bool vyr_removeFile(vyr_PackageStream* stream, char* filename) {
	// find the file
	for (uint32_t i = 0; i < stream->file_count; i++) {
		if (stream->files[i] && strcmp(filename,
			stream->files[i]->filename) == 0)
		{
			stream->files[i] = NULL;
			return true;
		}
	}

	// file not exist in the stream
	return false;
}

vyr_File* vyr_getFile(vyr_PackageStream* stream, char* filename) {
	for (uint32_t i = 0; i < stream->file_count; i++) {
		if (stream->files[i] && strcmp(filename, stream->files[i]->filename)
			== 0) return stream->files[i];
	}
	return NULL;
}

vyr_File* vyr_readFile(FILE* fd, char* filename) {
	if (!fd) return NULL;

	// set this back again on function exit
	size_t lastPos = ftell(fd);

	// go to file beginning
	fseek(fd, 0, SEEK_SET);

	// validate header
	uint8_t header[sizeof(vyr_header)];
	fread(header, sizeof(uint8_t), sizeof(vyr_header), fd);
	if (memcmp(header, vyr_header, sizeof(vyr_header)) != 0) return NULL;
	fseek(fd, sizeof(vyr_header), SEEK_SET);

	// check if format version is supported
	uint8_t format;
	fread(&format, sizeof(uint8_t), 1, fd);
	if (format != vyr_FORMAT_VERSION) return NULL;

	// number of files
	uint32_t file_count = __vyr_util_fread32BitIntLSB(fd);

	// search for the file
	for (uint32_t i = 0; i < file_count; i++) {
		// file name length
		uint16_t nameLength = __vyr_util_fread16BitIntLSB(fd);

		// get the file name
		char fileName[nameLength + 1];
		fread(fileName, sizeof(char), nameLength, fd);
		fileName[nameLength] = '\0';

		// file size
		uint32_t size = __vyr_util_fread32BitIntLSB(fd);

		// file found
		if (strcmp(filename, fileName) == 0) {

			// initialize this pointer of vyr_File instance
			vyr_File* file = (vyr_File*) malloc(sizeof(vyr_File));
			file->size = size;

			file->filename = strdup(fileName);
			if (!file->filename) return NULL;

			file->content = (uint8_t*) malloc(sizeof(uint8_t) * size);
			if (!file->content) {
				free(file->filename);
				return NULL;
			}

			// load the file content from the archive
			fread(file->content, sizeof(uint8_t), size, fd);

			// set back to last position indicator
			fseek(fd, lastPos, SEEK_SET);

			return file;
		}

		// skip the file content chunk
		fseek(fd, size, SEEK_CUR);
	}

	// set this back to last position
	fseek(fd, lastPos, SEEK_SET);
	return NULL;
}

uint32_t vyr_estimateSize(vyr_PackageStream* stream) {
	// initial and minimum size is: size of header, the format version, and
	// file count metadata
	uint32_t size = (sizeof(vyr_header) / sizeof(uint8_t)) + 5;

	for (uint32_t i = 0; i < stream->file_count; i++) {
		// name length variable size + content size variable size
		size += 6;
		// actual length of filename
		size += strlen(stream->files[i]->filename);
		// actual size of file
		size += stream->files[i]->size;
	}

	return size;
}

bool vyr_loadArchive(vyr_PackageStream* stream, uint8_t* input, size_t len) {
	size_t idx = 0;

	// invalid format
	if (memcmp(input, vyr_header, sizeof(vyr_header)) != 0) {
		return false;
	}

	// increment to skip header
	idx += sizeof(vyr_header);

	// the format version
	stream->format = input[idx];
	idx++;
	// archive format unsupported
	if (stream->format != vyr_FORMAT_VERSION) return false;

	// file count
	if ((len - idx) < 4) return false;
	stream->file_count = __vyr_util_read32BitIntLSB(input + idx);
	idx += 4;

	for (uint32_t i = 0; i < stream->file_count; i++) {
		vyr_File* file = (vyr_File*) malloc(sizeof(vyr_File));

		// get name length
		if ((len - idx) < 2) return false;
		uint16_t nameLength = __vyr_util_read16BitIntLSB(input + idx);
		idx += 2;

		// load the filename
		if ((len - idx) < nameLength) return false;
		char* filename = (char*) malloc(sizeof(char) * (nameLength + 1));
		if (!filename) {
			free(file);
			return false;
		}
		for (uint32_t j = 0; j < nameLength; j++) {
			filename[j] = input[idx + j];
		}
		filename[nameLength] = '\0';
		file->filename = filename;
		idx += nameLength;

		// get file size
		if ((len - idx) < 4) {
			free(file);
			free(filename);
			return false;
		}
		uint32_t fileSize = __vyr_util_read32BitIntLSB(input + idx);
		file->size = fileSize;
		idx += 4;

		// load raw file content
		if ((len - idx) < fileSize) {
			free(file);
			free(filename);
			return false;
		}
		uint8_t* fileContent = (uint8_t*) malloc(sizeof(uint8_t) * fileSize);
		if (!fileContent) {
			free(file);
			free(filename);
			return false;
		}
		memcpy(fileContent, input + idx, fileSize);
		file->content = fileContent;
		idx += fileSize;

		// now, add it on the package stream
		vyr_addFile(stream, file);
	}

	return true;
}

bool vyr_packArchive(vyr_PackageStream* stream, FILE* out) {
	if (!out) return false;

	// put the header
	fwrite(&vyr_header, sizeof(uint8_t), sizeof(vyr_header), out);

	// add the format version
	fwrite(&(uint8_t) { vyr_FORMAT_VERSION }, sizeof(uint8_t), 1, out);

	// add the file count
	__vyr_util_write32BitIntLSB(stream->file_count, out);

	for (uint32_t i = 0; i < stream->file_count; i++) {
		// file points to NULL
		if (!stream->files[i]) continue;

		// put name length
		uint16_t nameLength = strlen(stream->files[i]->filename);
		__vyr_util_write16BitIntLSB(nameLength, out);

		// copy the filename
		fwrite(stream->files[i]->filename, sizeof(char), nameLength, out);

		// the file size
		__vyr_util_write32BitIntLSB(stream->files[i]->size, out);

		// file content
		fwrite(stream->files[i]->content, sizeof(uint8_t),
			stream->files[i]->size, out);
	}

	return true;
}

void vyr_closeStream(vyr_PackageStream* stream) {
	if (stream && stream->files) {
		// we expect ->content and ->filename field is always in heap
		for (uint32_t i = 0; i < stream->file_count; i++) if (stream->files[i]) {
			if (stream->files[i]->content) free(stream->files[i]->content);
			if (stream->files[i]->filename) free(stream->files[i]->filename);
			free(stream->files[i]);
		}
		// free the list buffer
		free(stream->files);
	}
}

uint32_t __vyr_util_read32BitIntLSB(uint8_t* bytes) {
	return bytes[0] | (bytes[1] << 8) |
		(bytes[2] << 16) | (bytes[3] << 24);
}

uint32_t __vyr_util_fread32BitIntLSB(FILE* file) {
	return fgetc(file) | (fgetc(file) << 8) |
		(fgetc(file) << 16) | (fgetc(file) << 24);
}

uint16_t __vyr_util_read16BitIntLSB(uint8_t* bytes) {
	return bytes[0] | (bytes[1] << 8);
}

uint16_t __vyr_util_fread16BitIntLSB(FILE* file) {
	return fgetc(file) | (fgetc(file) << 8);
}

void __vyr_util_write32BitIntLSB(uint32_t num, FILE* file) {
	fputc(num & 0xff, file);
	fputc((num >> 8) & 0xff, file);
	fputc((num >> 16) & 0xff, file);
	fputc((num >> 24) & 0xff, file);
}

void __vyr_util_write16BitIntLSB(uint16_t num, FILE* file) {
	fputc(num & 0xff, file);
	fputc((num >> 8) & 0xff, file);
}
