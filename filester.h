#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>

#ifndef FILESTER_H
#define FILESTER_H

typedef struct file_info{
  char path[PATH_MAX];
  uint32_t size;
  int32_t timestamp;
  uint32_t st_mode;
}file_info;

void getFiles(file_info **files, char* path, uint32_t* length, uint32_t* max_size);

void merror(char *msg);

void mperror(char *msg);

int remove_file(const char* dirname);

int cmp(const void* a, const void* b);

#endif
