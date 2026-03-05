# FAT32 File System

**Description**: The purpose of this project is to gain a comprehensive understanding of basic file-system design and implementation by working with the FAT32 file system. Throughout this project, we will delve into the intricacies of the FAT32 file system, including its cluster-based storage, FAT tables, sectors, and directory structure.

## Group Members
- **Asia Thomas**: at22bv@fsu.edu
- **Cristhian Prado**: cp21h@fsu.edu
- **Jeongyeon Kim**: jk22bl@fsu.edu

## Division of Labor

### Part 1: Mounting the Image File
- **Responsibilities**: Mount the image file through command line arguments. Implement commands `info` which parses the boot sector and prints fields, and `exit` which safely closes the program. 
- **Assigned to**: Jeongyeon Kim
- **Completed by**: Jeongyeon Kim
 
### Part 2: Navigation
- **Responsibilities**: Implement the commands `cd [DIRNAME]` changing the current working directory to the specified `DIRNAME` and `ls` to print contents within the working directory.
- **Assigned to**: Asia Thomas
- **Completed by**: Asia Thomas

### Part 3: Create
- **Responsibilities**: Implement the commands `mkdir[DIRNAME` creating a new directory and `creat[FILENAME]` creating a new file.
- **Assigned to**: Cristhian Prado
- **Completed by**: Cristhian Prado

### Part 4: Read
- **Responsibilities**: Implement the commands `open[FILENAME][FLAGS]` to open a file in the current working directory. Also implement `close[FILENAME]` to close a file, `lsof` to list all opened files, `lseek[FILENAME][OFFSET]` to set the offset of the file for further reading, and `read[FILENAME][SIZE]` to read the data from a file in the current working directory.
- **Assigned to**: Jeongyeon Kim (open, close), Asia Thomas (lsof, lseek), Cristhian Prado (read)
- **Completed by**: Jeongyeon Kim (open, close), Asia Thomas (lsof, lseek)

### Part 5: Update
- **Responsibilities**: Implement the commands `write[FILENAME][STRING]` to write to a file in the current working directory and `mv [FILENAME/DIRNAME] [NEW_FILENAME/DIRECTORY]` to move a file to a new location.
- **Assigned to**: Cristhian Prado
- **Completed by**:

### Part 6: Delete
- **Responsibilities**: Implement the commands `rm[FILENAME]` to delete the file `FILENAME` and `rmdir[DIRNAME]` to remove a directory called `DIRNAME`. 
- **Assigned to**: Jeongyeon Kim (rm), Asia Thomas (rmdir)
- **Completed by**:


## File Listing
```
project3/
│
├── include/
│   ├── create.h
│   ├── fat32.h
│   ├── lexer.h
│   └── navigation.h
│
├── Makefile
├── README.md
└── src/
    ├── create.c
    ├── fat32.c
    ├── lexer.c
    ├── main.c
    └── navigation.c

```
## How to Compile & Execute

### Requirements
- **Compiler**: `gcc`


### Compilation
For a C/C++ example:
```bash
make
./bin/filesys <fat32_image_name.img>
```


## Bugs
- TBD...

## Notice
This project was originally developed as part of the Operating Systems course at Florida State University.
