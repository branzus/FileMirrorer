#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include "filester.h"
#include "netio.h"
#include<stdio.h>
#include <fcntl.h>
#include <time.h>
#include<utime.h>


#define MAXBUF 4096

int set_addr(struct sockaddr_in *addr, char *name, u_int32_t inaddr, short port){

  // Represents an entry in the host db.
  struct hostent *h;

  memset((void *)addr, 0, sizeof(*addr));
  addr->sin_family = AF_INET;
  if(name != NULL){
    h = gethostbyname(name);
    if (h == NULL){
      return -1;
    }

    // set the first address from the list.
    addr->sin_addr.s_addr = *(u_int32_t *) h->h_addr_list[0];
    
  }else{
    addr->sin_addr.s_addr = htonl(inaddr);
  }

  // htons converts format host to short.
  addr->sin_port = htons(port);
  return 0;
}

int stream_read(int sockfd, void *buff, int len){
  int nread;
  int remain = len;

  //printf("Reading %d bytes\n", len);
  
  while(remain > 0){
    if( -1 == (nread = read(sockfd, buff, remain)) )
      return -1;
    if(nread == 0)
      break;
    remain -= nread;
    buff += nread;
  }
  return len - remain;
}

int stream_write(int sockfd, const void *buff, int len){
  int nrw;
  int rem = len;
  
  //printf("Writing %d bytes\n", len);

  while(rem > 0){
    if ( -1 == (nrw = write(sockfd, buff, rem)))
      return -1;
    rem -= nrw;
    buff += nrw;
  }
  return len-rem;
}





int get_fileinfo(int sockfd, file_info* fileinfo)
{
  uint16_t size;

  if(stream_read(sockfd, &size, sizeof size) != sizeof size)
    return -1;
  if (size >= PATH_MAX)
    return -1;

  if(stream_read(sockfd, &(fileinfo->path), size) != size)
    return -1;

  fileinfo->path[size] = '\0';

  if(stream_read(sockfd, &(fileinfo->size), sizeof(fileinfo->size)) != sizeof(fileinfo->size) ||
     stream_read(sockfd, &(fileinfo->timestamp), sizeof(fileinfo->timestamp)) != sizeof(fileinfo->timestamp) ||
     stream_read(sockfd, &(fileinfo->st_mode), sizeof(fileinfo->st_mode)) != sizeof(fileinfo->st_mode))
    return -1;

  return 0;
}


int send_fileinfo(int sockfd, const file_info* fileinfo)
{
  uint16_t len = strlen(fileinfo->path);

  if(stream_write(sockfd, &len, sizeof len) != sizeof len ||
     stream_write(sockfd, &(fileinfo->path), len) != len ||
     stream_write(sockfd, &(fileinfo->size), sizeof(fileinfo->size)) != sizeof(fileinfo->size) ||
     stream_write(sockfd, &(fileinfo->timestamp), sizeof(fileinfo->timestamp)) != sizeof(fileinfo->timestamp) ||
     stream_write(sockfd, &(fileinfo->st_mode), sizeof(fileinfo->st_mode)) != sizeof(fileinfo->st_mode))
    return -1;
  
  return 0;
}


int send_filelist(int sockfd, const file_info *filelist, uint32_t length)
{
  // talk with client
  if(stream_write(sockfd, &length, sizeof(length)) != sizeof length)
    return -1;
  

  for(int i = 0; i < length; i++)
    if(send_fileinfo(sockfd, filelist + i))
      return -1;

  return 0;
}


int is_sane(const char *path)
{
  if (path[0] == '\0' || path[1] == '\0' || path[2] == '\0' || (path[1] == '.' && path[2] == '.' && path[3] == '/'))
    return 0;

  if (strstr(path, "/../"))
    return 0;

  return 1;
}

int get_filelist(int sockfd, file_info **filelist, uint32_t *length)
{
  
  if(stream_read(sockfd, length, sizeof(*length)) != sizeof(*length))
    return -1;
  
  *filelist = malloc(*length * sizeof(file_info));
  if (*filelist == NULL)
    merror("Not enough memory for filelist.");

  for(int i = 0; i < *length; i++)
    {
      if(get_fileinfo(sockfd, *filelist + i))
	return -1;
      if(!is_sane((*filelist)[i].path))
	merror("Server gives dangerous paths. Aborting...\n");
    }
  return 0;
}




int send_file(int sockfd, const char *file)
{
  int fd = open(file, O_RDONLY);
  
  if(fd == -1) {
    printf("Could not open %s\n", file);
    //send error codeg
    return -1;
  }
  int nread;
  char buf[MAXBUF];

  struct stat bstat;
  fstat(fd, &bstat);
  uint32_t mode = bstat.st_mode;
  uint32_t size = bstat.st_size;
  uint32_t mtime = bstat.st_mtime;

  if(
     stream_write(sockfd, &size, sizeof size) != sizeof size ||
     stream_write(sockfd, &mode, sizeof mode) != sizeof mode ||
     stream_write(sockfd, &mtime, sizeof mtime) != sizeof mtime)
    {
      printf("Could not send file info for %s\n", file);
      close(fd);
      return -1;
    }

  
  if (!S_ISDIR(mode))
    {  
      while(0 < (nread = read(fd, buf, MAXBUF)))
	{
	  if(stream_write(sockfd, buf, nread) != nread)
	    {
	      puts("Could not send file.");
	      close(fd);
	      return -1;
	    }
	}
      
      if (nread < 0)
	{
	  puts("Error reading from file.\n");
	  //send error code
	  close(fd);
	  return -1;
	}
    }

  close(fd);
  //printf("%s sent.\n", file);
  return 0;
}

int request_file(int sockfd, uint16_t fileIndex)
{
  uint32_t rq = FILE_REQUEST_COMMAND;


  if (stream_write(sockfd, &rq, sizeof rq) != sizeof rq ||
      stream_write(sockfd, &fileIndex, sizeof fileIndex) != sizeof fileIndex)
    return -1;

  //  stream_read(sockfd, filesize, sizeof filesize);

  return 0;
}


int get_file(int sockfd, const char* file, uint16_t fileIndex)
{
  
  printf("Requesting %s\n", file/*, fileIndex*/);
  if (request_file(sockfd, fileIndex))
    return -1;

  uint32_t st_size;
  uint32_t mtime;
  uint32_t st_mode;

  if ( 
      stream_read(sockfd, &st_size, sizeof st_size) != sizeof st_size ||
       stream_read(sockfd, &st_mode, sizeof st_mode) != sizeof st_mode ||
       stream_read(sockfd, &mtime, sizeof mtime) != sizeof mtime)
    {
      puts("Error reading file data.");
      return -1;
    }

  //printf("%d %X %X\n", st_size, st_mode, mtime);

  if (S_ISDIR(st_mode)) // directory?
    {
      if(mkdir(file, st_mode))
	{
	  puts("Could not create directory.");
	  return -1;
	}
    }
  else
    {
      //remove(file);
      int fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 00644);

      if (fd == -1)
	{
	  printf("Error creating %s\n", file);
	  return -1;
	}
      int nread;
      char buf[MAXBUF];
      
      uint32_t times = st_size / MAXBUF + 1;
      uint32_t remaining = st_size % MAXBUF;
      while(times)
	{
	  times--; 
	  nread = stream_read(sockfd, buf, times ? MAXBUF : remaining);
	  if (nread  != (times ? MAXBUF : remaining)) {
	    puts("Error reading\n");
	    break;
	  }
	  if (nread != write(fd, buf, nread)) 
	    {
	      printf("Error writing to %s\n", file);
	      close(fd);
	      return -1;
	    }
	}
      printf("Got file %s\n", file);
      if(fchmod(fd, st_mode))
	puts("Could not change file permissions.");
      close(fd);
    }
  
  struct utimbuf tim = {mtime, mtime};
  if(utime(file, &tim))
    {
      puts("Could not change time.");
      return -1;
    }

  return 0;
}

