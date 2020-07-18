
/******************************************************************************
  Simple framework for a cTc machine simulation

See https://learn.sparkfun.com/tutorials/samd51-thing-plus-hookup-guide

Install SAMD board definitions into Arduino env
    https://raw.githubusercontent.com/sparkfun/Arduino_Boards/master/IDE_Board_Manager/package_sparkfun_index.json
Download and Install Sparkfun libraries for QWiic button and Qwiic VEML6030 Ambient Light Sensor
Install elapsedMillis library

Hardware Connections:
  Sparkfun SAMD-51 Thing Plus with Qwiic connectors
  Sparkfun Qwiic I2C PCA9548A Mux board
  Sparkfun Qwiic button on mux port 0
  Sparkfun Qwiic VEML6030 Ambient Light Sensor on mux port 1
  up to 7x IOx32 I2C expanders in 2x chains, connected to mux ports 2 and 3

******************************************************************************/
#include <elapsedMillis.h>
#include "SparkFun_VEML6030_Ambient_Light_Sensor.h"
#include <SparkFun_Qwiic_Button.h>
#include "I2C_IO.h"

extern boolean enableMuxPort(byte portNumber);
extern boolean disableMuxPort(byte portNumber);

QwiicButton button;

//  Light Sensor
// Config values shouldn't need to change...
// ========
// I2C address:
#define LUX_SENSOR_ADDR 0x48
// Possible gain values: .125, .25, 1, 2
// Both .125 and .25 should be used in most cases except darker rooms.
// A gain of 2 should only be used if the sensor will be covered by a dark
// glass.
#define LUX_GAIN  .125
// Possible integration times in milliseconds: 800, 400, 200, 100, 50, 25
// Higher times give higher resolutions and should be used in darker light. 
#define LUX_INTEGRATION_TIME 100
// ========

SparkFun_Ambient_Light light(LUX_SENSOR_ADDR);

// Simulation values that might change
#define NUM_LUX_SAMPLES 10 // running average for room lighting level (to eliminate transients such as shadows, flashlights...) 
long luxAverage[NUM_LUX_SAMPLES];
long luxVal = 0; 
int  luxIdx = 0;

#define LIGHT_LEVEL_SLEEP 50
#define LIGHT_LEVEL_WAKE 110

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("cTc Simulation - basic framework");

    Wire.begin(); //Join I2C bus
    
    enableMuxPort(0); // Light Sensor
    enableMuxPort(1); // Button
    disableMuxPort(2);
    disableMuxPort(3);
    disableMuxPort(4);
    disableMuxPort(5);
    disableMuxPort(6);
    disableMuxPort(7);

    Serial.println("# MUX initialized");

    //check if button will acknowledge over I2C
    if (button.begin() == false) {
      Serial.println("# ERROR: Button Device did not acknowledge! Freezing.");
      while (1);
    }
    Serial.println("# Button acknowledged.");
  
    if (light.begin()) {
      Serial.println("# Ready to sense some light!"); 
    } else {
      Serial.println("# ERROR: Could not communicate with the sensor!  Freezing.");
      while (1);
    }
  
    // Light Sensor: the gain and integration times determine the resolution of the lux
    // value, and give different ranges of possible light readings. Check out
    // hoookup guide for more info. 
    light.setGain(LUX_GAIN);
    light.setIntegTime(LUX_INTEGRATION_TIME);
  
    Serial.println("# Light Sensor: Reading settings..."); 
    Serial.print("#    Gain: ");
    float gainVal = light.readGain();
    Serial.print(gainVal, 3); 
    Serial.println("");
    Serial.print("#    Integration Time: ");
    int timeVal = light.readIntegTime();
    Serial.println(timeVal);

    luxVal = light.readLight();
    for (int x = 0; x < NUM_LUX_SAMPLES; x++) {  // prime the averaging table with current value...
        luxAverage[x] = luxVal;
    }

    for (int bank = 0; bank <= 1; bank++) {
        for (int x = 0; x < 7; x++) {
            CONFIG[bank][x] = 0; // all bits are outputs...
            INSTALLED[bank][x] = false;
        }
    }
    INSTALLED[0][0] = true; // board exists...
    INSTALLED[1][0] = true;

    initIOXs();
    
}

bool isRunning = false;
bool isWaiting = false;
long stime = 0;

#define SECOND  1000
#define MINUTE  (60 * SECOND)
#define HOUR    (60 * MINUTE)
elapsedMillis checkTime;
#define TIME_TIME (1 * SECOND)       // 1 Second counter for doing "simulated" things...

elapsedMillis checkAuto;
// #define AUTO_TIME (15 * MINUTE)     // How often to initiate a simulation if nobody presses button?
#define AUTO_TIME (2 * MINUTE)       // How often to initiate a simulation if nobody presses button?

elapsedMillis checkLight;
#define LIGHT_TIME (3 * SECOND)     //  How often to sample room brightness?

elapsedMillis checkBlink;
bool curblink = false, blinker = false;
#define BLINK_TIME (1 * SECOND)

elapsedMillis checkButton;
bool needButton = true;
#define BUTTON_TIME 20 //mS

enum AnimationState { SLEEP,      // Get ready for low power/off mode
                      NIGHT,      // Off mode during the night
                      WAKE,       // Get ready for daytime mode
                      WAITING,    // Daytime - wait for button press ....  or night
                      BEGIN,      // Button pressed, get ready to run a simulated meet
                      RUNNING,    // actually run the simulation
                      FINISHED    // clean up after a simulation run, get ready for more WAITING...
                      };
AnimationState state = SLEEP;  // Start in low power mode...

void loop() {

    if (isRunning && (checkTime > TIME_TIME)) {
        stime++;
        checkTime = 0;
    }

    if (checkBlink > BLINK_TIME) {
        if (blinker) { blinker = false; } else { blinker = true; }
        checkBlink = 0;
    }
    // Keep a running average of the light level in luxVal
    // update samples every minute, average over 10 samples.
    // if average light level drops below threshold, the state machine will notice and turn off machine lamps...
    
    if (checkLight > LIGHT_TIME) {
        long l = light.readLight();
        luxAverage[luxIdx++] = l;
        if (luxIdx > NUM_LUX_SAMPLES) luxIdx = 0;
        luxVal = 0;
        for (int x = 0; x < NUM_LUX_SAMPLES; x++) {
          luxVal +=  luxAverage[x];
        }
        luxVal = luxVal / NUM_LUX_SAMPLES;
        checkLight = 0;
        Serial.print("# Checking room brightness: ");
        Serial.print ("Level: ");
        Serial.print(l);
        Serial.print(", average: ");
        Serial.println(luxVal);
    }
    
    switch (state) {           
      case SLEEP: // if illumination sensor is dark, turn off as much as possible
                  
                  Serial.println("# Going to sleep...");
                  // turn the machine off ...

                  OUTPUTS[0][0] = 0xFFFF;
                  OUTPUTS[1][0] = 0xFFFF;
                  writeIOXs();    

                  state = NIGHT;
                  break;
                  
      case NIGHT:
                 if (luxVal > LIGHT_LEVEL_WAKE) {
                    state = WAKE;
                    Serial.print("# Noticed it is getting brighter...");
                    Serial.println(luxVal);
                  }
       
                  curblink = bitRead(OUTPUTS[0][0], 0);
                  if (curblink != blinker) {
                      bitWrite(OUTPUTS[0][0], 0, blinker);  // DEMO: blink a bit to show we are sleeping...
                      writeIOXs();
                  }
                  break;
                  
      case WAKE:  // Things are getting brighter - someone turned on the lights

                  Serial.println("# Waking up...");

                  // Turn the machine back on...
       
                  initIOXs();
                  
                  // Turn on bits that need to be on, etc...
                  // bitSet(OUTPUTS[bank][board], bitnum);
                  // bitClear(OUTPUTS[bank][board], bitnum);
                  
                  OUTPUTS[0][0] = 0x00FF;
                  OUTPUTS[1][0] = 0x0F00;
                  writeIOXs();    
                                
                  checkAuto   = 0;  // reset auto run counter since we just woke up...
                  state = WAITING;
                  break;
                  
      case WAITING:  // Daytime mode - we want people to notice us and press our button
                     // waiting for user to press button or room to go dark...
                     
                  curblink = bitRead(OUTPUTS[0][0], 1);
                  if (curblink != blinker) {
                      bitWrite(OUTPUTS[0][0], 1, blinker);  // DEMO: blink a bit to show we are waiting...
                      writeIOXs();
                  }
                  // check for buttom press every few 10's of mS...
                  
                  if (checkButton > BUTTON_TIME) {
                      checkButton = 0;
                      if (button.isPressed() == true) {
                          Serial.print("# The button is pressed ... ");
                          while (button.isPressed() == true) {
                              delay(10);  //wait for user to stop pressing
                          }
                          Serial.println("and released");

                          state = BEGIN;
                      }
                  }

                  // Could readIOXs() and do something based on user pressing buttons on Machine...
                  
                  if (checkAuto > AUTO_TIME) {  // Nobody loves us, run a train anyways...
                      checkAuto = 0;
                      Serial.println("# Starting a simulation by myself...");
                      state = BEGIN;
                  }

                  // if room is dark, go to sleep
                  if (luxVal < LIGHT_LEVEL_SLEEP) {
                      state = SLEEP;
                      Serial.print("# Noticed it is getting darker...");
                      Serial.println(luxVal);
                  }
                  
                  break;
                  
    case BEGIN:   // Initialize things to begin a "meet sequence"...

                  // ... set up array/list of controls and indications, initialize counters, etc

                  Serial.println("# Initializing a simulation...");
                  stime = 0; // reset simulation timer
                  
                  // Read settings from machine (if needed)
                  // readIOXs();
                  //
                  // look at INPUTS[bank][board] as needed...
                  // if (bitRead(INPUTS[bank][board], bitnum)) { ... }
                  // 
                  // Turn on bits that need to be on, etc...
                  // bitSet(OUTPUTS[bank][board], bitnum);
                  // bitClear(OUTPUTS[bank][board], bitnum);
                  OUTPUTS[0][0] = 0xF00F;
                  OUTPUTS[1][0] = 0x0FF0;
                  writeIOXs();
                  
                  isRunning = true;
                  state = RUNNING;
                  break;

    case RUNNING: // collect actionable things and do them until done...

                  // *do* the simulation - send I2C packets to the cTc machine, etc
                  // The stime (simulation time) variable will be updated for you once every second
                  // so you can simply check to see if it is time to do the next thing on the list...

                  Serial.println("# Running a simulation...");
                  
                  // ...
                  
                  // update machine hardware:
                  
                  // Read settings from machine (if needed)
                  // readIOXs();
                  //
                  // look at INPUTS[bank][board] as needed...
                  // if (bitRead(INPUTS[bank][board], bitnum)) { ... }
                  // 
                  // Turn on bits that need to be on, etc...
                  // bitSet(OUTPUTS[bank][board], bitnum);
                  // bitClear(OUTPUTS[bank][board], bitnum);
                  //

                  OUTPUTS[0][0] = stime | (stime << 8);
                  OUTPUTS[1][0] = millis() | (millis() << 8);
                  writeIOXs();

                  
                  // FOR SIMPLE DEMO: 
                  // if stime > 10 we say simulation is done
                  
                  if (stime > 10) {
                      state = FINISHED;
                  } else {
                      delay( 1 * SECOND );
                  }
                  break;
                  
    case FINISHED:  // do any cleanup, reset cTc machine lights, etc.

                  Serial.println("# Cleaning up after a simulation...");
                  
                  // Read settings from machine (if needed)
                  // readIOXs();
                  //
                  // look at INPUTS[bank][board] as needed...
                  // if (bitRead(INPUTS[bank][board], bitnum)) { ... }
                  // 
                  // Turn on bits that need to be on, etc...
                  // bitSet(OUTPUTS[bank][board], bitnum);
                  // bitClear(OUTPUTS[bank][board], bitnum);
                  OUTPUTS[0][0] = 0xF00F;
                  OUTPUTS[1][0] = 0x0FF0;
                  writeIOXs();
                  
                  isRunning = false;
                  checkAuto   = 0;  // reset auto run counter since we just ran...
                  state = WAITING;
                  break;
    }
}
