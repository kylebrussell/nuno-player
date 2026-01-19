#include "nuno/format_decoder.h"
#include "minimp3.h"
#include "FLAC/stream_decoder.h"
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

    // FLAC-specific data
    FLAC__StreamDecoder* flac_decoder;
    uint32_t flac_sample_rate;
    uint32_t flac_channels;
    uint8_t flac_bits_per_sample;
    bool flac_eof;
    float* flac_buffer;      // interleaved float samples
    size_t flac_capacity;    // samples capacity
    size_t flac_samples;     // samples valid in flac_buffer
    size_t flac_pos;         // read position in samples
};

// Forward declaration
static bool read_next_frame(FormatDecoder* decoder);
static bool init_flac_decoder(FormatDecoder* decoder);
static void flac_compact_buffer(FormatDecoder* decoder);
static bool flac_ensure_capacity(FormatDecoder* decoder, size_t additional_samples);
static FLAC__StreamDecoderWriteStatus flac_write_callback(
    const FLAC__StreamDecoder* decoder,
    const FLAC__Frame* frame,
    const FLAC__int32* const buffer[],
    void* client_data);
static void flac_metadata_callback(const FLAC__StreamDecoder* decoder,
                                   const FLAC__StreamMetadata* metadata,
                                   void* client_data);
static void flac_error_callback(const FLAC__StreamDecoder* decoder,
                                FLAC__StreamDecoderErrorStatus status,
                                void* client_data);

// Audio format detection
enum FormatDecoderError detect_audio_format(const uint8_t* header, size_t size, AudioFormatInfo* info) {
    if (!header || !info || size < 4) {
        return FD_ERROR_INVALID_FORMAT;
    }

    // Check for FLAC format ("fLaC" marker)
    if (size >= 4 && memcmp(header, "fLaC", 4) == 0) {
        info->format_type = AUDIO_FORMAT_FLAC;
        return FD_ERROR_NONE;
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

    decoder->flac_decoder = NULL;
    decoder->flac_sample_rate = 0;
    decoder->flac_channels = 0;
    decoder->flac_bits_per_sample = 0;
    decoder->flac_eof = false;
    decoder->flac_buffer = NULL;
    decoder->flac_capacity = 0;
    decoder->flac_samples = 0;
    decoder->flac_pos = 0;
    
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

    switch (decoder->format_info.format_type) {
        case AUDIO_FORMAT_MP3:
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

        case AUDIO_FORMAT_FLAC:
            if (!init_flac_decoder(decoder)) {
                format_decoder_close(decoder);
                return false;
            }
            decoder->initialized = true;
            return true;

        default:
            decoder->last_error = FD_ERROR_INVALID_FORMAT;
            format_decoder_close(decoder);
            return false;
    }
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

static void flac_compact_buffer(FormatDecoder* decoder) {
    if (!decoder) {
        return;
    }
    if (decoder->flac_pos == 0 || decoder->flac_samples == 0) {
        return;
    }
    if (decoder->flac_pos >= decoder->flac_samples) {
        decoder->flac_pos = 0;
        decoder->flac_samples = 0;
        return;
    }

    size_t remaining = decoder->flac_samples - decoder->flac_pos;
    memmove(decoder->flac_buffer,
            decoder->flac_buffer + decoder->flac_pos,
            remaining * sizeof(float));
    decoder->flac_samples = remaining;
    decoder->flac_pos = 0;
}

static bool flac_ensure_capacity(FormatDecoder* decoder, size_t additional_samples) {
    if (!decoder) {
        return false;
    }

    flac_compact_buffer(decoder);

    size_t required = decoder->flac_samples + additional_samples;
    if (required <= decoder->flac_capacity) {
        return true;
    }

    size_t new_cap = required;
    if (new_cap < 4096) {
        new_cap = 4096;
    }

    float* newbuf = (float*)realloc(decoder->flac_buffer, new_cap * sizeof(float));
    if (!newbuf) {
        return false;
    }
    decoder->flac_buffer = newbuf;
    decoder->flac_capacity = new_cap;
    return true;
}

static FLAC__StreamDecoderWriteStatus flac_write_callback(
    const FLAC__StreamDecoder* flac_decoder,
    const FLAC__Frame* frame,
    const FLAC__int32* const buffer[],
    void* client_data) {
    (void)flac_decoder;
    if (!frame || !buffer || !client_data) {
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }

    FormatDecoder* decoder = (FormatDecoder*)client_data;
    uint32_t channels = frame->header.channels;
    uint32_t blocksize = frame->header.blocksize;
    if (channels == 0 || blocksize == 0) {
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }

    if (decoder->flac_channels == 0) {
        decoder->flac_channels = channels;
    }
    uint8_t bits = frame->header.bits_per_sample;
    if (bits == 0) {
        bits = decoder->flac_bits_per_sample;
    }
    if (bits == 0 || bits > 32) {
        bits = 16;
    }

    size_t frame_samples = (size_t)blocksize * (size_t)channels;
    if (!flac_ensure_capacity(decoder, frame_samples)) {
        decoder->last_error = FD_ERROR_MEMORY;
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }

    double scale = (bits <= 31) ? (double)(1u << (bits - 1)) : 2147483648.0;
    float* dest = decoder->flac_buffer + decoder->flac_samples;

    for (uint32_t i = 0; i < blocksize; i++) {
        for (uint32_t ch = 0; ch < channels; ch++) {
            float sample = (float)((double)buffer[ch][i] / scale);
            if (sample > 1.0f) {
                sample = 1.0f;
            } else if (sample < -1.0f) {
                sample = -1.0f;
            }
            *dest++ = sample;
        }
    }

    decoder->flac_samples += frame_samples;
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void flac_metadata_callback(const FLAC__StreamDecoder* flac_decoder,
                                   const FLAC__StreamMetadata* metadata,
                                   void* client_data) {
    (void)flac_decoder;
    if (!metadata || !client_data) {
        return;
    }

    FormatDecoder* decoder = (FormatDecoder*)client_data;
    if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
        decoder->flac_sample_rate = metadata->data.stream_info.sample_rate;
        decoder->flac_channels = metadata->data.stream_info.channels;
        decoder->flac_bits_per_sample = metadata->data.stream_info.bits_per_sample;
        decoder->format_info.sampling_rate = decoder->flac_sample_rate;
    }
}

static void flac_error_callback(const FLAC__StreamDecoder* flac_decoder,
                                FLAC__StreamDecoderErrorStatus status,
                                void* client_data) {
    (void)flac_decoder;
    (void)status;
    if (!client_data) {
        return;
    }
    FormatDecoder* decoder = (FormatDecoder*)client_data;
    decoder->last_error = FD_ERROR_DECODE;
}

static bool init_flac_decoder(FormatDecoder* decoder) {
    if (!decoder || !decoder->file) {
        return false;
    }

    decoder->flac_decoder = FLAC__stream_decoder_new();
    if (!decoder->flac_decoder) {
        decoder->last_error = FD_ERROR_MEMORY;
        return false;
    }

    FLAC__stream_decoder_set_metadata_respond(decoder->flac_decoder, FLAC__METADATA_TYPE_STREAMINFO);

    FLAC__StreamDecoderInitStatus status = FLAC__stream_decoder_init_FILE(
        decoder->flac_decoder,
        decoder->file,
        flac_write_callback,
        flac_metadata_callback,
        flac_error_callback,
        decoder);
    if (status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
        decoder->last_error = FD_ERROR_INVALID_FORMAT;
        return false;
    }

    if (!FLAC__stream_decoder_process_until_end_of_metadata(decoder->flac_decoder)) {
        decoder->last_error = FD_ERROR_DECODE;
        return false;
    }

    if (decoder->flac_channels == 0 || decoder->flac_sample_rate == 0) {
        decoder->last_error = FD_ERROR_INVALID_FORMAT;
        return false;
    }

    if (decoder->flac_sample_rate != 44100U && decoder->flac_sample_rate != 48000U) {
        decoder->last_error = FD_ERROR_INVALID_PARAM;
        return false;
    }

    decoder->flac_eof = false;
    decoder->flac_samples = 0;
    decoder->flac_pos = 0;
    return true;
}

size_t format_decoder_read(FormatDecoder* decoder, float* buffer, size_t frames) {
    if (!decoder || !decoder->initialized || !buffer) return 0;

    size_t frames_read = 0;

    switch (decoder->format_info.format_type) {
        case AUDIO_FORMAT_MP3:
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
            break;

        case AUDIO_FORMAT_FLAC: {
            uint32_t channels = decoder->flac_channels ? decoder->flac_channels : 2U;
            while (frames_read < frames) {
                size_t samples_available = (decoder->flac_samples > decoder->flac_pos)
                    ? (decoder->flac_samples - decoder->flac_pos)
                    : 0U;
                size_t frames_available = samples_available / channels;
                if (frames_available > 0) {
                    size_t frames_to_copy = frames - frames_read;
                    if (frames_to_copy > frames_available) {
                        frames_to_copy = frames_available;
                    }
                    size_t samples_to_copy = frames_to_copy * channels;
                    memcpy(&buffer[frames_read * channels],
                           &decoder->flac_buffer[decoder->flac_pos],
                           samples_to_copy * sizeof(float));
                    decoder->flac_pos += samples_to_copy;
                    frames_read += frames_to_copy;
                    if (decoder->flac_pos >= decoder->flac_samples) {
                        decoder->flac_pos = 0;
                        decoder->flac_samples = 0;
                    }
                    continue;
                }

                if (decoder->flac_eof) {
                    break;
                }

                if (!FLAC__stream_decoder_process_single(decoder->flac_decoder)) {
                    decoder->last_error = FD_ERROR_DECODE;
                    break;
                }

                if (FLAC__stream_decoder_get_state(decoder->flac_decoder) ==
                    FLAC__STREAM_DECODER_END_OF_STREAM) {
                    decoder->flac_eof = true;
                }
            }
            break;
        }

        default:
            break;
    }

    decoder->position += frames_read;
    return frames_read;
}

void format_decoder_seek(FormatDecoder* decoder, size_t frame_position) {
    if (!decoder || !decoder->initialized) return;

    decoder->last_error = FD_ERROR_NONE;

    // For now, we don't support seeking - just reset to beginning
    // This could be improved with proper seeking implementation
    size_t target_position = frame_position;

    // Apply seeking behavior based on configuration
    switch (decoder->config.seeking_behavior) {
        case SEEK_ACCURATE:
            target_position = frame_position;
            break;

        case SEEK_FAST: {
            // Fast seeking - align to a reasonable boundary for efficiency
            const size_t FRAME_ALIGNMENT = 1024;
            target_position = (frame_position / FRAME_ALIGNMENT) * FRAME_ALIGNMENT;
            break;
        }

        case SEEK_NEAREST_KEYFRAME:
            // For formats with keyframes, find the nearest one
            // This is format-specific and would need implementation for each format
            // For now, fall back to accurate seeking
            target_position = frame_position;
            break;
    }

    decoder->position = target_position;

    if (decoder->format_info.format_type == AUDIO_FORMAT_FLAC && decoder->flac_decoder) {
        decoder->flac_samples = 0;
        decoder->flac_pos = 0;
        decoder->flac_eof = false;
        if (!FLAC__stream_decoder_seek_absolute(decoder->flac_decoder, (FLAC__uint64)target_position)) {
            decoder->last_error = FD_ERROR_DECODE;
        }
    }
}

void format_decoder_close(FormatDecoder* decoder) {
    if (!decoder) return;

    if (decoder->flac_decoder) {
        if (FLAC__stream_decoder_get_state(decoder->flac_decoder) !=
            FLAC__STREAM_DECODER_UNINITIALIZED) {
            (void)FLAC__stream_decoder_finish(decoder->flac_decoder);
        }
        FLAC__stream_decoder_delete(decoder->flac_decoder);
        decoder->flac_decoder = NULL;
    }

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

    if (decoder->flac_buffer) {
        free(decoder->flac_buffer);
        decoder->flac_buffer = NULL;
        decoder->flac_capacity = 0;
        decoder->flac_samples = 0;
        decoder->flac_pos = 0;
    }
    decoder->flac_sample_rate = 0;
    decoder->flac_channels = 0;
    decoder->flac_bits_per_sample = 0;
    decoder->flac_eof = false;

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
    if (!decoder || !decoder->initialized) {
        return 0;
    }
    switch (decoder->format_info.format_type) {
        case AUDIO_FORMAT_FLAC:
            return decoder->flac_channels;
        case AUDIO_FORMAT_MP3:
        default:
            return decoder->frame_info.channels;
    }
}

uint32_t format_decoder_get_sample_rate(const FormatDecoder* decoder) {
    if (!decoder || !decoder->initialized) {
        return 0;
    }
    switch (decoder->format_info.format_type) {
        case AUDIO_FORMAT_FLAC:
            return decoder->flac_sample_rate;
        case AUDIO_FORMAT_MP3:
        default:
            return decoder->frame_info.hz;
    }
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
