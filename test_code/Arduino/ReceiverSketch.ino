#include <nRF24L01.h>
#include <printf.h>
#include <SPI.h>
#include <RF24.h>
#include <RF24_config.h>
#include "LowPower.h"

RF24 radio(7, 8);
unsigned long t;
unsigned long size_of_long;
unsigned long payload;
unsigned long payload_ack;
unsigned long receive_time; // test
const unsigned long mask_id = 4278190080;
const unsigned long mask_order = 15;
const unsigned long mask_payload = 16777200;
const unsigned int delay_before_v_measure = 7;
const unsigned int engine_direction_switch_delay = 1000; // delay before direction of engine movement can be switched to other direction

bool orders = false;
unsigned long order_new = 0;

bool active_order = false;

// MY ID
unsigned long my_id = 10;

// ENGINES definition
// For simplification the engine pins are HIGH due to the way how relay board is designed (can be done also with low as default but this is little bit more complicated as the pin_power is common for whole board (2, 4, 6 relays)
unsigned int number_of_engines = 1;
struct engine_control {
  int pin_power; // Power for the relay board
  int pin_on; // relay switching on the power to engine default (NC) OFF, when triggered (NO) ON
  int pin_down; // relay switching direction of engine, default (NC) UP, when triggered (NO) DOWN
  int last_status;
  unsigned long start;
  bool operating;
  unsigned long runtime;
};
struct engine_control engines [3];

// RADIO definition
unsigned int radio_power_pin = 9;
unsigned int radio_init_delay = 10;
bool radio_status = false;
byte addresses[][6] = {"1Node", "2Node"};

// receiving parameters
unsigned long receive_duration = 50; //duration of receive in miliseconds
unsigned long receive_loop_delay = 3; //delay of one receive loop in miliseconds

// Voltage parameters
static unsigned long v_in_resistor = 47000; //multiplication removed
static unsigned long v_gnd_resistor = 8200; //multiplication removed
static int v_measure_pin = 1;
static int v_measure_gnd = 10;

void setup() {
  unsigned int i;
  size_of_long = sizeof (unsigned long);
  Serial.begin (9600);
  printf_begin ();
  Serial.println ("START");
  analogReference( INTERNAL );
  Serial.println ("Setting engine pins");
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
  Serial.println ("Setup finished starting main loop");
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
  //  Serial.print ("Radio powered up, on ");
  //  Serial.print (millis ());
  //  Serial.println (" details follow: ");
  //  radio.printDetails ();
}

void shutdown_radio () {
  if (!radio_status) {
    return;
  }
  pinMode(radio_power_pin, INPUT);
  radio_status = false;
  //  Serial.print ("Radio shut down on ");
  //  Serial.println (millis ());

}

unsigned long prepareMessage (unsigned long target, unsigned long content, unsigned long order) {
  return ((target << 24) | ((content << 8) >> 4) | (order & mask_order));
}

unsigned long readVoltage (unsigned int internal, unsigned int voltage, unsigned int voltage_admux, unsigned int light, unsigned int light_admux) {
  unsigned long refVoltage;
  unsigned long measuredVoltage;
  unsigned int channel;
  unsigned long refmv;
  Serial.print (millis());
  Serial.print (" V ");
  if (internal != 0) {
    ADMUX = 78; // REFS0 (6), MUX 3 (3), MUX 2 (2), MUX1 (1)
    delay (delay_before_v_measure); // to settle voltage
    bitSet (ADCSRA, ADSC); // Start conversion ADSC (6)
    while (bit_is_set(ADCSRA, ADSC)); // Wail for ADSC to become 0
    refVoltage = ADCL; // ADCH is updated only after ADCL is read
    refVoltage |= ADCH << 8;
    Serial.print ("Ref: ");
    Serial.print (refVoltage);
    Serial.print (" / ");
    refmv = 1126400L / refVoltage;
    Serial.print (refmv);
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
    Serial.print (" Pin");
    Serial.print (channel & 15);
    Serial.print (": ");
    Serial.print (measuredVoltage);
    Serial.print ("/");
    measuredVoltage = (measuredVoltage * 1100L) / 1024L;
    Serial.print (measuredVoltage);
    Serial.print (" Vcalc: ");
    measuredVoltage = (measuredVoltage * (v_in_resistor + v_gnd_resistor)) / v_gnd_resistor;
    Serial.print (measuredVoltage);
    Serial.print ("mV ");
  }
  if (light != 0) {
    channel = light_admux;
    ADMUX = channel;
    delay (delay_before_v_measure); // to settle voltage
    //  ADCSRA |= 72; // Allow interrupt ADIE (3) bit set and start conversion ADSC (6)
    //  while (ADCSRA & 64 > 0); // Wail for ADSC to become 0
    bitSet (ADCSRA, ADSC); // Start conversion ADSC (6)
    while (bit_is_set(ADCSRA, ADSC)); // Wail for ADSC to become 0
    measuredVoltage = ADCL; // ADCH is updated only after ADCL is read
    measuredVoltage |= ADCH << 8;
    Serial.print (" Pin");
    Serial.print (channel & 15);
    Serial.print (": ");
    Serial.print (measuredVoltage);
    Serial.print ("/");
    measuredVoltage = (measuredVoltage * refmv) / 1024L;
    Serial.print (measuredVoltage);
    Serial.print (" ");
    Serial.print (round (100*measuredVoltage/refmv));
    Serial.print ("%");
  }
  Serial.println (".");
  return measuredVoltage;
} // readVoltage

bool receive_rf (unsigned long to_send_payload) {
  unsigned long tstart, tend;
  init_radio ();
  unsigned int i = 0;
  bool new_orders = false;
  tstart = millis ();
  while (millis () - tstart < receive_duration)
  {
    i++;
    while (radio.available()) {                                   // While there is data ready
      Serial.print ("Data to read ready\n");
      radio.read( &payload, sizeof(unsigned long) );             // Get the payload
      receive_time = millis ();
      if (((payload >> 24) == my_id) || (payload == 0)) {
        order_new = payload & mask_order;
        if (order_new != mask_order) {
          orders = true;
          new_orders = true;
        }
        else {
          continue;
        }
        Serial.print ("Received data: ");
        Serial.print (payload);
        Serial.print (" decoded order ");
        Serial.print (orders);
        Serial.print (" ");
        Serial.print (order_new);
        Serial.print (" on ");
        Serial.println (receive_time);
        payload_ack = payload;
        Serial.print ("Sending ack as: ");
        Serial.print (payload_ack);
        Serial.print (" on ");
        Serial.println (millis ());
        radio.stopListening();
        radio.write (&payload_ack, sizeof(unsigned long));
        radio.startListening();
      }
      else {
        Serial.println ("Not my ID, breaking receive cycle");
        break;
      }
    }
    delay (receive_loop_delay);
  }
  tend = millis ();
  if (to_send_payload > 0) {
    radio.stopListening();
    radio.write (&to_send_payload, sizeof(unsigned long));
  }
  shutdown_radio ();
  //  Serial.print ("Receive loop run: ");
  //  Serial.print (i);
  //  Serial.print (" times, from ");
  //  Serial.print (tstart);
  //  Serial.print (" to ");
  //  Serial.print (tend);
  //  Serial.print (" now is: ");
  //  Serial.println (millis ());
  return new_orders;
}

void engine_change (unsigned int e, unsigned int o, bool f) {
  Serial.print ("Engine ");
  Serial.print (e);
  Serial.print (" change by order ");
  Serial.print (o);
  Serial.print (" now is: ");
  Serial.println (millis ());
  if (e >= number_of_engines) {
    return;
  }
  if ((engines[e].last_status != o) && (o == 1)) {
    // start move down
    engines[e].last_status = o;
    if (engines[e].operating) {
      digitalWrite (engines[e].pin_on, HIGH); // stop power to the engine
      delay (engine_direction_switch_delay);
    } // end of delay if engine was operating (most probaly in oposite direction)
    engines[e].operating = true;
    active_order = true;
    digitalWrite (engines[e].pin_down, LOW);
    digitalWrite (engines[e].pin_on, LOW);
    digitalWrite (engines[e].pin_power, HIGH);
    engines[e].start = millis ();
    return;
  }
  if ((engines[e].last_status != o) && (o == 2)) {
    // start move up
    engines[e].last_status = o;
    if (engines[e].operating) {
      digitalWrite (engines[e].pin_on, HIGH); // stop power to the engine
      delay (engine_direction_switch_delay);
    } // end of delay if engine was operating (most probaly in oposite direction)
    engines[e].operating = true;
    active_order = true;
    digitalWrite (engines[e].pin_down, HIGH);
    digitalWrite (engines[e].pin_on, LOW);
    digitalWrite (engines[e].pin_power, HIGH);
    engines[e].start = millis ();
    return;
  }
  if ((engines[e].operating) && (o == 0)) {
    // stop
    engines[e].last_status = o;
    engines[e].operating = false;
    digitalWrite (engines[e].pin_on, HIGH);
    digitalWrite (engines[e].pin_down, HIGH);
    digitalWrite (engines[e].pin_power, HIGH);
    return;
  }
}

void process_orders ()
{
  int i;
  unsigned int engine_new;
  unsigned int mask_engine;
  if (orders) {
    Serial.print ("Processing orders ");
    Serial.print (orders);
    Serial.print (" ");
    Serial.print (order_new);
    Serial.print (" on ");
    Serial.println (receive_time);
  }
  // TODO ADD RE-TRANSMISSION CHECK HERE, as it can be deleted on next loop
  if (orders) {
    if (order_new == mask_order) {
      orders = false;
    }
    else {
      // start execution of new orders (or check if can execute them
      if (order_new == 0) { // all stop
        for (i = 0; i < number_of_engines; i++) {
          engine_change (i, 0, false);
        }
      } else {
        engine_change ((order_new >> 2) - 1, order_new & 3, false);
      }// loop to check engines
    } // new orders check
    orders = false;
  }
  if (active_order) {
    // Checking what is being worked on
    active_order = false;
    for (i = 0; i < number_of_engines; i++) {
      if (engines[i].operating) {
        if (millis () - engines[i].start < engines[i].runtime) {
          active_order = true;
        }
        else {
          //shutdown engine
          digitalWrite (engines[i].pin_on, HIGH);
          digitalWrite (engines[i].pin_down, HIGH);
          digitalWrite (engines[i].pin_power, HIGH);
          engines [i].operating = false;
        }
      }
    } // for all engines
  } // active orders check
  if (active_order) {
    Serial.print ("Process orders finished with active orders state ");
    Serial.print (active_order);
    Serial.print (" now is: ");
    Serial.println (millis ());
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  if (!active_order) {
    // NO ACTIVE ORDER
    delay (50);
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    //    delay (1000);
    delay (50);
//    receive_rf (prepareMessage (1, readVoltage (193), 1)); //Pin A1 read against reference voltage
//    receive_rf (prepareMessage (1, readVoltage (66), 1)); //Pin A2 read against Vin
    readVoltage (1,1,193,1,66);
    delay (100);
  } else
  { // active order
    delay (1000);
    receive_rf (0);
  } // active order
  // receive with build in power check
  // do something
  process_orders ();
}
