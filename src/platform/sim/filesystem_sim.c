#include "nuno/filesystem.h"

#include <stdio.h>

static FILE *s_current_file = NULL;

bool FileSystem_OpenFile(const char *filename) {
    if (s_current_file) {
        FileSystem_CloseFile();
    }

    s_current_file = fopen(filename, "rb");
    return s_current_file != NULL;
}

size_t FileSystem_ReadAudioData(void *buffer, size_t size) {
    if (!s_current_file || !buffer || size == 0U) {
        return 0U;
    }

    return fread(buffer, 1, size, s_current_file);
}

bool FileSystem_Seek(size_t position) {
    if (!s_current_file) {
        return false;
    }

    return fseek(s_current_file, (long)position, SEEK_SET) == 0;
}

void FileSystem_CloseFile(void) {
    if (s_current_file) {
        fclose(s_current_file);
        s_current_file = NULL;
    }
}
