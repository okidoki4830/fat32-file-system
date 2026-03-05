#ifndef FAT32_H
#define FAT32_H
// File opening mode flags
#define O_RDONLY 1  // Read only
#define O_WRONLY 2  // Write only
#define O_RDWR 3    // Read and write

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#define MAX_DIR_DEPTH 32 // assuming directory depth wont
			 // surpass 32

// FAT32 Boot Sector structure
typedef struct __attribute__((packed))
{
    uint8_t jump_boot[3];           // Jump instruction to boot code
    char oem_name[8];               // OEM Name in ASCII
    uint16_t bytes_per_sector;      // Bytes per sector
    uint8_t sectors_per_cluster;    // Sectors per cluster
    uint16_t reserved_sector_count; // Reserved sector count
    uint8_t num_fats;               // Number of FATs
    uint16_t root_entry_count;      // Root entry count (unused in FAT32)
    uint16_t total_sectors_16;      // Total sectors (unused in FAT32)
    uint8_t media_type;             // Media type
    uint16_t fat_size_16;           // FAT size in sectors (unused in FAT32)
    uint16_t sectors_per_track;     // Sectors per track
    uint16_t number_of_heads;       // Number of heads
    uint32_t hidden_sectors;        // Hidden sectors
    uint32_t total_sectors_32;      // Total sectors (used in FAT32)
    
    // FAT32 Structure
    uint32_t fat_size_32;           // FAT size in sectors
    uint16_t ext_flags;             // Extension flags
    uint16_t fs_version;            // File system version
    uint32_t root_cluster;          // First cluster of root directory
    uint16_t fs_info;               // Sector number of FSINFO structure
    uint16_t backup_boot_sector;    // Backup boot sector
    uint8_t reserved[12];           // Reserved for future expansion
    uint8_t drive_number;           // Drive number
    uint8_t reserved1;              // Reserved
    uint8_t boot_signature;         // Boot signature
    uint32_t volume_id;             // Volume ID
    char volume_label[11];          // Volume label
    char fs_type[8];                // File system type
    
    // Additional computed fields (not in the actual boot sector)
    uint32_t first_data_sector;     // First data sector
    uint32_t total_clusters;        // Total number of clusters in data region
    uint64_t volume_size;           // Total volume size in bytes
} BootSector;

// FAT32 Directory Entry structure
typedef struct __attribute__((packed))
{
    uint8_t name[11];               // 8.3 format name
    uint8_t attributes;             // File attributes
    uint8_t reserved;               // Reserved
    uint8_t creation_time_ms;       // Creation time (milliseconds)
    uint16_t creation_time;         // Creation time
    uint16_t creation_date;         // Creation date
    uint16_t last_access_date;      // Last access date
    uint16_t first_cluster_high;    // High 16 bits of first cluster
    uint16_t last_modified_time;    // Last modified time
    uint16_t last_modified_date;    // Last modified date
    uint16_t first_cluster_low;     // Low 16 bits of first cluster
    uint32_t file_size;             // File size
} DirectoryEntry;

// FAT32 context structure
typedef struct
{
    FILE *fp;                       // File pointer to the FAT32 image
    char *image_name;               // Name of the image file
    char current_path[256];         // Current working directory path
    BootSector boot_sector;         // Boot sector information
    uint32_t current_cluster;       // Current working directory cluster
    uint32_t cluster_stack[MAX_DIR_DEPTH]; // stack to keep track of directory path
    int stack_top;
} FAT32Context;

// Function prototypes - only Part 1 functions
int mount_fat32(FAT32Context *context, const char *image_path);
void display_info(FAT32Context *context);
void cleanup(FAT32Context *context);

// Part 2 added functions:
uint32_t cluster_to_sector(FAT32Context *context, uint32_t cluster);
uint32_t get_next_cluster(FAT32Context *context, uint32_t cluster);
void format_name(const char *input, char *output);
void get_display_name(const uint8_t *entry, char *output);

// Utility function to find a file in the current directory
DirectoryEntry* find_file_in_current_directory(FAT32Context *context, const char *name);

// Structure to store information about an open file
typedef struct
{
    char filename[12];     // File name (11 chars for 8.3 format + null terminator)
    char full_path[256];   // Full path to the file
    int mode;              // Opening mode (O_RDONLY, O_WRONLY, O_RDWR)
    unsigned int offset;   // Current file offset
    unsigned int first_cluster; // First cluster of the file
    unsigned int file_size;     // Size of the file
    int is_used;           // Whether this entry is in use (0: unused, 1: in use)
} OpenFileEntry;

// Open file table
extern OpenFileEntry open_files[10];

// File operation functions
int file_open(FAT32Context *context, const char *filename, const char *mode);
int file_close(FAT32Context *context, const char *filename);

// Part 4 c,d added functions:
void fat32_lsof(FAT32Context *context);
int fat32_lseek(const char *filename, uint32_t offset);

#endif // FAT32_H
