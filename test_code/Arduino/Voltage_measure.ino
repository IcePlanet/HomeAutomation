#include "LowPower.h"

static unsigned long v_in_resistor = 47000; //multiplication removed
static unsigned long v_gnd_resistor = 8200; //multiplication removed
static int v_measure_pin = 1;
static int v_measure_gnd = 10;
unsigned long v_in_constant = (v_in_resistor + v_gnd_resistor)/v_gnd_resistor;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println("Start");
}

// voltage readings
volatile bool adcDone;
ISR(ADC_vect) {
  adcDone = true;
}

long readVcc() {
  long result; // Read 1.1V reference against AVcc
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC);// Convert
  while (bit_is_set(ADCSRA, ADSC));
  result = ADCL; result |= ADCH << 8; result = 1126400L / result; // Back-calculate AVcc in mV
  return result;
}

long readVoltage (unsigned int pin) {
  unsigned long refVoltage;
  unsigned long measuredVoltage;
  unsigned int channel;
  Serial.print ("(");
  ADMUX = 206; // REFS1 REFS0 (6), MUX 3 (3), MUX 2 (2), MUX1 (1)
  delay (10); // to settle voltage
  bitSet (ADCSRA,ADSC); // Start conversion ADSC (6)
  while (bit_is_set(ADCSRA,ADSC)); // Wail for ADSC to become 0
  refVoltage = ADCL; // ADCH is updated only after ADCL is read
  refVoltage |= ADCH << 8;
  Serial.print ("ref:");
  Serial.print (refVoltage);
  Serial.print (" calc:");
  Serial.print (1126400L/refVoltage);
  ADMUX = 193; // REFS1 REFS0 (6) and MUX* set based on pin
  delay (5); // to settle voltage
//  ADCSRA |= 72; // Allow interrupt ADIE (3) bit set and start conversion ADSC (6)
//  while (ADCSRA & 64 > 0); // Wail for ADSC to become 0
  bitSet (ADCSRA,ADSC); // Start conversion ADSC (6)
  while (bit_is_set(ADCSRA,ADSC)); // Wail for ADSC to become 0
  measuredVoltage = ADCL; // ADCH is updated only after ADCL is read
  measuredVoltage |= ADCH << 8;
  Serial.print (" pinmeasured:");
  Serial.print (measuredVoltage);
  Serial.print (" pincalc:");
  measuredVoltage = (measuredVoltage*1100L)/1023L;
  Serial.print (measuredVoltage);
  Serial.print (" Vcalc:");
  measuredVoltage = (measuredVoltage*(v_in_resistor + v_gnd_resistor))/v_gnd_resistor;
  Serial.print (measuredVoltage);
  Serial.print (") ");
  return measuredVoltage;
} // readVoltage

void loop() {
  // put your main code here, to run repeatedly:
  // power check
  // reading 1   
  Serial.print ("Power check main: ");
  Serial.println (readVcc ());
  // delay between reads
  delay (1000);
  // reading 2
  Serial.print ("Power check own dev: ");
  Serial.println (readVoltage (1));
//  Serial.println (analogRead (1));
  // Delay
  delay (10000);
  //LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
}
