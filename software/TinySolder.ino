// ===================================================================================
// Project:   TinySolder - T12 Soldering Station based on ATtiny13A
// Version:   v1.1
// Year:      2020
// Author:    Stefan Wagner
// Github:    https://github.com/wagiminator
// EasyEDA:   https://easyeda.com/wagiminator
// License:   http://creativecommons.org/licenses/by-sa/3.0/
// ===================================================================================
//
// Description:
// ------------
// Simple T12 Quick Heating Soldering Station featuring:
// - Temperature measurement of the tip
// - Direct control of the heater
// - Temperature control via potentiometer
// - Handle movement detection (by checking tilt switch)
// - Time driven sleep/power off mode if iron is unused (movement detection)
//
// Indicator LEDs:
// - steady blue          - station is powered
// - steady red           - tip temperature has not reached setpoint yet
// - steady green         - tip temperature is at setpoint - iron is worky
// - blinking red/green   - iron is in sleep mode - move handle to wake up
// - steady red and green - iron is in off mode - move handle to start again
//
// Power supply should be in the range of 16V/2A to 24V/3A and well
// stabilized.
//
// For calibration you need a soldering iron tips thermometer. For best results
// wait at least three minutes after switching on the soldering station before 
// you start the calibration. Calibrate at your prefered temperature.
//
// Wiring:
// -------
//                         +-\/-+
//      --- RST ADC0 PB5  1|°   |8  Vcc
// Temp ------- ADC3 PB3  2|    |7  PB2 ADC1 -------- Tilt Switch
// Poti ------- ADC2 PB4  3|    |6  PB1 AIN1 OC0B --- Heater
//                   GND  4|    |5  PB0 AIN0 OC0A --- LED
//                         +----+
//
// Compilation Settings:
// ---------------------
// Controller:  ATtiny13A
// Core:        MicroCore (https://github.com/MCUdude/MicroCore)
// Clockspeed:  1.2 MHz internal
// BOD:         BOD 2.7V
// Timing:      Micros disabled
//
// Leave the rest on default settings. Don't forget to "Burn bootloader"!
// No Arduino core functions or libraries are used. Use the makefile if 
// you want to compile without Arduino IDE.
//
// Fuse settings: -U lfuse:w:0x2a:m -U hfuse:w:0xfb:m


// ===================================================================================
// Libraries and Definitions
// ===================================================================================

// Libraries
#include <avr/io.h>             // for gpio
#include <avr/sleep.h>          // for the sleep modes
#include <avr/interrupt.h>      // for the interrupts
#include <util/delay.h>         // for delays

// Pin definitions
#define LED           PB0       // connected to indicator LEDs
#define HEATER        PB1       // connected to gate of heater control MOSFET
#define SWITCH        PB2       // connected to tilt switch of handle
#define TEMP          PB3       // connected to P+ of tip (temp measurement)
#define POTI          PB4       // connected to temp selection pot

// ADC temperature calibration values
#define TEMPSLEEP     100       // ADC3 in sleep mode
#define TEMP150       118       // ADC3 at 150°C
#define TEMP300       221       // ADC3 at 300°C
#define TEMP450       324       // ADC3 at 450°C

// Timer definitions
#define CYCLETIME      50       // cycle delay time in milliseconds
#define TIME2SETTLE   900       // voltage settle time in microseconds
#define TIME2SLEEP    300       // time to enter sleep mode in seconds
#define TIME2OFF      600       // time to shut off heater in seconds

// Pin manipulation macros
#define pinInput(x)   DDRB  &= ~(1<<(x))  // set pin to INPUT
#define pinOutput(x)  DDRB  |=  (1<<(x))  // set pin to OUTPUT
#define pinLow(x)     PORTB &= ~(1<<(x))  // set pin to LOW
#define pinHigh(x)    PORTB |=  (1<<(x))  // set pin to HIGH
#define pinToggle(x)  PINB  |=  (1<<(x))  // TOGGLE pin
#define pinDisable(x) DIDR0 |=  (1<<(x))  // disable digital input buffer
#define pinIntEn(x)   PCMSK |=  (1<<(x))  // enable pin change interrupt
#define pinRead(x)    (PINB &   (1<<(x))) // READ pin
#define pinADC(x)     ((x)==2?1:((x)==3?3:((x)==4?2:0))) // convert pin to ADC port

// Global variables
volatile uint16_t handleTimer;  // time since the handle was last moved

// ===================================================================================
// ADC Implementation
// ===================================================================================

// Init ADC
void ADC_init(void) {
  ADCSRA = (1<<ADEN)                  // enable ADC
         | (1<<ADPS1) | (1<<ADPS0)    // set ADC clock prescaler to 8
         | (1<<ADIE);                 // ADC interrupts enable
}

// Average several ADC readings in sleep mode to denoise
uint16_t ADC_read(uint8_t port) {
  uint16_t result = 0;
  ADCSRA |= (1<<ADIF);                // turn off any pending interrupt
  ADMUX = port;                       // set port and reference to AVcc   
  set_sleep_mode (SLEEP_MODE_ADC);    // sleep during sample for noise reduction
  for (uint8_t i=16; i; i--) {        // get 16 readings
    sleep_mode();                     // go to sleep while taking ADC sample
    while (ADCSRA & (1<<ADSC));       // make sure sampling is completed
    result += ADC;                    // add them up
  }
  return (result >> 4);               // divide by 16 and return value
}

// ADC interrupt service routine
EMPTY_INTERRUPT(ADC_vect);            // nothing to be done here

// ===================================================================================
// Sleep Mode Implementation
// ===================================================================================

// Set sleep mode for iron
void sleep(void) {
  while(handleTimer) {                            // repeat until handle was moved
    pinLow(HEATER);                               // shut off heater
    if(handleTimer < TIME2OFF/CYCLETIME*1000) {   // if not in off mode
      handleTimer++;                              // increase timer
      _delay_us(TIME2SETTLE);                     // wait for voltages to settle
      uint16_t temp = ADC_read(pinADC(TEMP));     // read temperature
      if(temp < TEMPSLEEP) pinHigh(HEATER);       // turn on heater if below sleep temp
      pinToggle(LED);                             // toggle LED
    } else {                                      // if in off mode
      pinInput(LED);                              // turn red and green LED on
    }
    _delay_ms(CYCLETIME - 8);                     // wait for next cycle
  }
  pinOutput(LED);                                 // set LED pin as output again
}

// Pin change interrupt service routine (when handle was moved)
ISR(PCINT0_vect){
  handleTimer = 0;                                // reset handle timer
}

// ===================================================================================
// Main Function
// ===================================================================================

int main(void) {
  // Local variables
  uint16_t poti, temp, smooth, setpoint;

  // Setup pins
  pinOutput(LED);                       // LED pin as output
  pinOutput(HEATER);                    // heater control pin as output
  pinHigh(SWITCH);                      // pull-up for switch
  pinDisable(TEMP);                     // disable digital input buffer
  pinDisable(POTI);                     // disable digital input buffer

  // Setup pin change interrupt
  GIMSK = (1<<PCIE);                    // pin change interrupts enable
  pinIntEn(SWITCH);                     // turn on interrupt on switch pin
  sei();                                // enable global interrupts

  // Setup ADC and read temperature as start value for smoothing
  ADC_init();
  smooth = ADC_read(pinADC(TEMP));

  // Loop
  while(1) {
    // Read potentiometer setting and calculate setpoint accordingly
    poti = ADC_read(pinADC(POTI));
    if(poti < 512) setpoint = (uint32_t)(( poti        * (TEMP300 - TEMP150))>>9) + TEMP150;
    else           setpoint = (uint32_t)(((poti - 512) * (TEMP450 - TEMP300))>>9) + TEMP300;

    // Set heater according to temperature reading and setpoint
    pinLow(HEATER);                                 // shut off heater
    _delay_us(TIME2SETTLE);                         // wait for voltages to settle
    temp = ADC_read(pinADC(TEMP));                  // read temperature
    smooth = ((smooth << 3) - smooth + temp) >> 3;  // low pass filter
    if(smooth < setpoint) pinHigh(HEATER);          // turn on heater if below setpoint

    // Set status LED according to temperature and setpoint
    pinLow(LED);
    if((smooth + 10 > setpoint) && (setpoint + 10 > smooth)) pinHigh(LED);

    // Some timing
    if(handleTimer++ > TIME2SLEEP/CYCLETIME*1000) sleep();  // sleep mode if handle unused
    _delay_ms(CYCLETIME - 8);                               // wait for next cycle
  }
}
