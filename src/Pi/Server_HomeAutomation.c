/*
 * Sever for home automation
 */

#include "CommonAutomation.h"
#include <time.h>
#include "/usr/local/include/RF24/RF24.h"
#include <stdarg.h>

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
union frame {
    unsigned long frame;
    struct {
        char target;
        char payload1;
        char payload2;
        char order;
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
const unsigned char mask_retransfer_send = B01000000;
const unsigned char mask_ack = B10000000;
const unsigned char mask_engines = B00010000;
const unsigned char mask_voltage = B00100000;
const unsigned long mask_long_ack = 128;
const unsigned long mask_long_retransfer = mask_retransfer_send;

// LOG SETTINGS
const int log_level = 500; //Log level, the lower number the more priority log, so lower level means less messages
const char* timeformat = "%Y%m%d%H%M%S";

// QUEUE
struct queue_node { unsigned long queue_data; struct queue_node* queue_next; };
struct queue_node* queue_front = NULL;
struct queue_node* queue_rear = NULL;
const int max_queue_items_processed = 3; // Maximal number of items procesed before checking file again

// FILE FROM OPEN HAB
const char* file_queue_openhab = "/tmp/RF24_queue.txt";
const char* file_queue_process = "/tmp/RF24_in_progress.txt";
const unsigned long file_read_delay = 100000; // delay between checks of queue file on us

int log_message (int level, int detail, const char* message, ...) {
  struct timeval tv;
  time_t t = time(NULL);
  char tstr [21];
  if (level <= log_level)
  {
    gettimeofday (&tv,NULL);
    t = tv.tv_sec;
    strftime (tstr,20,timeformat,localtime (&t));
    if (detail == 1) {printf ("%s:%d ",tstr,level);} // standard print
    if (detail == 2) {printf ("%s:%d:%lu ",tstr,level,tv.tv_usec);} // include us 
    if (detail == 3) {printf ("%s:%d:%lu ",tstr,level,tv.tv_usec/1000);} // include ms
    if (detail > 10) {printf ("%s:%d:%lu ",tstr,level,tv.tv_usec/detail);} // divide by detail 
    va_list args;
    va_start (args, message);
    vprintf (message, args);
    va_end (args);
  }
}

void enqueue(unsigned long x) {
	struct queue_node* temp =  (struct queue_node*)malloc(sizeof(struct queue_node));
	temp->queue_data = x; 
	temp->queue_next = NULL;
	if(queue_front == NULL /*&& queue_rear == NULL*/){ queue_front = queue_rear = temp; return;}
	queue_rear->queue_next = temp;
	queue_rear = temp;
}

unsigned long dequeue() {
	struct queue_node* temp = queue_front;
	unsigned long return_value;
	if(queue_front == NULL) { log_message (10,1,"Error in queue processing, out of bounds\n"); return 0; }
	if(queue_front == queue_rear) { queue_front = queue_rear = NULL;}
	else { queue_front = queue_front->queue_next;	}
	return_value = temp->queue_data;
	free(temp);
	return return_value;
}

unsigned long queue_item() {
	if(queue_front == NULL) { log_message (10,1,"Error in queue processing, out of bounds\n"); return 0; }
	return queue_front->queue_data;
}

bool queue_info() {
	if(queue_front == NULL) { return false; }
	return true;
}

void queue_log() {
	struct queue_node* temp = queue_front;
	int i = 0;
	log_message (800,1,"Queue elements: ");
	while(temp != NULL) { log_message (800,0,"%lu -> ",temp->queue_data);	temp = temp->queue_next; i++; }
	log_message (800,0,"NULL (%d)\n",i);
}

void queue_clean() {
	queue_log ();
	log_message (800,1,"Queue cleaning ");
	while (queue_front != NULL) { dequeue(); log_message (800,1,"."); }
	log_message (800,1," cleaned\n");
	queue_log ();
}

void decodeMessage (unsigned long to_decode) {
  union frame t;
  t.frame = to_decode;
  if ((t.d.orders & mask_engines) != 0) { // Voltages message
    log_message (100,1,"D: %lu decoded as %u (p1) and %u (p2) \n",to_decode,t.d.payload1, t.d.payload2);
  }  // Voltages message
  else {log_message (300,1,"Decoder %lu dropped as unknown\n",to_decode);}
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
  log_message (300,2,"%lu = Sending message %lu expected ack %lu starting at %lu\n",content, content,content_ack,tv.tv_sec);
  do {
    i++;
    gettimeofday (&tv,NULL);
    log_message (900,2,"%lu - Send attempt %u at %lu from %lu\n",content,i,tv.tv_sec,send_start);
    radio.stopListening ();
    radio.write (&content, sizeof (content));
    radio.startListening ();
    usleep (sleep_time);
    while (radio.available ()) {
      radio.read (&received,sizeof (unsigned long));
      received = received | mask_long_retransfer;
      if (received == content_ack)
        {log_message (300,2,"%lu - Send attempt %u at %lu from %lu successfully received ack %lu\n",content,i,tv.tv_sec,send_start,received); ack_received = true; usleep (send_ack_received_delay);}
      else {decodeMessage (received);} // TODO replace with queue processing
    }  // while end
  } while ((i < send_loop_cycles) && !ack_received);
  usleep (send_loop_end_sleep);
}

unsigned long receive_msg (unsigned int receive_loops, unsigned long waiting_for) {
  unsigned int i = 1;
  unsigned long received = 0;
  log_message (800,1,"Receive loop started %u executions\n",receive_loops);
  radio.startListening ();
  for (i = 0; i < receive_loops; i++)
  {
      if (!radio.available ()) {usleep (receive_loop_cycle_wait);};
      while (radio.available ()) {
      radio.read (&received,sizeof (unsigned long));
      log_message (300,2,"Received %lu in loop %u\n",received,i);
      if ((received == waiting_for) && (waiting_for != 0))
        {return (received);}
      else
        {decodeMessage (received);}
      }  // While radio.available cycle
//    log_message (300,2,"Loop %u\n",i);
  }//receive for cycle
  return 0;
} // receive_msg

int read_queue_from_file () {
  FILE* queue_file;
  int i = 0;
  int destination;
  int engine;
  int order;
  union frame tmp_msg
  //fist check existence of file
  if (0 != access(file_queue_openhab, 0)) { log_message (600,1,"File %s did not exists, no queue read\n",file_queue_openhab); return 1;} 
  //block file for processing
  rename (file_queue_openhab, file_queue_process);
  queue_file = fopen (file_queue_process,"r");
  log_message (700,1,"File %s open\n",file_queue_process);
  while (!feof (queue_file))
  {
    i++;
    log_message (700,1," - Loop %i "); 
    if (fscanf (queue_file, "%d %d", &destination, &engine, &order) < 3) {log_message (600,1,"File %s read %i was not successfull, less than 2 required items read\n",file_queue_process,i); continue;};
    log_message (700,1," read destination %i engine %i order %i, file end: %s\n",destination, engine, order, feof (queue_file) ? "true" : "false");
    log_message (600,1,"File %s read %i set of data %i %i\n",file_queue_process,i,destination, order);
    if ((destination == 0 ) || (engine == 0)) 
    { // stop all
      tmp_msg.d.target = broadcast_id;
      tmp_msg.d.payload1 = tmp_msg.d.payload2 = 0 ;
      tmp_msg.d.orders = ( mask_engines || mask_retransfer_send);
      send_msg (tmp_msg.frame);
      queue_clean ();
      remove (file_queue_openhab);
      send_msg (tmp_msg.frame);
      break;
    } // end of 0 - STOP ALL
    tmp_msg.d.target = destination;
    tmp_msg.d.payload1 = engine;
    tmp_msg.d.payload2 = orders;
    tmp_msg.d.orders = ( mask_engines || mask_retransfer_send);
    enqueue (tmp_msg.frame);
    log_message (700,1," - After enqueue file end is: %s\n", feof (queue_file) ? "true" : "false");
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
  
  log_message (10,1,"START SERVER FOR RF24\n");

  start_radio ();

  while (running)
  {
    read_queue_from_file ();
    queue_log ();
    i = 0;
    while (queue_info ())
    { // queue there
      i++;
      send_msg (dequeue ());
      if (i >= max_queue_items_processed) {log_message (200,1,"Max queue items processed, checking queue files again\n"); should_sleep = false; break;}
    } // queue there
    if (should_sleep) {usleep (file_read_delay);} // sleeping before next queue check
    should_sleep = true;
    receive_msg (1000,0);
  } //main while loop
  stop_radio ();
} //main end