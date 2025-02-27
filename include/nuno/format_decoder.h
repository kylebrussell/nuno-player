#ifndef NUNO_FORMAT_DECODER_H
#define NUNO_FORMAT_DECODER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Forward declarations
typedef struct FormatDecoder FormatDecoder;

/**
 * Error codes for format decoder operations
 */
enum FormatDecoderError {
    FD_ERROR_NONE = 0,
    FD_ERROR_INVALID_PARAM,
    FD_ERROR_FILE_NOT_FOUND,
    FD_ERROR_FILE_READ,
    FD_ERROR_INVALID_FORMAT,
    FD_ERROR_MEMORY,
    FD_ERROR_DECODE
};

/**
 * Audio format types supported by the decoder
 */
enum AudioFormatType {
    AUDIO_FORMAT_UNKNOWN = 0,
    AUDIO_FORMAT_MP3,
    AUDIO_FORMAT_FLAC,
    AUDIO_FORMAT_WAV,
    AUDIO_FORMAT_AAC,
    AUDIO_FORMAT_OGG
};

/**
 * Information about the audio format
 */
typedef struct AudioFormatInfo {
    size_t offset;          // Offset to first audio frame
    bool has_vbr;           // Whether file has variable bitrate
    uint8_t channel_mode;   // Channel configuration
    uint32_t sampling_rate; // Sample rate in Hz
    enum AudioFormatType format_type; // Type of audio format
} AudioFormatInfo;

/**
 * Buffer requirements for different audio formats
 */
typedef struct BufferRequirements {
    size_t min_buffer_size;     // Minimum buffer size in bytes
    size_t optimal_buffer_size; // Optimal buffer size for efficient decoding
    size_t max_frame_size;      // Maximum size of a single frame in bytes
    uint32_t frames_per_buffer; // Recommended number of frames per buffer
} BufferRequirements;

// Format detection function (implemented in format_handlers.c)
enum FormatDecoderError detect_audio_format(const uint8_t* data, size_t size, 
                                          AudioFormatInfo* format_info);

/**
 * Creates a new format decoder instance
 * @return Pointer to the new decoder, or NULL if creation failed
 */
FormatDecoder* format_decoder_create(void);

/**
 * Opens an audio file for decoding
 * @param decoder The decoder instance
 * @param filepath Path to the audio file
 * @return true if successful, false otherwise
 */
bool format_decoder_open(FormatDecoder* decoder, const char* filepath);

/**
 * Reads decoded audio frames
 * @param decoder The decoder instance
 * @param buffer Output buffer for decoded audio
 * @param frames Number of frames to read
 * @return Number of frames actually read
 */
size_t format_decoder_read(FormatDecoder* decoder, float* buffer, size_t frames);

/**
 * Seeks to a specific frame position
 * @param decoder The decoder instance
 * @param frame_position Target frame position
 */
void format_decoder_seek(FormatDecoder* decoder, size_t frame_position);

/**
 * Closes the currently opened audio file
 * @param decoder The decoder instance
 */
void format_decoder_close(FormatDecoder* decoder);

/**
 * Destroys the decoder instance
 * @param decoder The decoder instance
 */
void format_decoder_destroy(FormatDecoder* decoder);

/**
 * Gets the number of channels in the audio file
 * @param decoder The decoder instance
 * @return Number of channels, 0 if no file is loaded
 */
uint32_t format_decoder_get_channels(const FormatDecoder* decoder);

/**
 * Gets the sample rate of the audio file
 * @param decoder The decoder instance
 * @return Sample rate in Hz, 0 if no file is loaded
 */
uint32_t format_decoder_get_sample_rate(const FormatDecoder* decoder);

/**
 * Gets the format type of the loaded audio file
 * @param decoder The decoder instance
 * @return Format type enum value, AUDIO_FORMAT_UNKNOWN if no file is loaded
 */
enum AudioFormatType format_decoder_get_format_type(const FormatDecoder* decoder);

/**
 * Gets the buffer requirements for a specific audio format
 * @param format_type The audio format type
 * @param requirements Output parameter for buffer requirements
 * @return true if successful, false if format is not supported
 */
bool format_decoder_get_buffer_requirements(enum AudioFormatType format_type, 
                                           BufferRequirements* requirements);

/**
 * Gets the buffer requirements for the currently loaded audio file
 * @param decoder The decoder instance
 * @param requirements Output parameter for buffer requirements
 * @return true if successful, false if no file is loaded or format is not supported
 */
bool format_decoder_get_current_buffer_requirements(const FormatDecoder* decoder,
                                                  BufferRequirements* requirements);

/**
 * Gets the last error that occurred
 * @param decoder The decoder instance
 * @return The last error code
 */
enum FormatDecoderError format_decoder_get_last_error(const FormatDecoder* decoder);

/**
 * Gets a human-readable string for an error code
 * @param error The error code
 * @return String describing the error
 */
const char* format_decoder_error_string(enum FormatDecoderError error);

#endif /* NUNO_FORMAT_DECODER_H */
