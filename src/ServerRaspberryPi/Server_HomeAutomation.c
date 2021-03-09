/*
 * Sever for home automation
 */

#include "CommonAutomation.h"
#include "/usr/local/include/RF24/RF24.h"

#include <limits.h>     /* for CHAR_BIT */
#include <sys/resource.h>     /* for priority changes */

using namespace std;

// MOST OFTEN MODIFIED CONSTANTS

const char* version_number = "1.2.7";
const char* version_text = "Rework delay module, now delays are really calculated before send depending on previous send, bugfix in send start";
const bool log_to_screen = false;
const bool log_to_syslog = true;

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
 * 11 - PETER'S ROOM BIG DOOR
 */

// SPECIAL VERSION FOR 433 COMMANDS
// Build using https://github.com/platenspeler/LamPI-3.x/blob/master/transmitters/livolo/livolo.cpp and https://github.com/midorineko/rpi_automation/blob/master/livolo.py and https://github.com/roidayan/LivoloPi/blob/master/livolo.cpp

struct radio433_item {
  unsigned char target; // Original target in openhab
  unsigned char engine; // Original engine number from openhab
  unsigned int remoteID; // Remote ID to be used for alternative sending via 433 // real remote IDs: 6400; 19303; 23783; 26339; 26348 tested "virtual" remote IDs: 10550; 8500; 7400
  unsigned char keycodeUP; // Remote keycode to be used for alternative sending via 433 UP // keycodes #1: 0, #2: 96, #3: 120, #4: 24, #5: 80=108, #6: 48=72, #7: 108=48, #8: 12, #9: 72=80; #10: 40, #OFF: 106
  unsigned char keycodeDOWN; // Remote keycode to be used for alternative sending via 433 DOWN // keycodes #1: 0, #2: 96, #3: 120, #4: 24, #5: 80, #6: 48, #7: 108, #8: 12, #9: 72; #10: 40, #OFF: 106
  unsigned char keycodeSTOP; // If 0 instead of stop keycode the keycode of last command is used
  unsigned char lastKeycode; // Last keycode send out (247 means nothing send out until now, or stop was the last command)
  bool nrf_send; // If to send NRF (together with 433 for the same target/engine combination)
};

struct radio433_item radio433_list[] = {
  { 51, 1, 6400, 120, 96, 0, 247, false},
  { 51, 2, 19303, 24, 96, 0, 247, false},
  { 52, 1, 7400, 80, 48, 0, 247, true},
  { 52, 2, 7400, 108, 12, 0, 247, true},
  { 52, 3, 7400, 72, 40, 0, 247, true}
};

unsigned char radio433_array_elements = (sizeof (radio433_list))/(sizeof (struct radio433_item)); 
const char radio433_PIN_POWER = 23; // If this is 0 it is assumed that 433 module is always on (it is GPIO number)
const char radio433_PIN_TX = 27; //GPIO number
//const char radio433_PIN_RX = 17; //GPIO number
const unsigned int radio433_power_delay = 300000; // delay in us after powering up/down 433 radio
const char radio_433_send_repeat = 80; // Very sensitive settings, depends on your 433 module, antenna, orientation... recommended is 150, but too many repeats and good signal cause stoping in movement, if your rollershutter starts to move and then stops decrease this value
bool radio433_high = true;
const bool r433_enabled = true;

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
const unsigned int send_loop_duration = 10; //duration of send attempt in seconds, recommed to be less than 32
const unsigned int send_loop_sending_duration = 30; // duration of send itself measured value
const unsigned int send_loop_cycles = send_loop_duration * 1000 / (send_loop_cycle_wait + send_loop_sending_duration); // number of cycles needed for one send
const unsigned int send_loop_nrf_433 = 300; // delay in mili seconds between sends when previous send was nrf and next send is 433
const unsigned int send_loop_433_nfr = 10; // delay in mili seconds between sends when previous send was 433 and next send is nrf
const unsigned int send_loop_nrf_nfr = 10; // delay in mili seconds between sends when previous send was nrf and next send is nrf
const unsigned int send_loop_433_433 = 800; // delay in mili seconds between sends when previous send was 433 and next send is 433
const unsigned int send_ack_received_delay = 1234; // delay after ack was received
unsigned int sleep_time = send_loop_cycle_wait * 1000; // technical calculation
struct timeval send_last_send, send_current_send;
char send_last_type = 0;

// PAYLOAD MASKS
// const unsigned char mask_retransfer_send = 0b01000000;
const unsigned char mask_retransfer_send = 0b00000000;
const unsigned char mask_ack = 0b10000000;
const unsigned char mask_engines = 0b00010000;
const unsigned char mask_voltage = 0b00100000;
const unsigned char mask_light = 0b00000010;
const unsigned char mask_day_night = 0b00001000;
const unsigned long mask_long_ack = 128;
const unsigned long mask_long_retransfer = mask_retransfer_send;

// LOG SETTINGS
const int log_level = 800; //Log level, the lower number the more priority log, so lower level means less messages
const char* timeformat = "%Y%m%d%H%M%S";
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
const unsigned long file_read_delay = 1000000; // delay between checks of queue file in us
const unsigned long file_open_delay = 90000; // delay between moving and opening of queue file in us, this is workaround for situation that there is still write in progress from openhab
const unsigned int faster_loop_divider = 100; // file_read_delay is divided by this value to get faster cycles and reaction times in case of consecutove commands, read also faster_loop_count description
const unsigned int faster_loop_count = 4567; // number of loops to use only 1/faster_loop_divider of file_read_delay, this is to improve reaction time in case of consecutive commands, best way to calculate this value is that if you want the system to react faster next (for example) 47 seconds after last command read calculate this value as 47*1000*1000/(file_read_delay/faster_loop_divider)

// OPEN HAB CONNECTION REST
const char *OH_LIGHT = "light";
const char *OH_VOLTAGE = "voltage";
const char *OH_UNKNOWN = "UNKNOWN";
const char *OH_SOUTH_FF = "southff";
const char *OH_DAY_NIGHT = "dayight";
const char *OH_REMOTE = "remote";
const char *OH_IP = "192.168.32.133";
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

// PRIORITY
id_t my_pid;
const int which_prio = PRIO_PROCESS;
const int radio_prio = -15;
const int normal_prio = 15;

// General running variable, set to false by signal reception
bool running = true;

// Forward declaration
unsigned long send_msg (unsigned long long_message); // Needed by decode message when handling remote control of day/night

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

void signal_callback_handler(int signum) {
   log_message (555,1,"Received signal %d\n", signum);
   running = false;
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
	log_message (790,1,"Queue elements: ");
	while(temp != NULL) { log_message (900,0,"%lu -> ",(*temp).f.frame);	temp = temp->queue_next; i++; }
	log_message (790,0,"NULL (%d)\n",i);
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
    case 12 :
      snprintf (open_hab_type, 31,"%s", OH_DAY_NIGHT);
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
    case 11 :
      snprintf (open_hab_id, 31,"%s", OH_SOUTH_FF);
      break ;
    case  66:
      snprintf (open_hab_id, 31,"%s", OH_REMOTE);
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
  union Frame tmp_ack;
  bool unknown_frame = true;
  t.frame = to_decode;
  if ((t.d.order & mask_voltage) != 0) { // Voltages message
    log_message (720,1,"Voltage %lu decoded as %u (p1) and %u (p2) from %u \n",to_decode,t.d.payload1, t.d.payload2, t.d.target);
    send_to_open_hab (t.d.target,32,t.d.payload1);
    unknown_frame = false;
  }  // Voltages message
  if ((t.d.order & mask_light) != 0) { // Light message
    log_message (720,1,"Light %lu decoded as %u (p1) and %u (p2) from %u \n",to_decode,t.d.payload1, t.d.payload2, t.d.target);
    send_to_open_hab (t.d.target,2,t.d.payload2);
    unknown_frame = false;
  }  // Light message
  if ((t.d.order & mask_day_night) != 0) { // Day night/switch/pir message
    log_message (720,1,"Switch %lu decoded as %u (p1) and %u (p2) from %u \n",to_decode,t.d.payload1, t.d.payload2, t.d.target);
    send_to_open_hab (t.d.target,12,t.d.payload2);
    // in this exceptional case we send ack
    tmp_ack.frame = to_decode;
    tmp_ack.d.order = tmp_ack.d.order | mask_ack;
    send_msg (tmp_ack.frame);
    unknown_frame = false;
  }  // Day night/switch/pir message
  if (unknown_frame) {
    log_message (500,1,"Decoder %lu dropped as unknown %u | %u | %u | %u\n",to_decode, t.d.target, t.d.payload1, t.d.payload2, t.d.order);
  }
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
  radio.setPALevel(RF24_PA_MAX); // TODO TO CHECK
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

// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// BELOW Based on LamPi-2.0/livolo from https://github.com/platenspeler/LamPI-2.0/tree/master/transmitters/livolo 

// keycodes #1: 0, #2: 96, #3: 120, #4: 24, #5: 80, #6: 48, #7: 108, #8: 12, #9: 72; #10: 40, #OFF: 106
// real remote IDs: 6400; 19303; 23783
// tested "virtual" remote IDs: 10550; 8500; 7400
// other IDs could work too, as long as they do not exceed 16 bit
// known issue: not all 16 bit remote ID are valid
// have not tested other buttons, but as there is dimmer control, some keycodes could be strictly system
// use: sendButton(remoteID, keycode), see example blink.ino; 

const int p_short = 120;									// 110 works quite OK
const int p_long = 315;									// 300 works quite OK
//const int p_start = 525;									// 520 works quite OK
const int p_start = 550;									// 520 works quite OK

void radio433_sendPulse(unsigned char txPulse) {
  // log_message (980,1,"DEBUG 433 transmit start for %d\n", txPulse);
	switch(txPulse) 								// transmit pulse
	{
	case 0: 										// Start
		digitalWrite(radio433_PIN_TX, LOW);
		delayMicroseconds(p_start); 				// 550
	break;
   
	case 1: 										// Start
		digitalWrite(radio433_PIN_TX, HIGH);
		delayMicroseconds(p_start); 				// 550
	break;

	case 2: 										// "High Zero"
		digitalWrite(radio433_PIN_TX, LOW);
		delayMicroseconds(p_short); 				// 110
	break;
 
	case 3: 										// "High One"
		digitalWrite(radio433_PIN_TX, LOW);
		delayMicroseconds(p_long); 					// 303
	break; 

	case 4: 										// "Low Zero"
		digitalWrite(radio433_PIN_TX, HIGH);
		delayMicroseconds(p_short);					// 110
	break;

	case 5:											// "Low One"
		digitalWrite(radio433_PIN_TX, HIGH);
		delayMicroseconds(p_long); 					// 290
	break; 
	}
  // log_message (980,1,"DEBUG 433 transmit end for %d\n", txPulse);
}

void radio433_selectPulse(unsigned char inBit) {
    switch (inBit) {
    case 0: 
        if (radio433_high == true) {   						// if current pulse should be high, send High Zero
          radio433_sendPulse(2); 
		  radio433_sendPulse(4);
        } else {              						// else send Low Zero
          radio433_sendPulse(4);
		  radio433_sendPulse(2);
        }
      break;
      case 1:                						// if current pulse should be high, send High One
        if (radio433_high == true) {
          radio433_sendPulse(3);
        } else {             						// else send Low One
          radio433_sendPulse(5);
        }
        radio433_high=!radio433_high; 								// invert next pulse
      break; 
    }
}

void radio433_sendButton(unsigned int remoteID, unsigned char keycode) {
  signed char i;
  unsigned char pulse;
  pinMode(radio433_PIN_TX, OUTPUT);
  for (pulse= 0; pulse <= radio_433_send_repeat; pulse++) 		// how many times to transmit a command
  {
    log_message (980,1,"DEBUG 433 send pulse %d\n", pulse);
    radio433_high = true;
		radio433_sendPulse(1); // first pulse is always high
    for (i = 15; i>=0; i--) { 						// transmit remoteID
      unsigned int txPulse = remoteID & ( 1<<i );	// read bits from remote ID
      log_message (980,1,"DEBUG 433 send pulse %d remote loop %d txpulse %d\n", pulse, i, txPulse);
      if (txPulse>0) { 
		radio433_selectPulse(1); 
      }
      else {
		radio433_selectPulse(0);
      }
    }
    for (i = 6; i>=0; i--) 							// XXX transmit keycode
    {
            log_message (980,1,"DEBUG 433 send pulse %d keycode loop %d\n", pulse, i);
		unsigned char txPulse= keycode & (1<<i); 	// read bits from keycode
		if (txPulse>0) {
			radio433_selectPulse(1); 
		}
		else {
			radio433_selectPulse(0);
		}
    } 
  }
  if (radio433_high)
  	digitalWrite(radio433_PIN_TX, LOW);
  else 
  	digitalWrite(radio433_PIN_TX, HIGH);
}

// ABOVE Based on LamPi-2.0/livolo from https://github.com/platenspeler/LamPI-2.0/tree/master/transmitters/livolo 
// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

void radio433_init () {
  //Initialize 433 module
  if ( radio433_PIN_POWER != 0 ) {
    pinMode(radio433_PIN_POWER, OUTPUT);
  }
  pinMode(radio433_PIN_TX, OUTPUT);
  //pinMode(radio433_PIN_RX, INPUT);
  if ( radio433_PIN_POWER != 0 ) {
    digitalWrite (radio433_PIN_POWER,LOW);
  }
  digitalWrite (radio433_PIN_TX,LOW);
}

void radio433_power (bool power_state) {
  // Power on or off 433 radio, power_state true=power on, false=power off
  if (power_state)
  { //power on
    if ( radio433_PIN_POWER != 0 )
    { //  Turn on 433 power start
        pinMode(radio433_PIN_POWER, OUTPUT);
        digitalWrite (radio433_PIN_POWER,HIGH);
        usleep (radio433_power_delay);
        log_message (850,2,"Radio 433 powered ON %d \n",power_state);
    } //  Turn on 433 power end
  } // power on
  else
  { // power off
    usleep (radio433_power_delay);
    radio433_sendPulse (0);
    if ( radio433_PIN_POWER != 0 )
    { //  Turn off 433 power start
        digitalWrite (radio433_PIN_POWER,LOW);
        log_message (850,2,"Radio 433 powered OFF %d \n",power_state);
        //usleep (radio433_power_delay);
    } //  Turn off 433 power end
  } // power off
}

// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// BELOW Based on roidayan/LivoloPi from https://github.com/roidayan/LivoloPi
#define LIVOLO_PREAMBLE_DURATION 525
#define LIVOLO_ZERO_DURATION 120
#define LIVOLO_ONE_DURATION 315
#define LIVOLO_NUM_REPEATS 150
bool mIsHigh;
int mTxPin;

void roidayan_tx(bool value)
{
  digitalWrite(radio433_PIN_TX, value ? HIGH : LOW);
}

void roidayan_sendPreamble()
{
  roidayan_tx(true);
  delayMicroseconds(LIVOLO_PREAMBLE_DURATION);
  roidayan_tx(false);
  mIsHigh = false;
}

void roidayan_sendOne()
{
  delayMicroseconds(LIVOLO_ONE_DURATION);
  mIsHigh = !mIsHigh;
  roidayan_tx(mIsHigh);
}

void roidayan_sendZero()
{
  delayMicroseconds(LIVOLO_ZERO_DURATION);
  roidayan_tx(!mIsHigh);
  delayMicroseconds(LIVOLO_ZERO_DURATION);
  roidayan_tx(mIsHigh);
}

void roidayan_sendCommand(uint32_t command, uint8_t numBits)
{
  //piHiPri(90);
  for (uint8_t repeat = 0; repeat < LIVOLO_NUM_REPEATS; ++repeat)
  {
    uint32_t mask = (1 << (numBits - 1));
    roidayan_sendPreamble();
    for (uint8_t i = numBits; i > 0; --i)
    {
      if ((command & mask) > 0)
      {
        roidayan_sendOne();
      }
      else
      {
        roidayan_sendZero();
      }
      mask >>= 1;
    }
  }
  roidayan_tx(false);
}

void roidayan_sendButton(uint16_t remoteId, uint8_t keyId)
{
  // 7 bit Key Id and 16 bit Remote Id
  mIsHigh = false;
  uint32_t command = ((uint32_t)keyId & 0x7F) | (remoteId << 7);
  roidayan_sendCommand(command, 23);
}

// ABOVE Based on roidayan/LivoloPi from https://github.com/roidayan/LivoloPi
// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

struct timeval send_delay (char send_actual_type) {
// send_actual_type: 0: initialize, 1: nrf, 2: 433, 7: Finished (update last send time)
  double elapsed_from_last_send = 0.0;  
  int actual_delay = 0;
  int real_delay = 0;
  if ((send_actual_type == 0) || (send_actual_type == 7)) {
    gettimeofday (&send_last_send,NULL);
    log_message (930,2,"Send delay update of last_send s %d us %d (%d)\n",send_last_send.tv_sec,send_last_send.tv_usec,send_actual_type);
  }
  else {
    gettimeofday (&send_current_send,NULL);
    if (send_last_type == 0) {
      send_last_type = send_actual_type;
      log_message (930,2,"Send delay send_last_type updated to %d\n",send_last_type);
      return (send_current_send);      
    }
    else if ((send_actual_type == 1) || (send_actual_type == 2)) {
      elapsed_from_last_send = send_current_send.tv_sec - send_last_send.tv_sec;
      if (elapsed_from_last_send > 3) {
        send_last_type = send_actual_type;
        log_message (930,2,"Send delay more than 3 seconds from last send send_last_type %d\n",send_last_type);
        return (send_current_send);
      }
      elapsed_from_last_send = (elapsed_from_last_send*1000.0) + ((send_current_send.tv_usec - send_last_send.tv_usec)/1000.0);
      if ((send_actual_type == 1) && (send_last_type == 1)) {
        actual_delay = send_loop_nrf_nfr;
      }
      if ((send_actual_type == 1) && (send_last_type == 2)) {
        actual_delay = send_loop_433_nfr;
      }
      if ((send_actual_type == 2) && (send_last_type == 1)) {
        actual_delay = send_loop_nrf_433;
      }
      if ((send_actual_type == 2) && (send_last_type == 2)) {
        actual_delay = send_loop_433_433;
      }
      if (actual_delay > elapsed_from_last_send) {
        real_delay = actual_delay - (int)elapsed_from_last_send;
        log_message (790,2,"Send delay sleeping %d ms (re-calculated from %d ms) for send_last_type %d and send_actual_type %d with actual_delay %d <= elapsed_from_last_send %lf (send_last_send:%d/%d send_current_send:%d/%d)\n",real_delay,actual_delay,send_last_type,send_actual_type,actual_delay,elapsed_from_last_send,send_last_send.tv_sec,send_last_send.tv_usec,send_current_send.tv_sec,send_current_send.tv_usec);
        usleep (real_delay*1000);
        gettimeofday (&send_current_send,NULL);
      }
      else {
        log_message (930,2,"Send delay NOT sleeping %d ms for send_last_type %d and send_actual_type %d with actual_delay %d > elapsed_from_last_send %lf (send_last_send:%d/%d send_current_send:%d/%d)\n",actual_delay,send_last_type,send_actual_type,actual_delay,elapsed_from_last_send,send_last_send.tv_sec,send_last_send.tv_usec,send_current_send.tv_sec,send_current_send.tv_usec);send_last_type = send_actual_type;
      }
      send_last_type = send_actual_type;
      log_message (930,2,"Send delay returning with send_last_type %d\n",send_last_type);
      return (send_current_send);
    }
  }
}

unsigned long send_msg (unsigned long long_message) {
  struct timeval tv;
  unsigned long send_start;
  unsigned int i = 0;
  unsigned int radio433_element;
  unsigned int r433_ID = 0; // Remote ID to be used for alternative sending via 433 // real remote IDs: 6400; 19303; 23783 tested "virtual" remote IDs: 10550; 8500; 7400
  unsigned char r433_command; // Remote keycode to be used for actual send via 433// keycodes #1: 0, #2: 96, #3: 120, #4: 24, #5: 80, #6: 48, #7: 108, #8: 12, #9: 72; #10: 40, #OFF: 106
  unsigned char r433_up; // Remote keycode to be used for alternative sending via 433 UP // keycodes #1: 0, #2: 96, #3: 120, #4: 24, #5: 80, #6: 48, #7: 108, #8: 12, #9: 72; #10: 40, #OFF: 106
  unsigned char r433_down; // Remote keycode to be used for alternative sending via 433 DOWN // keycodes #1: 0, #2: 96, #3: 120, #4: 24, #5: 80, #6: 48, #7: 108, #8: 12, #9: 72; #10: 40, #OFF: 106
  unsigned char r433_stop; // If 0 instead of stop keycode the keycode of last command is used
  unsigned char r433_last; // Last keycode send out (247 means nothing send out until now, or stop was the last command)
  bool nrf_sending = true;
  bool ack_received = false;
  unsigned long content_ack = (long_message | mask_long_ack) ;
  unsigned long content = long_message;
  unsigned long received = 0;
  union Frame tmp_msg; 
  // First we need to check if there is 433 sender for this, so we need to decode (little bit of workaround but this was added later
  tmp_msg.frame = long_message;
  for(i = 0; i < radio433_array_elements; i++) {
    if ((radio433_list[i].target == tmp_msg.d.target) && (radio433_list[i].engine == tmp_msg.d.payload1))
    { // 433 to be used for this target and engine
      r433_ID = radio433_list[i].remoteID;
      nrf_sending = radio433_list[i].nrf_send;
      if (tmp_msg.d.payload2 == 0) { // STOP
        if (radio433_list[i].keycodeSTOP == 0) {
          r433_command = radio433_list[i].lastKeycode;
        }
        else {
          r433_command = radio433_list[i].keycodeSTOP;
        }
      } // STOP
      else {
        if (tmp_msg.d.payload2 == 1 ) { // DOWN
          r433_command = radio433_list[i].keycodeDOWN;
          radio433_list[i].lastKeycode = r433_command; 
        } // DOWN
        else {
          if (tmp_msg.d.payload2 == 2 ) { // UP
            r433_command = radio433_list[i].keycodeUP;
            radio433_list[i].lastKeycode = r433_command; 
          }  // UP
        }
      }
      break;
    } // 433 to be used for this target and engine
  }
  if ( (r433_ID != 0) && (r433_enabled) ) { // Sending via 433
    tv = send_delay (2);
    send_start = tv.tv_sec;
    log_message (800,2,"Ready to send 433 via ID %d and key %d [%d|%d|%d|%d]\n",r433_ID, r433_command,tmp_msg.d.target,tmp_msg.d.payload1,tmp_msg.d.payload2,tmp_msg.d.order);
  	//roidayan_sendButton(r433_ID, r433_command);	
    //If needed turn on power for 433 sender
    radio433_power (true);
    setpriority(which_prio, my_pid, radio_prio);
  	radio433_sendButton(r433_ID, r433_command);	
    setpriority(which_prio, my_pid, normal_prio);
    radio433_power (false);
    log_message (750,2,"Done sending 433 via ID %d and key %d [%d|%d|%d|%d]\n",r433_ID, r433_command,tmp_msg.d.target,tmp_msg.d.payload1,tmp_msg.d.payload2,tmp_msg.d.order);
    //usleep (send_loop_end_sleep_433*1000);
    send_delay (7);
  } // Sending via 433
  if ( nrf_sending ) {  // Sending via NRF
    tv = send_delay (1);
    send_start = tv.tv_sec;
    log_message (800,2,"%lu = Sending message %lu expected ack %lu starting at %lu\n",content, content,content_ack,tv.tv_sec);
    i = 0;
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
          {log_message (750,2,"%lu - Send attempt %u at %lu from %lu successfully received ack %lu [%d|%d|%d|%d]\n",content,i,tv.tv_sec,send_start,received,tmp_msg.d.target,tmp_msg.d.payload1,tmp_msg.d.payload2,tmp_msg.d.order); ack_received = true; usleep (send_ack_received_delay);}
        else {decodeMessage (received);} // TODO replace with queue processing
      }  // while end
    } while ((i < send_loop_cycles) && !ack_received);
    if (!ack_received) {
      log_message (570,2,"%lu - Send attempts %d used at %lu from %lu missing ack [%d|%d|%d|%d]\n",content,i,tv.tv_sec,send_start,tmp_msg.d.target,tmp_msg.d.payload1,tmp_msg.d.payload2,tmp_msg.d.order);
    }
    //usleep (send_loop_end_sleep_nrf*1000);
    send_delay(7);
  } // Sending via NRF
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
  usleep (file_open_delay);
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
  int fast_loops = 0;
  int sleep_interval;
  unsigned long test_send;
  
  if (log_to_syslog) { openlog("HomeAutomationServer", LOG_PID, LOG_USER); }
  log_message (710,1,"START SERVER FOR RF24 VERSION: %s %s\n", version_number, version_text);
  signal(SIGINT, signal_callback_handler);
  signal(SIGTERM, signal_callback_handler);
  signal(SIGQUIT, signal_callback_handler);
  my_pid = getpid();
  
  empty_frame.frame = 0;
  
  start_radio ();
  radio433_init ();
  log_message (770,1,"Radio started\n");

  while (running)
  {
    read_queue_from_file ();
    //queue_log ();
    i = 0;
    while (queue_info ())
    { // queue there
      i++;
      send_msg (dequeue ());
      if (i >= max_queue_items_processed) {log_message (800,1,"Max queue items processed, checking queue files again\n"); should_sleep = false; break;}
      fast_loops = faster_loop_count;
    } // queue there
    if (should_sleep) { // sleeping before next queue check
      if (fast_loops > 0 ) {
        usleep (file_read_delay/faster_loop_divider);
        fast_loops--;
      }
      else {
        usleep (file_read_delay);
      }
    } // sleeping before next queue check
    should_sleep = true;
    if (fast_loops > 0 ) {
      receive_msg (3,0);
    }
    else {
      receive_msg (300,0);
    }
  } //main while loop
  log_message (710,1,"Stopping radio\n");
  stop_radio ();
  radio433_power (false);
  log_message (555,1,"Exitting...\n");
} //main end