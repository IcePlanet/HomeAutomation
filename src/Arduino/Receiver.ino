#include <nRF24L01.h>
#include <printf.h>
#include <SPI.h>
#include <RF24.h>
#include <RF24_config.h>
#include "LowPower.h"

RF24 radio(7, 8);

// MY ID
const unsigned char my_id = 10;

// Broadcast ID
const unsigned char broadcast_id = 255;

// SERVER ID
const unsigned char server_id = 1;

// Messages mode (if to write messages to serial)
const bool serial_messages = false;

// RADIO definition
const unsigned int radio_power_pin = 9;
const unsigned int radio_init_delay = 10;
bool radio_status = false;
byte addresses[][6] = {"1Node", "2Node"};
const unsigned long receive_duration = 50; //duration of receive in miliseconds
const unsigned long receive_loop_delay = 3; //delay of one receive loop in miliseconds
const bool retransfer_bit_set = false; // retransfer bit settings
const unsigned char retransfer_bit_position = 6; // Location of re-transfer bit
const unsigned char ack_bit_position = 7; // Location of ack bit
union frame {
    unsigned long frame;
    struct {
        char target;
        char payload1;
        char payload2;
        char order;
    } d;
};
const unsigned char rx_max_queue_size = 100;
unsigned char rx_queue_start = 0;
unsigned char rx_queue_count = 0;
union frame rx_tmp, rx_last;
union frame rx_queue [rx_max_queue_size];
const unsigned char tx_max_queue_size = 100;
unsigned char tx_queue_start = 0;
unsigned char tx_queue_count = 0;
union frame tx_tmp, tx_last;
union frame tx_queue [tx_max_queue_size];

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
unsigned char sleep_lock = 0 // bit field to check if anything is in proogress
const unsigned char sleep_lock_orders_to_be_processed = 0 // bit position for active orders
const unsigned char sleep_lock_orders_engine_running = 1 // bit position for running engine
const unsigned char orders_mask = B00010000 // Mask for orders this system is able to accept, if none of them is set to 1 it is considered as all-stop order

// VOLTAGE measure
unsigned char voltage_input = 0;  // Input voltage for the arduino board
unsigned char voltage_measure = 0;  // Voltage measure (up to 1.1V)
unsigned char voltage_light = 0; // Voltage measure with light sensor
const unsigned char voltage_orders_bit_position = 5; // Position of voltage bit
const unsigned long v_in_resistor = 47000; //multiplication removed
const unsigned long v_gnd_resistor = 8200; //multiplication removed
const unsigned char v_measure_pin = 1;
const unsigned char v_measure_gnd = 10;
const unsigned char delay_before_v_measure = 7;
const unsigned char light_sensor_power_pin = 0;
const unsigned char voltage_loops_read = 9; // read voltage each X loops (if set to 0 not executed)
const unsigned char voltage_loops_send = 9; // send voltage each X loops, but not read, always mus tbe read by the voltage read (if set to 0 not executed)
const unsigned char voltage_turn_on_external = 50;  // Treshold when to start external power source
const unsigned char voltage_turn_off_external = 240; // Treshold when to stop external power source
unsigned char v_read = 0; // voltage counter
unsigned char v_send = 0; // voltage counter

// Sleeping cycles in main loop
const unsigned int sleep_after_deep_sleep = 50;
const unsigned int sleep_during_sleep_lock = 500 + my_id;

const unsigned int size_of_long = sizeof (unsigned long);

bool queue_add (struct frame *a, struct frame *q, unsigned char *start, unsigned char *count, unsigned char max) {
  unsigned char new_pos = *start + *count;
  unsigned char i;
  if (new_pos >= max) {
    if (*start > 0) { // Can shift queue
      for (i = 0; i < *count; i++) { q [i] = q [i+*start]; }
      *start = 0;
      new_pos = *start + *count;
    } // Can shift queue
    else { return false; } // can not add
  q [new_pos] = *a;
  *count += 1;
  return true;
  }
}

bool queue_remove (unsigned char *start, unsigned char *count) {
  if (*count > 0) { *count -= 1; }
  if (*count == 0) { *start = 0; }
    else { *start += 1; }
  return true;
}

bool rx_queue_add (struct frame a) {
  unsigned char new_pos = rx_queue_start + rx_queue_count;
  unsigned char i;
  if (new_pos >= rx_max_queue_size) {
    if (rx_queue_start > 0) { // Can shift queue
      for (i = 0; i < rx_queue_count; i++) { rx_queue [i] = rx_queue [i+rx_queue_start]; }
      rx_queue_start = 0;
      new_pos = rx_queue_start + rx_queue_count;
    } // Can shift queue
    else { return false; } // can not add
  rx_queue [new_pos] = a;
  rx_queue_count += 1;
  return true;
  }
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
  pinMode(radio_power_pin, OUTPUT);
  digitalWrite (radio_power_pin, HIGH);
  delay (radio_init_delay);
  radio.begin();
  radio.setPALevel(RF24_PA_MAX);
  radio.openWritingPipe(addresses[1]);
  radio.openReadingPipe(1, addresses[0]);
  radio.startListening ();
  radio_status = true;
  if (serial_messages) { Serial.print ("Radio powered up, on "); Serial.print (millis ()); Serial.println (" details follow: "); radio.printDetails (); }
}

void shutdown_radio () {
  if (!radio_status) {
    return;
  }
  pinMode(radio_power_pin, INPUT);
  radio_status = false;
  if (serial_messages) { Serial.print ("Radio shut down on "); Serial.println (millis ()); }

}

bool rf_tx_only (unsigned long to_send_payload) {
  if (to_send_payload > 0) {
    radio.stopListening();
    radio.write (&to_send_payload, size_of_long));
    radio.startListening();
  }
}
  
bool rf_trx (unsigned long to_send_payload) {
  unsigned long tstart, tend, receive_time;
  unsigned int i = 0;
  bool clear_air = true;
  init_radio ();
  tstart = millis ();
  while (millis () - tstart < receive_duration)
  {
    i++;
    while (radio.available()) {                                   // While there is data ready
      if (serial_messages) { Serial.print ("Data to read ready\n"); }
      radio.read( &rx_tmp.frame, size_of_long );             // Get the payload
      receive_time = millis ();
      // TODO check re-transfer requests here
      if ((rx_tmp.d.target == my_id) || (rx_tmp.d.target == broadcast_id)) { // traffic is for this node
        if (serial_messages) { Serial.print ("RX: "); Serial.print (rx_tmp.frame); Serial.print (" T: "); Serial.println (receive_time); }
        if (bitRead (rx_tmp.frame, ack_bit_position) == 0) { // it is NOT ack message (we do not wait for ack's on client)
          if (serial_messages) { Serial.print (" O: "); Serial.print (rx_tmp.d.order); }
          if (retransfer_bit_set) { bitSet (rx_tmp.frame, retransfer_bit_position); } else { bitClear (rx_tmp.frame, retransfer_bit_position); }
          bitSet (rx_tmp.frame, ack_bit_position); // We always set ack bit to distinguisb later in rx_queue between 0 as order (equals 0) and 0 as result of processing (has ack set)
	        if (rx_tmp.target == my_id) { rf_tx_only (rx_tmp.frame); if (serial_messages) { Serial.print ("ACK: "); Serial.print (rx_tmp.frame); Serial.print (" T: ");Serial.println (millis ()); } } // Sending ack if it was for me
          if (rx.tmp.frame != rx_last.frame) { // New data have been received
            if ((rx_tmp.d.order & orders_mask) = 0) { rx_queue_start = 0; rx_queue_count = 1; rx_queue[rx_queue_start].frame = 0; } // stop all message is represented as 0 in rx_queue and also deletes queue until now (later)
            else { rx_queue_add (rx_tmp); }
            bitSet (sleep_lock,sleep_lock_orders_to_be_processed);
            if (serial_messages) { Serial.print (" Q val:"); Serial.print (rx_queue [rx_queue_start + rx_queue_count - 1]); Serial.print (" Q start:"); Serial.print (rx_queue_start); Serial.print (" Q count: "); Serial.print (rx_queue_count); }
	    rx_last = rx_tmp
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
      radio.stopListening();
      radio.write (&to_send_payload, size_of_long);
      radio.startListening ();
      if (serial_messages) { Serial.print ("SUM TRX: RX loops: "); Serial.print (i); Serial.print (" Tstart: "); Serial.print (tstart); Serial.print (" Tend "); Serial.print (tend); Serial.print (" Ttxp: "); Serial.println (millis ()); }
      return true;
    }
    else {
      if (tx_queue_count > 0) {
        to_send_payload = tx_queue[tx_queue_start].frame;
        radio.stopListening();
        radio.write (&to_send_payload, size_of_long);
        radio.startListening ();
        queue_remove (&tx_queue_start, &tx_queue_count);
        if (serial_messages) { Serial.print ("SUM TRX: RX loops: "); Serial.print (i); Serial.print (" Tstart: "); Serial.print (tstart); Serial.print (" Tend "); Serial.print (tend); Serial.print (" Ttxq: "); Serial.println (millis ()); }
      }
    } 
  }
  else {
    if (serial_messages) { Serial.print ("SUM RX: RX loops: "); Serial.print (i); Serial.print (" Tstart: "); Serial.print (tstart); Serial.print (" Tend "); Serial.print (tend); Serial.print (" Now: "); Serial.println (millis ()); }
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

unsigned long voltage_read (unsigned int internal, unsigned int voltage, unsigned int voltage_admux, unsigned int light, unsigned int light_admux) {
  // Internal is reference voltage measured against incomming
  // Voltage is external voltage measured against vref
  // Light is reference voltage measured against light sensor pin
  unsigned long refVoltage;
  unsigned long measuredVoltage;
  unsigned int channel;
  unsigned long refmv;
  if (serial_messages) { Serial.print ("V T: "); Serial.print (millis()); }
  if (internal != 0) {
    ADMUX = 78; // REFS0 (6), MUX 3 (3), MUX 2 (2), MUX1 (1)
    delay (delay_before_v_measure); // to settle voltage
    bitSet (ADCSRA, ADSC); // Start conversion ADSC (6)
    while (bit_is_set(ADCSRA, ADSC)); // Wail for ADSC to become 0
    refVoltage = ADCL; // ADCH is updated only after ADCL is read
    refVoltage |= ADCH << 8;
    voltage_input =  (((1100L * 1024L * 250L) / (5000L * refVoltage)) & 0x000000ffUL) ; // Conversion to range 0 - 255 where 250 = 5V
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
    voltage_measure =  (((measuredVoltage * 1100L * (v_in_resistor + v_gnd_resistor) * 250L) / (1024L * v_in_resistor * 1100L)) & 0x000000ffUL) ;// Conversion of external voltage measure where 250 = 1.1V
    if (serial_messages) { Serial.print (" M("); Serial.print (channel & 15); Serial.print ("): "); Serial.print (measuredVoltage); Serial.print (" / "); measuredVoltage = (measuredVoltage * 1100L) / 1024L; Serial.print (measuredVoltage); Serial.print (" / "); measuredVoltage = (measuredVoltage * (v_in_resistor + v_gnd_resistor)) / v_gnd_resistor; Serial.print (measuredVoltage); Serial.print (" p: "); Serial.print (voltage_measure); }
  }
  if (light != 0) {
    if (light_sensor_power_pin != 0) { pinMode (light_sensor_power_pin, OUTPUT); digitalWrite (light_sensor_power_pin, HIGH);} 
    channel = light_admux;
    ADMUX = channel;
    delay (delay_before_v_measure); // to settle voltage
    //  ADCSRA |= 72; // Allow interrupt ADIE (3) bit set and start conversion ADSC (6)
    //  while (ADCSRA & 64 > 0); // Wail for ADSC to become 0
    bitSet (ADCSRA, ADSC); // Start conversion ADSC (6)
    while (bit_is_set(ADCSRA, ADSC)); // Wail for ADSC to become 0
    if (light_sensor_power_pin != 0)  { digitalWrite (light_sensor_power_pin, LOW); pinMode (light_sensor_power_pin, INPUT); }
    measuredVoltage = ADCL; // ADCH is updated only after ADCL is read
    measuredVoltage |= ADCH << 8;
    voltage_light = ((measuredVoltage * 250L) / 1024L) & 0x000000ffUL) ; // Conversion of light sensor, only represents relative value to input voltage where 250 is equal to input voltage
    if (serial_messages) { Serial.print (" L("); Serial.print (channel & 15); Serial.print ("): "); Serial.print (measuredVoltage); Serial.print (" / "); measuredVoltage = (measuredVoltage * refmv) / 1024L; Serial.print (measuredVoltage); Serial.print (" / "); Serial.print (round (100*measuredVoltage/refmv)); Serial.print (" p: "); Serial.print (voltage_light); }
  }
  if (serial_messages) { Serial.println ("."); }
  return measuredVoltage;
} // readVoltage

void voltage_send (unsigned char p1, unsigned char p2) {
  union frame tx_t ;
  tx_t.frame = 0;
  tx_t.d.target = server_id;
  tx_t.d.payload1 = p1;
  tx_t.d.payload2 = p2;
  bitSet (tx.t.d.orders,voltage_orders_bit_position);
  queue_add (&tx_t, tx_queue, &tx_queue_start, &tx_queue_count, tx_max_queue_size);
}

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
  if (bitRead (rx_queue [rx_queue_start].d.orders,engine_orders_bit_position) == 1 )
  { // Engine orders received
    if (serial_messages) { Serial.print ("ENGINE orders: "); Serial.print (rx_queue [rx_queue_start].d.orders); Serial.print (" P1: "); Serial.print (rx_queue [rx_queue_start].d.payload1); Serial.print (" P2: "); Serial.print (rx_queue [rx_queue_start].d.payload2); }
    if ((rx_queue [rx_queue_start].d.payload1 == 0) && (rx_queue [rx_queue_start].d.payload2 == 0))
      { 
        if (serial_messages) { Serial.print (" all stop"); }
        engine_stop_all ();
      }
    else
      { // Change engines
        if (serial_messages) { Serial.print (" change engine: "); Serial.print (rx_queue [rx_queue_start].d.payload1-1); Serial.print (" to: "); Serial.print (rx_queue [rx_queue_start].d.payload2); }
        engine_change (rx_queue [rx_queue_start].d.payload1-1, rx_queue [rx_queue_start].d.payload2, true);
      } // Change engines
    bitClear (rx_queue [rx_queue_start].d.orders,engine_orders_bit_position);
    if (serial_messages) { Serial.print (" Done"); }
  } // Engine orders received
  if ((rx_queue [rx_queue_start].d.orders & orders_mask) == 0) { // all orders understood by this client processed
    rx_queue_remove ();
  } // all orders understood by this client processed
  if (rx_queue_count < 1) { bitClear(sleep_lock,sleep_lock_orders_to_be_processed); if (serial_messages) { Serial.print (" Q: "); Serial.print (rx_queue_count); Serial.print (" empty"); } }
  if (serial_messages) { Serial.println ("."); }
}

void sleep_lock_validate () {
  unsigned char i;
  if (bitRead (sleep_lock,sleep_lock_orders_engine_running) == 1) { // Engines blocking sleep
    bitClear (sleep_lock,sleep_lock_orders_engine_running);
    for (i = 0; i < number_of_engines; i++) { // Loop all engines
      if (engines[i].operating) { // Engine running
        if ((millis () - engines[i].start < engines[i].runtime) || (engines[i].pin_down == 0)) { // Engine running and should be running (either based on time or based on fact that it is switch with unlimitd runtime pin_down == 0)
          bitSet (sleep_lock,sleep_lock_orders_engine_running);
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
  engines [0].runtime = 15000; //miliseconds how long operate engine (not accurate)
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
  if (sleep_lock == 0) {
    // NO ACTIVE ORDER
    shutdown_radio ();
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    delay (sleep_after_deep_sleep);
  }
  else
  { // active orders can not sleep
    process_orders ();
    sleep_lock_validate ();
    delay (sleep_during_sleep_lock);
  } // active orders can not sleep
  if (voltage_loops_read > 0 ) {
    v_read ++;
    if (v_read >= voltage_loops_read) {
      v_read = 0;
      voltage_read (1,1,193,1,66);
    }
  }
  if (voltage_loops_send > 0 ) {
    v_send ++;
    if (v_send >= voltage_loops_send) {
      v_send = 0;
      voltage_send (voltage_input, voltage_light);
    }
  }
  rf_trx (0);
}