#include "navigation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/ioctl.h> // For TIOCGWINSZ, to get winddow size

// get window size to display directory list correctly
static int get_terminal_width()
{
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_col > 0 ? w.ws_col : 10; // Default to 10 columns
}


// List directory contents with FAT chain traversal
void list_directory(FAT32Context *context)
{
    uint32_t cluster = context->current_cluster;
    bool is_root = (cluster == context->boot_sector.root_cluster);
    uint32_t bytes_per_cluster = context->boot_sector.sectors_per_cluster *
                                context->boot_sector.bytes_per_sector;

    // Collect all entries -- added
    char **entries = NULL;
    size_t entry_count = 0;
 
    // Debug: Track cluster chain traversal
    //printf("[DEBUG] Starting at cluster: %u (0x%08X)\n", cluster, cluster);

    // Follow FAT cluster chain
    while(cluster < 0x0FFFFFF8)
    { // While not end-of-chain
     // printf("[DEBUG] Reading cluster: %u (0x%08X)\n", cluster, cluster);
        uint8_t *buffer = malloc(bytes_per_cluster);
        uint32_t sector = cluster_to_sector(context, cluster);

        // Read entire cluster
        fseek(context->fp, sector * context->boot_sector.bytes_per_sector, SEEK_SET);
        fread(buffer, 1, bytes_per_cluster, context->fp);

        // Process each entry
        for(uint32_t i = 0; i < bytes_per_cluster; i += 32)
	{
            uint8_t *entry = buffer + i;

	   //printf("[DEBUG] Entry @ offset %d: FirstByte=0x%02X, Attr=0x%02X\n",
           //          i, entry[0], entry[11]);

            if(entry[0] == 0x00) break; // End of entries
            if(entry[0] == 0xE5) continue; // Deleted entry
            if(entry[11] == 0x0F) continue; // Skip LFN entries
	    
            uint8_t attr = entry[11];
            if (attr & 0x02 || attr & 0x04) continue; // Skip hidden/system files
	    if (!(attr & 0x10) && !(attr & 0x20)) continue; // Not a dir/file

            // Skip . and .. in root directory
            if(is_root && (entry[0] == '.')) continue;

            char display_name[13];
            get_display_name(entry, display_name);
            //printf("%s\n", display_name);
	    
            entries = realloc(entries, sizeof(char *) * (entry_count + 1));
            entries[entry_count++] = strdup(display_name);

        }

        free(buffer);
        cluster = get_next_cluster(context, cluster); // Get next in chain
    }

    // Calculate terminal layout
    int term_width = get_terminal_width();
    int max_len = 0;
    for (size_t i = 0; i < entry_count; i++)
    {
        int len = strlen(entries[i]);
        if (len > max_len) max_len = len;
    }

    int col_width = max_len + 2; // Add padding
    int cols = term_width / col_width;
    if (cols == 0) cols = 1; // Minimum 1 column

    // Print entries in columns
    for (size_t i = 0; i < entry_count; i++) {
        printf("%-*s", col_width, entries[i]);
        if ((i + 1) % cols == 0) printf("\n");
    }
    if (entry_count % cols != 0) printf("\n"); // Final newline

    // Cleanup
    for (size_t i = 0; i < entry_count; i++) free(entries[i]);
    free(entries);

}

void change_directory(FAT32Context *context, const char *dir_name)
{
    // no change in directory
    if(strcmp(dir_name, ".") == 0) return;

    // change to parent directory
    if(strcmp(dir_name, "..") == 0)
    {

	if (context->stack_top <= 1)
        {
            // Already at root
            context->current_cluster = context->cluster_stack[0];
            strcpy(context->current_path, "/");
            return;
        }

	context->current_cluster = context->cluster_stack[context->stack_top - 1];
        context->stack_top--; // Pop current

        // Update path: remove the last folder name

	char *last = context->current_path + strlen(context->current_path) - 1;

	// Remove trailing slash if present (except root "/")
	if (last > context->current_path && *last == '/')
	{
    	    *last = '\0';
    	    last = strrchr(context->current_path, '/');
    	    if (last != NULL && last != context->current_path)
        	*(last + 1) = '\0'; // Keep trailing slash
    	    else
        	strcpy(context->current_path, "/"); // Back to root
	}

        return;
    }


    uint32_t original_cluster = context->current_cluster;
    uint32_t bytes_per_cluster = context->boot_sector.sectors_per_cluster *
                                context->boot_sector.bytes_per_sector;
    char formatted[11];
    format_name(dir_name, formatted);

    // Search directory cluster chain
    uint32_t cluster = original_cluster;
    while(cluster < 0x0FFFFFF8)
    {
        uint8_t *buffer = malloc(bytes_per_cluster);
        uint32_t sector = cluster_to_sector(context, cluster);

        fseek(context->fp, sector * context->boot_sector.bytes_per_sector, SEEK_SET);
        fread(buffer, 1, bytes_per_cluster, context->fp);

        for(uint32_t i = 0; i < bytes_per_cluster; i += 32)
	{
            uint8_t *entry = buffer + i;

            if(entry[0] == 0x00)
	    {
                free(buffer);
                goto not_found;
            }
            if(entry[0] == 0xE5 || entry[11] == 0x0F || (entry[11] & 0x08)) continue;

            // Check if directory and name matches
            if((entry[11] & 0x10) &&
               memcmp(entry, formatted, 11) == 0)
	    {
		// Extract next cluster
                uint32_t next_cluster = (*(uint16_t *)(entry + 20) << 16) |
                                        *(uint16_t *)(entry + 26);

                // Update cluster stack
                if (context->stack_top < MAX_DIR_DEPTH)
                {
                    context->cluster_stack[context->stack_top++] = context->current_cluster;
                }

                context->current_cluster = next_cluster;

                // Update path
                if (strcmp(context->current_path, "/") != 0)
                    strcat(context->current_path, dir_name);
                else
                    sprintf(context->current_path + strlen(context->current_path), "%s", dir_name);

                strcat(context->current_path, "/");

                free(buffer);
                return;
            }
        }

        free(buffer);
        cluster = get_next_cluster(context, cluster);
    }

not_found:
    printf("Directory not found: %s\n", dir_name);
}

