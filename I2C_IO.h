#ifndef _I2C_IO_H
#define _I2C_IO_H

#define IOX        // MCP23017 16 bit expanders
//#define IO_PLOCHER    // PCF8574 & PCF8574A 8-bit expanders

// #define DEBUG_IOX

// I2C IOX32 interface
// INPUTS and OUTPUTS are arrays of bits for/from the IOexpanders
extern bool     INSTALLED[2][8]; // is there an IO expander there?
extern uint16_t CONFIG[2][8];    // '1' bits are inputs, '0' are outputs
extern uint16_t INPUTS[2][8];
extern uint16_t OUTPUTS[2][8];

#define IOX_ADDR        0x20

#define IO8574A_ADDR    0x38
#define IO8574_ADDR     0x20

enum MCP23017Registers {
    MCP23017_IODIRA   = 0x00,
    MCP23017_IODIRB   = 0x01,
    MCP23017_IPOLA    = 0x02,
    MCP23017_IPOLB    = 0x03,
    MCP23017_GPINTENA = 0x04,
    MCP23017_GPINTENB = 0x05,
    MCP23017_DEFVALA  = 0x06,
    MCP23017_DEFVALB  = 0x07,
    MCP23017_INTCONA  = 0x08,
    MCP23017_INTCONB  = 0x09,
    MCP23017_IOCONA   = 0x0A,
    MCP23017_IOCONB   = 0x0B,
    MCP23017_GPPUA    = 0x0C,
    MCP23017_GPPUB    = 0x0D,
    MCP23017_INTFA    = 0x0E,
    MCP23017_INTFB    = 0x0F,
    MCP23017_INTCAPA  = 0x10,
    MCP23017_INTCAPB  = 0x11,
    MCP23017_GPIOA    = 0x12,
    MCP23017_GPIOB    = 0x13,
    MCP23017_OLATA    = 0x14,
    MCP23017_OLATB    = 0x15,
};
    
extern void initIOXs(void);
extern void readIOXs(void);
extern void writeIOXs(void);


#endif
