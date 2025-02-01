#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief Read audio data from the filesystem
 * 
 * @param buffer Pointer to buffer where audio data should be stored
 * @param size Number of bytes to read
 * @return size_t Number of bytes actually read
 */
size_t FileSystem_ReadAudioData(void* buffer, size_t size);

/**
 * @brief Set the current playback position in the file
 * 
 * @param position Position in bytes from start of file
 * @return bool True if successful, false otherwise
 */
bool FileSystem_Seek(size_t position);

/**
 * @brief Open an audio file for playback
 * 
 * @param filename Path to the audio file
 * @return bool True if file opened successfully, false otherwise
 */
bool FileSystem_OpenFile(const char* filename);

/**
 * @brief Close the currently open audio file
 */
void FileSystem_CloseFile(void);

#endif // FILESYSTEM_H