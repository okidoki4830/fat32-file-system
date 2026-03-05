#ifndef NAVIGATION_H
#define NAVIGATION_H

#include "fat32.h"

// List directory contents,'ls'
void list_directory(FAT32Context *context);
// Change directory, 'cd'
void change_directory(FAT32Context *context, const char *dir_name);

#endif
