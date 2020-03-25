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

#include "netio.h"
#include "filester.h"



int main(int argc, char* argv[]){
  int sockfd, connfd;
  struct sockaddr_in local_addr, rmt_addr;
  socklen_t rlen = sizeof(rmt_addr);
  
  if (argc < 3){
    printf("Please call: %s <directory> <port>\n", argv[0]);
    exit(1);
  }
  
  if(chdir(argv[1]))
    {
      mperror("Error chdir()");
    }
  
   
  // PF_INET=TCP/IP, 0=implicit.
  sockfd = socket(PF_INET, SOCK_STREAM, 0);

  if(sockfd == -1){
    mperror("Error socket()");
  }
  int port = atoi(argv[2]);
  // Set the server to listen to all interfaces on the selected port.
  if (set_addr(&local_addr, NULL, INADDR_ANY, port) == -1)
    merror("Unable to get server address");
  
  if (bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr)) == -1)
    mperror("Error bind()");
  
  if (listen(sockfd, 5) == -1)
    mperror("Error socket()");

  while(1){
    
    // Create a new socket for each connection.
    connfd = accept(sockfd, (struct sockaddr *)&rmt_addr, &rlen);
    
    if(connfd == -1)
      {
	perror("accept()");
	continue;
      }
    
    pid_t pid = fork();

    if(pid == 0) // new process
      {
	puts("New connection.");
	uint32_t length = 0;
	uint32_t max_length = 2000;

	file_info *files = malloc(max_length * sizeof(file_info));

	getFiles(&files, ".", &length, &max_length);
	puts("Done");
	qsort(files, length, sizeof(file_info), cmp);
  

	if (send_filelist(connfd, files, length))
	    mperror("Could not send filelist\n");

	uint32_t command=0;
	uint16_t index;
	
	while(sizeof(command) == stream_read(connfd, &command, sizeof command) &&
	      command == FILE_REQUEST_COMMAND)
	  {
	    if(stream_read(connfd, &index, sizeof index) != sizeof index)
	      {
		break;
	      }
	    
	    printf("Sending: %s\n", files[index].path);
	    if(send_file(connfd, files[index].path))
	      break;
	  }
	
	puts("Connection ending.");
	
	free(files);   
	close(connfd);
	exit(0);
      }
    else if(pid == -1) // fork error
      {
	perror("fork()");
      }
    else  // parent process
      {
	close(connfd);
      }
  }

  //should not get here
  close(sockfd);
  exit(0);

  return 0;
}
