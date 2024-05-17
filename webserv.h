#define MAX_PORT 65536
#define MIN_PORT 5000

#define SERVER "[Server]"

#define REQUEST_LENGTH 5000
#define MESSAGE_OFFSET 5

#define CONTENT_TYPE_PREFIX "Content-Type: "
#define CONTENT_LENGTH_PREFIX "Content-Length: "

static int DEBUG = 0;

typedef struct input_info{
  char* request_string;
  char* file_name;
  char* file_extension;
  int is_file;
  int is_image;
  int is_oven;
  int is_html;
  int is_executable;
  int is_listing;
  int empty;


  // For Histogram``
  int is_histogram;
  char* directory;


  // For Python Scripts
  int num_args;
  char** args;

}input_info;

typedef struct output_info{
  char* HTTPversion;
  char* status_code;
  char* status_message;
  char* content_type;
  char* content_length;
  int content_int;
  int empty;
  char* content;
  char* response;
}output_info;

#define OUTPUT_SERIAL 0 //means USB0

char* image_extensions[5] = {".jpg", ".jpeg", ".png", ".img", ".gif"};
int num_image_extensions = 5;
char* http_match = "HTTP/1.1";
char* ls_match = "ls";
char* html_match = ".html";


// Connects to a given port if it is available
void servConn(int port);

// Attempts to execute based on request field.
int servExecute(char* data, int sd);

// Gets the string that was in the request field.                               [First layer of parsing string]
void servGet(char* data, char* getString);

// Starts parsing the string so we can figure out what we want to do with it    [Second layter of parsing string]
void servGetFileInfo(input_info* fi, char* file_string);

// Helper function to check if we read in yet.                                  [Used in first layer of string parsing]
int servHttpMatch(char* buf);

// Checking if the input is an ls command
int servIsLs(char* str);

int servIsHTML(char* str);


void printInputInfo(input_info* fi);

int servIsImage(char* str);

int servIsArduino(char* str);


int servIsPy(char* str);

void serverSendPacket(output_info* qi, int sd);
// Updated version because it makes more sense to occasionally send the information separately.
void serverSendHeader(output_info* ii, int sd);


// Forking a child and then sending the output of the command to the webpage.
void servDoListing(input_info* ii, output_info* oi, int sd);

void servDoHTML(input_info* ii, output_info* oi, int sd);
