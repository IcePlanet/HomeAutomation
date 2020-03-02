// ANTI CYCLING OF HEATER

// When water temperature reaches certain treshold the controller is disconnected (realay activated) until timer expires and water temperature will be under certain treshold
// There is safety maximal timer how long relay can be activated (controller disconnected), if this timer is fired then there is minimal timer how long controller will be activated and only then normal operation will be resumed.
// Temperature sensing is done by voltage divider including termistor
// Currently no communication to server

// Table of expected measure for 100K6A1I SERIES II connected to + pole and fixed resistor 100k to - pole

// Celsius	Termistor resistance	Expected measurement
// 1	332620	237
// 2	315288	247
// 3	298954	257
// 4	283555	267
// 5	269034	277
// 6	255335	288
// 7	242408	299
// 8	230206	310
// 9	218684	321
// 10	207801	333
// 11	197518	344
// 12	187799	356
// 13	178610	368
// 14	169921	379
// 15	161700	391
// 16	153921	403
// 17	146558	415
// 18	139586	427
// 19	132983	440
// 20	126727	452
// 21	120799	464
// 22	115179	476
// 23	109850	488
// 24	104795	500
// 25	100000	512
// 26	95449	524
// 27	91129	536
// 28	87027	548
// 29	83131	559
// 30	79430	571
// 31	75913	582
// 32	72569	593
// 33	69390	605
// 34	66367	616
// 35	63491	626
// 36	60755	637
// 37	58150	647
// 38	55670	658
// 39	53309	668
// 40	51060	678
// 41	48917	688
// 42	46875	697
// 43	44929	707
// 44	43073	716
// 45	41303	725
// 46	39615	733
// 47	38005	742
// 48	36467	750
// 49	35000	759
// 50	33599	766
// 51	32262	774
// 52	30984	782
// 53	29763	789
// 54	28596	796
// 55	27481	803
// 56	26415	810
// 57	25395	817
// 58	24420	823
// 59	23487	829
// 60	22594	835
// 61	21740	841
// 62	20922	847
// 63	20138	852
// 64	19388	858
// 65	18670	863
// 66	17981	868
// 67	17322	873
// 68	16689	878
// 69	16083	882
// 70	15502	887
// 71	14945	891
// 72	14410	895
// 73	13897	899
// 74	13405	903
// 75	12932	907
// 76	12478	910
// 77	12043	914
// 78	11625	917
// 79	11223	921
// 80	10837	924
// 81	10466	927
// 82	10109	930
// 83	9767	933
#include <printf.h>
#include <SPI.h>
#include "LowPower.h"

// Messages mode (if to write messages to serial)
const bool serial_messages = false;

// TEMPERATURE SENSORS (max works as higher than, min works as lower than)
const unsigned int min_water_temperature = 670; // ~39
//const unsigned int min_water_temperature = 530; // For in hand test purposes ~25
const unsigned int max_water_temperature = 835; // ~60
//const unsigned int max_water_temperature = 560; // For in hand test purposes ~33

// TIMERS
const unsigned int cycle_duration = 1000; // in ms: one cycle = 1000ms = 1s
const unsigned int min_controller_off_cycles = 321; // Final
//const unsigned int min_controller_off_cycles = 20; // For in hand testing
const unsigned int max_controller_off_cycles = 2345;
//const unsigned int max_controller_off_cycles = 60; // For in hand testing
const unsigned int min_controller_on_cycles = 1000;
//const unsigned int min_controller_on_cycles = 30; // For in hand testing

// RELAYS PINS
const unsigned char commander_pin = 5;
//const unsigned char relay_board_vcc = 6; //not used now

// WATER TEMP MEASURE
const byte base_admux = 0b01000000; // measuring against VCC
const byte water_temp_pin = 1; // measure pin (must be analogue) = center of voltage divider
const byte water_temp_admux =  ( base_admux | water_temp_pin ); 
const unsigned char delay_before_v_measure = 17; // 7 should be OK as divider is constantly on added safety margin
unsigned int current_water_temp = 0; //resulting water temperature on scale 1 to 1024, see table at the beginning

// COUNTERS
unsigned int cycle_counter = 0;
unsigned int min_run_time_counter = 0;

// LOG
const unsigned int log_water_temp_cycles = 10;
const unsigned int log_cycle_count = 1;
const bool log_measured_temp = true;

const unsigned int size_of_long = sizeof (unsigned long);

unsigned long measure_temp () {
  // Internal is reference voltage measured against incomming
  // Voltage is external voltage measured against vref
  // Light is reference voltage measured against light sensor pin
  unsigned long measuredVoltage;
  ADMUX = water_temp_admux; // REFS0 (6), MUX1 (1)
  delay (delay_before_v_measure); // to settle voltage
  bitSet (ADCSRA, ADSC); // Start conversion ADSC (6)
  while (bit_is_set(ADCSRA, ADSC)); // Wail for ADSC to become 0
  measuredVoltage = ADCL; // ADCH is updated only after ADCL is read
  measuredVoltage |= ADCH << 8;
  return measuredVoltage;
} // measure_temp


void setup() {
  if (serial_messages) { Serial.begin (9600); printf_begin (); Serial.println ("START");}
  analogReference( INTERNAL );
  if (serial_messages) { Serial.print ("Initialize relay board... "); }
  pinMode (commander_pin, OUTPUT);
  digitalWrite (commander_pin, HIGH);
  //pinMode (relay_board_vcc, OUTPUT);
  //digitalWrite (relay_board_vcc, HIGH);
  if (serial_messages) { Serial.println ("OK"); }
  pinMode (water_temp_pin, INPUT);
  if (serial_messages) { Serial.println ("Setup finished starting main loop"); }
}

void loop() {
  delay (cycle_duration);
  current_water_temp = measure_temp ();
  if (serial_messages && log_measured_temp) { Serial.print ("Water temperature is: "); Serial.println (current_water_temp); }
  if (current_water_temp > max_water_temperature) {
    // Switching off commander
    if (serial_messages) { Serial.print ("Commander switching off (pin "); Serial.print (commander_pin); Serial.print (") due to water temp: "); Serial.print (current_water_temp); Serial.print (">"); Serial.print (max_water_temperature); Serial.print (" (MAX) ..."); }
    digitalWrite (commander_pin, LOW);
    if (serial_messages) { Serial.println ("OK"); }
    cycle_counter = 0;
    if (serial_messages) { Serial.print ("Waiting for min "); Serial.print (min_controller_off_cycles); Serial.print ("cycles ("); Serial.print (cycle_duration); Serial.print (") and water temp < "); Serial.print (min_water_temperature); Serial.print ("but max for "); Serial.print (max_controller_off_cycles); Serial.print ("cycles..."); }
    while ((cycle_counter <= min_controller_off_cycles || min_water_temperature <= current_water_temp) && cycle_counter <= max_controller_off_cycles) {
      // Waiting for water to cool down
      cycle_counter++;
      delay (cycle_duration);
      current_water_temp = measure_temp ();
      if (serial_messages) { if (cycle_counter % log_cycle_count == 0 ) { Serial.print ("."); } if (cycle_counter % log_water_temp_cycles == 0 ) { Serial.print (current_water_temp); } }
    }
    if (serial_messages) { Serial.print ("X Finished after"); Serial.print (cycle_counter); Serial.print ("cycles, water temp: "); Serial.println (current_water_temp); }
    if (cycle_counter >= max_controller_off_cycles) {
      // We reached max off time for controller so now we must let it run for minimal time, we are not interested in water temperature in this section
      if (serial_messages) { Serial.print ("MAX off time reached, switching on (pin "); Serial.print (commander_pin); Serial.print (") ..."); }
      digitalWrite (commander_pin, HIGH);
      if (serial_messages) { Serial.println ("OK"); }
      if (serial_messages) { Serial.print ("Leaving commander on for minimum "); Serial.print (min_controller_on_cycles); Serial.print (" cycles ..."); }
      for (min_run_time_counter=0; min_run_time_counter < min_controller_on_cycles ; min_run_time_counter++) {
        delay (cycle_duration);
        if (serial_messages) { if (min_run_time_counter % log_cycle_count == 0 ) { Serial.print ("."); } }
      }
      if (serial_messages) { Serial.print ("X Finished after"); Serial.print (min_run_time_counter); Serial.println ("cycles"); }
    }
    else {
      // Water has cooled down so we can resume normal operation
      // Switching ON commander
      if (serial_messages) { Serial.print ("Water had cooled off switching on commnder on pin "); Serial.print (commander_pin); Serial.print (") ..."); }
      digitalWrite (commander_pin, HIGH);
      if (serial_messages) { Serial.println ("OK"); }
    }
    if (serial_messages) { Serial.println ("Resuming normal operation..."); }
  }
}
