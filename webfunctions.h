#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include "webhelper.h"

#define OUTPUT_SERIAL 0
#define MAX_PKT_LEN 35 // One more than msg length sent from Android

void servDoListing(input_info* ii, output_info* oi, int sd);
void servDoImage(input_info* ii, output_info* oi);
void servArduino(input_info* ii, output_info* oi);
void servDoHTML(input_info* ii, output_info* oi, int sd);
void servDoFile(input_info* ii, output_info* oi, int sd);


/* Serial control type */
typedef struct sctrl {
  int connected; /* if not true, connect() must be run */
  int fd; /* File descriptor for the port */
} sctrl_t;

typedef struct {
  int sd; 			/* Socket */
  char *buf; 			/* Buffer */
  int bufsz;			/* Buffer length */

  int baud;			/* Serial device baud rate */
  int serial_port;		/* 0..4 for tty[USB][0..4] */
  sctrl_t s;			/* Serial control type */
} callback_obj_t;

static sctrl_t sc;

enum {
    FALSE,
    TRUE
};

void readSocket (int sd, char *buf, int bufsz);
int ctrlConn (sctrl_t *s, int baud, int port_num, int output);

char* constructFilePath(input_info* ii) {
  char* file_name = malloc(strlen(ii->file_name) + strlen(ii->file_extension));
  strncpy(file_name, ii->file_name, strlen(ii->file_name));
  strncpy(file_name + strlen(ii->file_name), ii->file_extension, strlen(ii->file_extension));
  return file_name;
}

void servDoFile(input_info* ii, output_info* oi, int sd){
  int i = 0, fd;

  // Creating extension string buffer to copy over
  char* extension_buffer = malloc(strlen(ii->file_extension));
  strncpy(extension_buffer, ii->file_extension + sizeof(char), strlen(ii->file_extension));

  // Creating the file_name of the file  we will try to open
  char* file_name = malloc(strlen(ii->file_name) + strlen(ii->file_extension) +1);
  strncpy(file_name, ii->file_name, strlen(ii->file_name));
  strncpy(file_name + strlen(ii->file_name), ii->file_extension, strlen(ii->file_extension));
  file_name[strlen(ii->file_name) + strlen(ii->file_extension)] = '\0';


  printf("File Path: %s\n", file_name);
  struct stat statbuf;

  if(ii->is_image){
      char* img = "image/";
      oi->content_type = malloc(strlen(ii->file_extension) + strlen(img));
      strncpy(oi->content_type, img, strlen(img));
      strncpy(oi->content_type + strlen(img) * sizeof(char), extension_buffer, strlen(extension_buffer));
  }

  // Attempting to open file. May want to adjust status code\n;
  if(stat(file_name, &statbuf) < 0){
    oi->status_code = "404";
    fprintf(stderr, "%s File: %s was not found!\n", SERVER, file_name);
    return;
  }

  char single_byte;
  int size = statbuf.st_size;
  int f_ptr_read;

  oi->content_int = size;

  if((f_ptr_read = open(file_name, O_RDONLY))){
    oi->content = calloc(1, size);
    int j = getIntSize(size, 10);
    oi->content_length = malloc(j);
    intToString(size, 10, j, oi->content_length);

    char* byte = malloc(2);
    byte[1] = 0;
    for(i = 0; i < size; i++){
      read(f_ptr_read, byte, 1);
      strncpy(oi->content + i, byte, 1);
    }
  }else{
    fprintf(stderr, "%s Failed to read file.\n", SERVER);
    return;
  }
}

void servDoListing(input_info* ii, output_info* oi, int sd){
  pid_t pid;
  int status, i;
  char** args;
  args = malloc(3 * sizeof(char*));

  args[0] = "ls";
  args[1] = "-l";
  args[2] = 0;

  oi->content_type = "text/plain";
  oi->content_length = "5000";

  serverSendHeader(oi, sd);

  pid = fork();
  if(pid == 0){
    // We are in the child
    dup2(sd, STDOUT_FILENO);
    close(sd);
    if(execvp("ls", args) != 0){
      fprintf(stderr, "%s Failed to execute ls\n", SERVER);
      exit(-1);
    }

  }else{
    close(sd);
    waitpid(pid, &status, 0);
    exit(-1);
  }
}


void servDoExecutable(input_info* request, output_info* response, int client){
  pid_t pid;
  int status, i;
  printf("Attempting to execute %s\n", constructFilePath(request));

  char commandString[100];
  if(request->num_args == 0) {
    printf("constructing single arg commandString\n");
    sprintf(commandString, "python %s%c", constructFilePath(request), '\0');
  } else {
    printf("constructing double arg commandString\n");
    sprintf(commandString, "python %s %s%c", constructFilePath(request), request->args[0], '\0');
  }

  printf("constructed commandString %s\n", commandString);
  char* args[] = {"sh\0", "-c\0", commandString, NULL};

  pid = fork();
  if(pid == 0) {
    int pipe_out = open("exectemp",  O_WRONLY|O_CREAT|O_TRUNC|O_APPEND, 0777);
    if(dup2(pipe_out, STDOUT_FILENO) < 0 || dup2(pipe_out, STDERR_FILENO) < 0){
      fprintf(stdout, "Piping failure.\n");
    }
    if(execvp("sh", args) != 0){
      fprintf(stdout, "%s Failed to execute %s\n", SERVER, request->file_name);
      exit(-1);
    }
  } else if(pid > 0) {
    waitpid(pid, &status, 0);
    printf("execvp child finished\n");

    int childResults = open("exectemp",  O_RDONLY, 0777);
    if(childResults == -1){
      fprintf(stderr, "Failed to open temp file");
    }

    struct stat statbuf;
    stat("exectemp", &statbuf);

    char single_byte;
    int size = statbuf.st_size;

    response->content_int = size;
    response->content_type = "text/plain";
    response->content = malloc(sizeof(char) * size);
    int j = getIntSize(size, 10);
    response->content_length = malloc(j);
    intToString(size, 10, j, response->content_length);

    char* byte = malloc(2);
    byte[1] = 0;
    for(i = 0; i < size; i++){
      read(childResults, byte, 1);
      strncpy(response->content + i, byte, 1);
    }


    if(!strcmp(request->file_name, "prettyprint") || !strcmp(request->file_name, "HTMLconverter")) {
      response->content_type = "text/html";
    } else if (!strcmp(request->file_name, "my-histogram")) {
      response->content_type = "image/jpeg";
    }

    close(childResults);
  }
}

void ctrlDevice (callback_obj_t *o) {
  
  char *token;
  int value;

  /* Setup serial connection to control [Arduino] device */
  ctrlConn (&(o->s), o->baud, o->serial_port, TRUE);

  while (1) {
    /* Get input from Android device */
    printf ("Reading from socket...\n");
    readSocket (o->sd, o->buf, o->bufsz);
    
    // Use strtok to tokenize buf and extract speed/dir values,
    // which can then be sent to the arduino for managing servo control
    token = strtok (o->buf, " "); // 1st token is speed
    token = strtok (NULL, " "); // 1st token is speed

    value = atoi (token);
    printf ("Speed: %d\n", value);
    write ((o->s).fd, &value, 1);
    
    token = strtok (NULL, " "); 
    token = strtok (NULL, " "); 
    
    value = atoi(token);
    printf ("Direction: %d\n", value);
    write ((o->s).fd, &value, 1);

    /* Reset buffer */
    memset (o->buf, 0, o->bufsz);
  }

  //ctrlDisconnect (&(o->s));
}

int ctrlConn (sctrl_t *s, int baud, int port_num, int output) {
  struct termios options;
  int count, ret;

  s->connected = 0; /* reset in case we fail to connect */
  s->fd = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY);

  if (s->fd == -1) 
      return 0;
  else {
      if (!output) 
	  fcntl (s->fd, F_SETFL, 0); /* Blocking fd */
      else
	fcntl (s->fd, F_SETFL, 0 /*O_NONBLOCK*/);
  }

  tcgetattr (s->fd, &options);
  cfsetispeed (&options, B9600);
	cfsetospeed (&options, B9600);

  /* Control options */
  options.c_cflag |= (CLOCAL | CREAD | HUPCL); /* enable */

  options.c_cflag &= ~PARENB; /* 8N1 */
  options.c_cflag &= ~CSTOPB;
  options.c_cflag &= ~CSIZE;
  options.c_cflag |= CS8;

  if (!output) {

      /* Disable h/w flow control */
      options.c_cflag &= ~CRTSCTS; 	

      
      /* Input options */
      options.c_iflag &= ~(IXON | IXOFF | IXANY | IMAXBEL | IUTF8 | ICRNL | IGNCR | INLCR | ISTRIP | INPCK | PARMRK | IGNPAR | BRKINT);
      options.c_iflag |= IGNBRK;


      /* Local options... */
      /* Select raw input as opposed to canonical (line-oriented) input. */
      options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG | IEXTEN | 
			   ECHOK | ECHONL | NOFLSH |
			   TOSTOP | ECHOPRT | ECHOCTL | ECHOKE);

      /* Output options */
      options.c_oflag &= ~(OPOST | OCRNL | ONLCR | ONOCR |
			   ONLRET | OFILL | OFDEL);
      options.c_oflag |= (NL0 | CR0 | TAB0 | BS0 | VT0 | FF0);

      /* Specify the raw input mininum number of characters to read in one
       * "packet" and timeout for data reception in 1/10ths of seconds. */
      options.c_cc[VMIN] = 1;
      options.c_cc[VTIME] = 5;
  }

  /* set all of the options */
  tcsetattr (s->fd, TCSANOW, &options);

  s->connected = 1;

  return 1;
}