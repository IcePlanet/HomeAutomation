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

// BELOW Based on LamPi-2.0/livolo from https://github.com/platenspeler/LamPI-2.0/tree/master/transmitters/livolo 

// Class removed as it was causing problems

// ABOVE Based on LamPi-2.0/livolo from https://github.com/platenspeler/LamPI-2.0/tree/master/transmitters/livolo 
