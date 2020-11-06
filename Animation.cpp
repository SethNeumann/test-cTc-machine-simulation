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
    {0, 0, 0, ~0x0000}, // Begin Meet, initialize all the the panel this card 2/20 is all model board
    {0, 0, 1, ~0x0000}, // 2/21 is all model board
    {0, 0, 2, ~0b0010010100000000}, // 2/22 Lo byte is model board, Hi byte has 1N 2N 3N all switchs and signals NORMAL
    {0, 0, 3, ~0b0100100100100101}, // 2/23 4N, 7N,8N, 12N, 14N
    {0, 0, 4, ~0b0010100100101001}, // 2/24 15N 16N 17N 19N 20N 21N
    {0, 0, 5, ~0b0010010000101000}, // 2/25 22N 23N 27N 28N
    {0, 1, 0, ~0b0101001010010000}, // 3/20 33N 34N 35N 36N 37N
    {0, 1, 1, ~0b0100100101001010}, // 3/21 38N 41N 42N 45N 46N
    {0, 1, 2, ~0b1001011010100101}, // 3/22 48N 49N 50N 53N 54N 55N 56N
    {0, 1, 3, ~0b0000000000000000}, // 3/23 lo byte is panel "jewelry" high byte is inputs
    
    {5000, 0, 0, ~0b0000000000000001}, // First Engine shows up at North Yard, 
    {6000, 0, 2, ~0b0000000001000000}, // Second Engine shows up 2 seconds later at Mtn Grove
    {8000, 1, 0, ~0b0011010010010000}, // Reverse switch 35R to put EB in the hole at Big Springs, turn off 35N
    {10000,1, 0, ~0b0010010010010000}, // Signal 36R clears the EB to the right, turn off 36N, switch stays reversed - working back to train per Breezy Gust

    {20000, WAIT_UNTIL, 0, 0}, // a NO-OP until the dancing girls show up ...
    {0, 0, 0, ~0x0000},      // all these will get processed after the above one with a real time...
    {0, 0, 1, ~0x0000},      // so we don't need to edit the times for everything in a block
    {0, 0, 2, 0x0000},
    {0, 0, 3, 0x0000},
    {0, 0, 4, 0x0000},
    {0, 0, 5, 0x0000},
    {0, 1, 0, 0x0000},
    {0, 1, 1, 0x0000},
    {0, 1, 2, 0x0000},
    {0, 1, 3, 0x0000},

    {21000, WAIT_UNTIL, 0, 0}, // ...  dancing girls left legs...
    {0, 0, 0, ~0b1010101010101010},
    {0, 0, 1, ~0b1010101010101010},
    {0, 0, 2, 0b1010101010101010},
    {0, 0, 3, 0b1010101010101010},
    {0, 0, 4, 0b1010101010101010},
    {0, 0, 5, 0b1010101010101010},
    {0, 1, 0, 0b1010101010101010},
    {0, 1, 1, 0b1010101010101010},
    {0, 1, 2, 0b1010101010101010},
    {0, 1, 3, 0b1010101010101010},

    {22000, WAIT_UNTIL, 0, 0}, // ...  dancing girls right legs...
    {0, 0, 0, 0b0101010101010101},
    {0, 0, 1, 0b0101010101010101},
    {0, 0, 2, 0b0101010101010101},
    {0, 0, 3, 0b0101010101010101},
    {0, 0, 4, 0b0101010101010101},
    {0, 0, 5, 0b0101010101010101},
    {0, 1, 0, 0b0101010101010101},
    {0, 1, 1, 0b0101010101010101},
    {0, 1, 2, 0b0101010101010101},
    {0, 1, 3, 0b0101010101010101},


    {23000, WAIT_UNTIL, 0, 0}, // ... more dancing girls...
    {0, 0, 0, 0b1010101010101010},
    {0, 0, 1, 0b1010101010101010},
    {0, 0, 2, 0b1010101010101010},
    {0, 0, 3, 0b1010101010101010},
    {0, 0, 4, 0b1010101010101010},
    {0, 0, 5, 0b1010101010101010},
    {0, 1, 0, 0b1010101010101010},
    {0, 1, 1, 0b1010101010101010},
    {0, 1, 2, 0b1010101010101010},
    {0, 1, 3, 0b1010101010101010},

    {24000, WAIT_UNTIL, 0, 0}, // ... and more dancing girls...
    {0, 0, 0, 0b0101010101010101},
    {0, 0, 1, 0b0101010101010101},
    {0, 0, 2, 0b0101010101010101},
    {0, 0, 3, 0b0101010101010101},
    {0, 0, 4, 0b0101010101010101},
    {0, 0, 5, 0b0101010101010101},
    {0, 1, 0, 0b0101010101010101},
    {0, 1, 1, 0b0101010101010101},
    {0, 1, 2, 0b0101010101010101},
    {0, 1, 3, 0b0101010101010101},

    {25000, WAIT_UNTIL, 0, 0}, // ... then kick everyone out and shut off the machine's lights
    {0, 0, 0, 0x0000},
    {0, 0, 1, 0x0000},
    {0, 0, 2, 0x0000},
    {0, 0, 3, 0x0000},
    {0, 0, 4, 0x0000},
    {0, 0, 5, 0x0000},
    {0, 1, 0, 0x0000},
    {0, 1, 1, 0x0000},
    {0, 1, 2, 0x0000},
    {0, 1, 3, 0x0000},

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
