#ifndef NUNO_FORMAT_DECODER_H
#define NUNO_FORMAT_DECODER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Forward declarations
typedef struct FormatDecoder FormatDecoder;
typedef struct AudioFormatInfo AudioFormatInfo;

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
