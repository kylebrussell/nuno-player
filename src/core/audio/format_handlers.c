#include <nuno/audio_buffer.h>
#include <nuno/audio_pipeline.h>
#include <nuno/format_decoder.h>
#include <string.h>

#define MP3_SYNC_WORD 0xFFF
#define ID3V2_HEADER_SIZE 10
#define ID3V1_TAG_SIZE 128

// FLAC constants
#define FLAC_SYNC_CODE 0xFFF8
#define FLAC_METADATA_BLOCK_STREAMINFO 0
#define FLAC_METADATA_BLOCK_SEEKTABLE 3
#define FLAC_LAST_METADATA_BLOCK_FLAG 0x80

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

// ID3v2 tag header structure
typedef struct {
    char identifier[3];
    uint8_t version[2];
    uint8_t flags;
    uint32_t size;
} ID3v2Header;

// VBR header structure (Xing/VBRI)
typedef struct {
    uint32_t frames;         // Total number of frames
    uint32_t bytes;          // Total number of bytes
    uint8_t toc[100];       // Table of contents
    uint32_t quality;        // VBR quality
} VBRHeader;

static bool parse_mp3_frame_header(const uint8_t* data, MP3FrameHeader* header) {
    if (!data || !header) return false;

    uint32_t header_raw = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    
    header->sync_word = (header_raw >> 20) & 0xFFF;
    if (header->sync_word != MP3_SYNC_WORD) return false;

    header->version = (header_raw >> 19) & 0x3;
    header->layer = (header_raw >> 17) & 0x3;
    header->protection = (header_raw >> 16) & 0x1;
    header->bitrate_index = (header_raw >> 12) & 0xF;
    header->sampling_rate = (header_raw >> 10) & 0x3;
    header->padding = (header_raw >> 9) & 0x1;
    header->private_bit = (header_raw >> 8) & 0x1;
    header->channel_mode = (header_raw >> 6) & 0x3;
    header->mode_extension = (header_raw >> 4) & 0x3;
    header->copyright = (header_raw >> 3) & 0x1;
    header->original = (header_raw >> 2) & 0x1;
    header->emphasis = header_raw & 0x3;

    return true;
}

static bool parse_id3v2_header(const uint8_t* data, ID3v2Header* header) {
    if (!data || !header) return false;

    memcpy(header->identifier, data, 3);
    if (memcmp(header->identifier, "ID3", 3) != 0) return false;

    header->version[0] = data[3];
    header->version[1] = data[4];
    header->flags = data[5];
    
    // ID3v2 size is stored as synchsafe integers
    header->size = ((data[6] & 0x7F) << 21) |
                  ((data[7] & 0x7F) << 14) |
                  ((data[8] & 0x7F) << 7) |
                  (data[9] & 0x7F);

    return true;
}

static bool find_vbr_header(const uint8_t* frame_data, size_t length, VBRHeader* vbr) {
    if (!frame_data || !vbr || length < 16) return false;

    // Search for Xing or VBRI header
    const char* xing_id = "Xing";
    const char* info_id = "Info";
    const char* vbri_id = "VBRI";

    // Skip past frame header
    const uint8_t* search_start = frame_data + 4;
    size_t search_length = length - 4;

    // Look for Xing/Info header
    if (memcmp(search_start, xing_id, 4) == 0 || memcmp(search_start, info_id, 4) == 0) {
        uint32_t flags = (search_start[4] << 24) | (search_start[5] << 16) |
                        (search_start[6] << 8) | search_start[7];
        
        const uint8_t* ptr = search_start + 8;
        
        if (flags & 0x1) {  // Frames field present
            vbr->frames = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];
            ptr += 4;
        }
        if (flags & 0x2) {  // Bytes field present
            vbr->bytes = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];
            ptr += 4;
        }
        if (flags & 0x4) {  // TOC present
            memcpy(vbr->toc, ptr, 100);
            ptr += 100;
        }
        if (flags & 0x8) {  // Quality indicator present
            vbr->quality = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];
        }
        return true;
    }
    
    // Look for VBRI header
    if (memcmp(search_start + 32, vbri_id, 4) == 0) {
        const uint8_t* ptr = search_start + 32 + 4;
        vbr->frames = (ptr[14] << 24) | (ptr[15] << 16) | (ptr[16] << 8) | ptr[17];
        vbr->bytes = (ptr[10] << 24) | (ptr[11] << 16) | (ptr[12] << 8) | ptr[13];
        return true;
    }

    return false;
}

/**
 * @brief Detects and validates audio format metadata
 * @param data Raw audio file data
 * @param size Size of the data buffer
 * @param format_info Output parameter for format information
 * @return Format detection error code
 */
enum FormatDecoderError detect_audio_format(const uint8_t* data, size_t size, 
                                          AudioFormatInfo* format_info) {
    if (!data || !format_info || size < ID3V2_HEADER_SIZE) {
        return FD_ERROR_INVALID_PARAM;
    }

    size_t data_offset = 0;
    ID3v2Header id3v2;

    // Check for ID3v2 tag
    if (parse_id3v2_header(data, &id3v2)) {
        data_offset = ID3V2_HEADER_SIZE + id3v2.size;
    }

    // Find first valid MP3 frame
    MP3FrameHeader frame_header;
    VBRHeader vbr_header;
    bool frame_found = false;
    
    while (data_offset + 4 <= size) {
        if (parse_mp3_frame_header(data + data_offset, &frame_header)) {
            frame_found = true;
            
            // Store format information
            format_info->offset = data_offset;
            format_info->has_vbr = find_vbr_header(data + data_offset, 
                                                  size - data_offset, 
                                                  &vbr_header);
            format_info->channel_mode = frame_header.channel_mode;
            format_info->sampling_rate = get_mp3_sample_rate(frame_header.sampling_rate);
            break;
        }
        data_offset++;
    }

    return frame_found ? FD_ERROR_NONE : FD_ERROR_INVALID_FORMAT;
}

// FLAC metadata parsing functions
static bool parse_flac_streaminfo(const uint8_t* data, size_t length, AudioFormatInfo* info) {
    if (length < 34) {
        return false;  // STREAMINFO block must be at least 34 bytes
    }
    
    // Extract sample rate (20 bits)
    uint32_t sample_rate = (data[10] << 12) | (data[11] << 4) | (data[12] >> 4);
    
    // Extract number of channels (3 bits)
    uint8_t channels = ((data[12] >> 1) & 0x07) + 1;
    
    // Extract bits per sample (5 bits)
    uint8_t bits_per_sample = ((data[12] & 0x01) << 4) | (data[13] >> 4);
    bits_per_sample += 1;
    
    // Extract total samples (36 bits)
    uint64_t total_samples = 
        ((uint64_t)data[13] & 0x0F) << 32 |
        (uint64_t)data[14] << 24 |
        (uint64_t)data[15] << 16 |
        (uint64_t)data[16] << 8 |
        (uint64_t)data[17];
    
    // Populate the AudioFormatInfo structure
    info->sample_rate = sample_rate;
    info->channels = channels;
    info->bits_per_sample = bits_per_sample;
    info->total_samples = total_samples;
    info->format_type = AUDIO_FORMAT_FLAC;
    
    return true;
}

static bool parse_flac_seektable(const uint8_t* data, size_t length, AudioFormatInfo* info) {
    if (length < 18) {
        return false;  // Each seek point is at least 18 bytes
    }
    
    // Number of seek points
    size_t num_seek_points = length / 18;
    
    // Allocate memory for seek points if needed
    // This is just a basic implementation - you might want to store this differently
    if (info->seek_table == NULL && num_seek_points > 0) {
        info->seek_table = malloc(sizeof(SeekPoint) * num_seek_points);
        info->seek_points_count = num_seek_points;
    }
    
    // Parse each seek point
    for (size_t i = 0; i < num_seek_points; i++) {
        const uint8_t* point_data = data + (i * 18);
        
        // Sample number (64 bits)
        uint64_t sample_number = 
            ((uint64_t)point_data[0] << 56) |
            ((uint64_t)point_data[1] << 48) |
            ((uint64_t)point_data[2] << 40) |
            ((uint64_t)point_data[3] << 32) |
            ((uint64_t)point_data[4] << 24) |
            ((uint64_t)point_data[5] << 16) |
            ((uint64_t)point_data[6] << 8) |
            ((uint64_t)point_data[7]);
        
        // Stream offset (64 bits)
        uint64_t stream_offset = 
            ((uint64_t)point_data[8] << 56) |
            ((uint64_t)point_data[9] << 48) |
            ((uint64_t)point_data[10] << 40) |
            ((uint64_t)point_data[11] << 32) |
            ((uint64_t)point_data[12] << 24) |
            ((uint64_t)point_data[13] << 16) |
            ((uint64_t)point_data[14] << 8) |
            ((uint64_t)point_data[15]);
        
        // Frame samples (16 bits)
        uint16_t frame_samples = 
            (point_data[16] << 8) |
            point_data[17];
        
        // Skip placeholder points (all bits set to 1)
        if (sample_number == 0xFFFFFFFFFFFFFFFF) {
            continue;
        }
        
        // Store the seek point
        if (info->seek_table != NULL) {
            info->seek_table[i].sample_number = sample_number;
            info->seek_table[i].stream_offset = stream_offset;
            info->seek_table[i].frame_samples = frame_samples;
        }
    }
    
    return true;
}

bool detect_flac_format(const uint8_t* data, size_t length, AudioFormatInfo* info) {
    // Check for minimum FLAC header size (4 bytes for "fLaC" marker + 4 bytes for STREAMINFO metadata block header)
    if (length < 8) {
        return false;
    }
    
    // Check for FLAC marker "fLaC"
    if (data[0] != 'f' || data[1] != 'L' || data[2] != 'a' || data[3] != 'C') {
        return false;
    }
    
    // Initialize the info structure
    memset(info, 0, sizeof(AudioFormatInfo));
    
    // Parse metadata blocks
    size_t offset = 4;  // Skip the "fLaC" marker
    bool last_block = false;
    bool found_streaminfo = false;
    
    while (!last_block && offset + 4 <= length) {
        // Read block header
        uint8_t block_type = data[offset] & 0x7F;
        last_block = (data[offset] & FLAC_LAST_METADATA_BLOCK_FLAG) != 0;
        
        // Get block length (24 bits)
        uint32_t block_length = 
            (data[offset + 1] << 16) |
            (data[offset + 2] << 8) |
            data[offset + 3];
        
        // Move past the block header
        offset += 4;
        
        // Check if we have enough data for this block
        if (offset + block_length > length) {
            break;
        }
        
        // Process the block based on its type
        switch (block_type) {
            case FLAC_METADATA_BLOCK_STREAMINFO:
                if (parse_flac_streaminfo(data + offset, block_length, info)) {
                    found_streaminfo = true;
                }
                break;
                
            case FLAC_METADATA_BLOCK_SEEKTABLE:
                parse_flac_seektable(data + offset, block_length, info);
                break;
                
            default:
                // Skip other block types
                break;
        }
        
        // Move to the next block
        offset += block_length;
    }
    
    // FLAC format is valid only if we found a STREAMINFO block
    return found_streaminfo;
}

bool detect_flac_sync(const uint8_t* data, size_t length) {
    // FLAC sync code is 14 bits (0x3FFE) followed by 2 reserved bits
    // We need at least 2 bytes to check for the sync code
    if (length < 2) {
        return false;
    }
    
    // Check for FLAC sync code (0xFFF8) in the first 14 bits
    uint16_t sync_code = ((data[0] << 8) | data[1]) & 0xFFFE;
    return (sync_code == FLAC_SYNC_CODE);
}

// Update the detect_format function to include FLAC detection
AudioFormatType detect_format(const uint8_t* data, size_t length, AudioFormatInfo* info) {
    // ... existing code ...
    
    // Try to detect FLAC format
    if (detect_flac_format(data, length, info)) {
        return AUDIO_FORMAT_FLAC;
    }
    
    // ... existing code ...
    
    return AUDIO_FORMAT_UNKNOWN;
}
