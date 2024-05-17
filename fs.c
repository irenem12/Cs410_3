#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "fs.h"
#include "disk.h"
// Need to be careful about how we are handling the final byte of the file.

struct super_block* sb;
struct file_descriptor filedes[MAX_FILEDES];
static int FAT[BLOCKS_MAX];
struct dir_entry DIR[MAX_FILES];

static int mounted;
static int initialized = 0;



int block_indexer = BLOCKS_SIZE / sizeof(int);

// Creates a fresh and empty filesystem on the virtual disk.
// Returns 0 on success and -1 if could not be created.

int make_fs(char* disk_name){


  initialized = 1;

  // Checking if we can make the disk.
  if(make_disk(disk_name) < 0)
    return -1;

  if(open_disk(disk_name) < 0)
    return -1;


  sb = calloc(1, sizeof(struct super_block));

  int fat_blocks = BLOCKS_MAX * sizeof(int) / BLOCKS_SIZE;

  int dir_blocks = sizeof(dir_entry) * MAX_FILES / BLOCKS_SIZE + 1;

  int i;

  sb->fat_idx = 1;
  sb->fat_len = fat_blocks;

  sb->dir_idx = sb->fat_idx + sb->fat_len;
  sb->dir_len = dir_blocks;

  sb->data_idx = sb->dir_idx + sb->dir_len;

  // Initializing values of out FAT.
  FAT[0] = BLOCK_SUPER;

  // Creating FAT Blocks
  for(i = 0; i < fat_blocks; i++){
    FAT[sb->fat_idx + i] = BLOCK_FAT;
  }

  // Assigning DIR Blocks
  for(i = 0; i < dir_blocks; i++){
    FAT[sb->dir_idx + i] = BLOCK_DIR;
  }

  // Assigning FREE Blocks
  for(i = sb->data_idx; i < BLOCKS_MAX; i++){
    FAT[i] = BLOCK_FREE;
  }

  char fat_block[BLOCKS_SIZE];
  memset(fat_block, 0, BLOCKS_SIZE);

  //Now that the FAT was updated, we need to write it to DISK_BLOCKS
  for(i = 0; i < sb->fat_len; i++){
    memcpy(fat_block, FAT + i * BLOCKS_SIZE / sizeof(int), BLOCKS_SIZE);
    if(block_write(sb->fat_idx + i, fat_block) < 0){
      fprintf(stderr, "Error: make_fs() Unable to write FAT_BLOCK to disk\n");
      return -1;
    }
  }


  // Super Block Calculations.
  char sb_block[BLOCKS_SIZE];
  memset(sb_block, 0, BLOCKS_SIZE);
  memcpy(sb_block, sb, sizeof(struct super_block));

  //printf("Length: %d\n", sb->fat_len);

  // Writing the SUPER_BLOCK
  if(block_write(0, sb_block) < 0){
    fprintf(stderr, "Error: Unable to write SUPER_BLOCK to disk\n");
    return -1;
  }


  // Making sure we can close the disk.
  if(close_disk() < 0){
    fprintf(stderr, "Error: make_fs() Unable to close disk\n");
    return -1;
  }

  return 0;
}

int mount_fs(char* disk_name){

  int i;

  initialized = 1;

  // Checking if we can open the disk.
  if(open_disk(disk_name) < 0)
    return -1;

  sb = calloc(1, sizeof(struct super_block));


  char read_buffer[BLOCKS_SIZE];

  // Reading in Super Block
  memset(read_buffer, 0, BLOCKS_SIZE);
  if(block_read(0, read_buffer) < 0){
    fprintf(stderr, "Error: mount_fs() Failed to read SUPER_BLOCK\n");
    return -1;
  }
  memcpy(sb, &read_buffer, sizeof(struct super_block));


  memset(read_buffer, 0, BLOCKS_SIZE);
  if(block_read(sb->dir_idx, read_buffer) < 0){
    fprintf(stderr, "Error: mount_fs() Failed to read DIR_BLOCK\n");
    return -1;
  }
  memcpy(DIR, &read_buffer, sizeof(struct dir_entry) * MAX_FILES);

  for(i = 0; i < sb->fat_len; i++){
    //printf("Attempting to read FAT block\n");
    memset(read_buffer, 0, BLOCKS_SIZE);
    if(block_read(sb->fat_idx + i, read_buffer) < 0){
      fprintf(stderr, "Error: mount_fs() Failed to read FAT_BLOCK: %d\n", i);
      return -1;
    }
    memcpy(FAT + i * block_indexer, &read_buffer, BLOCKS_SIZE);
    //printf("Read in FAT: %s\n", read_buffer);
  }

  //printf("READ Fat Blocks\n");


  // Initializing Directory Table
  for(i = 0; i < MAX_FILES; i++){
    DIR[i].ref_cnt = 0;
  }

  // Initializing Filedescriptor Table
  for(i = 0; i < MAX_FILEDES; i++){
    filedes[i].used = C_FALSE;
    filedes[i].offset = 0;
    filedes[i].head = -1;
  }

  //printf("All blocks have been read in.\n");
  //print_fs();

  return 0;
}

int umount_fs(char* disk_name){

  if(!initialized){
    fprintf(stderr, "Error: unmount_fs() Tried to unmount unitialized fs\n");
    return -1;
  }

  initialized = 0;



  int i;

  char buffer[BLOCKS_SIZE];

  //printf("SB Length: %d\n", sb->fat_len);


  // Writing Super Block
  memset(buffer, 0, BLOCKS_SIZE);
  memcpy(buffer, sb, sizeof(struct super_block));
  if(block_write(0, buffer) < 0){
    fprintf(stderr, "Error: unmount_fs() Unable to unmount SUPER_BLOCK\n");
    return -1;
  }




  // Writing FAT Block(s)
  for(i = 0; i < sb->fat_len; i++){
    memcpy(buffer, FAT + i * BLOCKS_SIZE / sizeof(int), BLOCKS_SIZE);
    //printf("Wrote Fat: %s\n", &buffer);

    if(block_write(sb->fat_idx + i, buffer) < 0){
      fprintf(stderr, "Error: unmount_fs() Unable to unmount FAT_BLOCK: %d\n", i);
      return -1;
    }
  }

  memset(buffer, 0, BLOCKS_SIZE);
  memcpy(buffer, DIR, sizeof(struct dir_entry) * MAX_FILES);
  //printf("Wrote Dir: %s\n", &buffer);
  if(block_write(sb->dir_idx, &buffer) < 0){
    fprintf(stderr, "Error: unmount_fs() Failed to read DIR_BLOCK: %d\n", sb->dir_idx);
    return -1;
  }


  if(close_disk() < 0)
    return -1;

  return 0;

}

int fs_create(char* name){

  int i, dir_index = -1, start_block;
  if(strlen(name) > 16){
    fprintf(stderr, "Error: fs_create() File name is too long.\n");
    return -1;
  }

  for(i = 0; i < MAX_FILES; i++){
    if(DIR[i].used == C_FALSE && dir_index == -1){
      dir_index = i;
    }else{
      // Checking if we already have this file in our system.
      if(strcmp(DIR[i].name, name) == 0){
        //fprintf(stderr, "Error: fs_create() File name is already in directory\n");
        return -1;
      }
    }
  }

  if(dir_index < 0){
    fprintf(stderr, "Error: fs_create() Directory is full!\n");
    return -1;
  }

  start_block = fs_next_free();

  if(fs_next_free() == -1){
    fprintf(stderr, "Error: fs_create() Memory is full!\n");
    return -1;
  }

  // Adding information to our tables.
  FAT[start_block] = dir_index;

  DIR[dir_index].used = C_TRUE;
  DIR[dir_index].size = 0;
  DIR[dir_index].ref_cnt = 0;
  DIR[dir_index].head = start_block;

  strncpy(DIR[dir_index].name, name, strlen(name));

  return 0;
}

int fs_next_free(){
  int i;
  for(i = sb->data_idx; i < BLOCKS_MAX; i++){
    if(FAT[i] == BLOCK_FREE)
      return i;
  }
  return -1;
}


int fs_open(char* name){
  int i, j;

  // Scanning to find file name in DIR
  for(i = 0; i < MAX_FILES; i++){
    if(strcmp(DIR[i].name, name) == 0 && DIR[i].used == C_TRUE){
        for(j = 0; j < MAX_FILEDES; j++){
          // Found available filedes.
          if(filedes[j].used == C_FALSE){
            DIR[i].ref_cnt++;

            strncpy(filedes[j].name, DIR[i].name, strlen(name));
            filedes[j].head = DIR[i].head;
            filedes[j].offset = 0;
            filedes[j].used = C_TRUE;
            return j;
          }
        }
        fprintf(stderr, "Error: fs_open() No available file descriptors.\n");
        return -1;
    }
  }
  return -1;
}

int fs_close(int fildes){
  int i;

  if(fildes >= 0 && fildes < MAX_FILEDES){
    if(filedes[fildes].used == C_FALSE){
      fprintf(stderr, "Error: fs_close() Attempted to close bad file descriptor.\n");
      return -1;
    }

    for(i = 0; i < MAX_FILES; i++){
      if(DIR[i].head == filedes[fildes].head){
        DIR[i].ref_cnt--;
      //  DIR[i].head = -1;

        filedes[fildes].used = C_FALSE;
        return 0;
      }
    }
  }

  fprintf(stderr, "Error: fs_close() Attempted to close out-of-bounds file descriptor.\n");
  return -1;

}

int fs_delete(char* name){

  int i, j;

  for(i = 0; i < MAX_FILES; i++){
    if(strcmp(DIR[i].name, name) == 0 && DIR[i].used == C_TRUE){
      if(DIR[i].ref_cnt > 0){
        fprintf(stderr, "Error: fs_delete() Attempted to delete file that is still being used.\n");
        return -1;
      }

      // Can be improved when I implement size.
      for(j = sb->data_idx; j < BLOCKS_MAX; j++){
        if(FAT[j] == i){
          FAT[j] = BLOCK_FREE;
        }
      }
      DIR[i].used = C_FALSE;
      return 0;
    }
  }

  return -1;
}


int fs_get_filesize(int fildes){

  int i;
  if(filedes[fildes].used == C_FALSE){
    fprintf(stderr, "Error: fs_get_file__size() File not currently being used.\n");
    return -1;
  }

  for(i = 0; i < MAX_FILES; i++){
    if(DIR[i].head == filedes[fildes].head){
      return DIR[i].size;
    }
  }

  fprintf(stderr, "Error: fs_get_file__size() File Does Not Existn");
  return -1;
}

// Pointer to a list of files.
int fs_listfiles(char*** files){

  int i, j = 0;

  // Mallocing a string for each file name.
  *files = (char**) malloc(MAX_FILES * sizeof(char*));

  // Mallocing a slot for all of the FILES
  for(i = 0; i < MAX_FILES; i++){
    (*files)[i] = (char*)malloc((MAX_F_NAME + 1) * sizeof(char));
    memset((*files)[i], 0, MAX_F_NAME + 1);
  }

  // Copying the file name if it is used.
  for(i = 0; i < MAX_FILES; i++){
    if(DIR[i].used == C_TRUE){
      strncpy((*files)[j], DIR[i].name, MAX_F_NAME + 1);
      j++;
    }
  }

  (*files)[j] = NULL;


  return 0;

}

int fs_read(int fildes, void* buf, size_t nbyte){

  int i, block_off, block_start, block_index = 0, bytes_read = 0, block_reading, dir_index, copy_counter = 0;;


  if(filedes[fildes].used == C_FALSE){
    fprintf(stderr, "Error: fs_read() Attempted to read file desctiptor that is not open.\n");
    return -1;
  }

  for(i = 0; i < MAX_FILES; i++){
    if(DIR[i].head == filedes[fildes].head){
      dir_index = i;
      break;
    }
  }


  block_off = filedes[fildes].offset % BLOCKS_SIZE;
  block_index = filedes[fildes].offset / BLOCKS_SIZE;

  char* buffer = malloc(nbyte);
  memset(buffer, 0, nbyte);
  char small_buffer[BLOCKS_SIZE];
  char read_buffer[BLOCKS_SIZE];


  // Need to check if someone is reading past the end of the file or just the last byte of the file.
  if(filedes[fildes].offset >= fs_get_filesize(fildes)){
    //fprintf(stderr, "Error: fs_write() Attempted to read at EOF\n");
    return 0;
  }

  if(filedes[fildes].offset > fs_get_filesize(fildes)){
    fprintf(stderr, "Error: fs_write() Attempted to read past the end of the file.\n");
    return -1;
  }

  int counter = 0, mini_read = 0;

  while(1){
    block_reading = fs_get_block(dir_index, block_index);
    memset(read_buffer, 0, BLOCKS_SIZE);
    // This either means we need to allocate a new block or memory may potentially be full.
    if(block_reading == -1){
      if(bytes_read > 0){
        while(copy_counter < (bytes_read - BLOCKS_SIZE)){
          memcpy(buf + copy_counter, buffer + copy_counter, BLOCKS_SIZE);
          copy_counter+= BLOCKS_SIZE;
        }
        memcpy(buf + copy_counter, buffer + copy_counter, bytes_read - copy_counter);
        filedes[fildes].offset++;
        return bytes_read;
      }
      return 0;
    }
    //printf("\tReading: %d\n", block_reading);
    if(block_read(block_reading, read_buffer) < 0){
      fprintf(stderr, "Error: fs_write() Unable to read from block when reading.\n");
      return -1;
    }
    // Checking to start reading values from the start of BLOCK_OFF to a maximum of BLOCKS_SIZE
    for(i = block_off; i < BLOCKS_SIZE; i++){
    //  printf("Reading: %c Into %d\n", read_buffer[i], bytes_read);
      buffer[bytes_read++] = read_buffer[i];

      // Checking if we read all the bytes that we came here to read.
      if(filedes[fildes].offset == fs_get_filesize(fildes)){
        bytes_read--;
        while(copy_counter < (bytes_read - BLOCKS_SIZE)){
          //printf("Storing %d\n", BLOCKS_SIZE);
          memcpy(buf + copy_counter, buffer + copy_counter, BLOCKS_SIZE);
          copy_counter+= BLOCKS_SIZE;
        }
        memcpy(buf + copy_counter, buffer + copy_counter, bytes_read - copy_counter);

        //memcpy(buf, buffer, --bytes_read);

        filedes[fildes].offset++;
        //free(buffer);

        return bytes_read;
      }

      if(nbyte == bytes_read){
        while(copy_counter < (bytes_read - BLOCKS_SIZE)){
          memcpy(buf + copy_counter, buffer + copy_counter, BLOCKS_SIZE);
          copy_counter+= BLOCKS_SIZE;
        }

        memcpy(buf + copy_counter, buffer + copy_counter, bytes_read - copy_counter);
        //memcpy(&buf, buffer, bytes_read);
        filedes[fildes].offset++;
        //free(buffer);

        return bytes_read;
      }
      filedes[fildes].offset++;
    }
    block_off = 0;
    block_index++;
  }

  free(buffer);
  return 0;
}

int fs_write(int fildes){

  int i, dir_index, block_off, block_writing, block_start, block_counter = 0, block_index = 0, bytes_written = 0, block_free;
  size_t nbyte;

  // New Code

  struct stat statbuf;

  if(stat(filedes[fildes].name, &statbuf) < 0){
    return-3;
  }

  char single_byte;
  nbyte = statbuf.st_size;
  //filedes[fildes].size = nbyte;
  int f_ptr_read;

  char* buf = malloc(nbyte);

  if((f_ptr_read = open(filedes[fildes].name, O_RDONLY))){

    char* byte = malloc(2);
    byte[1] = 0;
    for(i = 0; i < nbyte; i++){
      read(f_ptr_read, byte, 1);
      strncpy(buf + i, byte, 1);
    }
  }else{
    fprintf(stderr, "%s Failed to read file.\n", "SERVER");
    return;
  }

  // New Code

  if(filedes[fildes].used == C_FALSE){
    fprintf(stderr, "Error: fs_read() Attempted to read file desctiptor that is not open.\n");
    return -1;
  }

  for(i = 0; i < MAX_FILES; i++){
    if(DIR[i].head == filedes[fildes].head){
      dir_index = i;
      break;
    }
  }

  if(nbyte < 0){
    fprintf(stderr, "Error: fs_write() Please write a file anywhere in size of 0 MB - 16 MB\n");
    return -1;
  }

  if(nbyte == 0){
    fprintf(stderr, "Error: fs_write() Please write a file anywhere in size of 0 MB - 16 MB\n");
    return 0;
  }

  if(filedes[fildes].offset == MAX_FILE_SIZE){
    fprintf(stderr, "Error: fs_write() Please write a file anywhere in size of 0 MB - 16 MB\n");
    return 0;
  }

  int malloc_size = nbyte;
  if(nbyte > MAX_FILE_SIZE){
    malloc_size = MAX_FILE_SIZE;
  }


  char* write_buffer = malloc(malloc_size);
  //char write_buffer[nbyte];
  for(i = 0; i < malloc_size; i++){
    memcpy(write_buffer + i, (char*) (buf + i), 1);
  }

  block_off = filedes[fildes].offset % BLOCKS_SIZE;
  block_index = filedes[fildes].offset / BLOCKS_SIZE;

  char block_buffer[BLOCKS_SIZE];
  //printf("Writing to block: \n");
  // While we need to try and write, we will try to write.
  while(1){
    block_writing = fs_get_block(dir_index, block_index);
    if(block_writing == -1){
      block_free = fs_next_free();
      if(block_free == -1){
        free(write_buffer);
        if(bytes_written > 0){
          return bytes_written;

        }
        fprintf(stderr, "Error: fs_write() Ran out of memory.\n");
        return 0;
      }
      FAT[block_free] = dir_index;
      continue;

    }
    if(block_read(block_writing, block_buffer) < 0){
      fprintf(stderr, "Error: fs_write() Unable to read from block before writing.\n");
      free(write_buffer);
      return -1;
    }
    for(i = block_off; i < BLOCKS_SIZE; i++){
      block_buffer[i] = write_buffer[bytes_written++];
      //printf("Storing: %c Into %d\n", block_buffer[i], i);

      filedes[fildes].offset++;
      if(filedes[fildes].offset > fs_get_filesize(fildes)){
        DIR[dir_index].size++;
      }
      if(nbyte == bytes_written || (fs_get_filesize(fildes) == MAX_FILE_SIZE && MAX_FILE_SIZE == filedes[fildes].offset)){
        if(block_write(block_writing, block_buffer) < 0){
          fprintf(stderr, "Error: fs_write() Unable to write block\n");
          free(write_buffer);
          return -1;
        }
        free(write_buffer);
        return bytes_written;
      }
    }
    if(block_write(block_writing, block_buffer) < 0){
      fprintf(stderr, "Error: fs_write() Unable to write block\n");
      free(write_buffer);
      return -1;
    }
    block_off = 0;
    block_index++;
  }
  free(write_buffer);
  return bytes_written;
}

int fs_get_block(int fildes, int block_number){
  int i, count = 0;
  for(i = sb->data_idx; i <  BLOCKS_MAX; i++){
    if(FAT[i] == fildes){
      if(count == block_number){
        return i;
      }
      count++;
    }
  }
  return -1;
}

int fs_lseek(int fildes, off_t offset){

  if(fildes > MAX_FILEDES || fildes < 0){
    fprintf(stderr, "Error: fs_lseek() Illegal File Descriptor\n");
    return -1;
  }

  if(offset < 0 || offset > (fs_get_filesize(fildes))){
    fprintf(stderr, "Error: fs_lseek() Illegal Offset\n");
    return -1;
  }

  if(filedes[fildes].used == C_FALSE){
    fprintf(stderr, "Error: fs_lseek() Invalid file descriptor\n");
    return -1;
  }

  filedes[fildes].offset = offset;
  return 0;

}


void print_dir(){
  int i;
  printf("\nDirectories:\n");
  for(i = 0; i < MAX_FILES; i++){
    printf("DIR: %d\n", i);
    printf("\tSTATUS: %d\n", DIR[i].used);
    if(DIR[i].used == C_TRUE){
      printf("\tNAME: %s\n", DIR[i].name);
      printf("\tSIZE: %d\n", DIR[i].size);
    }
  }
}
