#include <nRF24L01.h>
#include <printf.h>
#include <SPI.h>
#include <RF24.h>
#include <RF24_config.h>
#include "LowPower.h"

// Following header file is separate for each arduino
#include "Receiver13.h"
//#include "ReceiverTEST.h"

// Following code is the same for each arduino, in case any const is added to code it must be defined below this line or in header file for EACH arduino
// RADIO VARIABLES
bool radio_status = false; // if radio is active
bool radio_sleeping = false; // True if radio is in deep sleep
bool radio_voltage = false; // if radio has voltage (is powered up)
bool radio_trx_ready = false; // if radio is ready for transmission
bool rf_trx_done = false; // If trx was executed in this cycle
unsigned char radio_wait_time = radio_wait_after_power;
unsigned long radio_start_time = 0; // Time when radio was started up
union Frame {
    unsigned long frame;
    struct {
        unsigned char order;
        unsigned char payload2;
        unsigned char payload1;
        unsigned char target;
    } d;
};
unsigned char rx_queue_start = 0;
unsigned char rx_queue_count = 0;
union Frame rx_tmp, rx_last;
union Frame rx_queue [rx_max_queue_size];
unsigned char tx_queue_start = 0;
unsigned char tx_queue_count = 0;
union Frame tx_tmp, tx_last;
union Frame tx_queue [tx_max_queue_size];

// ENGINES definition
unsigned long engines_last_stop_command_received = 0; //timestamp when last stop command was received

// ORDERS DEFAULTS
bool new_orders = false; // if there are new not yet processeed orders
unsigned char sleep_lock = 0; // bit field to check if anything is in proogress
const unsigned char sleep_lock_orders_to_be_processed = 0; // bit position for active orders
const unsigned char sleep_lock_orders_engine_running = 1; // bit position for running engine
const unsigned char sleep_lock_wait_after_engine_stop = 2; // bit position where stop delay is indicated in sleep validation
const unsigned char sleep_lock_mains_on = 3; // bit position where minimal on time of mains is controlled
const unsigned char sleep_lock_battery_mains_on = 4; // bit position where minimal mains on for battery charging is indicated

// VOLTAGE measure + LIGHT
unsigned char voltage_input = 0;  // Input voltage for the arduino board
unsigned long voltage_tmp_input = 0;  // Input voltage for the arduino board
unsigned char voltage_measure = 0;  // Voltage measure (up to 1.1V)
unsigned long voltage_tmp_measure = 0;  // Voltage measure (up to 1.1V)
unsigned char voltage_light = 0; // Voltage measure with light sensor
unsigned long voltage_tmp_light = 0; // Voltage measure with light sensor
unsigned long voltage_read_last = 0; // Time when voltage was last read
unsigned long voltage_send_last = 0; // Time when voltahe was last time sent

// MAINS SWITCHER variables
unsigned long mains_last_on_time = 0; // Time when mains was last turned on
unsigned char mains_on_requests = 0; // bit array to process demands
bool mains_turned_on = false;
const unsigned char mains_lock_bit_engines = sleep_lock_orders_engine_running; // bit to be used for engines
const unsigned char mains_lock_bit_self = sleep_lock_mains_on; // bit used only for startup sequence
const unsigned char mains_lock_bit_battery = sleep_lock_battery_mains_on; // bit used for battery charging

// BATTERY maintenance
unsigned long battery_last_measured = 0; // last time when battery was measured
unsigned long battery_start_charging = 0; // time when battery charging started
bool battery_charging_on = false;

// LIGHT measure REFACTORED
unsigned long light_ldr_last_run = 0; // timestamp when it was last time run
unsigned char light_ldr_last_measured = 0; // last measured value that was also send (!)

const unsigned int size_of_long = sizeof (unsigned long);

bool queue_add (union Frame *a, union Frame *q, unsigned char *start, unsigned char *count, unsigned char max) {
  unsigned char new_pos = *start + *count;
  unsigned char i;
  if (new_pos >= max) {
    if (*start > 0) { // Can shift queue
      for (i = 0; i < *count; i++) { q [i] = q [i+*start]; }
      *start = 0;
      new_pos = *start + *count;
    } // Can shift queue
    else { return false; } // can not add
  }
  q [new_pos] = *a;
  *count += 1;
  return true;
}

bool queue_remove (unsigned char *start, unsigned char *count) {
  if (*count > 0) { *count -= 1; }
  if (*count == 0) { *start = 0; }
    else { *start += 1; }
  return true;
}

bool rx_queue_add (union Frame a) {
  unsigned char new_pos = rx_queue_start + rx_queue_count;
  unsigned char i;
  if (new_pos >= rx_max_queue_size) { 
    if (rx_queue_start > 0) { // Can shift queue
      for (i = 0; i < rx_queue_count; i++) { rx_queue [i] = rx_queue [i+rx_queue_start]; }
      rx_queue_start = 0;
      new_pos = rx_queue_start + rx_queue_count;
    } // Can shift queue
    else { return false; } // can not add
  }
  rx_queue [new_pos] = a;
  rx_queue_count += 1;
  return true;
}

bool rx_queue_remove () {
  if (rx_queue_count > 0) { rx_queue_count -= 1; }
  if (rx_queue_count == 0) { rx_queue_start = 0; }
    else { rx_queue_start += 1; }
  return true;
}

bool mains_on (unsigned char author_bit) {
  if (mains_switch_connected) {
    if (author_bit < 8) { bitSet (mains_on_requests, author_bit); }
    if (!mains_turned_on) {
      if (mains_switch_on_notify_server) payload_send (10,author_bit+10, 5, mains_on_requests);
      pinMode(mains_switch_pin, OUTPUT);
      digitalWrite (mains_switch_pin, HIGH);
      delay (mains_turn_on_time);
      mains_last_on_time = millis ();
      mains_turned_on = true;
//      bitSet (sleep_lock, sleep_lock_mains_on);  // Sleep lock commented as the mains on should not prevent sleep
    }
  }
  return (mains_turned_on);
}

bool mains_off (unsigned char author_bit) {
  if (mains_switch_connected) {
    if (mains_turned_on) {
      if (author_bit < 8) { bitClear (mains_on_requests, author_bit); }
      if (mains_on_requests == 0) {
        if (millis () - mains_last_on_time > mains_min_switch_on_time ) {
          if (mains_switch_off_notify_server) payload_send (10,author_bit, 5, mains_on_requests);
          digitalWrite (mains_switch_pin, LOW);
          delay (mains_turn_off_time);
          mains_turned_on = false;
          //bitClear (sleep_lock, sleep_lock_mains_on); // Sleep lock commented as the mains on should not prevent sleep, exception is first run of program
        }
      }
    }
    else {
      if (author_bit < 8) { bitClear (mains_on_requests, author_bit); }
    }
  }
  return (mains_turned_on);
}

void mains_startup_sequence () {
  if (mains_switch_connected) {
    pinMode(mains_switch_pin, OUTPUT);
    if (mains_switch_latching) {
      digitalWrite (mains_switch_pin, LOW);
      delay (mains_turn_off_time);
    }
    mains_on (mains_lock_bit_self);
    mains_off (mains_lock_bit_self);
    bitSet (sleep_lock, sleep_lock_mains_on);
  }
}

void init_radio () {
  if (radio_status) {
    return;
  }
  radio_trx_ready = false;
  if (!radio_voltage) {
    if (radio_power_from_arduino) {
      pinMode(radio_power_pin, OUTPUT);
      digitalWrite (radio_power_pin, HIGH);
      delay (radio_init_delay);
    }
		radio.begin();
    radio_start_time = millis ();
    radio_wait_time = radio_wait_after_power;
		radio.setPALevel(RF24_PA_MAX);
		radio.openWritingPipe(addresses[1]);
		radio.openReadingPipe(1, addresses[0]);
		radio_voltage = true;
	}
  else { // Radio was powered up
    if (radio_sleeping) { // Radio is sleeping
      radio.powerUp();
      radio_start_time = millis ();
      radio_wait_time = radio_wait_after_sleep;
    } // Radio is sleeping
  } // Radio was powered up
  // radio.startListening (); // we do not start listening here any more
	// delay (radio_sleep_wakeup_delay);
  radio_sleeping = false;
  radio_status = true;
//  if (serial_messages) { Serial.print ("Radio powered up, on "); Serial.print (millis ()); Serial.println (" details follow: "); radio.printDetails (); }
}

void shutdown_radio () { // hard power down, disconnecting power from radio module
  radio_trx_ready = false;
  if (radio_voltage) { // if radio is powered up
    if (radio_shutdown_power) { // radio should shutdown power on power down
      if (radio_power_from_arduino) {
        pinMode(radio_power_pin, INPUT);
        radio_voltage = false;
      }
      else { // if radio is not powered from arduino and is ordered to cut power, only enter to standby is performed
        radio.powerDown();
        radio_sleeping = true;
      }
      radio_status = false;
//      if (serial_messages) { Serial.print ("Radio shut down on "); Serial.println (millis ()); }
    } // radio should shutdown power
    else { // Radio is still powered up, only entering deep sleep mode
      radio.powerDown();
      radio_status = false;
      radio_sleeping = true;
//      if (serial_messages) { Serial.print ("Radio entered deep sleep on "); Serial.println (millis ()); }
    } // Radio is still powered up, only entering deep sleep mode
  } // radio is powered up
}

void radio_ready () { // Will wait required time to make sure that radio is ready, depends on type of power_down
	//if (serial_messages) { Serial.print ("R: "); Serial.print (millis ()); Serial.print ("-"); Serial.print (radio_start_time); Serial.print ("(="); Serial.print (millis () - radio_start_time ); Serial.print (") <="); Serial.print (radio_wait_time);}
  if (!radio_status) { init_radio (); }
  if (!radio_trx_ready) { // Waiting for radio to get ready
    while (millis () - radio_start_time < radio_wait_time) {
      delay (1);
    } // Waiting for radio to become ready
  } // Waiting for radio to get ready
  radio.startListening ();
  radio_trx_ready = true;
  //if (serial_messages) { Serial.println ("<");}
} // Will wait required time to make sure that radio is ready, depends on type of power_down

bool rf_tx_only (unsigned long to_send_payload) {
//  if (serial_messages) { Serial.print (" Send start "); Serial.print (to_send_payload); }
  if (to_send_payload > 0) {
    radio_ready (); // Get radio ready and operational
    radio.stopListening ();
    //if (serial_messages) { Serial.print (" stop listening "); }
    radio.write (&to_send_payload, size_of_long);
    //if (serial_messages) { Serial.print (" S: "); Serial.print (to_send_payload); }
    radio.startListening();
    //if (serial_messages) { Serial.print (" start listening "); }
  }
  if (serial_messages) { Serial.print (" Send end "); }
}
  
void payload_tx_only (unsigned char p1_bit, unsigned char p1_payload, unsigned char p2_bit, unsigned char p2_payload) {
  // If no payload is present then bit should be > 8 to indicate no content (payloads are always copied)
  union Frame tx_t ;
  tx_t.frame = 0;
  tx_t.d.target = my_id; // Arduino is responding with own ID as target (effectivelly target has become source for arduino to server communication)
  tx_t.d.payload1 = p1_payload;
  tx_t.d.payload2 = p2_payload;
  if (p1_bit < 8) { bitSet (tx_t.d.order,p1_bit); }
  if (p2_bit < 8) { bitSet (tx_t.d.order,p2_bit); }
  rf_tx_only (tx_t.frame); // Can result in data not send in case there was another transmission ongoing, but these data can be lost and are not so valuable
}

bool rf_trx (unsigned long to_send_payload) {
  unsigned long tstart, tend, receive_time;
  unsigned int i = 0;
  bool clear_air = true;
  radio_ready ();
  tstart = millis ();
  while (millis () - tstart < receive_duration)
  {
    i++;
    while (radio.available()) {                                   // While there is data ready
//      if (serial_messages) { Serial.print ("Data to read ready"); }
      radio.read( &rx_tmp.frame, size_of_long );             // Get the payload
      receive_time = millis ();
      // TODO check re-transfer requests here (maybe after the below if, but can not be as else due to re-transfer of broadcasts)
      if ((rx_tmp.d.target == my_id) || (rx_tmp.d.target == broadcast_id) || (rx_tmp.d.target == all_id) ) { // traffic is for this node
        if (serial_messages) { Serial.print ("RX: "); Serial.print (rx_tmp.frame); Serial.print (" T: "); Serial.print (receive_time); }
        if (bitRead (rx_tmp.frame, ack_bit_position) == 0) { // it is NOT ack message (we do not wait for ack's on client)
          if (serial_messages) { Serial.print (" O: "); Serial.print (rx_tmp.d.order); }
          if (retransfer_bit_set) { bitSet (rx_tmp.frame, retransfer_bit_position); } else { bitClear (rx_tmp.frame, retransfer_bit_position); }
          bitSet (rx_tmp.frame, ack_bit_position); // We always set ack bit to distinguish later in rx_queue between 0 as order (equals 0) and 0 as result of processing (has ack set)
          if (serial_messages) { Serial.print (" Ret+Ack Set "); Serial.print (rx_tmp.frame);}
          if (rx_tmp.d.target == my_id) { rf_tx_only (rx_tmp.frame); if (serial_messages) { Serial.print (" ACK: "); Serial.print (rx_tmp.frame); Serial.print (" T: ");Serial.print (millis ()); } } // Sending ack if it was for me
          if (rx_tmp.frame != rx_last.frame) { // New data have been received
            if ((rx_tmp.d.order & orders_mask) == 0) { rx_queue_start = 0; rx_queue_count = 1; rx_queue[rx_queue_start].frame = 0; if (serial_messages) { Serial.print (" STOP ALL "); } } // stop all message is represented as 0 in rx_queue and also deletes queue until now (later)
            else { rx_queue_add (rx_tmp); /*DEBUG:payload_tx_only (10, ((rx_queue_start*10)+rx_queue_count), 10, 48);*/ if (serial_messages) { Serial.print (" Q+"); } }
            bitSet (sleep_lock,sleep_lock_orders_to_be_processed);
//            if (serial_messages) { Serial.print (" Q val:"); Serial.print (rx_queue [rx_queue_start + rx_queue_count - 1].frame); Serial.print (" Q start:"); Serial.print (rx_queue_start); Serial.print (" Q count: "); Serial.print (rx_queue_count); }
            rx_last = rx_tmp ;
   	      } // New data have been received
        } // Message is NOT ack
        Serial.println (".");
      } // Traffic is for this node
      else { // Traffic not for this site, breaking cycle
        if (serial_messages) { Serial.print ("Traffic not for my_id ("); Serial.print (my_id); Serial.print ("): "); Serial.print (rx_tmp.d.target); Serial.println (" breaking cycle"); }
        clear_air = false;
        break;
      } // Traffic not for this site, breaking cycle
    }  // while there are data ready
    if (clear_air) { delay (receive_loop_delay); }
      else { break; }
  } // receive loop
  tend = millis ();
  if (clear_air) {
		if (to_send_payload > 0) {
      rf_tx_only (to_send_payload);
      if (serial_messages) { Serial.print ("SUM TRX: RX loops: "); Serial.print (i); Serial.print (" Tstart: "); Serial.print (tstart); Serial.print (" Tend "); Serial.print (tend); Serial.print (" Ttxp: "); Serial.println (millis ()); }
    }
    if (tx_queue_count > 0) {
      to_send_payload = tx_queue[tx_queue_start].frame;
      rf_tx_only (to_send_payload);
      queue_remove (&tx_queue_start, &tx_queue_count);
      if (serial_messages) { Serial.print ("SUM TRX: RX loops: "); Serial.print (i); Serial.print (" Tstart: "); Serial.print (tstart); Serial.print (" Tend "); Serial.print (tend); Serial.print (" Ttxq: "); Serial.println (millis ()); }
    }
    rf_trx_done = true;
    return true;
  }
  else {
    if (serial_messages) { Serial.print ("SUM RX: RX loops: "); Serial.print (i); Serial.print (" Tstart: "); Serial.print (tstart); Serial.print (" Tend "); Serial.print (tend); Serial.print (" Now: "); Serial.println (millis ()); }
    rf_trx_done = false;
    return false;
  }
}

void payload_send (unsigned char p1_bit, unsigned char p1_payload, unsigned char p2_bit, unsigned char p2_payload) {
  // If no payload is present then bit should be > 8 to indicate no content (payloads are always copied)
  union Frame tx_t ;
  tx_t.frame = 0;
  tx_t.d.target = my_id; // Arduino is responding with own ID as target (effectivelly target has become source for arduino to server communication)
  tx_t.d.payload1 = p1_payload;
  tx_t.d.payload2 = p2_payload;
  if (p1_bit < 8) { bitSet (tx_t.d.order,p1_bit); }
  if (p2_bit < 8) { bitSet (tx_t.d.order,p2_bit); }
  rf_trx (tx_t.frame); // Can result in data not send in case there was another transmission ongoing, but these data can be lost and are not so valuable
}

void engine_change (unsigned int e, unsigned int o, bool f) {
  if (serial_messages) { Serial.print ("Engine "); Serial.print (e); Serial.print (" change by order "); Serial.print (o); Serial.print (" now is: "); Serial.println (millis ()); }
  if (e >= number_of_engines) { return; }
  //DEBUG:payload_tx_only (10, ((engines[e].operating*100)+(engines[e].last_status*10)+o), 10, 10+e);
  if ((engines[e].last_status != o) && (o == 1)) {
    // start move down
    if (f) { engines[e].last_status = o; }
    //DEBUG:payload_tx_only (10, ((engines[e].operating*100)+(engines[e].last_status*10)+o), 10, 110+e);
    if (engines[e].operating) {
      digitalWrite (engines[e].pin_on, HIGH); // stop power to the engine
      delay (engine_direction_switch_delay);
    } // end of delay if engine was operating (most probaly in oposite direction)
    //DEBUG:payload_tx_only (10, ((engines[e].operating*100)+(engines[e].last_status*10)+o), 10, 120+e);
    engines[e].operating = true;
    bitSet (sleep_lock,sleep_lock_orders_engine_running);
    mains_on (mains_lock_bit_engines);
    //DEBUG:payload_tx_only (10, ((engines[e].operating*100)+(engines[e].last_status*10)+o), 10, 130+e);
    if (engines[e].pin_down != 0) { digitalWrite (engines[e].pin_down, LOW); }
    //DEBUG:payload_tx_only (10, ((engines[e].operating*100)+(engines[e].last_status*10)+o), 10, 140+e);
    digitalWrite (engines[e].pin_on, LOW);
    digitalWrite (engines[e].pin_power, HIGH);
    //DEBUG:payload_tx_only (10, ((engines[e].operating*100)+(engines[e].last_status*10)+o), 10, 150+e);
    engines[e].start = millis ();
    //DEBUG:payload_tx_only (10, ((engines[e].operating*100)+(engines[e].last_status*10)+o), 10, 100+e);
    return;
  }
  //DEBUG:payload_tx_only (10, ((engines[e].operating*100)+(engines[e].last_status*10)+o), 10, 20+e);
  if ((engines[e].last_status != o) && (o == 2)) {
    // start move up
    if (f) { engines[e].last_status = o; }
    //DEBUG:payload_tx_only (10, ((engines[e].operating*100)+(engines[e].last_status*10)+o), 10, 210+e);
    if (engines[e].operating) {
      digitalWrite (engines[e].pin_on, HIGH); // stop power to the engine
      delay (engine_direction_switch_delay);
    } // end of delay if engine was operating (most probaly in oposite direction)
    //DEBUG:payload_tx_only (10, ((engines[e].operating*100)+(engines[e].last_status*10)+o), 10, 220+e);
    engines[e].operating = true;
    bitSet (sleep_lock,sleep_lock_orders_engine_running);
    mains_on (mains_lock_bit_engines);
    //DEBUG:payload_tx_only (10, ((engines[e].operating*100)+(engines[e].last_status*10)+o), 10, 230+e);
    if (engines[e].pin_down != 0) { digitalWrite (engines[e].pin_down, HIGH); }
    //DEBUG:payload_tx_only (10, ((engines[e].operating*100)+(engines[e].last_status*10)+o), 10, 240+e);
    digitalWrite (engines[e].pin_on, LOW);
    digitalWrite (engines[e].pin_power, HIGH);
    //DEBUG:payload_tx_only (10, ((engines[e].operating*100)+(engines[e].last_status*10)+o), 10, 250+e);
    engines[e].start = millis ();
    //DEBUG:payload_tx_only (10, ((engines[e].operating*100)+(engines[e].last_status*10)+o), 10, 200+e);
    return;
  }
  //DEBUG:payload_tx_only (10, ((engines[e].operating*100)+(engines[e].last_status*10)+o), 10, 130+e);
  if ((engines[e].last_status != o) && (o == 3)) {
    // switch on
    if (f) { engines[e].last_status = o; }
    if (engines[e].operating) { return; } // Do nothing if switch is already on
    engines[e].operating = true;
    bitSet (sleep_lock,sleep_lock_orders_engine_running);
    mains_on (mains_lock_bit_engines);
    digitalWrite (engines[e].pin_on, LOW);
    digitalWrite (engines[e].pin_power, HIGH);
    return;
  }
  //DEBUG:payload_tx_only (10, ((engines[e].operating*100)+(engines[e].last_status*10)+o), 10, 140+e);
  if ((engines[e].operating) && (o == 0)) {
    // stop
    if (f) { engines[e].last_status = o; }
    engines[e].operating = false;
    digitalWrite (engines[e].pin_on, HIGH);
    engines_last_stop_command_received = millis ();
    bitSet (sleep_lock,sleep_lock_wait_after_engine_stop);
    if (engines[e].pin_down != 0) { digitalWrite (engines[e].pin_down, HIGH); }
    digitalWrite (engines[e].pin_power, HIGH);
    return;
  }
}

void engine_stop_all () {
  int i;
  for (i = 0; i < number_of_engines; i++) { engine_change (i, 0, true); }
}

void voltage_send (unsigned char p1, unsigned char bit_p2, unsigned char p2) { 
  // UNUSED KEPT ONLY FOR REFERENCE
  // bit_p2 is used to indicate which bit to set for payload p2, if bit_p2 is >= 8 then p2 is empty and no bit is set and p2 is not included in the transfer
  union Frame tx_t ;
  tx_t.frame = 0;
  tx_t.d.target = server_id;
  tx_t.d.payload1 = p1;
  if (bit_p2 < 8 ) {
    tx_t.d.payload2 = p2;
    bitSet (tx_t.d.order,bit_p2);
  }
  else {
    tx_t.d.payload2 = 0;
  }
  bitSet (tx_t.d.order,voltage_orders_bit_position);
  //rf_tx_only (tx_t.frame); // replaced by rf_trx 
  rf_trx (tx_t.frame); // Can result in data not send in case there was another transmission ongoing, but these data can be lost and are not so valuable
}

unsigned long voltage_read (unsigned int internal, unsigned int voltage, unsigned int voltage_admux, unsigned int light, unsigned int light_admux) {
  // OBSOLETE CODE TO BE DELETED
  // Internal is reference voltage measured against incomming
  // Voltage is external voltage measured against vref
  // Light is reference voltage measured against light sensor pin
  unsigned long refVoltage;
  unsigned long measuredVoltage;
  unsigned int channel;
  unsigned long refmv;
	unsigned long millis_now = millis ();
	if (serial_messages) { Serial.print ("V T: "); Serial.print (millis_now); Serial.print ("-"); Serial.print (voltage_read_last); Serial.print ("(="); Serial.print (millis_now - voltage_read_last ); Serial.print (") <="); Serial.print (voltage_read_ms);}
  if (millis_now - voltage_read_last <= voltage_read_ms ) { return 0 ; }
	//voltage_read_last = millis_now; see end of measure where new value is read to not count for measure time
  if (internal != 0) {
    ADMUX = 78; // REFS0 (6), MUX 3 (3), MUX 2 (2), MUX1 (1)
    delay (delay_before_v_measure); // to settle voltage
    bitSet (ADCSRA, ADSC); // Start conversion ADSC (6)
    while (bit_is_set(ADCSRA, ADSC)); // Wail for ADSC to become 0
    refVoltage = ADCL; // ADCH is updated only after ADCL is read
    refVoltage |= ADCH << 8;
   voltage_tmp_input = ((1100L * 1024L) / (refVoltage)); // Main calculation step 1 - voltage calculation
    voltage_tmp_input = (voltage_tmp_input * 250L) / 5000L ; // Main calculateion step 2 - normalization to scale 0 .. 250 where 250 = 5V
//    voltage_tmp_input = ((1100L * 1024L * 250L) / (refVoltage * 5000L)); // summary of the 2 above steps
//    voltage_tmp_input = ((56320L) / (refVoltage)); // summary of the 2 above steps simplified
    if (voltage_tmp_input > 255) { voltage_input = 255; } else {voltage_input = voltage_tmp_input; }
    if (serial_messages) { refmv = 1126400L / refVoltage; Serial.print (" R: "); Serial.print (refVoltage); Serial.print (" / "); Serial.print (refmv); Serial.print (" p: "); Serial.print (voltage_input); }
  } // internal voltage measure
  if (voltage != 0) {
    channel = voltage_admux;
    ADMUX = channel;
    delay (delay_before_v_measure); // to settle voltage
    //  ADCSRA |= 72; // Allow interrupt ADIE (3) bit set and start conversion ADSC (6)
    //  while (ADCSRA & 64 > 0); // Wail for ADSC to become 0
    bitSet (ADCSRA, ADSC); // Start conversion ADSC (6)
    while (bit_is_set(ADCSRA, ADSC)); // Wail for ADSC to become 0
    measuredVoltage = ADCL; // ADCH is updated only after ADCL is read
    measuredVoltage |= ADCH << 8;
    //voltage_measure =  (((measuredVoltage * 1100L * (v_in_resistor + v_gnd_resistor) * 250L) / (1024L * v_gnd_resistor * 5000L)) & 0x000000ffUL) ;// Conversion of external voltage measure where 250 = 1.1V (can not be processed by arduino due to size of values
    voltage_tmp_measure =  (measuredVoltage * 1100L) / 1024L;
    voltage_tmp_measure =  (voltage_tmp_measure * (v_in_resistor + v_gnd_resistor)) / v_gnd_resistor;
    voltage_tmp_measure =  (voltage_tmp_measure * 250L) / 5000L;
//    voltage_tmp_measure =  (((measuredVoltage * 55L * (v_in_resistor + v_gnd_resistor)) / (1024L * v_gnd_resistor))) ;// Conversion of external voltage measure where 250 = 1.1V, precalculated the fixed numbers, to be tested instead of above stepwise calculation
    if (voltage_tmp_measure > 255) { voltage_measure = 255; } else {voltage_measure = voltage_tmp_measure; }
    if (serial_messages) { Serial.print (" M("); Serial.print (channel & 15); Serial.print ("): "); Serial.print (measuredVoltage); Serial.print (" / "); measuredVoltage = (measuredVoltage * 1100L) / 1024L; Serial.print (measuredVoltage); Serial.print (" / "); measuredVoltage = (measuredVoltage * (v_in_resistor + v_gnd_resistor)) / v_gnd_resistor; Serial.print (measuredVoltage); }
  }
  if (light != 0) {
    if (light_sensor_power_pin != 0) { pinMode (light_sensor_power_pin, OUTPUT); digitalWrite (light_sensor_power_pin, HIGH);} 
    channel = light_admux;
    ADMUX = channel;
    if (light_sensor_power_pin != 0) { delay (delay_before_v_measure); } // to settle voltage
    //  ADCSRA |= 72; // Allow interrupt ADIE (3) bit set and start conversion ADSC (6)
    //  while (ADCSRA & 64 > 0); // Wail for ADSC to become 0
    bitSet (ADCSRA, ADSC); // Start conversion ADSC (6)
    while (bit_is_set(ADCSRA, ADSC)); // Wail for ADSC to become 0
    if (light_sensor_power_pin != 0)  { digitalWrite (light_sensor_power_pin, LOW); pinMode (light_sensor_power_pin, INPUT); }
    measuredVoltage = ADCL; // ADCH is updated only after ADCL is read
    measuredVoltage |= ADCH << 8;
    voltage_tmp_light = (((measuredVoltage * 255L) / 1024L)) ; // Conversion of light sensor, only represents relative value to input voltage where 255 is equal to input voltage
    if (voltage_tmp_light > 255) { voltage_light = 255; } else {voltage_light = voltage_tmp_light; }
    if (serial_messages) { Serial.print (" L("); Serial.print (channel & 15); Serial.print ("): "); Serial.print (measuredVoltage); Serial.print (" / "); measuredVoltage = (measuredVoltage * refmv) / 1024L; Serial.print (measuredVoltage); Serial.print (" / "); Serial.print (round (100*measuredVoltage/refmv)); Serial.print (" p: "); Serial.print (voltage_light); }
  }
  if (serial_messages) { Serial.println ("."); }
	voltage_read_last = millis ();
  if (voltage_read_last - voltage_send_last <= voltage_send_ms ) { return measuredVoltage; } 
  else {
    if ( light !=0 ) {
      if ( ( voltage_light < voltage_ignore_min ) || ( voltage_light > voltage_ignore_max ) ) {
        if (voltage_light >= voltage_ignore_range) { voltage_ignore_min = voltage_light - voltage_ignore_range; } else { voltage_ignore_min =  0; }
        if (voltage_light <= (255 - voltage_ignore_range)) { voltage_ignore_max = voltage_light + voltage_ignore_range; } else { voltage_ignore_max =  255; }
        if (internal != 0) {
          payload_send (voltage_orders_bit_position,voltage_input, light_orders_bit_position, voltage_light); 
        }
        else {
          payload_send (10,0, light_orders_bit_position, voltage_light);
        }
      }
    }
    else {
      if (internal != 0) { payload_send (voltage_orders_bit_position,voltage_input,10,0); }
    }
    voltage_send_last = millis ();
  }
  //if (serial_messages) { Serial.println ("<"); }
	return measuredVoltage;
} // readVoltage

unsigned int adac_read (unsigned char channel) {
  unsigned int measured_value;
  ADMUX = channel;
  delay (delay_before_v_measure); // to settle voltage
  bitSet (ADCSRA, ADSC); // Start conversion ADSC (6)
  while (bit_is_set(ADCSRA, ADSC)); // Wail for ADSC to become 0
  measured_value = ADCL; // ADCH is updated only after ADCL is read
  measured_value |= ADCH << 8;
  return measured_value;
}

unsigned int analog_pin_to_admux (unsigned char analog_pin) {
  unsigned char channel = analog_pin;
  switch (analog_pin) {
    case A0:
      channel = 0; break;
    case A1:
      channel = 1; break;
    case A2:
      channel = 2; break;
    case A3:
      channel = 3; break;
    case A4:
      channel = 4; break;
    case A5:
      channel = 5; break;
    case A6:
      channel = 6; break;
    case A7:
      channel = 7; break;
    case 0:
      channel = 14; break;
  }
  return (channel);
}

unsigned int ldr_measure () {
  unsigned long ldr_current_run = millis ();
  unsigned long ldr_measured_1024;
  unsigned char ldr_measured_255;
  unsigned char channel = 0;
  if ( (light_ldr_sensor_connected) && (light_ldr_read_send_ms > 0) && (ldr_current_run - light_ldr_last_run > light_ldr_read_send_ms) ) {
    // light sensor interval known and also already passed
    if (light_ldr_sensor_arduino_powered) {
      pinMode (light_ldr_sensor_pin_power, OUTPUT);
      digitalWrite (light_ldr_sensor_pin_power, HIGH);
      delay (light_ldr_delay_after_power);
    }
    channel = analog_pin_to_admux (light_ldr_sensor_pin_measure) | 64 /* AVCC with external capacitor at AREF pin */ ;
    ldr_measured_1024 = adac_read (channel);
    if (light_ldr_sensor_arduino_powered) {
      pinMode (light_ldr_sensor_pin_power, INPUT);
    }
    ldr_measured_1024 = (((ldr_measured_1024 * 255L) / 1024L)) ; // Conversion of light sensor, only represents relative value to input voltage where 255 is equal to input voltage
    if ( ldr_measured_1024 < 0 ) {
      ldr_measured_255 = 0;
    }
    else if ( ldr_measured_1024 > 255 ) {
      ldr_measured_255 = 255;
    }
    else {
      ldr_measured_255 = ldr_measured_1024;
    }
    if ( ((int)light_ldr_last_measured - (int)ldr_measured_255) < light_ldr_ignore_range ) {
      // sending value as it has bigger difference
      payload_send (10,0, light_ldr_orders_bit_position, ldr_measured_255);
      light_ldr_last_measured = ldr_measured_255;
    }
    light_ldr_last_run = ldr_current_run;
  }
}

void battery_monitor (bool force) {
  unsigned int battery_voltage_read;
  unsigned long minimal_timer = battery_measure_interval;
  if (battery_connected) {
    if (battery_charging_on) {
      if (battery_measure_interval > battery_min_charging_time ) { 
        minimal_timer = battery_min_charging_time;
      }
    }
    if ( (force) || ( millis () - battery_last_measured > minimal_timer )) {
      battery_voltage_read = adac_read (analog_pin_to_admux (battery_measure_pin) | 64 /* AVCC with external capacitor at AREF pin */) ;
      battery_last_measured = millis ();
      if ( battery_measure_pin == 0 ) {
        if (battery_voltage_read > battery_start_mains) {
          battery_start_charging = millis();
          //payload_send (10,(unsigned char)(((battery_voltage_read * 255L) / 1024L)), 5, 77); //DEBUG TODO DELETE
          mains_on (mains_lock_bit_battery);
          battery_charging_on = true;
        }
        if (battery_voltage_read < battery_stop_mains) {
          if (millis () - battery_start_charging > battery_min_charging_time) {
            mains_off (mains_lock_bit_battery);
            battery_charging_on = false;
          }
        }
      } // reverse measure
      else {
        if (battery_voltage_read < battery_start_mains) {
          battery_start_charging = millis();
          mains_on (mains_lock_bit_battery);
          battery_charging_on = true;
        }
        if (battery_voltage_read > battery_stop_mains) {
          if (millis () - battery_start_charging > battery_min_charging_time) {
            mains_off (mains_lock_bit_battery);
            battery_charging_on = false;
          }
        }
      } // positive measure
    } // measure timer check if ending here
  }
}

void process_orders () {
  // Here is the engine number decreased by 1 as in openhab 0 is all stop, in arduino world frame 0 is all stop, engine orders with engine 0 and orders 0 is stop all engines on this target
  if (serial_messages) { Serial.print ("Processing orders "); }
  if (rx_queue_count < 1) { bitClear(sleep_lock,sleep_lock_orders_to_be_processed); if (serial_messages) { Serial.print ("Q: "); Serial.print (rx_queue_count); Serial.println (" empty."); } return; }
  if (rx_queue [rx_queue_start].frame == 0)
  { // All stop message received
    if (serial_messages) { Serial.print ("ALL STOP: "); Serial.print (rx_queue [rx_queue_start].frame); }
    engine_stop_all ();
    rx_queue_start = 0;
    rx_queue_count = 0;
    if (serial_messages) { Serial.println (" STOPPED."); }
    return ;
  } // All stop message received
  if (bitRead (rx_queue [rx_queue_start].d.order,engine_orders_bit_position) == 1 )
  { // Engine orders received
    if (serial_messages) { Serial.print ("ENGINE: "); Serial.print (rx_queue [rx_queue_start].d.order); Serial.print (" P1: "); Serial.print (rx_queue [rx_queue_start].d.payload1); Serial.print (" P2: "); Serial.print (rx_queue [rx_queue_start].d.payload2); }
    if ((rx_queue [rx_queue_start].d.payload1 == 0) && (rx_queue [rx_queue_start].d.payload2 == 0))
      { 
        if (serial_messages) { Serial.print (" all stop"); }
        engine_stop_all ();
      }
    else
      { // Change engines
        if (serial_messages) { Serial.print (" E: "); Serial.print (rx_queue [rx_queue_start].d.payload1-1); Serial.print (" to: "); Serial.print (rx_queue [rx_queue_start].d.payload2); }
        //DEBUG:payload_tx_only (10, ((rx_queue [rx_queue_start].d.payload1-1)*10+rx_queue [rx_queue_start].d.payload2), 10, 49);
        engine_change (rx_queue [rx_queue_start].d.payload1-1, rx_queue [rx_queue_start].d.payload2, true);
      } // Change engines
    bitClear (rx_queue [rx_queue_start].d.order,engine_orders_bit_position);
    if (serial_messages) { Serial.print (" Done"); }
  } // Engine orders received
  if ((rx_queue [rx_queue_start].d.order & orders_mask) == 0) { // all orders understood by this client processed
    rx_queue_remove ();
  } // all orders understood by this client processed
  if (rx_queue_count < 1) { bitClear(sleep_lock,sleep_lock_orders_to_be_processed); if (serial_messages) { Serial.print (" Q: "); Serial.print (rx_queue_count); Serial.print (" empty"); } }
  if (serial_messages) { Serial.println ("."); }
}

void sleep_lock_validate () {
  unsigned char i;
  bool mains_turn_off_attempt = true;
  if (bitRead (sleep_lock,sleep_lock_orders_engine_running) == 1) { // Engines blocking sleep
    bitClear (sleep_lock,sleep_lock_orders_engine_running);
    for (i = 0; i < number_of_engines; i++) { // Loop all engines
      if (engines[i].operating) { // Engine running
        if ((millis () - engines[i].start < engines[i].runtime) || (engines[i].pin_down == 0)) { // Engine running and should be running (either based on time or based on fact that it is switch with unlimitd runtime pin_down == 0)
          bitSet (sleep_lock,sleep_lock_orders_engine_running);
				  break; // We can break the cycle as we already know that sleep will be mot possible
        } // Engine running and should be running
        else { // Engine running, but should be shut down
          //payload_send (10,i, 10, 47); // TODO this is only debug notification can be removed in future
          engine_change (i,0,false);
        } // Engine running, but should be shut down
      } // Engine running
    } // Loop all engines
  } // Engines blocking sleep
  if (bitRead (sleep_lock,sleep_lock_wait_after_engine_stop) == 1) { // Delay after stop command when active listening
    if (millis () - engines_last_stop_command_received >= wait_after_engine_stop) {
      bitClear (sleep_lock,sleep_lock_wait_after_engine_stop);
    }
  }
  if (bitRead (sleep_lock,sleep_lock_orders_engine_running) == 0) { // All engines are shut down, we try to shut down mains
    mains_turn_off_attempt = mains_off (mains_lock_bit_engines);
    if (mains_turn_off_attempt) {
      bitSet (sleep_lock,sleep_lock_mains_on); // Turn off was not success so we setup blocker and try to turn off later
    }
  }
  if (bitRead (sleep_lock,sleep_lock_mains_on) == 1) { // We have mains on, this is mainly for self lock (as there is nobody else to turn it off) so we try to turn off and if we turn off we clear also the lock
    mains_turn_off_attempt = mains_off (mains_lock_bit_self);
    if (!mains_turn_off_attempt) {
      bitClear (sleep_lock,sleep_lock_mains_on);
    }
  } 
  mains_off (mains_lock_bit_self); // security to make sure mains do not stay hanging
} // sleep_lock_validate

void setup() {
  unsigned int i;
  if (serial_messages) { Serial.begin (9600); printf_begin (); Serial.println ("START");}
  analogReference( INTERNAL );
  if (serial_messages) { Serial.println ("Setting engine pins"); }
  for (i = 0; i < number_of_engines; i++) {
    pinMode (engines[i].pin_down, OUTPUT);
    digitalWrite (engines[i].pin_down, HIGH);
    pinMode (engines[i].pin_on, OUTPUT);
    digitalWrite (engines[i].pin_on, HIGH);
    pinMode (engines[i].pin_power, OUTPUT);
    digitalWrite (engines[i].pin_power, HIGH);
    engines[i].last_status = 0;
  }
  rx_last.frame = 0;
  mains_startup_sequence ();
  battery_monitor (true); // we start battery monitor at the beginning with force (ignoring last read time)
  if (serial_messages) { Serial.println ("Setup finished starting main loop"); }
}

void loop() {
  rf_trx_done = false;
  if (sleep_lock == 0) {
    // NO ACTIVE ORDER
    shutdown_radio ();
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    //delay (8000); // To be commented in final code
    // Adjust timers as during deep sleep time is not ticking
    //voltage_read_last = voltage_read_last - 8000; //DELETE OBSOLETE CODE
    //voltage_send_last = voltage_send_last - 8000; //DELETE OBSOLETE CODE
    if (light_ldr_sensor_connected) { light_ldr_last_run = light_ldr_last_run - 8000; }
    if (battery_connected) { battery_last_measured = battery_last_measured - 8000; }
    if (mains_switch_connected) { mains_last_on_time = mains_last_on_time - 8000; }
    delay (sleep_after_deep_sleep);
  }
  else
  { // active orders can not sleep
    init_radio ();
    process_orders ();
    delay (sleep_during_sleep_lock);
    sleep_lock_validate ();
  } // active orders can not sleep
  init_radio ();
  //if (voltage_read_ms > 0) { voltage_read (1,1,193,1,66); } //DELETE OBSOLETE CODE
  //if (voltage_read_ms > 0) { voltage_read (0,0,193,1,66); } //DELETE OBSOLETE CODE
  if (light_ldr_sensor_connected) { ldr_measure (); }
  if (battery_connected) { battery_monitor (false); }
  if (!rf_trx_done) { rf_trx (0); }
}
