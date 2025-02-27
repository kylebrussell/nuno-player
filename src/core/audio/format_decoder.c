#include "nuno/format_decoder.h"
#include "minimp3.h"
#include "minimp3_ex.h"
#include <stdlib.h>
#include <string.h>

struct FormatDecoder {
    mp3dec_t mp3d;
    mp3dec_file_info_t info;
    AudioFormatInfo format_info;
    size_t position;
    bool initialized;
    enum FormatDecoderError last_error;
};

// Buffer requirement constants for MP3
#define MP3_MIN_BUFFER_SIZE (2 * 1152 * 2 * sizeof(float))  // 2 frames * max samples per frame * stereo * float size
#define MP3_OPTIMAL_BUFFER_SIZE (32 * 1152 * 2 * sizeof(float))  // 32 frames worth of data
#define MP3_MAX_FRAME_SIZE (1152 * 2 * sizeof(float))  // Max samples per frame * stereo * float size
#define MP3_FRAMES_PER_BUFFER 32  // Recommended frames per buffer

// Buffer requirement constants for FLAC
#define FLAC_MIN_BUFFER_SIZE (4096 * 8 * sizeof(float))  // Minimum block size * max channels * float size
#define FLAC_OPTIMAL_BUFFER_SIZE (65536 * 8 * sizeof(float))  // Optimal for efficient decoding
#define FLAC_MAX_FRAME_SIZE (65536 * 8 * sizeof(float))  // Maximum block size * max channels * float size
#define FLAC_FRAMES_PER_BUFFER 4  // FLAC frames are larger, so fewer per buffer

FormatDecoder* format_decoder_create(void) {
    FormatDecoder* decoder = (FormatDecoder*)malloc(sizeof(FormatDecoder));
    if (!decoder) return NULL;
    
    decoder->position = 0;
    decoder->initialized = false;
    decoder->last_error = FD_ERROR_NONE;
    decoder->format_info.format_type = AUDIO_FORMAT_UNKNOWN;
    mp3dec_init(&decoder->mp3d);
    
    return decoder;
}

bool format_decoder_open(FormatDecoder* decoder, const char* filepath) {
    if (!decoder || !filepath) {
        if (decoder) decoder->last_error = FD_ERROR_INVALID_PARAM;
        return false;
    }
    
    // Reset state
    decoder->position = 0;
    decoder->last_error = FD_ERROR_NONE;
    
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        decoder->last_error = FD_ERROR_FILE_NOT_FOUND;
        return false;
    }

    // Read file into buffer for format detection
    uint8_t header_buffer[8192];  // Reasonable size for detection
    size_t bytes_read = fread(header_buffer, 1, sizeof(header_buffer), file);
    
    // Detect format
    decoder->last_error = detect_audio_format(header_buffer, bytes_read, 
                                            &decoder->format_info);
    if (decoder->last_error != FD_ERROR_NONE) {
        fclose(file);
        return false;
    }
    
    // Rewind file and decode
    fseek(file, 0, SEEK_SET);
    int load_result = mp3dec_load(&decoder->mp3d, filepath, &decoder->info, NULL, NULL);
    
    if (load_result != 0) {
        decoder->last_error = load_result == MP3D_E_MEMORY ? 
            FD_ERROR_MEMORY : FD_ERROR_DECODE;
        fclose(file);
        return false;
    }

    fclose(file);
    decoder->initialized = true;
    return true;
}

size_t format_decoder_read(FormatDecoder* decoder, float* buffer, size_t frames) {
    if (!decoder || !decoder->initialized || !buffer) return 0;
    
    size_t available_frames = decoder->info.samples / decoder->info.channels;
    size_t remaining_frames = available_frames - decoder->position;
    size_t frames_to_read = (frames < remaining_frames) ? frames : remaining_frames;
    
    if (frames_to_read > 0) {
        size_t sample_offset = decoder->position * decoder->info.channels;
        size_t samples_to_copy = frames_to_read * decoder->info.channels;
        
        memcpy(buffer, 
               decoder->info.buffer + sample_offset, 
               samples_to_copy * sizeof(float));
               
        decoder->position += frames_to_read;
    }
    
    return frames_to_read;
}

void format_decoder_seek(FormatDecoder* decoder, size_t frame_position) {
    if (!decoder || !decoder->initialized) return;
    
    size_t total_frames = decoder->info.samples / decoder->info.channels;
    decoder->position = (frame_position < total_frames) ? frame_position : total_frames;
}

void format_decoder_close(FormatDecoder* decoder) {
    if (!decoder) return;
    
    if (decoder->initialized) {
        free(decoder->info.buffer);
        decoder->initialized = false;
    }
}

void format_decoder_destroy(FormatDecoder* decoder) {
    if (!decoder) return;
    
    format_decoder_close(decoder);
    free(decoder);
}

uint32_t format_decoder_get_channels(const FormatDecoder* decoder) {
    return decoder && decoder->initialized ? decoder->info.channels : 0;
}

uint32_t format_decoder_get_sample_rate(const FormatDecoder* decoder) {
    return decoder && decoder->initialized ? decoder->info.hz : 0;
}

enum AudioFormatType format_decoder_get_format_type(const FormatDecoder* decoder) {
    return decoder ? decoder->format_info.format_type : AUDIO_FORMAT_UNKNOWN;
}

bool format_decoder_get_buffer_requirements(enum AudioFormatType format_type, 
                                           BufferRequirements* requirements) {
    if (!requirements) return false;
    
    switch (format_type) {
        case AUDIO_FORMAT_MP3:
            requirements->min_buffer_size = MP3_MIN_BUFFER_SIZE;
            requirements->optimal_buffer_size = MP3_OPTIMAL_BUFFER_SIZE;
            requirements->max_frame_size = MP3_MAX_FRAME_SIZE;
            requirements->frames_per_buffer = MP3_FRAMES_PER_BUFFER;
            return true;
            
        case AUDIO_FORMAT_FLAC:
            requirements->min_buffer_size = FLAC_MIN_BUFFER_SIZE;
            requirements->optimal_buffer_size = FLAC_OPTIMAL_BUFFER_SIZE;
            requirements->max_frame_size = FLAC_MAX_FRAME_SIZE;
            requirements->frames_per_buffer = FLAC_FRAMES_PER_BUFFER;
            return true;
            
        default:
            // For unsupported formats, return false
            return false;
    }
}

bool format_decoder_get_current_buffer_requirements(const FormatDecoder* decoder,
                                                  BufferRequirements* requirements) {
    if (!decoder || !requirements) return false;
    
    // If no file is loaded, return false
    if (!decoder->initialized) return false;
    
    // Get requirements based on the format type of the loaded file
    return format_decoder_get_buffer_requirements(decoder->format_info.format_type, requirements);
}

enum FormatDecoderError format_decoder_get_last_error(const FormatDecoder* decoder) {
    return decoder ? decoder->last_error : FD_ERROR_INVALID_PARAM;
}

const char* format_decoder_error_string(enum FormatDecoderError error) {
    switch (error) {
        case FD_ERROR_NONE: return "No error";
        case FD_ERROR_INVALID_PARAM: return "Invalid parameter";
        case FD_ERROR_FILE_NOT_FOUND: return "File not found";
        case FD_ERROR_FILE_READ: return "File read error";
        case FD_ERROR_INVALID_FORMAT: return "Invalid audio format";
        case FD_ERROR_MEMORY: return "Memory allocation failed";
        case FD_ERROR_DECODE: return "Decoding error";
        default: return "Unknown error";
    }
}
