#include "nuno/format_decoder.h"
#include "minimp3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct FormatDecoder {
    mp3dec_t mp3d;
    mp3dec_frame_info_t frame_info;
    AudioFormatInfo format_info;
    DecoderConfig config;
    size_t position;
    bool initialized;
    enum FormatDecoderError last_error;
    void* format_specific_data;  // For format-specific decoder state

    // MP3-specific data
    FILE* file;
    // Encoded (input) buffer
    uint8_t* buffer;
    size_t buffer_size;
    size_t buffer_pos;
    size_t buffer_len;

    // Decoded PCM (output) buffer
    uint8_t* pcm_buffer;     // stores interleaved int16 frames
    size_t pcm_capacity;
    size_t pcm_size;         // bytes valid in pcm_buffer
    size_t pcm_pos;          // read position in pcm_buffer
};

// Forward declaration
static bool read_next_frame(FormatDecoder* decoder);

// Audio format detection
enum FormatDecoderError detect_audio_format(const uint8_t* header, size_t size, AudioFormatInfo* info) {
    if (!header || !info || size < 4) {
        return FD_ERROR_INVALID_FORMAT;
    }

    // Check for MP3 format (ID3 tag or MP3 frame header)
    if (header[0] == 'I' && header[1] == 'D' && header[2] == '3') {
        // Skip ID3 tag
        size_t skip = 10; // ID3v2 header is at least 10 bytes
        if (size >= skip) {
            // Look for MP3 frame sync word (0xFFF)
            for (size_t i = skip; i < size - 4; i++) {
                if ((header[i] & 0xFF) == 0xFF && (header[i+1] & 0xE0) == 0xE0) {
                    info->format_type = AUDIO_FORMAT_MP3;
                    return FD_ERROR_NONE;
                }
            }
        }
    } else if ((header[0] & 0xFF) == 0xFF && (header[1] & 0xE0) == 0xE0) {
        // MP3 frame sync word
        info->format_type = AUDIO_FORMAT_MP3;
        return FD_ERROR_NONE;
    }

    return FD_ERROR_INVALID_FORMAT;
}

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
    decoder->file = NULL;
    decoder->buffer = NULL;
    decoder->buffer_size = 0;
    decoder->buffer_pos = 0;
    decoder->buffer_len = 0;
    decoder->pcm_buffer = NULL;
    decoder->pcm_capacity = 0;
    decoder->pcm_size = 0;
    decoder->pcm_pos = 0;
    
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

    // Open file
    decoder->file = fopen(filepath, "rb");
    if (!decoder->file) {
        decoder->last_error = FD_ERROR_FILE_NOT_FOUND;
        return false;
    }

    // Read initial buffer for format detection
    uint8_t header_buffer[8192];  // Reasonable size for detection
    size_t bytes_read = fread(header_buffer, 1, sizeof(header_buffer), decoder->file);

    // Detect format
    decoder->last_error = detect_audio_format(header_buffer, bytes_read,
                                            &decoder->format_info);
    if (decoder->last_error != FD_ERROR_NONE) {
        fclose(decoder->file);
        decoder->file = NULL;
        return false;
    }

    // Rewind after detection so decoding starts from the beginning
    fseek(decoder->file, 0, SEEK_SET);

    // Initialize MP3 decoder
    mp3dec_init(&decoder->mp3d);

    // Allocate buffer for frame reading
    decoder->buffer_size = 8192;
    decoder->buffer = (uint8_t*)malloc(decoder->buffer_size);
    if (!decoder->buffer) {
        decoder->last_error = FD_ERROR_MEMORY;
        fclose(decoder->file);
        decoder->file = NULL;
        return false;
    }

    decoder->buffer_pos = 0;
    decoder->buffer_len = 0;
    decoder->pcm_size = 0;
    decoder->pcm_pos = 0;

    // Read first frame to get format info
    if (!read_next_frame(decoder)) {
        format_decoder_close(decoder);
        return false;
    }

    decoder->initialized = true;
    return true;
}

static bool read_next_frame(FormatDecoder* decoder) {
    if (!decoder->file || !decoder->buffer) {
        return false;
    }

    // Iterate to find the next decodable frame; avoid recursion to prevent stack overflow
    for (;;) {
        // Refill encoded buffer if exhausted
        if (decoder->buffer_pos >= decoder->buffer_len) {
            size_t bytes_read = fread(decoder->buffer, 1, decoder->buffer_size, decoder->file);
            if (bytes_read == 0) {
                return false; // End of file
            }
            decoder->buffer_pos = 0;
            decoder->buffer_len = bytes_read;
        }

        int16_t frame_buffer[MINIMP3_MAX_SAMPLES_PER_FRAME];
        int samples = mp3dec_decode_frame(&decoder->mp3d,
                                          decoder->buffer + decoder->buffer_pos,
                                          decoder->buffer_len - decoder->buffer_pos,
                                          frame_buffer,
                                          &decoder->frame_info);

        if (samples <= 0) {
            // Advance by reported frame bytes if available, else by 1 byte to search for sync
            int advance = decoder->frame_info.frame_bytes > 0 ? decoder->frame_info.frame_bytes : 1;
            decoder->buffer_pos += (size_t)advance;
            continue;
        }

        // We decoded a frame. Copy to PCM buffer (interleaved int16)
        uint32_t channels = (decoder->frame_info.channels == 0) ? 2U : (uint32_t)decoder->frame_info.channels;
        size_t bytes = (size_t)samples * (size_t)channels * sizeof(int16_t);
        if (bytes > decoder->pcm_capacity) {
            size_t new_cap = bytes;
            // round up to at least 4KB to limit realloc frequency
            if (new_cap < 4096) new_cap = 4096;
            uint8_t* newbuf = (uint8_t*)realloc(decoder->pcm_buffer, new_cap);
            if (!newbuf) {
                return false;
            }
            decoder->pcm_buffer = newbuf;
            decoder->pcm_capacity = new_cap;
        }
        memcpy(decoder->pcm_buffer, frame_buffer, bytes);
        decoder->pcm_size = bytes;
        decoder->pcm_pos = 0;

        // Advance encoded buffer position by the frame bytes consumed
        size_t consumed = (size_t)(decoder->frame_info.frame_bytes > 0 ? decoder->frame_info.frame_bytes : 1);
        decoder->buffer_pos += consumed;

        return true;
    }
}

size_t format_decoder_read(FormatDecoder* decoder, float* buffer, size_t frames) {
    if (!decoder || !decoder->initialized || !buffer) return 0;

    size_t frames_read = 0;

    while (frames_read < frames) {
        // If we have decoded PCM available, copy it
        if (decoder->pcm_pos < decoder->pcm_size) {
            uint32_t channels = decoder->frame_info.channels ? (uint32_t)decoder->frame_info.channels : 2U;
            size_t bytes_per_frame = (size_t)channels * sizeof(int16_t);
            size_t bytes_available = decoder->pcm_size - decoder->pcm_pos;
            size_t bytes_needed = (frames - frames_read) * bytes_per_frame;
            if (bytes_needed > bytes_available) {
                bytes_needed = bytes_available;
            }

            size_t samples_to_copy = bytes_needed / sizeof(int16_t);
            int16_t* int_buffer = (int16_t*)(decoder->pcm_buffer + decoder->pcm_pos);
            for (size_t i = 0; i < samples_to_copy; i++) {
                buffer[frames_read * channels + i] = int_buffer[i] / 32768.0f;
            }

            decoder->pcm_pos += bytes_needed;
            frames_read += bytes_needed / bytes_per_frame;
            if (frames_read >= frames) {
                break;
            }
        }

        // Need to decode next frame into PCM buffer
        if (!read_next_frame(decoder)) {
            break; // End of file
        }
    }

    decoder->position += frames_read;
    return frames_read;
}

void format_decoder_seek(FormatDecoder* decoder, size_t frame_position) {
    if (!decoder || !decoder->initialized) return;

    // For now, we don't support seeking - just reset to beginning
    // This could be improved with proper seeking implementation
    decoder->position = frame_position;

    // Apply seeking behavior based on configuration
    switch (decoder->config.seeking_behavior) {
        case SEEK_ACCURATE:
            // Precise seeking - just set the position
            decoder->position = frame_position;
            break;

        case SEEK_FAST:
            // Fast seeking - align to a reasonable boundary for efficiency
            {
                // For example, align to 1024-frame boundaries
                const size_t FRAME_ALIGNMENT = 1024;
                frame_position = (frame_position / FRAME_ALIGNMENT) * FRAME_ALIGNMENT;
                decoder->position = frame_position;
            }
            break;

        case SEEK_NEAREST_KEYFRAME:
            // For formats with keyframes, find the nearest one
            // This is format-specific and would need implementation for each format
            // For now, fall back to accurate seeking
            decoder->position = frame_position;
            break;
    }
}

void format_decoder_close(FormatDecoder* decoder) {
    if (!decoder) return;

    if (decoder->file) {
        fclose(decoder->file);
        decoder->file = NULL;
    }

    if (decoder->buffer) {
        free(decoder->buffer);
        decoder->buffer = NULL;
    }

    if (decoder->pcm_buffer) {
        free(decoder->pcm_buffer);
        decoder->pcm_buffer = NULL;
        decoder->pcm_capacity = 0;
        decoder->pcm_size = 0;
        decoder->pcm_pos = 0;
    }

    decoder->initialized = false;

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
    return decoder && decoder->initialized ? decoder->frame_info.channels : 0;
}

uint32_t format_decoder_get_sample_rate(const FormatDecoder* decoder) {
    return decoder && decoder->initialized ? decoder->frame_info.hz : 0;
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
