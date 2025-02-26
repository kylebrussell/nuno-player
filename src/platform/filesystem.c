#include "nuno/filesystem.h"
#include "nuno/platform.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// File format handlers
typedef struct {
    const char* extension;
    bool (*probe)(FILE* file);
    size_t (*read)(void* buffer, size_t size, FILE* file);
} FormatHandler;

// Cache configuration
#define CACHE_SIZE (32 * 1024)  // 32KB cache
#define CACHE_BLOCK_SIZE 4096

// File system state
typedef struct {
    FILE* current_file;
    char current_filename[256];
    FormatHandler* current_format;
    
    // Cache
    uint8_t* cache_buffer;
    size_t cache_offset;
    size_t cache_valid_bytes;
    bool cache_dirty;
    
    // Error tracking
    uint32_t error_count;
    char last_error[128];
} FileSystemState;

static FileSystemState fs_state = {0};

// Forward declarations for format handlers
static bool probe_mp3(FILE* file);
static bool probe_flac(FILE* file);
static size_t read_mp3(void* buffer, size_t size, FILE* file);
static size_t read_flac(void* buffer, size_t size, FILE* file);

// Include our new format handlers header
#include "nuno/format_handlers.h"
#include "nuno/audio_buffer.h"

// Static buffer for MP3 processing
static uint8_t mp3_probe_buffer[ID3V2_HEADER_SIZE + 4];  // Enough for ID3v2 header + frame header

// Format handler registry
static FormatHandler format_handlers[] = {
    {".mp3", probe_mp3, read_mp3},
    {".flac", probe_flac, read_flac},
    {NULL, NULL, NULL}
};

// Initialize the filesystem
static bool FileSystem_Init(void) {
    if (fs_state.cache_buffer == NULL) {
        fs_state.cache_buffer = malloc(CACHE_SIZE);
        if (!fs_state.cache_buffer) {
            strncpy(fs_state.last_error, "Failed to allocate cache buffer", sizeof(fs_state.last_error));
            return false;
        }
    }
    return true;
}

// Detect file format and set appropriate handler
static FormatHandler* detect_format(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (!ext) return NULL;
    
    for (FormatHandler* handler = format_handlers; handler->extension != NULL; handler++) {
        if (strcasecmp(ext, handler->extension) == 0) {
            return handler;
        }
    }
    return NULL;
}

bool FileSystem_OpenFile(const char* filename) {
    if (!FileSystem_Init()) return false;
    
    // Close any existing file
    FileSystem_CloseFile();
    
    // Open new file
    FILE* file = fopen(filename, "rb");
    if (!file) {
        snprintf(fs_state.last_error, sizeof(fs_state.last_error), 
                "Failed to open file: %s", filename);
        fs_state.error_count++;
        return false;
    }
    
    // Detect format
    FormatHandler* handler = detect_format(filename);
    if (!handler || !handler->probe(file)) {
        fclose(file);
        snprintf(fs_state.last_error, sizeof(fs_state.last_error), 
                "Unsupported file format: %s", filename);
        fs_state.error_count++;
        return false;
    }
    
    // Update state
    fs_state.current_file = file;
    fs_state.current_format = handler;
    strncpy(fs_state.current_filename, filename, sizeof(fs_state.current_filename));
    fs_state.cache_offset = 0;
    fs_state.cache_valid_bytes = 0;
    fs_state.cache_dirty = false;
    
    return true;
}

size_t FileSystem_ReadAudioData(void* buffer, size_t size) {
    if (!fs_state.current_file || !fs_state.current_format) {
        strncpy(fs_state.last_error, "No file open", sizeof(fs_state.last_error));
        return 0;
    }
    
    size_t total_read = 0;
    uint8_t* dest = (uint8_t*)buffer;
    
    // First try to read from cache
    if (fs_state.cache_valid_bytes > 0) {
        size_t cache_available = fs_state.cache_valid_bytes;
        size_t to_copy = (size < cache_available) ? size : cache_available;
        
        memcpy(dest, fs_state.cache_buffer + fs_state.cache_offset, to_copy);
        fs_state.cache_offset += to_copy;
        fs_state.cache_valid_bytes -= to_copy;
        total_read += to_copy;
        
        if (total_read == size) {
            return total_read;
        }
        dest += to_copy;
        size -= to_copy;
    }
    
    // If we need more data, read directly from file
    if (size >= CACHE_BLOCK_SIZE) {
        size_t direct_read = fs_state.current_format->read(dest, size, fs_state.current_file);
        total_read += direct_read;
    } else {
        // Fill cache with new block
        fs_state.cache_valid_bytes = fs_state.current_format->read(
            fs_state.cache_buffer, 
            CACHE_SIZE,
            fs_state.current_file
        );
        fs_state.cache_offset = 0;
        
        if (fs_state.cache_valid_bytes > 0) {
            size_t to_copy = (size < fs_state.cache_valid_bytes) ? size : fs_state.cache_valid_bytes;
            memcpy(dest, fs_state.cache_buffer, to_copy);
            fs_state.cache_offset += to_copy;
            fs_state.cache_valid_bytes -= to_copy;
            total_read += to_copy;
        }
    }
    
    return total_read;
}

bool FileSystem_Seek(size_t position) {
    if (!fs_state.current_file) {
        strncpy(fs_state.last_error, "No file open", sizeof(fs_state.last_error));
        return false;
    }
    
    // Invalidate cache
    fs_state.cache_valid_bytes = 0;
    fs_state.cache_offset = 0;
    
    if (fseek(fs_state.current_file, position, SEEK_SET) != 0) {
        snprintf(fs_state.last_error, sizeof(fs_state.last_error),
                "Seek failed to position: %zu", position);
        fs_state.error_count++;
        return false;
    }
    
    return true;
}

void FileSystem_CloseFile(void) {
    if (fs_state.current_file) {
        fclose(fs_state.current_file);
        fs_state.current_file = NULL;
        fs_state.current_format = NULL;
        fs_state.current_filename[0] = '\0';
        fs_state.cache_valid_bytes = 0;
        fs_state.cache_offset = 0;
        fs_state.cache_dirty = false;
    }
}

// Updated MP3 format handling implementation
static bool probe_mp3(FILE* file) {
    // Read initial bytes for probing
    if (fread(mp3_probe_buffer, 1, sizeof(mp3_probe_buffer), file) != sizeof(mp3_probe_buffer)) {
        fseek(file, 0, SEEK_SET);
        return false;
    }
    
    // Create temporary AudioBuffer for probing
    AudioBuffer temp_buffer = {
        .data = mp3_probe_buffer,
        .size = sizeof(mp3_probe_buffer),
        .position = 0
    };
    
    // Use our MP3 format handler to validate the data
    bool is_valid = process_mp3_data(&temp_buffer);
    
    // Reset file position
    fseek(file, 0, SEEK_SET);
    return is_valid;
}

static size_t read_mp3(void* buffer, size_t size, FILE* file) {
    size_t bytes_read = fread(buffer, 1, size, file);
    if (bytes_read == 0) return 0;
    
    // Create temporary AudioBuffer for processing
    AudioBuffer temp_buffer = {
        .data = buffer,
        .size = bytes_read,
        .position = 0
    };
    
    // Process the MP3 data if this is the first read
    if (ftell(file) == bytes_read) {
        if (process_mp3_data(&temp_buffer)) {
            // Adjust the returned size based on any skipped metadata
            bytes_read -= (temp_buffer.data - (uint8_t*)buffer);
            
            // If we skipped some data, read more to fill the buffer
            if (bytes_read < size) {
                size_t additional = fread(temp_buffer.data + bytes_read, 1, 
                                       size - bytes_read, file);
                bytes_read += additional;
            }
        }
    }
    
    return bytes_read;
}

// Format handler stubs - to be implemented fully
static bool probe_flac(FILE* file) {
    uint8_t header[4];
    if (fread(header, 1, 4, file) != 4) return false;
    fseek(file, 0, SEEK_SET);
    return (memcmp(header, "fLaC", 4) == 0);
}

static size_t read_flac(void* buffer, size_t size, FILE* file) {
    // Basic implementation - to be enhanced with proper FLAC frame handling
    return fread(buffer, 1, size, file);
}