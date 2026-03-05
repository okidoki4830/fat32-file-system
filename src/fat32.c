#include "../include/fat32.h"
#include <ctype.h>

// Global open file table
OpenFileEntry open_files[10] = {0};

// Mount a FAT32 image file
int mount_fat32(FAT32Context *context, const char *image_path)
{
    // Open the image file
    context->fp = fopen(image_path, "r+b");
    if (!context->fp)
    {
        perror("Error opening image file");
        return -1;
    }
    
    // Store the image name
    context->image_name = strdup(image_path);
    if (!context->image_name)
    {
        perror("Memory allocation error");
        fclose(context->fp);
        return -1;
    }
    
    // Read the boot sector - simplified approach, no validation
    if (fseek(context->fp, 0, SEEK_SET) != 0)
    {
        perror("Error seeking to boot sector");
        free(context->image_name);
        fclose(context->fp);
        return -1;
    }
    
    // Read the entire boot sector directly into the structure
    size_t read_bytes = fread(&context->boot_sector, 1, sizeof(BootSector), context->fp);
    if (read_bytes < 90) // Minimum required size
    {
        perror("Error reading boot sector");
        free(context->image_name);
        fclose(context->fp);
        return -1;
    }
    
    // Skip all validation for now - accept any image file
    
    // Calculate some important values
    // Default values for sectors per FAT if not provided
    uint32_t fatSz = context->boot_sector.fat_size_32;
    if (fatSz == 0) fatSz = context->boot_sector.fat_size_16;
    if (fatSz == 0) fatSz = 32; // Default value if both are 0
    
    // Default value for root cluster if not provided
    if (context->boot_sector.root_cluster == 0)
        context->boot_sector.root_cluster = 2; // Typical starting cluster
    
    // Calculate data sectors
    uint32_t reserved_sectors = context->boot_sector.reserved_sector_count;
    if (reserved_sectors == 0) reserved_sectors = 32; // Default value
    
    uint32_t num_fats = context->boot_sector.num_fats;
    if (num_fats == 0) num_fats = 2; 
    
    uint32_t data_sectors = context->boot_sector.total_sectors_32 - 
                           (reserved_sectors + num_fats * fatSz);
    
    // If sectors per cluster is 0, set a default value
    if (context->boot_sector.sectors_per_cluster == 0)
        context->boot_sector.sectors_per_cluster = 8;
    
    context->boot_sector.total_clusters = data_sectors / context->boot_sector.sectors_per_cluster;
    
    // Calculate first data sector
    context->boot_sector.first_data_sector = reserved_sectors + (num_fats * fatSz);
    
    // Calculate total volume size
    uint32_t total_sectors = context->boot_sector.total_sectors_32;
    if (total_sectors == 0) total_sectors = context->boot_sector.total_sectors_16;
    if (total_sectors == 0)
    {
        // Try to determine the size from the file itself
        fseek(context->fp, 0, SEEK_END);
        long file_size = ftell(context->fp);
        fseek(context->fp, 0, SEEK_SET);
        total_sectors = file_size / context->boot_sector.bytes_per_sector;
        if (total_sectors == 0) total_sectors = file_size / 512; // Assume 512 bytes per sector
    }
    
    context->boot_sector.volume_size = (uint64_t)total_sectors * 
                                      (uint64_t)context->boot_sector.bytes_per_sector;
    
    // Set initial working directory to root
    context->stack_top = 0;
    context->current_cluster = context->boot_sector.root_cluster;
    context->cluster_stack[context->stack_top++] = context->current_cluster;
    strcpy(context->current_path, "/");

    
    // Print some debug information to verify what we read
    /*fprintf(stderr, "Debug: FAT32 boot sector information\n");
    fprintf(stderr, "Bytes per sector: %d\n", context->boot_sector.bytes_per_sector);
    fprintf(stderr, "Sectors per cluster: %d\n", context->boot_sector.sectors_per_cluster);
    fprintf(stderr, "Reserved sectors: %d\n", context->boot_sector.reserved_sector_count);
    fprintf(stderr, "Number of FATs: %d\n", context->boot_sector.num_fats);
    fprintf(stderr, "FAT size: %d\n", fatSz);
    fprintf(stderr, "Root cluster: %d\n", context->boot_sector.root_cluster);
    */
    return 0;
}

// Display FAT32 file system information
void display_info(FAT32Context *context)
{
    printf("FAT32 File System Information:\n");
    printf("Position of root cluster: %d\n", context->boot_sector.root_cluster);
    printf("Bytes per sector: %d\n", context->boot_sector.bytes_per_sector);
    printf("Sectors per cluster: %d\n", context->boot_sector.sectors_per_cluster);
    printf("Total # of clusters in data region: %d\n", context->boot_sector.total_clusters);
    printf("# of entries in one FAT: %d\n", context->boot_sector.fat_size_32 * context->boot_sector.bytes_per_sector / 4);
    printf("Size of image: %lu bytes\n", (unsigned long)context->boot_sector.volume_size);
}


// Convert cluster number to sector address
uint32_t cluster_to_sector(FAT32Context *context, uint32_t cluster)
{
    return context->boot_sector.first_data_sector + 
          (cluster - 2) * context->boot_sector.sectors_per_cluster;
}

// Get next cluster from FAT table
uint32_t get_next_cluster(FAT32Context *context, uint32_t cluster)
{
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = context->boot_sector.reserved_sector_count +
                         (fat_offset / context->boot_sector.bytes_per_sector);
    uint32_t entry_offset = fat_offset % context->boot_sector.bytes_per_sector;
    uint32_t fat_value;

    // Read FAT entry
    fseek(context->fp, (fat_sector * context->boot_sector.bytes_per_sector) + entry_offset, SEEK_SET);
    fread(&fat_value, sizeof(uint32_t), 1, context->fp);
    //printf("[DEBUG] FAT Entry for %u: 0x%08X\n", cluster, fat_value);
    //if (fat_value >= 0x0FFFFFF8)
    //	printf("[DEBUG] End of cluster chain reached.\n");

    // Mask out high 4 bits (reserved)
    return fat_value & 0x0FFFFFFF;
}

// Convert file names to FAT32 format
void format_name(const char *input, char *output) 
{
    memset(output, ' ', 11); // Initialize with spaces

    const char *dot = strchr(input, '.');
    size_t base_len = dot ? (size_t)(dot - input) : strlen(input);

    // Process base name (up to 8 chars)
    for(size_t i = 0; i < 8 && i < base_len; i++) 
    {
        output[i] = toupper(input[i]);
    }

    // Process extension (up to 3 chars)
    if(dot && *(++dot)) 
    {
        for(size_t i = 0; i < 3 && *dot; i++) 
        {
            output[8 + i] = toupper(*dot++);
        }
    }
}

// Convert directory entry to printable name
void get_display_name(const uint8_t *entry, char *output) 
{
    // Format: "BASE    EXT" -> "BASE.EXT" 
    // Trims trailing spaces and adds dot if extension exists
   
    memcpy(output, entry, 11);
    output[11] = '\0';
    
    // Find last non-space in base
    int base_end = 7;
    while(base_end >= 0 && output[base_end] == ' ') base_end--;
    
    // Find last non-space in extension
    int ext_end = 10;
    while(ext_end >= 8 && output[ext_end] == ' ') ext_end--;
    
    if(ext_end >= 8) 
    {
        output[base_end + 1] = '.';
        memmove(output + base_end + 2, output + 8, ext_end - 7);
        output[base_end + 2 + (ext_end - 8)] = '\0';
    } 
    else 
    {
        output[base_end + 1] = '\0';
    }
}

/**
 * Find a file or directory entry in the current working directory
 * @param context FAT32 context
 * @param name The name of the file or directory to find
 * @return Pointer to the directory entry, or NULL if not found
 */
DirectoryEntry* find_file_in_current_directory(FAT32Context *context, const char *name)
{
    static DirectoryEntry dir_entry;
    uint32_t cluster = context->current_cluster;
    uint32_t sector = cluster_to_sector(context, cluster);
    uint32_t bytes_per_sector = context->boot_sector.bytes_per_sector;
    uint32_t sectors_per_cluster = context->boot_sector.sectors_per_cluster;
    uint32_t entries_per_sector = bytes_per_sector / sizeof(DirectoryEntry);
    
    // Format the target name to FAT32 format for comparison
    char target_name[12];
    format_name(name, target_name);
    
    // Allocate a buffer for reading one sector
    uint8_t *buffer = malloc(bytes_per_sector);
    if (!buffer)
    {
        printf("Error: Memory allocation failed\n");
        return NULL;
    }
    
    // Read all clusters in the directory chain
    while (cluster >= 2 && cluster < 0x0FFFFFF8)
    {
        // Read all sectors in this cluster
        for (uint32_t i = 0; i < sectors_per_cluster; i++)
        {
            // Read sector
            fseek(context->fp, (sector + i) * bytes_per_sector, SEEK_SET);
            fread(buffer, bytes_per_sector, 1, context->fp);
            
            // Process each directory entry in this sector
            DirectoryEntry *dir_entries = (DirectoryEntry *)buffer;
            for (uint32_t j = 0; j < entries_per_sector; j++)
            {
                // Skip empty entries or LFN entries
                if (dir_entries[j].name[0] == 0 || 
                    dir_entries[j].name[0] == 0xE5 || 
                    (dir_entries[j].attributes & 0x0F) == 0x0F)
                {
                    continue;
                }
                
                // Compare names
                char entry_name[12];
                memcpy(entry_name, dir_entries[j].name, 11);
                entry_name[11] = '\0';
                
                if (memcmp(entry_name, target_name, 11) == 0)
                {
                    // Found the file/directory
                    memcpy(&dir_entry, &dir_entries[j], sizeof(DirectoryEntry));
                    free(buffer);
                    return &dir_entry;
                }
            }
        }
        
        // Move to the next cluster
        cluster = get_next_cluster(context, cluster);
        if (cluster < 2 || cluster >= 0x0FFFFFF8)
        {
            break; // End of cluster chain
        }
        
        // Update sector for next cluster
        sector = cluster_to_sector(context, cluster);
    }
    
    // File/directory not found
    free(buffer);
    return NULL;
}

/**
 * Open a file with the specified mode
 * @param context FAT32 context
 * @param filename The name of the file to open
 * @param mode The mode to open the file in ("-r", "-w", "-rw", or "-wr")
 * @return The index of the file in the open_files table, or -1 on error
 */
int file_open(FAT32Context *context, const char *filename, const char *mode)
{
    // Check if the file already exists in the current directory
    DirectoryEntry *entry = find_file_in_current_directory(context, filename);
    if (!entry)
    {
        printf("Error: File '%s' does not exist\n", filename);
        return -1;
    }
    
    // Check if the entry is a directory
    if (entry->attributes & 0x10)
    {
        printf("Error: '%s' is a directory, not a file\n", filename);
        return -1;
    }
    
    // Determine the mode flag
    int mode_flag = 0;
    if (strcmp(mode, "-r") == 0)
    {
        mode_flag = O_RDONLY;
    }
    else if (strcmp(mode, "-w") == 0)
    {
        mode_flag = O_WRONLY;
    }
    else if (strcmp(mode, "-rw") == 0 || strcmp(mode, "-wr") == 0)
    {
        mode_flag = O_RDWR;
    }
    else
    {
        printf("Error: Invalid mode '%s'. Valid modes are: -r, -w, -rw, -wr\n", mode);
        return -1;
    }
    
    // Check if the file is already open
    for (int i = 0; i < 10; i++)
    {
        if (open_files[i].is_used)
        {
            char display_name[12];
            get_display_name(entry->name, display_name);
            if (strcmp(open_files[i].filename, display_name) == 0)
            {
                printf("Error: File '%s' is already open\n", filename);
                return -1;
            }
        }
    }
    
    // Find an empty slot in the open files table
    int empty_slot = -1;
    for (int i = 0; i < 10; i++)
    {
        if (!open_files[i].is_used)
        {
            empty_slot = i;
            break;
        }
    }
    
    if (empty_slot == -1)
    {
        printf("Error: Maximum number of open files reached (10)\n");
        return -1;
    }
    
    // Initialize the open file entry
    char display_name[12];
    get_display_name(entry->name, display_name);
    strncpy(open_files[empty_slot].filename, display_name, 11);
    open_files[empty_slot].filename[11] = '\0';
    
    // Copy the current working directory path
    strncpy(open_files[empty_slot].full_path, context->current_path, 255);
    open_files[empty_slot].full_path[255] = '\0';
    
    open_files[empty_slot].mode = mode_flag;
    open_files[empty_slot].offset = 0;
    
    // Extract file information from directory entry
    uint32_t first_cluster = (entry->first_cluster_high << 16) | entry->first_cluster_low;
    open_files[empty_slot].first_cluster = first_cluster;
    open_files[empty_slot].file_size = entry->file_size;
    open_files[empty_slot].is_used = 1;
    
    printf("File '%s' opened successfully in mode '%s'\n", filename, mode);
    return empty_slot;
}

/**
 * Close an open file
 * @param context FAT32 context
 * @param filename The name of the file to close
 * @return 0 on success, -1 on error
 */
int file_close(FAT32Context *context, const char *filename)
{
    // Check if the file exists in the current directory
    DirectoryEntry *entry = find_file_in_current_directory(context, filename);
    if (!entry)
    {
        printf("Error: File '%s' does not exist\n", filename);
        return -1;
    }
    
    // Get the display name for the file
    char display_name[12];
    get_display_name(entry->name, display_name);
    
    // Find the file in the open files table
    int file_index = -1;
    for (int i = 0; i < 10; i++)
    {
        if (open_files[i].is_used && strcmp(open_files[i].filename, display_name) == 0)
        {
            file_index = i;
            break;
        }
    }
    
    if (file_index == -1)
    {
        printf("Error: File '%s' is not open\n", filename);
        return -1;
    }
    
    // Close the file by marking the slot as unused
    open_files[file_index].is_used = 0;
    
    printf("File '%s' closed successfully\n", filename);
    return 0;
}

// function to list open files 
void fat32_lsof(FAT32Context *context)
{
    bool any_open = false;
    
    // check if any files are open
    for (int i = 0; i < 10; i++)
    {
        if (open_files[i].is_used)
	{
            any_open = true;
            break;
        }
    }

    // list header
    if (any_open)
    {
        printf("%-5s  %-11s  %-4s  %-6s  %s\n", 
               "INDEX", "NAME", "MODE", "OFFSET", "PATH");
    }

    // print open files
    for (int i = 0; i < 10; i++)
    {
        if (open_files[i].is_used)
	{
	    // convert mode to string
            const char *mode_str;
            switch (open_files[i].mode)
	    {
                case O_RDONLY: mode_str = "r"; break;
                case O_WRONLY: mode_str = "w"; break;
                case O_RDWR:   mode_str = "rw"; break;
                default:       mode_str = "?"; break;
            }

	    // build absolute path
            char abs_path[512];
            snprintf(abs_path, sizeof(abs_path), "%s%s", 
                    context->image_name,
                    open_files[i].full_path); //

	    // print rows of opened files
            printf("%-5d  %-11s  %-4s  %-6u  %s\n",
                   i,
                   open_files[i].filename,
                   mode_str,
                   open_files[i].offset,
                   abs_path);
        }
    }

    if (!any_open)
        printf("No files opened.\n");
}

// function to set file offset 
int fat32_lseek(const char *filename, uint32_t offset)
{
    for (int i = 0; i < 10; i++)
    {
        if (open_files[i].is_used && 
	    strcmp(open_files[i].filename, filename) == 0)
	{
            if (offset > open_files[i].file_size)
	    {
		// if offset is too large
                open_files[i].offset = open_files[i].file_size;
                return -1; 
            }
            open_files[i].offset = offset;
            return 0; 	// successful
        }
    }
    return -2; 		// file not open
}

// Clean up resources
void cleanup(FAT32Context *context)
{
    if (context->fp)
    {
        fclose(context->fp);
        context->fp = NULL;
    }
    
    if (context->image_name)
    {
        free(context->image_name);
        context->image_name = NULL;
    }
}
