#ifndef CREATE_H
#define CREATE_H

#include "fat32.h"

// mkdir[DIRNAME] - Create a directory
void fat32_mkdir(FAT32Context *context, const char *dir_name);

// creat[FILENAME] - Create a file
void creat(FAT32Context *context, const char *file_name);

#endif