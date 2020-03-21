#include <nRF24L01.h>
#include <printf.h>
#include <SPI.h>
#include <RF24.h>
#include <RF24_config.h>
#include "LowPower.h"

RF24 radio(7, 8);

// MY ID
//const unsigned char my_id = 12; // Test device
const unsigned char my_id = 11; // 1st floor Peter BIG

// Broadcast ID
const unsigned char broadcast_id = 255;
const unsigned char all_id = 0;

// SERVER ID
const unsigned char server_id = 1;

// Messages mode (if to write messages to serial)
const bool serial_messages = false;

// RADIO definition
const unsigned int radio_power_pin = 9;
const unsigned int radio_init_delay = 10; // Delay after power is applied to radio, should be 105.3 ms (MIN: 101.6 ms; MAX: 110.3 ms) if this is proven as true it makes no sense to power off radio between sleep cycles (ms) NOT NEEDED ANY MORE, REWORKED
const unsigned char radio_sleep_wakeup_delay = 2; // Delay when radio is waken up from sleep state should be 1.5 ms according to datasheet (ms)
const unsigned char radio_wait_after_power = 100; // In miliseconds how long we need to wait after power is applied to radio
const unsigned char radio_wait_after_sleep = 10; // in miliseconds how long we need to wait after radio is waken up from deep sleep
unsigned char radio_wait_time = radio_wait_after_power;
bool radio_status = false; // if radio is active
bool radio_sleeping = false; // True if radio is in deep sleep
bool radio_voltage = false; // if radio has voltage (is powered up)
bool radio_trx_ready = false; // if radio is ready for transmission
bool rf_trx_done = false; // If trx was executed in this cycle
const bool radio_shutdown_power = true; // true to shutdown radio power on power down, false to enter sleep mode on power down
byte addresses[][6] = {"1Node", "2Node"};
const unsigned long receive_duration = 20; //duration of receive in miliseconds (ms)
const unsigned long receive_loop_delay = 1; //delay of one receive loop in miliseconds (ms)
const bool retransfer_bit_set = false; // retransfer bit settings
const unsigned char retransfer_bit_position = 6; // Location of re-transfer bit
const unsigned char ack_bit_position = 7; // Location of ack bit
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
const unsigned char rx_max_queue_size = 10;
unsigned char rx_queue_start = 0;
unsigned char rx_queue_count = 0;
union Frame rx_tmp, rx_last;
union Frame rx_queue [rx_max_queue_size];
const unsigned char tx_max_queue_size = 10;
unsigned char tx_queue_start = 0;
unsigned char tx_queue_count = 0;
union Frame tx_tmp, tx_last;
union Frame tx_queue [tx_max_queue_size];

// ENGINES definition
// For simplification the engine pins are HIGH due to the way how relay board is designed (can be done also with low as default but this is little bit more complicated as the pin_power is common for whole board (2, 4, 6 relays))
const unsigned int number_of_engines = 1;
const unsigned char engine_orders_bit_position = 4;
const unsigned int engine_direction_switch_delay = 678; // delay before direction of engine movement can be switched to other direction
struct engine_control {
  int pin_power; // Power for the relay board
  int pin_on; // relay switching on the power to engine default (NC) OFF, when triggered (NO) ON
  int pin_down; // relay switching direction of engine, default (NC) UP, when triggered (NO) DOWN (pin_down can be 0 if there is no engine, but simple switch)
  int last_status;
  unsigned long start;
  bool operating;
  unsigned long runtime;
};
struct engine_control engines [number_of_engines];

// Orders defaults
bool new_orders = false; // if there are new not yet processeed orders
unsigned char sleep_lock = 0; // bit field to check if anything is in proogress
const unsigned char sleep_lock_orders_to_be_processed = 0; // bit position for active orders
const unsigned char sleep_lock_orders_engine_running = 1; // bit position for running engine
const unsigned char orders_mask = B00010000; // Mask for orders this system is able to accept, if none of them is set to 1 it is considered as all-stop order

// VOLTAGE measure + LIGHT
unsigned char voltage_input = 0;  // Input voltage for the arduino board
unsigned long voltage_tmp_input = 0;  // Input voltage for the arduino board
unsigned char voltage_measure = 0;  // Voltage measure (up to 1.1V)
unsigned long voltage_tmp_measure = 0;  // Voltage measure (up to 1.1V)
unsigned char voltage_light = 0; // Voltage measure with light sensor
unsigned long voltage_tmp_light = 0; // Voltage measure with light sensor
unsigned long voltage_read_last = 0; // Time when voltage was last read
unsigned long voltage_send_last = 0; // Time when voltahe was last time sent
const unsigned char voltage_orders_bit_position = 5; // Position of voltage bit
const unsigned char light_orders_bit_position = 1; // Position of light bit
const unsigned long v_in_resistor = 470; // Resistor on voltage divider for main current (100ohm) be carefull: (1024 * 55 * (v_in_resistor + v_gnd_resistor)) <  4294967295 otherwise calculation will be broken !!!
const unsigned long v_gnd_resistor = 82; // Resistor on voltage divider for ground (100ohm) be carefull: (1024 * 55 * (v_in_resistor + v_gnd_resistor)) <  4294967295 otherwise calculation will be broken !!!
const unsigned char v_measure_pin = 1;
const unsigned char v_measure_gnd = 10;
const unsigned char delay_before_v_measure = 17; // 7
const unsigned char light_sensor_power_pin = A3;
//const unsigned int voltage_read_ms = 65432; // interval to read voltage (0 = disabled, implies also 0 on voltage_send_ms) (ms MAX:65535) 
const unsigned int voltage_read_ms = 32100; // interval to read voltage (0 = disabled, implies also 0 on voltage_send_ms) (ms MAX:65535)
const unsigned int voltage_send_ms = 32100; // interval to send voltage (0 = disabled) (ms MAX:65535), voltage will be NOT read, voltage will be send on NEXT read after this timer has expired
const unsigned char voltage_turn_on_external = 50;  // Treshold when to start external power source
const unsigned char voltage_turn_off_external = 240; // Treshold when to stop external power source
unsigned char v_read = 0; // voltage counter
unsigned char v_send = 0; // voltage counter
unsigned char voltage_ignore_range = 3; // what must be the minimal difference from last measure to send to server (total range is 2x voltage_ignore_range)
unsigned char voltage_ignore_min = 0; //setup in a way that condition will be always met on 1st run
unsigned char voltage_ignore_max = 0; 

// Sleeping cycles in main loop
const unsigned int sleep_after_deep_sleep = 1; // in ms normally cca 50 (ms)
const unsigned int sleep_during_sleep_lock = 500 + my_id;
const unsigned int sleep_engine_change = 11; // in ms, sleep after engine manipulation - if check is executed too fast (in the same ms as engine set) the result might be 0 leading to immediate engine switch off
const unsigned int sleep_before_lock_validate = 11; // in ms, sleep before validation of sleep lock validation - if check is executed too fast (in the same ms as engine set) the result might be 0 leading to immediate engine switch off

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

void init_radio () {
  if (radio_status) {
    return;
  }
  radio_trx_ready = false;
  if (!radio_voltage) {
		pinMode(radio_power_pin, OUTPUT);
		digitalWrite (radio_power_pin, HIGH);
		delay (radio_init_delay);
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
      pinMode(radio_power_pin, INPUT);
      radio_status = false;
      radio_voltage = false;
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

void power_down_radio () {   // Soft power down, enter standby mode (should be 900 nA according to datasheet, in reality seems to be 1.6 to 2.02 mA) according to datasheet startup time from this status is 1.5ms
// Do not use any more this function to be removed !!!
	if (radio_status) {
    radio.powerDown ();
		radio_status = false;
		if (serial_messages) { Serial.print ("Radio entered standby on: "); Serial.println (millis ()); }
  }
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
            else { rx_queue_add (rx_tmp); if (serial_messages) { Serial.print (" Q+"); } }
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

void engine_change (unsigned int e, unsigned int o, bool f) {
  if (serial_messages) { Serial.print ("Engine "); Serial.print (e); Serial.print (" change by order "); Serial.print (o); Serial.print (" now is: "); Serial.println (millis ()); }
  if (e >= number_of_engines) { return; }
  if ((engines[e].last_status != o) && (o == 1)) {
    // start move down
    if (f) { engines[e].last_status = o; }
    if (engines[e].operating) {
      digitalWrite (engines[e].pin_on, HIGH); // stop power to the engine
      delay (engine_direction_switch_delay);
    } // end of delay if engine was operating (most probaly in oposite direction)
    engines[e].operating = true;
    bitSet (sleep_lock,sleep_lock_orders_engine_running);
    if (engines[e].pin_down != 0) { digitalWrite (engines[e].pin_down, LOW); }
    digitalWrite (engines[e].pin_on, LOW);
    digitalWrite (engines[e].pin_power, HIGH);
    engines[e].start = millis ();
    delay (sleep_engine_change);
    return;
  }
  if ((engines[e].last_status != o) && (o == 2)) {
    // start move up
    if (f) { engines[e].last_status = o; }
    if (engines[e].operating) {
      digitalWrite (engines[e].pin_on, HIGH); // stop power to the engine
      delay (engine_direction_switch_delay);
    } // end of delay if engine was operating (most probaly in oposite direction)
    engines[e].operating = true;
    bitSet (sleep_lock,sleep_lock_orders_engine_running);
    if (engines[e].pin_down != 0) { digitalWrite (engines[e].pin_down, HIGH); }
    digitalWrite (engines[e].pin_on, LOW);
    digitalWrite (engines[e].pin_power, HIGH);
    engines[e].start = millis ();
    delay (sleep_engine_change);
    return;
  }
  if ((engines[e].last_status != o) && (o == 3)) {
    // switch on
    if (f) { engines[e].last_status = o; }
    if (engines[e].operating) { return; } // Do nothing if switch is already on
    engines[e].operating = true;
    bitSet (sleep_lock,sleep_lock_orders_engine_running);
    digitalWrite (engines[e].pin_on, LOW);
    digitalWrite (engines[e].pin_power, HIGH);
    delay (sleep_engine_change);
    return;
  }
  if ((engines[e].operating) && (o == 0)) {
    // stop
    if (f) { engines[e].last_status = o; }
    engines[e].operating = false;
    digitalWrite (engines[e].pin_on, HIGH);
    if (engines[e].pin_down != 0) { digitalWrite (engines[e].pin_down, HIGH); }
    digitalWrite (engines[e].pin_power, HIGH);
    return;
  }
}

void engine_stop_all () {
  int i;
  for (i = 0; i < number_of_engines; i++) { engine_change (i, 0, true); }
  bitClear (sleep_lock,sleep_lock_orders_engine_running);
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

unsigned long voltage_read (unsigned int internal, unsigned int voltage, unsigned int voltage_admux, unsigned int light, unsigned int light_admux) {
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
    voltage_measure =  (((measuredVoltage * 1100L * (v_in_resistor + v_gnd_resistor) * 250L) / (1024L * v_gnd_resistor * 5000L)) & 0x000000ffUL) ;// Conversion of external voltage measure where 250 = 1.1V (can not be processed by arduino due to size of values
    voltage_tmp_measure =  (measuredVoltage * 1100L) / 1024L;
    voltage_tmp_measure =  (voltage_tmp_measure * (v_in_resistor + v_gnd_resistor)) / v_gnd_resistor;
    voltage_tmp_measure =  (voltage_tmp_measure * 250L) / 5000L;
//    voltage_tmp_measure =  (((measuredVoltage * 55L * (v_in_resistor + v_gnd_resistor)) / (1024L * v_gnd_resistor))) ;// Conversion of external voltage measure where 250 = 1.1V, precalculated the fixed numbers, to be tested instead of above stepwise calculation
    if (voltage_tmp_measure > 255) { voltage_measure = 255; } else {voltage_measure = voltage_tmp_measure; }
    if (serial_messages) { Serial.print (" M("); Serial.print (channel & 15); Serial.print ("): "); Serial.print (measuredVoltage); Serial.print (" / "); measuredVoltage = (measuredVoltage * 1100L) / 1024L; Serial.print (measuredVoltage); Serial.print (" / "); measuredVoltage = (measuredVoltage * (v_in_resistor + v_gnd_resistor)) / v_gnd_resistor; Serial.print (measuredVoltage); Serial.print (" p: "); Serial.print (voltage_measure); }
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

void process_orders () {
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
  delay (sleep_before_lock_validate);
  if (bitRead (sleep_lock,sleep_lock_orders_engine_running) == 1) { // Engines blocking sleep
    bitClear (sleep_lock,sleep_lock_orders_engine_running);
    for (i = 0; i < number_of_engines; i++) { // Loop all engines
      if (engines[i].operating) { // Engine running
        if ((millis () - engines[i].start < engines[i].runtime) || (engines[i].pin_down == 0)) { // Engine running and should be running (either based on time or based on fact that it is switch with unlimitd runtime pin_down == 0)
          bitSet (sleep_lock,sleep_lock_orders_engine_running);
				  break; // We can break the cycle as we already know that sleep will be mot possible
        } // Engine running and should be running
        else { // Engine running, but should be shut down
          engine_change (i,0,false);
        } // Engine running, but should be shut down
      } // Engine running
    } // Loop all engines
  } // Engines blocking sleep
} // sleep_lock_validate

void setup() {
  unsigned int i;
  if (serial_messages) { Serial.begin (9600); printf_begin (); Serial.println ("START");}
  analogReference( INTERNAL );
  if (serial_messages) { Serial.println ("Setting engine pins"); }
  engines [0].pin_power = 6;
  engines [0].pin_on = 5;
  engines [0].pin_down = 3;
  engines [0].last_status = -1;
  engines [0].operating = false;
  engines [0].runtime = 30000; //miliseconds how long operate engine (not accurate)
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
    voltage_read_last = voltage_read_last - 8000;
    voltage_send_last = voltage_send_last - 8000;
    delay (sleep_after_deep_sleep);
  }
  else
  { // active orders can not sleep
    process_orders ();
    sleep_lock_validate ();
    delay (sleep_during_sleep_lock);
  } // active orders can not sleep
  init_radio ();
  //if (voltage_read_ms > 0) { voltage_read (1,1,193,1,66); }
  if (voltage_read_ms > 0) { voltage_read (0,0,193,1,66); }
  if (!rf_trx_done) { rf_trx (0); }
}
