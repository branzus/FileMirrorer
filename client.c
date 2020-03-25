#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include "netio.h"
#include "filester.h"


#define SERVER_PORT     5678


/* 
 *isPresentOnServer: checks if a file is present on the server version and returns the index of the file. 
 * Otherwise returns -1.
 */
int file_in_list(file_info local_file, file_info* server_files, int length)
{
  for(int i=0; i<length; i++)
    {
      int res = strcmp(local_file.path, server_files[i].path);
      if( res == 0)
	return i;
      else if (res < 0)
	break;
    }  

  return -1;
}

/*
 * dateModified: returns 0 if the file on the server has the same timestamp as the local one
 * and the difference otherwise.
 */
int dateModified(file_info local, file_info server)
{
  if(S_ISDIR(local.st_mode) && S_ISDIR(server.st_mode))
    return 0;
  return local.timestamp - server.timestamp;
}

/*
 * sizeDifferent: returns 0 if the file on the server has the same size ad the local one
 * and an the difference otherwise
 */
int sizeDifferent(file_info local, file_info server)
{
  return local.size - server.size;
}

/* 
 *isOnClient: checks if a file from the server is present on the client version and returns it. 
 * Otherwise returns null.
 */
/*
file_info* isOnClient(file_info server_file, file_info* local_files, int length)
{
  for(int i=0; i<length; i++){
    int res = strcmp(server_file.path, local_files[i].path);
    if( res == 0)
      return &local_files[i];

  }
  
  return NULL;
}
*/



int main(int argc, char** argv)
{
  int sockfd;
  char root[2048];
  struct sockaddr_in local_addr;


  if(argc < 4)
    {
      printf("Please call: %s <directory> <ip> <port> [-d]\n", argv[0]);
      puts("-d \t delete local files missing on server");
      exit(0);
    }

  int delete = 0;
  if (argc > 4 && !strcmp(argv[4], "-d"))
    delete = 1;
  else {
    puts("Delete flag not specified.\n");
  }
  
  strncpy(root, argv[1], sizeof(root));

  sockfd = socket(PF_INET, SOCK_STREAM, 0);
  if(sockfd == -1){
    mperror("socket()");
  }

  set_addr(&local_addr, NULL, INADDR_ANY, 0);

  // We also use bind on client because we want to connect on a specific port.
  if(bind(sockfd, (struct sockaddr *)& local_addr , sizeof(local_addr)))
    mperror("bind()");
  
  int port = atoi(argv[3]);
  if (set_addr(&local_addr, argv[2], 0, port) == -1)
    merror("Unable to get client address");

  if(connect(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr)) == -1)
    mperror("connect()");
  
  uint32_t length = 0;
  uint32_t max_length = 100;

  file_info *local_version = malloc(max_length * sizeof(file_info));
  
  if(chdir(root))
    {
      puts(root);
      mperror("chdir()");
    }
  uint32_t server_files_length;
  file_info* server_files;
  
  if(get_filelist(sockfd, &server_files, &server_files_length))
    merror("Can't get filelist from server.\n");

  getFiles(&local_version, ".", &length, &max_length);
  qsort(local_version, length, sizeof(file_info), cmp);
  
  //qsort(server_files, server_files_length, sizeof(file_info), cmp);

  puts("Filelist on server: ");
  for(int i=0; i<server_files_length; i++){
    puts(server_files[i].path);
    //printf("%X %X\n", server_files[i].size, server_files[i].timestamp);
  }
  puts("\n------------\n");
  puts("Local filelist: ");
  for(int i=0; i<length; i++){
    puts(local_version[i].path);
    //printf("%X %X\n", local_version[i].size, local_version[i].timestamp);
  }

  printf("\n");

  // Delete local files that are not on server or get them if they are modified.
  int fileOnServer;
  for(int i=0; i<length; i++)
    {

      if( (fileOnServer = file_in_list(local_version[i], server_files, server_files_length)) == -1)
	{
	  if (delete)
	    {
	      printf("Deleting %s\n", local_version[i].path);
	      if(remove_file(local_version[i].path) != 0)
		printf("Could not delete file: %s\n", local_version[i].path);
	    }
	  else
	    {
	      printf("Not on server: %s\n", local_version[i].path);
	    }
	}
      else
	{
	  if(dateModified(local_version[i], server_files[fileOnServer]) || sizeDifferent(local_version[i], server_files[fileOnServer])){
	    remove_file(server_files[fileOnServer].path);
	    get_file(sockfd, server_files[fileOnServer].path, fileOnServer);
	  }
      
	}
      
    }

  // Get the files from the server that are not on client.
  for(int i=0; i<server_files_length; i++){

    if( file_in_list(server_files[i], local_version, length)  == -1)
      {
	get_file(sockfd, server_files[i].path, i);
      }
  }

  close(sockfd);  
}
