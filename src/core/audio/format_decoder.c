#include "nuno/format_decoder.h"
#include "minimp3.h"
#include "minimp3_ex.h"
#include <stdlib.h>
#include <string.h>

enum FormatDecoderError {
    FD_ERROR_NONE = 0,
    FD_ERROR_INVALID_PARAM,
    FD_ERROR_FILE_NOT_FOUND,
    FD_ERROR_FILE_READ,
    FD_ERROR_INVALID_FORMAT,
    FD_ERROR_MEMORY,
    FD_ERROR_DECODE
};

struct FormatDecoder {
    mp3dec_t mp3d;
    mp3dec_file_info_t info;
    size_t position;
    bool initialized;
    enum FormatDecoderError last_error;
};

FormatDecoder* format_decoder_create(void) {
    FormatDecoder* decoder = (FormatDecoder*)malloc(sizeof(FormatDecoder));
    if (!decoder) return NULL;
    
    decoder->position = 0;
    decoder->initialized = false;
    decoder->last_error = FD_ERROR_NONE;
    mp3dec_init(&decoder->mp3d);
    
    return decoder;
}

bool format_decoder_open(FormatDecoder* decoder, const char* filepath) {
    if (!decoder) {
        return false;
    }
    
    if (!filepath) {
        decoder->last_error = FD_ERROR_INVALID_PARAM;
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
    
    // Load and decode MP3 file
    int load_result = mp3dec_load(&decoder->mp3d, filepath, &decoder->info, NULL, NULL);
    if (load_result != 0) {
        decoder->last_error = load_result == MP3D_E_MEMORY ? 
            FD_ERROR_MEMORY : FD_ERROR_DECODE;
        fclose(file);
        return false;
    }
    
    if (decoder->info.channels == 0 || decoder->info.hz == 0) {
        decoder->last_error = FD_ERROR_INVALID_FORMAT;
        free(decoder->info.buffer);
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
