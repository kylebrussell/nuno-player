#ifndef FORMAT_HANDLERS_H
#define FORMAT_HANDLERS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <nuno/audio_buffer.h>

// MP3 frame header structure
typedef struct {
    uint16_t sync_word;      // 12 bits
    uint8_t version;         // 2 bits
    uint8_t layer;           // 2 bits
    uint8_t protection;      // 1 bit
    uint8_t bitrate_index;   // 4 bits
    uint8_t sampling_rate;   // 2 bits
    uint8_t padding;         // 1 bit
    uint8_t private_bit;     // 1 bit
    uint8_t channel_mode;    // 2 bits
    uint8_t mode_extension;  // 2 bits
    uint8_t copyright;       // 1 bit
    uint8_t original;        // 1 bit
    uint8_t emphasis;        // 2 bits
} MP3FrameHeader;

// VBR header structure
typedef struct {
    uint32_t frames;         // Total number of frames
    uint32_t bytes;          // Total number of bytes
    uint8_t toc[100];       // Table of contents
    uint32_t quality;        // VBR quality
} VBRHeader;

/**
 * @brief Process MP3 data in the given buffer
 * 
 * This function handles:
 * - MP3 frame detection
 * - ID3 tag parsing (v1 and v2)
 * - VBR header detection (Xing/VBRI)
 * 
 * After processing, the buffer will be adjusted to point to the first valid
 * MP3 frame, skipping any metadata.
 * 
 * @param buffer Pointer to the audio buffer containing MP3 data
 * @return true if valid MP3 data was found and processed, false otherwise
 */
bool process_mp3_data(AudioBuffer* buffer);

/**
 * @brief Parse an MP3 frame header from raw data
 * 
 * @param data Pointer to at least 4 bytes of MP3 frame data
 * @param header Pointer to header structure to fill
 * @return true if valid header was parsed, false otherwise
 */
bool parse_mp3_frame_header(const uint8_t* data, MP3FrameHeader* header);

/**
 * @brief Detect and parse VBR header information
 * 
 * Supports both Xing and VBRI header formats.
 * 
 * @param frame_data Pointer to MP3 frame containing VBR header
 * @param length Length of frame data
 * @param vbr Pointer to VBR header structure to fill
 * @return true if VBR header was found and parsed, false otherwise
 */
bool find_vbr_header(const uint8_t* frame_data, size_t length, VBRHeader* vbr);

#endif /* FORMAT_HANDLERS_H */
