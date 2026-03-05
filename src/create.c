#include "create.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Find the first free cluster in the FAT
uint32_t find_free_cluster(FAT32Context *context) {
    // fat_entries = (size of each FAT entry * # bytes in each sector) / 4 => total size / 4
    uint32_t fat_entries = context->boot_sector.fat_size_32 *
                            context->boot_sector.bytes_per_sector / 4;

    // data begins at i=2
    for (uint32_t i = 2; i < fat_entries; i++) {
        uint32_t cluster = get_next_cluster(context, i);
        if (cluster == 0) {
            return i;   // free cluster
        }
    }

    return 0; // no free cluster
}

// Update the FAT by writing to it for the given cluster
void write_fat(FAT32Context *context, uint32_t cluster, uint32_t value) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = context->boot_sector.reserved_sector_count +
                            (fat_offset / context->boot_sector.bytes_per_sector);
    uint32_t entry_offset = fat_offset % context->boot_sector.bytes_per_sector;

    for (int curr = 0; curr < context->boot_sector.num_fats; curr++) {
        uint32_t fat_pos = (fat_sector + curr * context->boot_sector.fat_size_32) *
                            context->boot_sector.bytes_per_sector + entry_offset;

        fseek(context->fp, fat_pos, SEEK_SET);
        fwrite(&value, sizeof(uint32_t), 1, context->fp);
    }
}

// Initialize the directory with attributes
void init_dir(uint8_t *entry, const char *name, uint32_t cluster) {
    // Zero out entry
    memset(entry, 0, 32);

    // Name formatting (special cases - regular naming)
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        memset(entry, ' ', 11);
        memcpy(entry, name, strlen(name));
    } else {
        char formatted_name[11];
        format_name(name, formatted_name);
        memcpy(entry, formatted_name, 11);
    }
    

    // Set directory attribute
    entry[11] = 0x10;

    // Set cluster numbers
    entry[20] = (cluster >> 16) & 0xFF;
    entry[21] = (cluster >> 24) & 0xFF;
    entry[26] = cluster & 0xFF;
    entry[27] = (cluster >> 8) & 0xFF;

    // Zero the size since it is a directory
    memset(entry + 28, 0, 4);
}

// Check for a file name or directory name already existing
bool name_exists(FAT32Context *context, const char *name) {
    char formatted_name[11];
    format_name(name, formatted_name);

    uint32_t current_cluster = context->current_cluster;
    uint32_t bytes_per_cluster = context->boot_sector.sectors_per_cluster *
                                context->boot_sector.bytes_per_sector;

    while (current_cluster < 0x0FFFFFF8) {
        uint8_t *buffer = malloc(bytes_per_cluster);
        uint32_t sector = cluster_to_sector(context, current_cluster);

        fseek(context->fp, sector * context->boot_sector.bytes_per_sector, SEEK_SET);
        fread(buffer, 1, bytes_per_cluster, context->fp);

        for (uint32_t i = 0; i < bytes_per_cluster; i += 32) {
            uint8_t *entry = buffer + i;

            // skip deleted or empty
            if (entry[0] == 0x00 || entry[0] == 0xE5) continue;

            // compare names
            if (memcmp(entry, formatted_name, 11) == 0) {
                free(buffer);
                return true;
            }
        }

        // were not the same, go to next cluster and check
        free(buffer);
        current_cluster = get_next_cluster(context, current_cluster);
    }

    // name does not exist
    return false;
}

// Create the directory
void fat32_mkdir(FAT32Context *context, const char *dir_name) {
    // Directory Name Validation: Not empty
    if (strlen(dir_name) == 0) {
        printf("Error: Directory name cannot be empty\n");
        return;
    }

    // Directory Name Validation: If the same as an existing directory
    if (name_exists(context, dir_name)) {
        printf("Error: A file or directory already exists with that name\n");
        return;
    }

    // Find a free cluster 
    uint32_t dir_cluster = find_free_cluster(context);
    if (dir_cluster == 0) {
        printf("Error: No free space available for a new directory\n");
        return;
    }

    // Mark cluster as EOC
    write_fat(context, dir_cluster, 0x0FFFFFFF);
    fflush(context->fp);

    uint32_t current_cluster = context->current_cluster;
    // Calculate bytes in one cluster
    uint32_t bytes_per_cluster = context->boot_sector.sectors_per_cluster *
                                context->boot_sector.bytes_per_sector;
    bool entry_added = false; 

    // find/allocate space for the new directory in the current directory
    // while looping until we find a free spot
    while (current_cluster < 0x0FFFFFF8 && !entry_added) {
        uint8_t *buffer = malloc(bytes_per_cluster);
        uint32_t sector = cluster_to_sector(context, current_cluster);

        // read current directory cluster
        fseek(context->fp, sector * context->boot_sector.bytes_per_sector, SEEK_SET);
        fread(buffer, 1, bytes_per_cluster, context->fp);

        // look for free entry 
        for (uint32_t i = 0; i < bytes_per_cluster; i += 32) {
            uint8_t *entry = buffer + i;

            if (entry[0] == 0x00 || entry[0] == 0xE5) {
                // if found free entry, add the directory here
                init_dir(entry, dir_name, dir_cluster);

                // write updated directory cluster
                fseek(context->fp, sector * context->boot_sector.bytes_per_sector, SEEK_SET);
                fwrite(buffer, 1, bytes_per_cluster, context->fp);
                fflush(context->fp);

                entry_added = true;
                break;
            }
        }

        free(buffer);

        // if not available, move to next cluster
        if(!entry_added) {
            current_cluster = get_next_cluster(context, current_cluster);
        }
    }

    // Case where we don't have space, return
    if(!entry_added) {
        printf("Error: No space in the current directory to add entry\n");
        return;
    }

    // Init '.' and '..'
    uint8_t *dir_buffer = calloc(1, bytes_per_cluster);
    init_dir(dir_buffer, ".", dir_cluster);                         // "."
    init_dir(dir_buffer + 32, "..", context->current_cluster);      // ".."

    dir_buffer[64] = 0x00;  // EOC 

    // Write new directory cluster
    uint32_t dir_sector = cluster_to_sector(context, dir_cluster);
    fseek(context->fp, dir_sector * context->boot_sector.bytes_per_sector, SEEK_SET);
    fwrite(dir_buffer, 1, bytes_per_cluster, context->fp);
    fflush(context->fp);
    
    free(dir_buffer);
}

// Create a file
void creat(FAT32Context *context, const char *file_name) {
    // Directory Name Validation: Not empty
    if (strlen(file_name) == 0) {
        printf("Error: File name cannot be empty\n");
        return;
    }

    // Directory Name Validation: If the same as an existing directory
    if (name_exists(context, file_name)) {
        printf("Error: A file or directory already exists with that name\n");
        return;
    }

    // Start from the current directory cluster
    uint32_t current_cluster = context->current_cluster;
    // Calculate bytes in one cluster
    uint32_t bytes_per_cluster = context->boot_sector.sectors_per_cluster *
                                context->boot_sector.bytes_per_sector;
    bool entry_added = false; 

    // find/allocate space for the new file in the current directory
    // while looping until we find a free spot
    while (current_cluster < 0x0FFFFFF8 && !entry_added) {
        // allocate mem to read current cluster
        uint8_t *buffer = malloc(bytes_per_cluster);
        uint32_t sector = cluster_to_sector(context, current_cluster);

        // read cluster data into buffer
        fseek(context->fp, sector * context->boot_sector.bytes_per_sector, SEEK_SET);
        fread(buffer, 1, bytes_per_cluster, context->fp);

        // look through cluster 32 bytes at a time since that is how much
        // each entry is
        for (uint32_t i = 0; i < bytes_per_cluster; i += 32) {
            uint8_t *entry = buffer + i;

            // check if entry is available
            if (entry[0] == 0x00 || entry[0] == 0xE5) {
                // file name formatting
                char formatted_name[11];
                format_name(file_name, formatted_name);
                memcpy(entry, formatted_name, 11);

                // set attribute to archive
                entry[11] = 0x20;   // ATTR_ARCHIVE

                // no cluster allocation
                entry[20] = entry[21] = 0;
                entry[26] = entry[27] = 0;

                // set the file size to 0
                memset(entry + 28, 0, 4);

                // write new file to the current directory
                fseek(context->fp, sector * context->boot_sector.bytes_per_sector, SEEK_SET);
                fwrite(buffer, 1, bytes_per_cluster, context->fp);
                fflush(context->fp);

                entry_added = true;
                break;
            }
        }

        free(buffer);

        // if not available, move to next cluster
        if (!entry_added) {
            current_cluster = get_next_cluster(context, current_cluster);
        }
    }

    if (!entry_added) {
        printf("Error: No space in the current directory to create the file\n");
    }
}