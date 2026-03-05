#include "../include/fat32.h"
#include "../include/lexer.h"
#include "../include/navigation.h"
#include "../include/create.h"

#define MAX_CMD_SIZE 256

// Function to process and execute commands
void process_command(FAT32Context *context, tokenlist *tokens);

int main(int argc, char *argv[])
{
    // Check if the image file path is provided
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s [FAT32 ISO]\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    // Initialize FAT32 context
    FAT32Context context;
    memset(&context, 0, sizeof(FAT32Context));
    
    // Mount the FAT32 image
    if (mount_fat32(&context, argv[1]) != 0)
    {
        fprintf(stderr, "Failed to mount the FAT32 image: %s\n", argv[1]);
        return EXIT_FAILURE;
    }
    
    int running = 1;
    
    // Main command loop
    while (running)
    {
        // Display prompt with current path
        printf("%s%s> ", context.image_name, context.current_path);
        fflush(stdout);
        
        // Read command
        char *input = get_input();
        if (input == NULL)
        {
            break;
        }
        
        // Process command
        tokenlist *tokens = get_tokens(input);
        free(input);

        if (tokens->size > 0)
        {
            process_command(&context, tokens);
        }

        free_tokens(tokens);
    }
    
    // Clean up resources
    cleanup(&context);
    
    return EXIT_SUCCESS;
}

// Process and execute commands
void process_command(FAT32Context *context, tokenlist *tokens)
{
    // Parse command and arguments
    char *cmd = tokens->items[0];
    if (!cmd)
    {
        return; // Empty command
    }
    
    // Handle "info" command
    if (strcmp(cmd, "info") == 0)
    {
        display_info(context);
    }
    // Handle "ls" command
    else if (strcmp(cmd, "ls") == 0)
    {
        list_directory(context);
    }
    // Handle "cd" command
    else if (strcmp(cmd, "cd") == 0)
    {
        if (tokens->size < 2)
            printf("Usage: cd <directory>\n");
        else
            change_directory(context, tokens->items[1]);
    }
    // Handle "mkdir" command
    else if (strcmp(cmd, "mkdir") == 0) 
    {
        if (tokens->size < 2)
            printf("Usage: mkdir <directory_name>\n");
        else 
            fat32_mkdir(context, tokens->items[1]);
    }
    // Handle "creat" command
    else if (strcmp(cmd, "creat") == 0) 
    {
        if (tokens->size < 2)
            printf("Usage: creat <file_name>\n");
        else 
            creat(context, tokens->items[1]);
    }
    // Handle "open" command
    else if (strcmp(cmd, "open") == 0)
    {
        if (tokens->size < 3)
            printf("Error: Usage: open [FILENAME] [FLAGS]\n");
        else
            file_open(context, tokens->items[1], tokens->items[2]);
    }
    // Handle "close" command
    else if (strcmp(cmd, "close") == 0)
    {
        if (tokens->size < 2)
            printf("Error: Usage: close [FILENAME]\n");
        else
            file_close(context, tokens->items[1]);
    }
    // Handle "lsof" command
    else if (strcmp(cmd, "lsof") == 0)
    {
    	fat32_lsof(context);
    }
    // Handle "lseek" command
    else if (strcmp(cmd, "lseek") == 0)
    {
        if (tokens->size < 3)
	{
            printf("Usage: lseek [FILENAME] [OFFSET]\n");
        }
	else
	{
            uint32_t offset = atoi(tokens->items[2]);
            int result = fat32_lseek(tokens->items[1], offset);
            if (result == -1) 
		printf("Error: Offset exceeds file size.\n");
            else if (result == -2) 
		printf("Error: File not open.\n");
        }
    }
    // Handle "exit" command
    else if (strcmp(cmd, "exit") == 0)
    {
        cleanup(context);
        exit(EXIT_SUCCESS);
    }
    // Unknown command
    else
    {
        printf("Unknown command: %s\n", cmd);
        printf("Available commands: info, ls, cd, mkdir, creat, open, close, lsof, lseek, exit\n");
    }
}
