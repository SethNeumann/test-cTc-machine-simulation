#include <Arduino.h>
#include <elapsedMillis.h>
#include "I2C_IO.h"

/**
 *  An action signifies some state to change at a certain time
 *
 *  The idea is that a timer and an index are set to zero at the beginning of the simulation,
 *  (see beginSimulation())
 *  and as the millis() based timer reaches the trigger time in the action, the values are sent to the
 *  associated I2C expander and the index counter is incremented - repeating until a trigger time is
 *  encountered that is still in the future, at which point we go back to waiting for time to pass...
 *
 *  The "end of list" indication is a trigger time of LASTCALL
 */

#define LASTCALL 999999999  // Marker for "end of simulation array"
#define WAIT_UNTIL      99  // Marker for "no expander for this item"

struct Actions {
    unsigned long  time_to_trigger;  // mS - don't send these out until the right time...
    int bank, expander;              // where to send the value
    int value;                       // ... da bits to output ...
};

/**
 *  A list of actions, with trigger times in chronological order.
 */

Actions StepTable[] = { 
    {0, 0, 0, 0x1100}, // Begin Meet, initialize all the jewelry on the panel
    {0, 0, 1, 0x8F00}, // blah
    {0, 0, 6, 0xBEEE}, // yadda blah
    { 5000, 1, 1, 0xFFFF}, // First Engine shows up
    {15000, 0, 1, 0xFFFF}, // Second Engine shows up 10 seconds later

    {20000, WAIT_UNTIL, 0, 0}, // a NO-OP until the dancing girls show up ...
    {0, 0, 0, 0x0000},      // all these will get processed after the above one with a real time...
    {0, 0, 1, 0x0000},      // so we don't need to edit the times for everything in a block
    {0, 0, 2, 0x0000},
    {0, 0, 3, 0x0000},
    {0, 0, 4, 0x0000},
    {0, 0, 5, 0x0000},
    {0, 1, 0, 0x0000},
    {0, 2, 1, 0x0000},
    {0, 3, 2, 0x0000},
    {0, 4, 3, 0x0000},

    {25000, WAIT_UNTIL, 0, 0}, // ...  dancing girls left legs...
    {0, 0, 0, 0b1010101010101010},
    {0, 0, 1, 0b1010101010101010},
    {0, 0, 2, 0b1010101010101010},
    {0, 0, 3, 0b1010101010101010},
    {0, 0, 4, 0b1010101010101010},
    {0, 0, 5, 0b1010101010101010},
    {0, 1, 0, 0b1010101010101010},
    {0, 2, 1, 0b1010101010101010},
    {0, 3, 2, 0b1010101010101010},
    {0, 4, 3, 0b1010101010101010},

    {30000, WAIT_UNTIL, 0, 0}, // ...  dancing girls right legs...
    {0, 0, 0, 0b0101010101010101},
    {0, 0, 1, 0b0101010101010101},
    {0, 0, 2, 0b0101010101010101},
    {0, 0, 3, 0b0101010101010101},
    {0, 0, 4, 0b0101010101010101},
    {0, 0, 5, 0b0101010101010101},
    {0, 1, 0, 0b0101010101010101},
    {0, 2, 1, 0b0101010101010101},
    {0, 3, 2, 0b0101010101010101},
    {0, 4, 3, 0b0101010101010101},


    {35000, WAIT_UNTIL, 0, 0}, // ... more dancing girls...
    {0, 0, 0, 0b1010101010101010},
    {0, 0, 1, 0b1010101010101010},
    {0, 0, 2, 0b1010101010101010},
    {0, 0, 3, 0b1010101010101010},
    {0, 0, 4, 0b1010101010101010},
    {0, 0, 5, 0b1010101010101010},
    {0, 1, 0, 0b1010101010101010},
    {0, 2, 1, 0b1010101010101010},
    {0, 3, 2, 0b1010101010101010},
    {0, 4, 3, 0b1010101010101010},

    {40000, WAIT_UNTIL, 0, 0}, // ... and more dancing girls...
    {0, 0, 0, 0b0101010101010101},
    {0, 0, 1, 0b0101010101010101},
    {0, 0, 2, 0b0101010101010101},
    {0, 0, 3, 0b0101010101010101},
    {0, 0, 4, 0b0101010101010101},
    {0, 0, 5, 0b0101010101010101},
    {0, 1, 0, 0b0101010101010101},
    {0, 2, 1, 0b0101010101010101},
    {0, 3, 2, 0b0101010101010101},
    {0, 4, 3, 0b0101010101010101},

    {50000, WAIT_UNTIL, 0, 0}, // ... then kick everyone out and shut off the machine's lights
    {0, 0, 0, 0x0000},
    {0, 0, 1, 0x0000},
    {0, 0, 2, 0x0000},
    {0, 0, 3, 0x0000},
    {0, 0, 4, 0x0000},
    {0, 0, 5, 0x0000},
    {0, 1, 0, 0x0000},
    {0, 2, 1, 0x0000},
    {0, 3, 2, 0x0000},
    {0, 4, 3, 0x0000},

    {LASTCALL, 0, 0, 0},   // LAST thing, ignore data
};

elapsedMillis simTimer;
unsigned int simItem;


void beginSimulation(void) {
    simTimer = 0;
    simItem = 0;
}

/**
 *  Walk thru the above table, gated on the milis() based timer "simTimer"
 *  and the trigger time of the next item...
 *
 *      If the time to trigger is LASTCALL, we are at the end of the list, return FALSE
 *      If the bank is WAIT_FOR, there isn't a real expander to write to
 *      else write out the value to the addressed expander and return TRUE, signifying more to come...
 */
 
bool runSimulation(void) {
    if (StepTable[simItem].time_to_trigger == LASTCALL) {
        return false;
    }
    while (simTimer >= StepTable[simItem].time_to_trigger) {
        if (StepTable[simItem].bank == WAIT_UNTIL) {
            // skip
        } else {
            OUTPUTS[StepTable[simItem].bank][StepTable[simItem].expander] = StepTable[simItem].value;
            writeIOXs();
        }
        simItem++;
    }
    return true;
}
