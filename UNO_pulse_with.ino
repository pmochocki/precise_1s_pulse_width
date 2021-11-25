/*
MIT License
Copyright (c) 2021 Piotr Mochocki
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <avr\io.h>
#include <avr\interrupt.h>

#define VERBOSE 0         // How verbose to be on the serial output. Set to 0 when collecting bulk data.
#define LOOP 1            // One time measurment or repeat in loop forever. Set to 1 when collecting bulk data.
#define NOISE_CANCELER 0  // Set to 1 to enable Input Capture Noise Canceler

volatile uint16_t timestamp;          // Set in timer capture interrupt.
volatile uint16_t overflowCount;      // Incremented in timer overflow interrupt. 
                                      // Cleared in main loop. (STATE_INIT_MEASURMENT)
volatile uint16_t overflowCountAtICR; // Set in timer capture interrupt based on overflowCount.

typedef enum : uint8_t {   
   STATE_INIT_MEASURMENT,
   STATE_WAIT_FOR_MEASURMENT, 
   STATE_BEGIN_MEASURMENT, 
   STATE_MEASURMENT,
   STATE_END_MEASURMENT, 
   STATE_POST_MEASURMENT} state;      // The state machine. The state names are self-explanatory. :)

volatile state currentState;

ISR(TIMER1_OVF_vect)
{
  overflowCount++;
}

ISR(TIMER1_CAPT_vect)
{
  timestamp = ICR1;
  overflowCountAtICR = overflowCount; 
  currentState = currentState + 1;    //Keep it short and simple :)
}

void timer_init()
{
  TCCR1A = 0;                       // Needed because in the init() function arduino puts the timer1 in 8-bit phase correct pwm mode
  TCCR1B = (1<<CS10)| (1<<ICES1);   // Prescaler set to 1 and enable trigger by rising edge
  if(NOISE_CANCELER)
    TCCR1B |= (1<<ICNC1);           // Enable Input Capture Noise Canceler if requred
  TIMSK1= (1<<ICIE1) | (1<<TOIE1);  // Enable ICP i overflow interrupts
}

void setup() {
  pinMode(8, INPUT);
  Serial.begin(250000);             // Remember to adjut the serial bound rate
  currentState = STATE_INIT_MEASURMENT;
  timer_init();
}

void loop() {
  // Names are self-explanatory. Volatile is requred - gcc will optimise to much without it
  volatile uint16_t overflowCountAtBeginning; 
  volatile uint16_t overflowCountAtEnd;
  volatile uint16_t timestampAtBeginning;
  volatile uint16_t timestampAtEnd; 
  
  if(currentState == STATE_INIT_MEASURMENT) {
    if(VERBOSE)
      Serial.print("Let us start...");
    overflowCount = 0;
    currentState = STATE_WAIT_FOR_MEASURMENT;
  } 
  else if(currentState == STATE_WAIT_FOR_MEASURMENT) {
    if(VERBOSE)
      Serial.print(".");
  }
  // Collect data at the begining of the pulse 
  else if(currentState == STATE_BEGIN_MEASURMENT) {
    overflowCountAtBeginning = overflowCountAtICR;
    timestampAtBeginning = timestamp;
    currentState = STATE_MEASURMENT;
    if(VERBOSE) {
      Serial.println("");
      Serial.print("Measurement in progress...");
    }
  }
  else if(currentState == STATE_MEASURMENT) {
    if(VERBOSE)    
      Serial.print(".");
  }
  // Collect data at the end of the pulse 
  else if(currentState == STATE_END_MEASURMENT) {
    overflowCountAtEnd = overflowCountAtICR;
    timestampAtEnd = timestamp;
    if(VERBOSE) {
      Serial.println("");
      Serial.println("Measurement finished!");   
    }   
    // Calculate the "pulse length"
    uint16_t overflowCountMeasured = overflowCountAtEnd - overflowCountAtBeginning;
    uint16_t ticksMeasured = timestampAtEnd - timestampAtBeginning;
    
    // If the timestamp is bigger at the begining then at the and we need to ajust 
    // As a reminder (ISO/IEC 9899:1999 (E) §6.2.5/9):
    // A computation involving unsigned operands can never overflow, because a result
    // that cannot be represented by the resulting unsigned integer type is reduced 
    // modulo the number that is one greater than the largest value that can be 
    // represented by the resulting type. 
    //
    // (unsigned)0 - (unsigned)1 equals -1 modulo UINT_MAX+1, or in other words, UINT_MAX
    // Remeber to supstract 1 :)
    if( timestampAtBeginning > timestampAtEnd ){
      overflowCountMeasured--;
      ticksMeasured--;
    }
    
    if(VERBOSE) {
      Serial.println("");
      Serial.print("Overflow Count: ");
      Serial.println(overflowCountMeasured);
      Serial.print("Ticks Count: ");
      Serial.println(ticksMeasured);
      Serial.println("");
      Serial.println("");
    }
    else {
      Serial.println(ticksMeasured);
    }
    currentState = STATE_POST_MEASURMENT;
  }
  else if(currentState >= STATE_POST_MEASURMENT){
    if(LOOP)  
      currentState = STATE_INIT_MEASURMENT;
    else
      currentState = STATE_POST_MEASURMENT;
  }
  delay(100);
}
