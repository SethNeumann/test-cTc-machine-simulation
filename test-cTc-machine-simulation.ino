
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
#include <SparkFun_Qwiic_Button.h>
#include "I2C_IO.h"

extern boolean enableMuxPort(byte portNumber);
extern boolean disableMuxPort(byte portNumber);


extern void beginSimulation(void);
extern bool runSimulation(void);


QwiicButton button;
enum AnimationState { SLEEP, NIGHT, WAKE, WAITING, BEGIN, RUNNING, FINISHED };

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("cTc Simulation - basic framework");

    Wire.begin(); //Join I2C bus
    
    enableMuxPort(0); // Light Sensor
    enableMuxPort(1); // Button
    disableMuxPort(2); //turn these off, this is handled is handled elsewhere
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

    for (int bank = 0; bank <= 1; bank++) {
        for (int x = 0; x < 7; x++) {
            CONFIG[bank][x] = 0; // all bits are outputs...
            INSTALLED[bank][x] = false;
        }
    }

    INSTALLED[0][0] = true; // board exists...
    INSTALLED[0][1] = true;
    INSTALLED[0][2] = true;
    INSTALLED[0][3] = true;
    INSTALLED[0][4] = true;
    INSTALLED[0][5] = true;
    
    INSTALLED[1][0] = true; // second chain
    INSTALLED[1][1] = true;
    INSTALLED[1][2] = true;
    INSTALLED[1][3] = true; //

    initIOXs();
    
}

bool isRunning = false;
bool isWaiting = false;
long stime = 0;
int tick = 100;
int  writeword = 0;

#define SECOND  1000
#define MINUTE  (60 * SECOND)
#define HOUR    (60 * MINUTE)

elapsedMillis checkTime;
#define TIME_TIME (1 * SECOND)       // 1 Second counter for doing "simulated" things...

elapsedMillis checkAuto;
// #define AUTO_TIME (15 * MINUTE)     // How often to initiate a simulation if nobody presses button?
#define AUTO_TIME (2 * MINUTE)       // How often to initiate a simulation if nobody presses button?

elapsedMillis checkBlink;
bool curblink = false, blinker = false;
#define BLINK_TIME (1 * SECOND)

elapsedMillis checkButton;
bool needButton = true;
#define BUTTON_TIME 20 //mS

elapsedMillis checkIOXs;
#define IOXREAD_TIME 100 //mS

AnimationState state = WAITING; 

void loop() {

    if (isRunning && (checkTime > TIME_TIME)) {
        stime++;
        checkTime = 0;
    }

    if (checkBlink > BLINK_TIME) {
        if (blinker) { blinker = false; } else { blinker = true; }
        checkBlink = 0;
    }

    if (checkIOXs > IOXREAD_TIME) {
        readIOXs();
        // do something based on the value of INPUTS[bank][expander]...
        checkIOXs = 0;
    }
    
    switch (state) {

      case SLEEP:
      case NIGHT:
      case WAKE:
      case WAITING:  // Attract mode - we want people to notice us and press our button
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
                  
                  if (checkAuto > AUTO_TIME) {  // Nobody loves us, run a train anyways...
                      checkAuto = 0;
                      Serial.println("# Starting a simulation by myself...");
                      state = BEGIN;
                  }
                  break;
                  
    case BEGIN:   // Initialize things to begin a "meet sequence"...

                  // ... set up array/list of controls and indications, initialize counters, etc

                  Serial.println("# Initializing a simulation...");
                  beginSimulation();
                  isRunning = true;
                  state = RUNNING;
                  break;

    case RUNNING: // collect actionable things and do them until done...

                  Serial.println("# Running a simulation...");
                  if (!runSimulation()) {
                      state = FINISHED;
                  }
                  break;
                  
    case FINISHED:  // do any cleanup, reset cTc machine lights, etc.

                  Serial.println("# Cleaning up after a simulation...");

                  isRunning = false;
                  checkAuto   = 0;  // reset auto run counter since we just ran...
                  state = WAITING;                  
                  break;
    }
}
