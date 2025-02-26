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

/* FLAC-specific structures */

/* FLAC StreamInfo structure - contains metadata about the entire stream */
typedef struct {
    uint16_t min_block_size;        /* Minimum block size (in samples) used in the stream */
    uint16_t max_block_size;        /* Maximum block size (in samples) used in the stream */
    uint32_t min_frame_size;        /* Minimum frame size (in bytes) used in the stream */
    uint32_t max_frame_size;        /* Maximum frame size (in bytes) used in the stream */
    uint32_t sample_rate;           /* Sample rate in Hz */
    uint8_t  num_channels;          /* Number of channels */
    uint8_t  bits_per_sample;       /* Bits per sample */
    uint64_t total_samples;         /* Total samples in stream (0 if unknown) */
    uint8_t  md5_signature[16];     /* MD5 signature of the unencoded audio data */
} FLAC_StreamInfo;

/* FLAC Frame Header structure - contains info about each audio frame */
typedef struct {
    uint8_t  blocking_strategy;     /* 0: fixed-blocksize, 1: variable-blocksize */
    uint16_t block_size;            /* Number of samples in frame */
    uint32_t sample_rate;           /* Sample rate in Hz (0 if same as stream) */
    uint8_t  channel_assignment;    /* Channel assignment */
    uint8_t  sample_size;           /* Sample size in bits */
    uint64_t frame_number;          /* Frame number (if fixed blocksize) or sample number */
    uint8_t  crc8;                  /* CRC-8 of the frame header */
} FLAC_FrameHeader;

/* FLAC Subframe types */
typedef enum {
    FLAC_SUBFRAME_CONSTANT = 0,
    FLAC_SUBFRAME_VERBATIM = 1,
    FLAC_SUBFRAME_FIXED = 2,
    FLAC_SUBFRAME_LPC = 3
} FLAC_SubframeType;

/* FLAC Subframe structure - contains encoded audio data for a channel */
typedef struct {
    FLAC_SubframeType type;         /* Subframe encoding type */
    uint8_t  wasted_bits;           /* Number of wasted bits per sample */
    uint8_t  order;                 /* Prediction order (for FIXED and LPC) */
    
    union {
        int32_t constant_value;     /* For CONSTANT subframes */
        
        struct {                    /* For LPC subframes */
            uint8_t precision;      /* Quantized linear predictor coefficient precision */
            int8_t  shift;          /* Quantized linear predictor coefficient shift */
            int32_t *coefficients;  /* Quantized linear predictor coefficients */
        } lpc;
        
        /* For VERBATIM, we just point to the raw samples */
        int32_t *verbatim_samples;
        
        /* For FIXED, we use the residual directly */
    };
    
    /* Residual coding parameters */
    struct {
        uint8_t  coding_method;     /* Rice coding (0) or Rice2 coding (1) */
        uint8_t  partition_order;   /* 2^order partitions */
        uint8_t  *parameters;       /* Rice/Rice2 parameters for each partition */
        int32_t  *residual;         /* Residual values */
    } residual;
    
} FLAC_Subframe;

/* FLAC SeekPoint structure - represents a single point in the seek table */
typedef struct {
    uint64_t sample_number;         /* Sample number of target frame */
    uint64_t stream_offset;         /* Offset (in bytes) from first frame */
    uint16_t frame_samples;         /* Number of samples in target frame */
} FLAC_SeekPoint;

/* FLAC SeekTable structure - contains seek points for navigation */
typedef struct {
    uint32_t     num_seek_points;   /* Number of seek points */
    FLAC_SeekPoint *points;         /* Array of seek points */
} FLAC_SeekTable;

/* FLAC Constants */

/* FLAC Metadata Block Types */
#define FLAC_METADATA_STREAMINFO     0
#define FLAC_METADATA_PADDING        1
#define FLAC_METADATA_APPLICATION    2
#define FLAC_METADATA_SEEKTABLE      3
#define FLAC_METADATA_VORBIS_COMMENT 4
#define FLAC_METADATA_CUESHEET       5
#define FLAC_METADATA_PICTURE        6

/* FLAC Frame Sync Code - 14 bits */
#define FLAC_FRAME_SYNC_CODE         0x3FFE

/* FLAC Sample Size Constants */
#define FLAC_SAMPLE_SIZE_UNKNOWN     0
#define FLAC_SAMPLE_SIZE_8_BITS      1
#define FLAC_SAMPLE_SIZE_12_BITS     2
#define FLAC_SAMPLE_SIZE_16_BITS     4
#define FLAC_SAMPLE_SIZE_20_BITS     5
#define FLAC_SAMPLE_SIZE_24_BITS     6
#define FLAC_SAMPLE_SIZE_32_BITS     7

/* FLAC Channel Assignment */
#define FLAC_CHANNEL_INDEPENDENT     0  /* Independent channels */
#define FLAC_CHANNEL_LEFT_SIDE       8  /* Left/side stereo */
#define FLAC_CHANNEL_RIGHT_SIDE      9  /* Right/side stereo */
#define FLAC_CHANNEL_MID_SIDE        10 /* Mid/side stereo */

/* FLAC Block Size Special Values */
#define FLAC_BLOCKSIZE_RESERVED      0
#define FLAC_BLOCKSIZE_192           1
#define FLAC_BLOCKSIZE_576           2
#define FLAC_BLOCKSIZE_1152          3
#define FLAC_BLOCKSIZE_2304          4
#define FLAC_BLOCKSIZE_4608          5
#define FLAC_BLOCKSIZE_GET_FROM_NEXT_BYTE 6
#define FLAC_BLOCKSIZE_GET_FROM_NEXT_2BYTES 7

/* FLAC Sample Rate Special Values */
#define FLAC_SAMPLERATE_STREAMINFO   0
#define FLAC_SAMPLERATE_88200        1
#define FLAC_SAMPLERATE_176400       2
#define FLAC_SAMPLERATE_192000       3
#define FLAC_SAMPLERATE_8000         4
#define FLAC_SAMPLERATE_16000        5
#define FLAC_SAMPLERATE_22050        6
#define FLAC_SAMPLERATE_24000        7
#define FLAC_SAMPLERATE_32000        8
#define FLAC_SAMPLERATE_44100        9
#define FLAC_SAMPLERATE_48000        10
#define FLAC_SAMPLERATE_96000        11
#define FLAC_SAMPLERATE_GET_FROM_NEXT_BYTE 12
#define FLAC_SAMPLERATE_GET_FROM_NEXT_2BYTES 13
#define FLAC_SAMPLERATE_GET_FROM_NEXT_3BYTES 14
#define FLAC_SAMPLERATE_INVALID      15

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

/**
 * @brief Process FLAC data in the given buffer
 * 
 * This function handles:
 * - FLAC signature verification
 * - Metadata block parsing
 * - First frame location
 * 
 * After processing, the buffer will be adjusted to point to the first valid
 * FLAC frame, skipping any metadata.
 * 
 * @param buffer Pointer to the audio buffer containing FLAC data
 * @return true if valid FLAC data was found and processed, false otherwise
 */
bool process_flac_data(AudioBuffer* buffer);

/**
 * @brief Parse FLAC metadata blocks
 * 
 * Parses all metadata blocks in a FLAC file, including STREAMINFO,
 * SEEKTABLE, VORBIS_COMMENT, etc.
 * 
 * @param data Pointer to the start of FLAC metadata
 * @param length Length of available data
 * @param stream_info Pointer to stream info structure to fill
 * @param seek_table Pointer to seek table structure to fill (can be NULL)
 * @return Size of all metadata blocks if successful, 0 if error
 */
size_t parse_flac_metadata(const uint8_t* data, size_t length, 
                          FLAC_StreamInfo* stream_info, 
                          FLAC_SeekTable* seek_table);

/**
 * @brief Parse a single FLAC metadata block
 * 
 * @param data Pointer to the start of the metadata block
 * @param length Length of available data
 * @param block_type Pointer to variable to store the block type
 * @param is_last Pointer to variable to store if this is the last metadata block
 * @return Size of the metadata block if successful, 0 if error
 */
size_t parse_flac_metadata_block(const uint8_t* data, size_t length,
                                uint8_t* block_type, bool* is_last);

/**
 * @brief Parse FLAC STREAMINFO metadata block
 * 
 * @param data Pointer to the start of the STREAMINFO block data
 * @param length Length of available data
 * @param stream_info Pointer to stream info structure to fill
 * @return true if successful, false if error
 */
bool parse_flac_streaminfo(const uint8_t* data, size_t length,
                          FLAC_StreamInfo* stream_info);

/**
 * @brief Parse FLAC SEEKTABLE metadata block
 * 
 * @param data Pointer to the start of the SEEKTABLE block data
 * @param length Length of available data
 * @param seek_table Pointer to seek table structure to fill
 * @return true if successful, false if error
 */
bool parse_flac_seektable(const uint8_t* data, size_t length,
                         FLAC_SeekTable* seek_table);

/**
 * @brief Parse a FLAC frame header
 * 
 * @param data Pointer to the start of the frame
 * @param length Length of available data
 * @param header Pointer to frame header structure to fill
 * @param stream_info Pointer to stream info for reference values
 * @return Size of the frame header if successful, 0 if error
 */
size_t parse_flac_frame_header(const uint8_t* data, size_t length,
                              FLAC_FrameHeader* header,
                              const FLAC_StreamInfo* stream_info);

/**
 * @brief Parse a FLAC subframe
 * 
 * @param data Pointer to the start of the subframe
 * @param length Length of available data
 * @param subframe Pointer to subframe structure to fill
 * @param bits_per_sample Bits per sample for this subframe
 * @param block_size Number of samples in this subframe
 * @return Size of the subframe if successful, 0 if error
 */
size_t parse_flac_subframe(const uint8_t* data, size_t length,
                          FLAC_Subframe* subframe,
                          uint8_t bits_per_sample,
                          uint16_t block_size);

/**
 * @brief Parse FLAC residual coding
 * 
 * @param data Pointer to the start of the residual data
 * @param length Length of available data
 * @param residual Pointer to the residual structure in the subframe
 * @param order Prediction order
 * @param block_size Number of samples in the block
 * @return Size of the residual data if successful, 0 if error
 */
size_t parse_flac_residual(const uint8_t* data, size_t length,
                          struct FLAC_Subframe::residual* residual,
                          uint8_t order,
                          uint16_t block_size);

/**
 * @brief Calculate CRC-8 for FLAC frame header
 * 
 * @param data Pointer to the data
 * @param length Length of data to calculate CRC for
 * @return The calculated CRC-8 value
 */
uint8_t flac_crc8(const uint8_t* data, size_t length);

/**
 * @brief Calculate CRC-16 for FLAC frame
 * 
 * @param data Pointer to the data
 * @param length Length of data to calculate CRC for
 * @return The calculated CRC-16 value
 */
uint16_t flac_crc16(const uint8_t* data, size_t length);

/**
 * @brief Decode a FLAC frame into PCM samples
 * 
 * @param frame_data Pointer to the FLAC frame data
 * @param frame_length Length of the frame data
 * @param buffer Pointer to the audio buffer to fill with decoded samples
 * @param stream_info Pointer to stream info for reference values
 * @return Number of samples decoded if successful, 0 if error
 */
uint32_t decode_flac_frame(const uint8_t* frame_data, size_t frame_length,
                          AudioBuffer* buffer,
                          const FLAC_StreamInfo* stream_info);

/**
 * @brief Find and validate a FLAC frame
 * @param data Raw audio file data
 * @param size Size of the data buffer
 * @param offset Starting offset to search from
 * @param frame_info Output parameter for frame information
 * @return Offset of the next frame, or 0 if no valid frame found
 */
size_t find_flac_frame(const uint8_t* data, size_t size, size_t offset, 
                      FLAC_FrameHeader* frame_info);

/**
 * @brief Validate a complete FLAC frame including CRC-16
 * @param data Pointer to the frame data
 * @param length Total length of the frame including CRC-16
 * @return true if the frame is valid, false otherwise
 */
bool validate_flac_frame(const uint8_t* data, size_t length);

#endif /* FORMAT_HANDLERS_H */
