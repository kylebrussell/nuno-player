#include "nuno/format_decoder.h"
#include "minimp3.h"
#include "minimp3_ex.h"
#include <stdlib.h>
#include <string.h>

struct FormatDecoder {
    mp3dec_t mp3d;
    mp3dec_file_info_t info;
    AudioFormatInfo format_info;
    DecoderConfig config;
    size_t position;
    bool initialized;
    enum FormatDecoderError last_error;
    void* format_specific_data;  // For format-specific decoder state
};

// Default decoder configuration
static const DecoderConfig default_config = {
    .seeking_behavior = SEEK_ACCURATE,
    .error_mode = ERROR_TOLERANT,
    .buffer_size = 16384,
    .use_float_output = true,
    .enable_replaygain = false,
    .replaygain_preamp = 0.0f,
    .enable_gapless = true,
    .enable_caching = true,
    .cache_size = 1024 * 1024,  // 1MB cache
    .target_sample_rate = 0,    // Native sample rate
    .target_bit_depth = 0       // Native bit depth
};

// Format-specific capabilities
static const DecoderCapabilities mp3_capabilities = {
    .max_sample_rate = 48000,
    .min_sample_rate = 8000,
    .supported_depths = {BIT_DEPTH_16, BIT_DEPTH_FLOAT, 0, 0},
    .max_channels = 2,
    .supports_vbr = true,
    .supports_seeking = true,
    .supports_streaming = true,
    .supports_gapless = true,
    .supports_replaygain = true,
    .max_buffer_size = 1024 * 1024 * 10  // 10MB
};

static const DecoderCapabilities flac_capabilities = {
    .max_sample_rate = 192000,
    .min_sample_rate = 8000,
    .supported_depths = {BIT_DEPTH_16, BIT_DEPTH_24, BIT_DEPTH_FLOAT, 0},
    .max_channels = 8,
    .supports_vbr = false,
    .supports_seeking = true,
    .supports_streaming = false,
    .supports_gapless = true,
    .supports_replaygain = true,
    .max_buffer_size = 1024 * 1024 * 20  // 20MB
};

static const DecoderCapabilities wav_capabilities = {
    .max_sample_rate = 192000,
    .min_sample_rate = 8000,
    .supported_depths = {BIT_DEPTH_8, BIT_DEPTH_16, BIT_DEPTH_24, BIT_DEPTH_32},
    .max_channels = 8,
    .supports_vbr = false,
    .supports_seeking = true,
    .supports_streaming = false,
    .supports_gapless = true,
    .supports_replaygain = false,
    .max_buffer_size = 1024 * 1024 * 50  // 50MB
};

static const DecoderCapabilities aac_capabilities = {
    .max_sample_rate = 96000,
    .min_sample_rate = 8000,
    .supported_depths = {BIT_DEPTH_16, BIT_DEPTH_FLOAT, 0, 0},
    .max_channels = 8,
    .supports_vbr = true,
    .supports_seeking = true,
    .supports_streaming = true,
    .supports_gapless = false,
    .supports_replaygain = false,
    .max_buffer_size = 1024 * 1024 * 10  // 10MB
};

static const DecoderCapabilities ogg_capabilities = {
    .max_sample_rate = 192000,
    .min_sample_rate = 8000,
    .supported_depths = {BIT_DEPTH_16, BIT_DEPTH_FLOAT, 0, 0},
    .max_channels = 8,
    .supports_vbr = true,
    .supports_seeking = true,
    .supports_streaming = true,
    .supports_gapless = true,
    .supports_replaygain = true,
    .max_buffer_size = 1024 * 1024 * 10  // 10MB
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

// Buffer requirement constants for WAV
#define WAV_MIN_BUFFER_SIZE (1024 * 8 * sizeof(float))
#define WAV_OPTIMAL_BUFFER_SIZE (8192 * 8 * sizeof(float))
#define WAV_MAX_FRAME_SIZE (4096 * 8 * sizeof(float))
#define WAV_FRAMES_PER_BUFFER 8

// Buffer requirement constants for AAC
#define AAC_MIN_BUFFER_SIZE (1024 * 8 * sizeof(float))
#define AAC_OPTIMAL_BUFFER_SIZE (8192 * 8 * sizeof(float))
#define AAC_MAX_FRAME_SIZE (2048 * 8 * sizeof(float))
#define AAC_FRAMES_PER_BUFFER 16

// Buffer requirement constants for OGG
#define OGG_MIN_BUFFER_SIZE (1024 * 8 * sizeof(float))
#define OGG_OPTIMAL_BUFFER_SIZE (8192 * 8 * sizeof(float))
#define OGG_MAX_FRAME_SIZE (4096 * 8 * sizeof(float))
#define OGG_FRAMES_PER_BUFFER 8

FormatDecoder* format_decoder_create(void) {
    FormatDecoder* decoder = (FormatDecoder*)malloc(sizeof(FormatDecoder));
    if (!decoder) return NULL;
    
    decoder->position = 0;
    decoder->initialized = false;
    decoder->last_error = FD_ERROR_NONE;
    decoder->format_info.format_type = AUDIO_FORMAT_UNKNOWN;
    decoder->format_specific_data = NULL;
    
    // Initialize with default configuration
    memcpy(&decoder->config, &default_config, sizeof(DecoderConfig));
    
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
    
    // Apply seeking behavior based on configuration
    switch (decoder->config.seeking_behavior) {
        case SEEK_ACCURATE:
            // Precise seeking - just set the position
            decoder->position = (frame_position < total_frames) ? frame_position : total_frames;
            break;
            
        case SEEK_FAST:
            // Fast seeking - align to a reasonable boundary for efficiency
            {
                // For example, align to 1024-frame boundaries
                const size_t FRAME_ALIGNMENT = 1024;
                frame_position = (frame_position / FRAME_ALIGNMENT) * FRAME_ALIGNMENT;
                decoder->position = (frame_position < total_frames) ? frame_position : total_frames;
            }
            break;
            
        case SEEK_NEAREST_KEYFRAME:
            // For formats with keyframes, find the nearest one
            // This is format-specific and would need implementation for each format
            // For now, fall back to accurate seeking
            decoder->position = (frame_position < total_frames) ? frame_position : total_frames;
            break;
    }
}

void format_decoder_close(FormatDecoder* decoder) {
    if (!decoder) return;
    
    if (decoder->initialized) {
        free(decoder->info.buffer);
        decoder->initialized = false;
    }
    
    // Free any format-specific data
    if (decoder->format_specific_data) {
        free(decoder->format_specific_data);
        decoder->format_specific_data = NULL;
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
            
        case AUDIO_FORMAT_WAV:
            requirements->min_buffer_size = WAV_MIN_BUFFER_SIZE;
            requirements->optimal_buffer_size = WAV_OPTIMAL_BUFFER_SIZE;
            requirements->max_frame_size = WAV_MAX_FRAME_SIZE;
            requirements->frames_per_buffer = WAV_FRAMES_PER_BUFFER;
            return true;
            
        case AUDIO_FORMAT_AAC:
            requirements->min_buffer_size = AAC_MIN_BUFFER_SIZE;
            requirements->optimal_buffer_size = AAC_OPTIMAL_BUFFER_SIZE;
            requirements->max_frame_size = AAC_MAX_FRAME_SIZE;
            requirements->frames_per_buffer = AAC_FRAMES_PER_BUFFER;
            return true;
            
        case AUDIO_FORMAT_OGG:
            requirements->min_buffer_size = OGG_MIN_BUFFER_SIZE;
            requirements->optimal_buffer_size = OGG_OPTIMAL_BUFFER_SIZE;
            requirements->max_frame_size = OGG_MAX_FRAME_SIZE;
            requirements->frames_per_buffer = OGG_FRAMES_PER_BUFFER;
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

bool format_decoder_get_capabilities(enum AudioFormatType format_type,
                                    DecoderCapabilities* capabilities) {
    if (!capabilities) return false;
    
    switch (format_type) {
        case AUDIO_FORMAT_MP3:
            memcpy(capabilities, &mp3_capabilities, sizeof(DecoderCapabilities));
            return true;
            
        case AUDIO_FORMAT_FLAC:
            memcpy(capabilities, &flac_capabilities, sizeof(DecoderCapabilities));
            return true;
            
        case AUDIO_FORMAT_WAV:
            memcpy(capabilities, &wav_capabilities, sizeof(DecoderCapabilities));
            return true;
            
        case AUDIO_FORMAT_AAC:
            memcpy(capabilities, &aac_capabilities, sizeof(DecoderCapabilities));
            return true;
            
        case AUDIO_FORMAT_OGG:
            memcpy(capabilities, &ogg_capabilities, sizeof(DecoderCapabilities));
            return true;
            
        default:
            return false;
    }
}

bool format_decoder_configure(FormatDecoder* decoder, const DecoderConfig* config) {
    if (!decoder || !config) {
        if (decoder) decoder->last_error = FD_ERROR_INVALID_PARAM;
        return false;
    }
    
    // Validate configuration against capabilities
    if (decoder->initialized) {
        DecoderCapabilities capabilities;
        if (format_decoder_get_capabilities(decoder->format_info.format_type, &capabilities)) {
            // Check if requested sample rate is supported
            if (config->target_sample_rate != 0 && 
                (config->target_sample_rate < capabilities.min_sample_rate || 
                 config->target_sample_rate > capabilities.max_sample_rate)) {
                decoder->last_error = FD_ERROR_INVALID_PARAM;
                return false;
            }
            
            // Check if requested bit depth is supported
            if (config->target_bit_depth != 0) {
                bool supported = false;
                for (int i = 0; i < 4 && capabilities.supported_depths[i] != 0; i++) {
                    if (capabilities.supported_depths[i] == config->target_bit_depth) {
                        supported = true;
                        break;
                    }
                }
                
                if (!supported) {
                    decoder->last_error = FD_ERROR_INVALID_PARAM;
                    return false;
                }
            }
            
            // Check if gapless is supported
            if (config->enable_gapless && !capabilities.supports_gapless) {
                // We'll still accept the configuration but won't enable gapless
                DecoderConfig* mutable_config = (DecoderConfig*)config;
                mutable_config->enable_gapless = false;
            }
            
            // Check if ReplayGain is supported
            if (config->enable_replaygain && !capabilities.supports_replaygain) {
                // We'll still accept the configuration but won't enable ReplayGain
                DecoderConfig* mutable_config = (DecoderConfig*)config;
                mutable_config->enable_replaygain = false;
            }
        }
    }
    
    // Apply configuration
    memcpy(&decoder->config, config, sizeof(DecoderConfig));
    
    return true;
}

bool format_decoder_get_config(const FormatDecoder* decoder, DecoderConfig* config) {
    if (!decoder || !config) return false;
    
    memcpy(config, &decoder->config, sizeof(DecoderConfig));
    return true;
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
