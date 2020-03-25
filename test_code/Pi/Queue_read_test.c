/*
 * Test file
 */

#include <stdarg.h>
#include <limits.h>     /* for CHAR_BIT */
#include <stdio.h>
#include <cstdlib>
//#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <RF24/RF24.h>
#include <syslog.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <sys/socket.h> /* socket, connect */
#include <arpa/inet.h>
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netinet/tcp.h>
#include <signal.h>

using namespace std;

const unsigned long file_read_delay = 100000; // delay between checks of queue file on us
unsigned int total_queue_read = 0;
bool running = true;
const char* file_queue_input = "/tmp/queue.txt";
const char* file_queue_process = "/tmp/queue_in_progress.txt";

int read_queue_from_file () {
  FILE* queue_file;
  int p1;
  int p2;
  int p3;
  
  if (0 != access(file_queue_input, 0)) { return 1;} 
  rename (file_queue_input, file_queue_process);
  queue_file = fopen (file_queue_process,"r");
  while (!feof (queue_file))
  {
    if (fscanf (queue_file, "%d %d %d", &p1, &p2, &p3) < 3) {continue;};
    total_queue_read++;
  } // end of while reading queue file
  fclose (queue_file);
  remove (file_queue_process);
}

void signal_callback_handler(int signum) {
   running = false;
}

int main (int argc, char** argv)
{
  signal(SIGINT, signal_callback_handler);
  signal(SIGTERM, signal_callback_handler);
  signal(SIGQUIT, signal_callback_handler);
  while (running)
  {
    read_queue_from_file ();
    usleep (file_read_delay);
  } //main while loop
  printf ("Finished with %d queue items read: ",total_queue_read);
} //main end

