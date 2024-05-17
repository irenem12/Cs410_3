#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include "webserv.h"
#include "webfunctions.h"
static sctrl_t sc;

void AduinoservConn (int port, void (*callback)(callback_obj_t *obj), callback_obj_t *arg);

int main(int argc, char* argv[]) {
  if(argc != 2) {
    fprintf(stderr, "Error: Must specify port.\n");
    return -1;
  }

  int port = atoi(argv[1]);
  if(port < MIN_PORT || port > MAX_PORT){
    fprintf(stderr, "Error: Must specify port in range %d-%d\n", MIN_PORT, MAX_PORT);
    return -1;
  }

  servConn(port);	/* start server on given port. */
  return 0;
}

void servConn(int port) {

  int sd, new_sd;
  struct sockaddr_in name, cli_name;
  int sock_opt_val = 1;
  socklen_t cli_len;
  char data[REQUEST_LENGTH];		/* Our receive data buffer. */

  //set server reciever socket properties
  if ((sd = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("(servConn): socket() error");
    exit (-1);
  }

  if (setsockopt (sd, SOL_SOCKET, SO_REUSEADDR, (char *) &sock_opt_val,
		  sizeof(sock_opt_val)) < 0) {
    perror ("(servConn): Failed to set SO_REUSEADDR on INET socket");
    exit (-1);
  }

  name.sin_family = AF_INET;
  name.sin_port = htons (port);
  name.sin_addr.s_addr = htonl(INADDR_ANY);

  printf("server starting...\n");
  if (bind (sd, (struct sockaddr *)&name, sizeof(name)) < 0) {
    perror ("(servConn): bind() error");
    exit (-1);
  }

  listen (sd, 20);

  //loop on server port, spinning off new socket and child for each new request
  for (;;) {
    //load new request into new_sd socket
    new_sd = accept (sd, (struct sockaddr *) &cli_name, &cli_len);


    if (DEBUG) printf ("Assigning new socket descriptor:  %d\n", new_sd);

    if (new_sd < 0) {
      perror ("(servConn): accept() error");
      exit (-1);
    }

    if (fork () == 0) {	/* Child process. */
      read (new_sd, &data, REQUEST_LENGTH); /* Read our string: "Hello, World!" */
      servExecute(data, new_sd);
      shutdown(new_sd, SHUT_WR);
      close (sd);
      exit (0);
    }
  }
}

int servExecute(char* data, int sd){
  char* requestString = malloc(50 * sizeof(char));
  servGet(data, requestString);

  input_info* fi = malloc(sizeof(input_info));
  output_info* qi = malloc(sizeof(output_info));

  qi->HTTPversion = "HTTP/1.1";
  qi->status_code = "200";
  qi->status_message = "OK";

  servGetFileInfo(fi, requestString);
  printInputInfo(fi);

  // Implementing logic to figure out what we need to do.

  if(fi->is_file){
    if(fi->is_executable){
      servDoExecutable(fi, qi, sd);
    } else {
      servDoFile(fi, qi, sd);
      if(fi->is_html){
        qi->content_type = "text/html";
      }
      // qi->content_type = "text/html";
    }
  }else {
    if(fi->is_listing){
      servDoListing(fi, qi, sd);
    }else if(fi->is_histogram){
      printf("Should do histogram of: %s\n", fi->directory);
    } else {
      qi->status_code = "501";
      fprintf(stderr, "Functionality not supported");
    }
  }
  serverSendPacket(qi, sd);

  shutdown(sd, SHUT_RD); //needed? so server can't send requests anymore
  return 1;

}

void servGetFileInfo(input_info* fi, char* file_string){
  int length=strlen(file_string)-1, found_file=0, i=0;

  int pyFlag = 0, arduinoFlag = 0;

  fi->file_name = malloc(length * sizeof(char));
  fi->file_extension = malloc(length * sizeof(char));

  while(!found_file && i < length){
    if(file_string[i] == '.'){
      fi->file_name[i] = '\0';
      found_file = 1;
      break;
    }else if(file_string[i] == '?'){
      i+=5;
      fi->is_histogram = 1;
      fi->directory = malloc(length - i);
      int count = 0;
      while(i < length){
        printf("%c\n", file_string[i]);
        fi->directory[count] = file_string[i];
        i++;count++;
      }
    }
    fi->file_name[i] = file_string[i];
    i++;
  }

  fi->is_file = found_file;

  while(i < length){
    if(file_string[i] == '?'){
      pyFlag = servIsPy(fi->file_extension);
      arduinoFlag = servIsArduino(fi->file_extension);
      fi->is_executable = pyFlag | arduinoFlag;
      break;
    }
    fi->file_extension[i - strlen(fi->file_name)] = file_string[i];
    i++;
  }

  fi->is_image = servIsImage(fi->file_extension);
  fi->is_html = servIsHTML(fi->file_extension);
  fi->is_executable = servIsPy(fi->file_extension);
  if(length <= 0 || servIsLs(fi->file_name)){
    fi->is_listing = 1;
  }else{
    fi->is_listing = 0;
  }
  int index, arg_index = 0;
  if(pyFlag == 1 || arduinoFlag == 1){
    fi->args = calloc(length/2, sizeof(char*));
    while(i<length){
      if(file_string[i] == '='){
        i++;
        fi->args[arg_index] = malloc(length);
        index = 0;
        while(i < length && file_string[i] != '?' && file_string[i] != '\0'){
          fi->args[arg_index][index] = file_string[i];
          i++;
          index++;
        }
        fi->args[arg_index][index] = '\0';
        arg_index++;
      }
      i++;
    }
    fi->num_args = arg_index;
  }


}

int servIsArduino(char* str){
  if(strcmp(str, ".arduino") == 0){
    printf("Is ARDUINO\n");
    return 1;
  }

  int option;
  static callback_obj_t obj;

  /* Defaults that can be over-ridden from the command-line */
  int baud = 9600;
  int tcp_port = 5060;	     /* Default */
  int output_port = OUTPUT_SERIAL; /* /dev/ttyUSB0 */

  obj.baud = baud;
  obj.serial_port = output_port; 
  obj.s = sc;
  AduinoservConn (tcp_port, ctrlDevice, &obj);
  return 0;
}

void AduinoservConn (int port, void (*callback)(callback_obj_t *obj), callback_obj_t *arg) {

  int sd, new_sd;
  struct sockaddr_in name, cli_name;
  int sock_opt_val = 1;
  int cli_len;
  static char data[MAX_PKT_LEN];		/* Our receive data buffer. */
  
  if ((sd = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("(servConn): socket() error");
    exit (-1);
  }

  if (setsockopt (sd, SOL_SOCKET, SO_REUSEADDR, (char *) &sock_opt_val,
		  sizeof(sock_opt_val)) < 0) {
    perror ("(servConn): Failed to set SO_REUSEADDR on INET socket");
    exit (-1);
  }

  name.sin_family = AF_INET;
  name.sin_port = htons (port);
  name.sin_addr.s_addr = htonl(INADDR_ANY);
  
  if (bind (sd, (struct sockaddr *)&name, sizeof(name)) < 0) {
    perror ("(servConn): bind() error");
    exit (-1);
  }

  listen (sd, 5);

  for (;;) {
      cli_len = sizeof (cli_name);
      new_sd = accept (sd, (struct sockaddr *) &cli_name, &cli_len);
      printf ("Assigning new socket descriptor:  %d\n", new_sd);
      
      if (new_sd < 0) {
	perror ("(servConn): accept() error");
	exit (-1);
      }

      if (fork () == 0) {	/* Child process */
	close (sd);

	arg->sd = new_sd;
	arg->buf = data;
	arg->bufsz = sizeof(data);
	
	(*callback)(arg);		/* Invoke callback */
	exit (0);
      }
  }
}

void readSocket (int sd, char *buf, int bufsz) {
  
  read (sd, buf, bufsz);
  printf ("%s\n", buf);

}

int servIsPy(char* str){
  if(strcmp(str, ".py") == 0){
    return 1;
  }
  return 0;
}

int servIsImage(char* str){
  int i;
  for(i = 0; i < num_image_extensions; i++){
    if(strcmp(str, image_extensions[i]) == 0){
      return 1;
    }
  }
  return 0;
}

int servIsLs(char* str){
  if(strcmp(str, ls_match) == 0){
    printf("Is LS!\n");
    return 1;
  }
  return 0;
}

int servIsHTML(char* str){
  if(strcmp(str, html_match) == 0){
    printf("Is HTML!\n");
    return 1;
  }
  return 0;
}

void servGet(char* data, char* return_string){
  int i, j;
  char buf[9];
  memset(buf, 0, 9);
  while(!servHttpMatch(buf)){
    return_string[i] = data[i + MESSAGE_OFFSET];
    i++;
    for(j = 0; j < strlen(http_match) && i + j + MESSAGE_OFFSET < REQUEST_LENGTH; j++){
      buf[j] = data[i+MESSAGE_OFFSET + j];
    }
  }
  return_string[i] = 0;
}

int servHttpMatch(char* buf){
  return !strcmp(buf, http_match);
}

void printInputInfo(input_info* ii){
  int i;
  printf("File Name: %s\nFile Extension: %s\n", ii->file_name, ii->file_extension);
  printf("Is Image: %d\n", ii->is_image);
  printf("Is File: %d\n", ii->is_file);
  printf("Is Executable: %d\n", ii->is_executable);
  printf("Arg count: %d\n", ii->num_args);
  for(i = 0; i < ii->num_args; i++) {
    printf("Args[%d]: %s\n", i, ii->args[i]);
  }
  printf("\n");
}

void serverSendHeader(output_info* ii, int sd){
  ii->response = malloc(REQUEST_LENGTH + ii->content_int);
  memset(ii->response, 0, REQUEST_LENGTH + ii->content_int);

  // HTTP Version   |    status_code    | status_message
  sprintf(ii->response, "%s %s %s\n"
                        "Content-Type: %s\n"
                        "Content-Length: %s\n\n"
                        ,ii->HTTPversion, ii->status_code, ii->status_message, ii->content_type, ii->content_length);

  write(sd, ii->response, strlen(ii->response));

  // printf("Sent: \n%s\n", ii->response);

}

void serverSendPacket(output_info* r, int sd){
  // printf("Sending Packet\n");
  serverSendHeader(r, sd);
  write(sd, r->content, r->content_int);
  // printf("Sent: %d\n%s\n%d\n", r->content_int, r->content, r->content_int);

}
