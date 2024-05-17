#ifndef __FILESYS__
#define __FILESYS__

#define MAX_FILES 64
#define MAX_FILE_SIZE 16777216
#define MAX_F_NAME 15
#define MAX_FILEDES 32
#define MAX_FAT 4096

#define BLOCK_SUPER -1
#define BLOCK_FAT -2
#define BLOCK_DIR -3
#define BLOCK_FREE -4
#define BLOCK_SPACING -5



#define BLOCKS_MAX 8192
#define BLOCKS_SIZE 4096


#define C_TRUE 1
#define C_FALSE 0

typedef struct super_block{
  int fat_idx;      // First block of the FAT
  int fat_len;      // Length of FAT blocks
  int dir_idx;      // First block of the directory
  int dir_len;   // Length of Directory blocks
  int data_idx;     // First block of File Data
}super_block;

typedef struct dir_entry{
    int used;
    char name[MAX_F_NAME + 1];
    int size;       // File Size
    int head;       // First block of data
    int ref_cnt;    // How many open file descriptors. If > 0, cannot delete.
}dir_entry;

typedef struct file_descriptor{
  int used;         // fd in use
  int head;         // The first block of the file to which fd refers to.
  int offset;       // Position of fd within f.
  int size;
  char name[MAX_F_NAME + 1];
}file_descriptor;


int make_fs(char* disk_name);
int mount_fs(char* disk_name);
int umount_fs(char* disk_name);

int fs_open(char* name);
int fs_close(int fd);
int fs_create(char* name);
int fs_delete(char* name);

int fs_next_free();
int fs_get_block(int fd, int block_number);

int fs_get_filesize(int fd);
int fs_listfiles(char*** files);

int fs_read(int fd, void* buf, size_t nbyte);
int fs_write(int fd);

int fs_lseek(int fd, off_t offset);
int fs_truncate(int fd, off_t length);

void print_fs();
void print_dir();
void print_fd();

#endif
