RF24 radio(7, 8);

// MY ID
const unsigned char my_id = 13; // 13 - MARTIN DVERE

// Broadcast ID
const unsigned char broadcast_id = 255;
const unsigned char all_id = 0;

// SERVER ID
const unsigned char server_id = 1;

// Messages mode (if to write messages to serial)
const bool serial_messages = false;

// RADIO definition
const bool radio_power_from_arduino = false; // Set to true if radio is powered from arduino pin "radio_power_pin" if radio is powered directly from DC rail, set to false
const unsigned int radio_power_pin = 9; // Arduino pin where radio power pin is connected
const unsigned int radio_init_delay = 10; // Delay in ms after power is applied to radio until we can start use radio library (begin), should be 105.3 ms (MIN: 101.6 ms; MAX: 110.3 ms) if this is proven as true it makes no sense to power off radio between sleep cycles (ms)
const unsigned char radio_sleep_wakeup_delay = 2; // Delay when radio is waken up from sleep state should be 1.5 ms according to datasheet (ms)
const unsigned char radio_wait_after_power = 100; // In miliseconds how long we need to wait after power is applied to radio to be able to start sending/receiving
const unsigned char radio_wait_after_sleep = 10; // in miliseconds how long we need to wait after radio is waken up from deep sleep
const bool radio_shutdown_power = false; // true to shutdown radio power on power down, false to enter sleep mode on power down
byte addresses[][6] = {"1Node", "2Node"};
const unsigned long receive_duration = 20; //duration of receive in miliseconds (ms)
const unsigned long receive_loop_delay = 1; //delay of one receive loop in miliseconds (ms)
const bool retransfer_bit_set = false; // retransfer bit settings
const unsigned char retransfer_bit_position = 6; // Location of re-transfer bit
const unsigned char ack_bit_position = 7; // Location of ack bit
const unsigned char rx_max_queue_size = 10;
const unsigned char tx_max_queue_size = 10;

// ENGINES definition
// For simplification the engine pins are HIGH due to the way how relay board is designed (can be done also with low as default but this is little bit more complicated as the pin_power is common for whole board (2, 4, 6 relays))
const unsigned int number_of_engines = 1;
const unsigned char engine_orders_bit_position = 4;
const unsigned int engine_direction_switch_delay = 789; // delay before direction of engine movement can be switched to other direction
const unsigned int wait_after_engine_stop = 6000; // delay in miliseconds after stop command received as we can expect other command to arrive after stop
struct engine_control {
  unsigned int pin_power; // Power for the relay board, kept high together with other pins, changing pin_on or pin_down activates relay
  unsigned int pin_on; // relay switching on the power to engine default (NC) OFF, when triggered (NO) ON
  unsigned int pin_down; // relay switching direction of engine, default (NC) UP, when triggered (NO) DOWN (pin_down can be 0 if there is no engine, but simple switch)
  unsigned int last_status;
  unsigned long start;
  bool operating;
  unsigned long runtime;
};
struct engine_control engines [number_of_engines] = {
  [0] = { .pin_power = 6, .pin_on = 5, .pin_down = 3, .last_status = 0, .start = 0, .operating = false, .runtime = 30000 }
};

// Orders defaults
const unsigned char orders_mask = B00010000; // Mask for orders this system is able to accept, if none of them is set to 1 it is considered as all-stop order

// VOLTAGE measure + LIGHT
const unsigned char voltage_orders_bit_position = 5; // Position of voltage bit
const unsigned char light_orders_bit_position = 1; // Position of light bit
const unsigned long v_in_resistor = 470; // Resistor on voltage divider for main current (100ohm) be carefull: (1024 * 55 * (v_in_resistor + v_gnd_resistor)) <  4294967295 otherwise calculation will be broken !!!
const unsigned long v_gnd_resistor = 82; // Resistor on voltage divider for ground (100ohm) be carefull: (1024 * 55 * (v_in_resistor + v_gnd_resistor)) <  4294967295 otherwise calculation will be broken !!!
const unsigned char v_measure_pin = 1;
const unsigned char v_measure_gnd = 10;
const unsigned char delay_before_v_measure = 17; // 7
const unsigned char light_sensor_power_pin = A3;
//const unsigned int voltage_read_ms = 65432; // interval to read voltage (0 = disabled, implies also 0 on voltage_send_ms) (ms MAX:65535) 
const unsigned int voltage_read_ms = 0; // interval to read voltage (0 = disabled, implies also 0 on voltage_send_ms) (ms MAX:65535)
const unsigned int voltage_send_ms = 0; // interval to send voltage (0 = disabled) (ms MAX:65535), voltage will be NOT read, voltage will be send on NEXT read after this timer has expired
const unsigned char voltage_turn_on_external = 50;  // Treshold when to start external power source
const unsigned char voltage_turn_off_external = 240; // Treshold when to stop external power source
unsigned char v_read = 0; // voltage counter
unsigned char v_send = 0; // voltage counter
unsigned char voltage_ignore_range = 3; // what must be the minimal difference from last measure to send to server (total range is 2x voltage_ignore_range)
unsigned char voltage_ignore_min = 0; //setup in a way that condition will be always met on 1st run
unsigned char voltage_ignore_max = 0; 

// LIGHT measure re-factored
const bool light_ldr_sensor_connected = false;
const bool light_ldr_sensor_arduino_powered = true; // If true light sensor is powered (input to voltage divider) by arduino, if false it is assumed that light sensor is powered directly or by other means
const int light_ldr_sensor_pin_power = 7; // Pin that will be put high to measure light, valid only in case light_sensor_arduino_powered is true
const int light_ldr_sensor_pin_measure = A3; // Pin connected to middle of voltage divider, there is fixed resistor between this pin and GND and ldr between this pin and VCC (or light_ldr_sensor_pin_power)
const unsigned int light_ldr_delay_after_power = 17; // In miliseconds how ong to wait after power is applied to light_ldr_sensor_pin_power (to settle voltage), the higher resistance the more time needed
const unsigned int light_ldr_read_send_ms = 0; // interval to read and send voltage (0 = disabled) (ms MAX:65535)
const unsigned char light_ldr_ignore_range = 3; // what must be the minimal difference from last measure to send to server (total range is 2x light_ldr_ignore_range)
const unsigned char light_ldr_orders_bit_position = 1; // Position of light bit

// MAINS switching
const bool mains_switch_connected = false; // if set to true mains switch pin mains_switch_pin high means mains on low means off
const bool mains_switch_latching = true; // the only difference is in startup procedure where for latching we need to be sure to discharge capacitor to ensure latch position
const int mains_switch_pin = 10; // Pin that will be put high connect mains and low to disconnect mains
const unsigned int mains_min_switch_on_time = 12000; // Minimum mains on time in miliseconds
const unsigned char mains_turn_on_time = 5; // turn on time in miliseconds
const unsigned char mains_turn_off_time = 5; // turn off time in miliseconds, this is very important to have correct startup procedure in case of latching relay
const bool mains_switch_on_notify_server = true; // if to notify master server in case mains is turned on
const bool mains_switch_off_notify_server = true; // if to notify master server in case mains is turned off

// BATTERY MONITOR
const bool battery_connected = false;
const unsigned int battery_stop_mains = 321; // relative measure as by adc, for battery_measure_pin = 0 this is reverse as 1.1V is measured against VCC, lower value means higher VCC, for nalog pins they are measured against VCC higher number means higher battery voltage
const unsigned int battery_start_mains = 375;
const unsigned long battery_min_charging_time = 234567; // Minimum battery charging time in miliseconds
const unsigned long battery_measure_interval = 654321; // Time in miliseconds how often to check battery status
const unsigned char battery_measure_pin = 0; // Only analog in pins can be located here, if 0 used internal VCC of arduino is measured resulting in oposite scale of start/stop mains

// Sleeping cycles in main loop
const unsigned int sleep_after_deep_sleep = 1; // in ms normally cca 50 (ms)
const unsigned int sleep_during_sleep_lock = 500 + my_id;
const unsigned int sleep_engine_change = 11; // in ms, sleep after engine manipulation - if check is executed too fast (in the same ms as engine set) the result might be 0 leading to immediate engine switch off
const unsigned int sleep_before_lock_validate = 11; // in ms, sleep before validation of sleep lock validation - if check is executed too fast (in the same ms as engine set) the result might be 0 leading to immediate engine switch off
