#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include "filester.h"
#include "netio.h"

void merror(char *msg)
{
  fputs(msg, stderr);
  exit(1);
}

void mperror(char *msg)
{
  perror(msg);
  exit(1);
}


int cmp(const void* a, const void* b)
{
  return strcmp( ((file_info *)a)->path, ((file_info *)b)->path );
}

int remove_file(const char *dirname)
{

  if (!remove(dirname))
    return 0;

  DIR *dir;
  struct dirent *entry;
  char path[PATH_MAX];

  dir = opendir(dirname);
  if (dir == NULL) {
    perror("Error opendir()");
    return -1;
  }

  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
      snprintf(path, (size_t) PATH_MAX, "%s/%s", dirname, entry->d_name);
      remove_file(path);
    }

  }
  closedir(dir);
  
  remove(dirname);
  
  return 0;
}



void getFiles(file_info **filelist, char* path, uint32_t* length, uint32_t* max_size ){
  DIR *d;
  struct dirent *sdir;
  struct stat mstat;
  char* newpath = malloc(PATH_MAX);
  if(!newpath)
    merror("Don't have enough memory for directory parsing.");

  if(( d = opendir(path)) == NULL ){
    puts(path);
    mperror("opendir()");
  }
  file_info *files = *filelist;

  while((sdir = readdir(d)) != NULL){
    snprintf(newpath, PATH_MAX, "%s/%s", path, sdir->d_name);
   
    lstat(newpath, &mstat);
    if((strcmp(sdir->d_name, "..") == 0) || (strcmp(sdir->d_name, ".") == 0 ))
       continue;

    // Expand memory for more files.    
    if(*length >= *max_size){
      *max_size = (*max_size) + (*max_size)/2;
      if ( (*filelist = realloc(*filelist, (*max_size)*sizeof(file_info))) == NULL)
	merror("Could not allocate memory for files");
      files = *filelist;
    }

    if(S_ISDIR(mstat.st_mode)){
      strncpy(files[*length].path, newpath, sizeof(files[*length].path)); 
      puts(newpath);
      files[*length].size = (uint32_t)mstat.st_size;
      files[*length].timestamp = (int32_t)mstat.st_mtime;
      files[*length].st_mode = mstat.st_mode;
      
      (*length)++;
      getFiles(filelist, newpath, length, max_size);
      files = *filelist;
    }else if(S_ISREG(mstat.st_mode) || S_ISLNK(mstat.st_mode)){
      strncpy(files[*length].path, newpath, sizeof(files[*length].path)); 
      puts(newpath);
      files[*length].size = (uint32_t)mstat.st_size;
      files[*length].timestamp = (int32_t)mstat.st_mtime;
      
      (*length)++;
    }
  }
  closedir(d);
  free(newpath);
}
