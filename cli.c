#include "vyr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

void createDirectories(char* path) {
	char* copy = strdup(path);
	char* token = strtok(copy, "/");
	char dir[256] = {
		0
	};

#ifdef _WIN32
	snprintf(dir, sizeof(dir), "%s", token);
#else
	snprintf(dir, sizeof(dir), "/%s", token);
#endif

	while (token != NULL) {
#ifdef _WIN32
		mkdir(dir);
#else
		mkdir(dir, 0755);
#endif

		token = strtok(NULL, "/");
#ifdef _WIN32
		snprintf(dir, sizeof(dir), "%s/%s", dir, token);
#else
		if (token != NULL) {
			snprintf(dir, sizeof(dir), "%s/%s", dir, token);
		}
#endif
	}

	free(copy);
}

char* customDirname(char* path) {
	// path is NULL
	if (!path) return NULL;

	// root path
	if (strcmp(path, "/") == 0 || strcmp(path, "\\") == 0) return strdup(path);

	char* lastOccurrence = NULL;

	// traverse the string from the end
	for (int i = strlen(path) - 1; i >= 0; --i) {

		if (path[i] == '/' || path[i] == '\\') {
			// found the last occurrence, exit the loop
			lastOccurrence = &path[i];
			break;
		}

	}

	// no dirname
	if (!lastOccurrence) return strdup(path);

	// extract path
	size_t nameLength = lastOccurrence - path;
	char* parentFolder = (char*) malloc(sizeof(char) * (nameLength + 1));

	if (parentFolder) {
		strncpy(parentFolder, path, nameLength);
		parentFolder[nameLength] = '\0';
	}

	return parentFolder;
}

int main(int argc, char** argv) {

	if (argc < 2) {
		printf(
			"vyr - vyt archiver\n"
			"\n"
			"usage: %s [sub-command] [args ...]\n"
			"\n"
			"sub-commands\n"
			"    pack <output.vyr> [filepath[=arcname] ...]\n"
			"    unpack <input.vyr>\n"
			"    delete <input.vyr> [arcname ...]\n", argv[0]);
		return 1;
	}

	if (strcmp(argv[1], "pack") == 0) {

		vyr_PackageStream stream;

		if (!vyr_openStream(&stream)) {
			perror("failed to initialize package stream");
			return 1;
		}

		for (int i = 3; i < argc; i++) {
			char* item = argv[i];

			char* filepath;
			char* arcname;

			char* tok = strchr(item, '=');

			if (tok) {
				size_t delimeterPos = tok - item;

				// process filepath
				filepath = (char*) malloc(delimeterPos + 1);

				if (!filepath) {
					vyr_closeStream(&stream);
					perror("internal error");
					return 1;
				}

				strncpy(filepath, item, delimeterPos);
				filepath[delimeterPos] = '\0';

				// process arcname
				arcname = (char*) malloc(strlen(item) - delimeterPos);

				if (!arcname) {
					free(filepath);
					vyr_closeStream(&stream);
					perror("internal error");
					return 1;
				}

				strcpy(arcname, tok + 1);

			} else {

				filepath = strdup(item);
				if (!filepath) {
					vyr_closeStream(&stream);
					perror("internal error");
					return 1;
				}

				arcname = strdup(item);
				if (!arcname) {
					free(filepath);
					vyr_closeStream(&stream);
					perror("internal error");
					return 1;
				}

			}

			printf("%s: %s\n", arcname, filepath);

			FILE* fd = fopen(filepath, "rb");

			if (!fd) {
				vyr_closeStream(&stream);
				fprintf(stderr, "failed to load file: %s: ", filepath);
				free(filepath);
				free(arcname);
				perror("");
				continue;
			}

			// get the file size
			fseek(fd, 0, SEEK_END);
			uint32_t size = ftell(fd);
			fseek(fd, 0, SEEK_SET);

			// allocate for file content
			uint8_t* content = (uint8_t*) malloc(sizeof(uint8_t) * size);

			if (!content) {
				vyr_closeStream(&stream);
				free(filepath);
				free(arcname);
				fclose(fd);
				perror("internal error");
				return 1;
			}

			fread(content, sizeof(uint8_t), size, fd);
			fclose(fd);

			vyr_File* file = (vyr_File*) malloc(sizeof(vyr_File));
			if (!file) {
				vyr_closeStream(&stream);
				free(filepath);
				free(arcname);
				perror("internal error");
				return 1;
			}

			file->filename = arcname;
			file->size = size;
			file->content = content;

			vyr_addFile(&stream, file);

		}

		FILE* out = fopen(argv[2], "wb");

		if (!out) {
			perror("failed to open output file");
			vyr_closeStream(&stream);
			return 1;
		}

		vyr_packArchive(&stream, out);
		vyr_closeStream(&stream);

		printf("pack done!\n");

	} else if (strcmp(argv[1], "unpack") == 0) {

		FILE* fd = fopen(argv[2], "rb");

		if (!fd) {
			fprintf(stderr, "failed to load file: %s: ", argv[2]);
			perror("");
			return 1;
		}

		vyr_PackageStream stream;

		if (!vyr_openStream(&stream)) {
			perror("failed to initialize package stream");
			fclose(fd);
			return 1;
		}

		fseek(fd, 0, SEEK_END);
		uint32_t size = ftell(fd);
		fseek(fd, 0, SEEK_SET);

		uint8_t* content = (uint8_t*) malloc(sizeof(uint8_t) * size);
		if (!content) {
			fclose(fd);
			vyr_closeStream(&stream);
			return 1;
		}

		fread(content, sizeof(uint8_t), size, fd);

		if (!vyr_loadArchive(&stream, content, size)) {
			fprintf(stderr, "failed to parse archive\n");
			fclose(fd);
			vyr_closeStream(&stream);
			return 1;
		}

		free(content);
		fclose(fd);

		for (uint32_t i = 0; i < stream.file_count; i++) {
			// maybe, file is NULL
			if (!stream.files[i]) continue;

			// the file
			vyr_File* file = stream.files[i];

			printf("%s\n", file->filename);

			// ensure folder exists
			char* parentDir = customDirname(file->filename);
			createDirectories(parentDir);
			free(parentDir);

			// open file
			FILE* out = fopen(file->filename, "wb");
			if (!out) {
				vyr_closeStream(&stream);
				fprintf(stderr, "failed to open output stream: %s: ", file->filename);
				return 1;
			}

			fwrite(file->content, sizeof(char), file->size, out);
			fclose(out);
		}

		vyr_closeStream(&stream);

	} else if (strcmp(argv[1], "delete") == 0) {

		// TODO
		printf("not implemented\n");

	} else {
		fprintf(stderr, "unknown sub-command: %s\n", argv[1]);
		return 1;
	}

	return 0;
}
