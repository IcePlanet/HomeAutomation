/*
 * Sever for home automation
 */

#include "CommonAutomation.h"
#include "/usr/local/include/RF24/RF24.h"
#include <stdarg.h>
#include <sys/socket.h> /* socket, connect */
#include <arpa/inet.h>
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netinet/tcp.h>
//#include <netdb.h> /* struct hostent, gethostbyname */

#include <limits.h>     /* for CHAR_BIT */

#define BITMASK(b) (1 << ((b) % CHAR_BIT))
#define BITSLOT(b) ((b) / CHAR_BIT)
#define BITSET(a, b) ((a)[BITSLOT(b)] |= BITMASK(b))
#define BITCLEAR(a, b) ((a)[BITSLOT(b)] &= ~BITMASK(b))
#define BITTEST(a, b) ((a)[BITSLOT(b)] & BITMASK(b))
#define BITNSLOTS(nb) ((nb + CHAR_BIT - 1) / CHAR_BIT)

using namespace std;

// Hardware configuration
// Radio CE Pin, CSN Pin, SPI Speed
// Physical pin numbers version
 RF24 radio(RPI_V2_GPIO_P1_22, BCM2835_SPI_CS0, BCM2835_SPI_SPEED_8MHZ);
// GPIO pin numbers version
// RF24 radio(RPI_V2_GPIO_P1_25, BCM2835_SPI_CS0, BCM2835_SPI_SPEED_8MHZ);

/*
 * Real connection is:
 * PIN #     GPIO     ALT NAME   NRF PIN     NRF NAME
 * 20        NONE     GROUND     1           GND 
 * 17        NONE     3.3 V      2           VCC
 * 22        25                  3           CE
 * 24        8        SPI0_CE0_N 4           CSN
 * 23        11       SPI0_SCLK  5           SCK
 * 19        10       SPI0_MOSI  6           MOSI
 * 21        9        SPI0_MISO  7           MISO
 */

/* ORDERS DEFINITION
 *  0 (0000) - all stop
 *  1 (0001) - Voltage (in payload)
 *  2 (0010) - Temp (in payload)
 *  3 (0011) - Rain (in payload)
 *  4 (0100) - engine 1 stop
 *  5 (0101) - engine 1 down
 *  6 (0110) - engine 1 up
 *  8 (1000) - engine 2 stop
 *  9 (1001) - engine 2 down
 * 10 (1010) - engine 2 up
 * 12 (1100) - engine 3 stop
 * 13 (1101) - engine 3 down
 * 14 (1110) - engine 3 up
 * 15 (1111) - Light (in payload)
 */

/* TARGET
 *  0 - BROADCAST
 */

 // Broadcast ID
const unsigned char broadcast_id = 255;

// RADIO SETTINGS
bool radio_initialized=false; //Identifies if radio.begin was done
bool radio_up=false; //Identifies if radio is powered up
const uint8_t pipes[][6] = {"1Node","2Node"};
union Frame {
    unsigned long frame;
    struct {
        unsigned char order;
        unsigned char payload2;
        unsigned char payload1;
        unsigned char target;
    } d;
};
const unsigned char ack_bit_position = 7; // Location of ack bit
const unsigned int receive_loop_cycle_wait = 20000; // delay in us during one receive loop cycle
const unsigned int send_loop_cycle_wait = 10; // delay in ms during one send loop
const unsigned int send_loop_duration = 10; //duration of send attempt in seconds
const unsigned int send_loop_sending_duration = 30; // duration of send itself measured value
const unsigned int send_loop_cycles = send_loop_duration * 1000 / (send_loop_cycle_wait + send_loop_sending_duration); // number of cycles needed for one send
const unsigned int send_loop_end_sleep = 100; // delay in ms after send before next send is processed
const unsigned int send_ack_received_delay = 1000; // delay after ack was received
unsigned int sleep_time = send_loop_cycle_wait * 1000; // technical calculation

// PAYLOAD MASKS
// const unsigned char mask_retransfer_send = 0b01000000;
const unsigned char mask_retransfer_send = 0b00000000;
const unsigned char mask_ack = 0b10000000;
const unsigned char mask_engines = 0b00010000;
const unsigned char mask_voltage = 0b00100000;
const unsigned long mask_long_ack = 128;
const unsigned long mask_long_retransfer = mask_retransfer_send;

// LOG SETTINGS
const int log_level = 999; //Log level, the lower number the more priority log, so lower level means less messages
const char* timeformat = "%Y%m%d%H%M%S";
const bool log_to_screen = true;
const bool log_to_syslog = false;
const unsigned int log_level_emergency = 100;
const unsigned int log_level_alert = 200;
const unsigned int log_level_critical = 300;
const unsigned int log_level_error = 400;
const unsigned int log_level_warning = 500;
const unsigned int log_level_notice = 600;
const unsigned int log_level_info = 700;
const unsigned int log_level_debug = 800;
char log_time [21];
char log_prefix [100];
char log_content [1000];
int log_syslog_priority;

// QUEUE
struct queue_node { union Frame f; struct queue_node* queue_next; };
struct queue_node* queue_front = NULL;
struct queue_node* queue_rear = NULL;
struct QueueFrames { queue_node* start; queue_node* end; bool ok; union Frame t; } send_queue ;
union Frame empty_frame; // Do not forget to define as first item in the code !
const int max_queue_items_processed = 3; // Maximal number of items procesed before checking file again

// FILE FROM OPEN HAB
const char* file_queue_openhab = "/tmp/RF24_queue.txt";
const char* file_queue_process = "/tmp/RF24_in_progress.txt";
const unsigned long file_read_delay = 100000; // delay between checks of queue file on us

// OPEN HAB CONNECTION REST
const char *OH_LIGHT = "light";
const char *OH_VOLTAGE = "voltage";
const char *OH_UNKNOWN = "UNKNOWN";
const char *OH_SOUTH_FF = "southff";
const char *OH_IP = "192.168.2.222";
const short unsigned int OH_PORT = 8080;
const char *OH_PATH = "/rest/items/";
struct sockaddr_in sin = { 0 };
int sock;
int garbage;
int opt=1;
char * oh_tpl = (char *)"POST %s%s HTTP/1.0\r\n"
        "Content-Type: text/plain\r\n"
        "Accept: application/json\r\n"
        "Content-Length: %d\r\n"
        "\r\n"
        "%s";

int log_message (int level, int detail, const char* message, ...) {
  struct timeval tv;
  time_t t = time(NULL);
  if (log_to_screen || log_to_syslog ) {
    gettimeofday (&tv,NULL);
    t = tv.tv_sec;
    strftime (log_time,20,timeformat,localtime (&t));
    if (detail == 1) {snprintf (log_prefix, 99, "%s:%d ",log_time,level);} // standard print
    if (detail == 2) {snprintf (log_prefix, 99, "%s:%d:%lu ",log_time,level,tv.tv_usec);} // include us 
    if (detail == 3) {snprintf (log_prefix, 99, "%s:%d:%lu ",log_time,level,tv.tv_usec/1000);} // include ms
    if (detail > 10) {snprintf (log_prefix, 99, "%s:%d:%lu ",log_time,level,tv.tv_usec/detail);} // divide by detail 
    va_list args;
    va_start (args, message);
    vsnprintf (log_content, 999, message, args);
    va_end (args);
  }
  if (log_to_screen) {
    if (level <= log_level)
    {
      printf ("%s%s",log_prefix, log_content);
    }
  }
  if (log_to_syslog) {
    if (level >= log_level_debug) {
      log_syslog_priority = LOG_DEBUG;
    }
    else if (level >= log_level_info) {
      log_syslog_priority = LOG_INFO;
    }
    else if (level >= log_level_notice) {
      log_syslog_priority = LOG_NOTICE;
    }
    else if (level >= log_level_warning) {
      log_syslog_priority = LOG_WARNING;
    }
    else if (level >= log_level_error) {
      log_syslog_priority = LOG_ERR;
    }
    else if (level >= log_level_critical) {
      log_syslog_priority = LOG_CRIT;
    }
    else if (level >= log_level_alert) {
      log_syslog_priority = LOG_ALERT;
    }
    else {
      log_syslog_priority = LOG_EMERG;
    }
    syslog (log_syslog_priority, "%s%s", log_prefix, log_content);
  }
}

bool uq_init (struct QueueFrames *x) {
	(*x).start = (*x).end = NULL;
  (*x).ok = true ;
  return (*x).ok ;
}

bool uq_enqueue (struct QueueFrames *x) {
	struct queue_node* temp =  (struct queue_node*)malloc(sizeof(struct queue_node));
	(*temp).f = (*x).t; 
	temp->queue_next = NULL;
  (*x).ok = true;
	if((*x).start == NULL /*&& queue_rear == NULL*/){ (*x).start = (*x).end = temp; return (*x).ok;}
	(*(*x).end).queue_next = temp;
	(*x).end = temp;
  return (*x).ok ;
}

bool uq_dequeue (struct QueueFrames *x) {
  struct queue_node* temp;
  (*x).t = empty_frame;
	if((*x).start == NULL) { log_message (410,1,"Error in queue processing, out of bounds\n"); (*x).ok = false; return (*x).ok; }
  (*x).t = (*(*x).start).f ;
  temp = (*x).start;
	if((*x).start == (*x).end) { (*x).start = (*x).end = NULL;}
	else { (*x).start = (*(*x).start).queue_next;	}
	free(temp);
  (*x).ok = true;
	return (*x).ok;
}

bool uq_item (struct QueueFrames *x) {
	if((*x).start == NULL) { log_message (410,1,"Error in queue processing, out of bounds\n"); (*x).ok = false; return (*x).ok; }
  (*x).t = (*(*x).start).f ;
  (*x).ok = true;
	return (*x).ok;
}

bool uq_info (struct QueueFrames *x) {
	if((*x).start == NULL) { return false; }
	return true;
}

void uq_log (struct QueueFrames *x) {
  struct queue_node* temp = (*x).start;
	int i = 0;
	log_message (900,1,"Queue elements: ");
	while(temp != NULL) { log_message (900,0,"%lu -> ",temp->f.frame);	temp = temp->queue_next; i++; }
	log_message (900,0,"NULL (%d)\n",i);
}

void uq_clean (struct QueueFrames *x) {
	uq_log (x);
	log_message (900,1,"Queue cleaning ");
	while ((*x).start != NULL) { uq_dequeue(x); log_message (900,1,"."); }
	log_message (900,1," cleaned\n");
	uq_log (x);
}

void enqueue(unsigned long x) {
	struct queue_node* temp =  (struct queue_node*)malloc(sizeof(struct queue_node));
	(*temp).f.frame = x; 
	temp->queue_next = NULL;
	if(queue_front == NULL /*&& queue_rear == NULL*/){ queue_front = queue_rear = temp; return;}
	queue_rear->queue_next = temp;
	queue_rear = temp;
}

unsigned long dequeue() {
	struct queue_node* temp = queue_front;
	unsigned long return_value;
	if(queue_front == NULL) { log_message (410,1,"Error in queue processing, out of bounds\n"); return 0; }
	if(queue_front == queue_rear) { queue_front = queue_rear = NULL;}
	else { queue_front = queue_front->queue_next;	}
	return_value = (*temp).f.frame;
	free(temp);
	return return_value;
}

unsigned long queue_item() {
	if(queue_front == NULL) { log_message (410,1,"Error in queue processing, out of bounds\n"); return 0; }
	return (*queue_front).f.frame;
}

bool queue_info() {
	if(queue_front == NULL) { return false; }
	return true;
}

void queue_log() {
	struct queue_node* temp = queue_front;
	int i = 0;
	log_message (900,1,"Queue elements: ");
	while(temp != NULL) { log_message (900,0,"%lu -> ",(*temp).f.frame);	temp = temp->queue_next; i++; }
	log_message (900,0,"NULL (%d)\n",i);
}

void queue_clean() {
	queue_log ();
	log_message (900,1,"Queue cleaning ");
	while (queue_front != NULL) { dequeue(); log_message (900,1,"."); }
	log_message (900,1," cleaned\n");
	queue_log ();
}

void send_to_open_hab (unsigned char source, unsigned char type, unsigned char value) {
  char open_hab_type [32];
  char open_hab_id [32];
  char sensor_name[100];
  char message [1024];
  char new_value_as_text [10];
  switch (type)
  {
    case 2 :
      snprintf (open_hab_type, 31,"%s", OH_LIGHT);
      break ;
    case 32 :
      snprintf (open_hab_type, 31,"%s", OH_VOLTAGE);
      break ;
    default :
      log_message (570,1,"W: Unknown type %u (source %u value %u)\n",type, source, value);
      return;
  }
  switch (source)
  {
    case 10 :
      snprintf (open_hab_id, 31,"%s", OH_SOUTH_FF);
      break ;
    default :
      log_message (570,1,"W: Unknown source %u (type %u value %u)\n",source, type, value);
      return;
  }
  snprintf (sensor_name, 99, "S_%s_%sxINRAW", open_hab_type, open_hab_id);
  log_message (880,1,"D: Detected sensor is %s from %u, %u\n",sensor_name, type, source);
  // https://stackoverflow.com/questions/7901945/c-open-socket-on-a-specific-ip
  // curl -X POST --header "Content-Type: text/plain" --header "Accept: application/json" -d "47" "http://192.168.2.222:8080/rest/items/S_light_southffxINRAW"
  sin.sin_family = AF_INET;
  sin.sin_port = htons (OH_PORT);
  //inet_aton(OH_IP, &sin.sin_addr);
  sin.sin_addr.s_addr = inet_addr(OH_IP);
  //sock = socket(AF_INET, SOCK_STREAM, 0);
  sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  log_message (880,1,"D: Socket for %s open as %d\n",sensor_name, sock);
  setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(int));
  log_message (880,1,"D: Socket for %s options set %d\n",sensor_name, sock);
  //bind(sock, (struct sockaddr *) &sin, sizeof(sin));
  connect(sock, (struct sockaddr *)&sin, sizeof(struct sockaddr_in));
  log_message (880,1,"D: Socket for %s connect %d\n",sensor_name, sock);
  snprintf (new_value_as_text, 9, "%u", (unsigned) value);
  snprintf (message, 1023, oh_tpl, OH_PATH, sensor_name, (int) strlen(new_value_as_text), new_value_as_text);
  log_message (880,1,"D: Socket for %s message ready as %d/%s\n",sensor_name, sock, message);
  garbage = send(sock, message, strlen(message), 0);
  log_message (880,1,"D: Socket for %s send %d/%s\n",sensor_name, sock, message);
  close(sock);
}

void decodeMessage (unsigned long to_decode) {
  union Frame t;
  t.frame = to_decode;
  if ((t.d.order & mask_voltage) != 0) { // Voltages message
    log_message (720,1,"D: %lu decoded as %u (p1) and %u (p2) \n",to_decode,t.d.payload1, t.d.payload2);
    // TODO this is wrong as currently arduino is not sending own ID so we do not know from which arduino information was received, will be p1 in future
    send_to_open_hab (10,2,t.d.payload2);
  }  // Voltages message
  else {log_message (500,1,"Decoder %lu dropped as unknown\n",to_decode);}
  return;
} // decode message

int start_radio () {
  if (!radio_initialized)
  {
    radio.begin ();
    radio_initialized = true;
    radio.openWritingPipe(pipes[0]);
    radio.openReadingPipe(1,pipes[1]);
  }
  if (!radio_up)
  {
    radio.powerUp ();
    radio_up = true;
  }
  radio.startListening ();
  return 0;
}

int stop_radio () {
  if (radio_initialized && radio_up)
  {
    radio.powerDown ();
    radio_up = false;
  }
  return 0;
}

unsigned long send_msg (unsigned long long_message) {
  struct timeval tv;
  unsigned long send_start;
  unsigned int i = 0;
  bool sending = 1;
  bool ack_received = false;
  unsigned long content_ack = (long_message | mask_long_ack) ;
  unsigned long content = long_message;
  unsigned long received = 0;
  gettimeofday (&tv,NULL);
  send_start = tv.tv_sec;
  log_message (800,2,"%lu = Sending message %lu expected ack %lu starting at %lu\n",content, content,content_ack,tv.tv_sec);
  do {
    i++;
    gettimeofday (&tv,NULL);
    log_message (990,2,"%lu - Send attempt %u at %lu from %lu\n",content,i,tv.tv_sec,send_start);
    radio.stopListening ();
    radio.write (&content, sizeof (content));
    radio.startListening ();
    usleep (sleep_time);
    while (radio.available ()) {
      radio.read (&received,sizeof (unsigned long));
      received = received | mask_long_retransfer;
      if (received == content_ack)
        {log_message (750,2,"%lu - Send attempt %u at %lu from %lu successfully received ack %lu\n",content,i,tv.tv_sec,send_start,received); ack_received = true; usleep (send_ack_received_delay);}
      else {decodeMessage (received);} // TODO replace with queue processing
    }  // while end
  } while ((i < send_loop_cycles) && !ack_received);
  usleep (send_loop_end_sleep);
}

unsigned long receive_msg (unsigned int receive_loops, unsigned long waiting_for) {
  unsigned int i = 1;
  unsigned long received = 0;
  log_message (850,1,"Receive loop started %u executions\n",receive_loops);
//  radio.startListening ();
  for (i = 0; i < receive_loops; i++)
  {
      if (!radio.available ()) {usleep (receive_loop_cycle_wait);};
      while (radio.available ()) {
      radio.read (&received,sizeof (unsigned long));
      received = received | mask_long_retransfer;
      log_message (750,2,"Received %lu in loop %u\n",received,i);
      if ((received == waiting_for) && (waiting_for != 0))
        {return (received);}
      else
        {decodeMessage (received);}
      }  // While radio.available cycle
//    log_message (990,2,"Loop %u\n",i);
  }//receive for cycle
  return 0;
} // receive_msg

int read_queue_from_file () {
  FILE* queue_file;
  int i = 0;
  int destination;
  int engine;
  int order;
  union Frame tmp_msg ;
  //fist check existence of file
  if (0 != access(file_queue_openhab, 0)) { log_message (900,1,"File %s did not exists, no queue read\n",file_queue_openhab); return 1;} 
  //block file for processing
  rename (file_queue_openhab, file_queue_process);
  queue_file = fopen (file_queue_process,"r");
  log_message (900,1,"File %s open\n",file_queue_process);
  while (!feof (queue_file))
  {
    i++;
    log_message (900,1," - Loop %i "); 
    if (fscanf (queue_file, "%d %d %d", &destination, &engine, &order) < 3) {log_message (900,1,"File %s read %i was not successful, less than 2 required items read\n",file_queue_process,i); continue;};
    log_message (900,1," read destination %i engine %i order %i, file end: %s\n",destination, engine, order, feof (queue_file) ? "true" : "false");
    log_message (900,1,"File %s read %i set of data %i %i\n",file_queue_process,i,destination, order);
    if ((destination == 0 ) || (engine == 0)) // stop all are processed immediately
    { // stop all
      tmp_msg.d.target = broadcast_id;
      tmp_msg.d.payload1 = tmp_msg.d.payload2 = 0 ;
      tmp_msg.d.order = ( mask_engines | mask_retransfer_send);
      send_msg (tmp_msg.frame);
      queue_clean ();
      remove (file_queue_openhab);
      send_msg (tmp_msg.frame);
      break;
    } // end of 0 - STOP ALL
    tmp_msg.d.target = destination;
    tmp_msg.d.payload1 = engine;
    tmp_msg.d.payload2 = order;
    tmp_msg.d.order = ( mask_engines | mask_retransfer_send);
    enqueue (tmp_msg.frame);
    log_message (990,1," - After enqueue file end is: %s\n", feof (queue_file) ? "true" : "false");
  } // end of while reading queue file
  fclose (queue_file);
  remove (file_queue_process);
}

int main(int argc, char** argv) {
  bool nrf_sender = true;
  bool should_sleep = true;
  int i = 0;
  int loops;
  int sleep_interval;
  unsigned long test_send;
  bool running = true;
  
  if (log_to_syslog) { openlog("HomeAutomationServer", LOG_PID, LOG_USER); }
  log_message (710,1,"START SERVER FOR RF24\n");

// TEST CODE START
//  send_to_open_hab (10,2,47);
//  exit (0);
// TEST CODE END
  
  empty_frame.frame = 0;
  
  start_radio ();
  log_message (770,1,"Radio started\n");
  
  while (running)
  {
    read_queue_from_file ();
    queue_log ();
    i = 0;
    while (queue_info ())
    { // queue there
      i++;
      send_msg (dequeue ());
      if (i >= max_queue_items_processed) {log_message (800,1,"Max queue items processed, checking queue files again\n"); should_sleep = false; break;}
    } // queue there
    if (should_sleep) {usleep (file_read_delay);} // sleeping before next queue check
    should_sleep = true;
    receive_msg (300,0);
  } //main while loop
  stop_radio ();
} //main end