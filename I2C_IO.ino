#include "I2C_IO.h"

// I2C IOX32 interface
// INPUTS and OUTPUTS are arrays of bits for/from the IOexpanders
bool     INSTALLED[2][8]; // is there an IO expander there?
uint16_t CONFIG[2][8];    // '1' bits are inputs, '0' are outputs
uint16_t INPUTS[2][8];
uint16_t OUTPUTS[2][8];

    
void initIOXs(void) {
    for (int bank = 0; bank <= 1; bank++) {
        int channel      = (bank == 0) ? 2 : 3;
        int otherchannel = (bank == 0) ? 3 : 2;
        enableMuxPort(channel);
        disableMuxPort(otherchannel);
        for (int x = 0; x < 7; x++) {
            if (INSTALLED[bank][x]) {
                uint16_t dir = CONFIG[bank][x];
#if defined(DEBUG_IOX)
                Serial.print("IOX: INIT bank ");
                Serial.print(bank);
                Serial.print(", board ");
                Serial.print(x);
                Serial.print(", config ");
                Serial.println(dir);
#endif
#ifdef IOX           
                Wire.beginTransmission(IOX_ADDR + x);
                Wire.write(MCP23017_IODIRA);
                Wire.write(0xff & dir);         // Low byte
                Wire.endTransmission();
            
                Wire.beginTransmission(IOX_ADDR + x);
                Wire.write(MCP23017_IODIRB);
                Wire.write(0xff & (dir >> 8));  // High byte
                Wire.endTransmission();
            
                // enable 100k pullups on all inputs...
                Wire.beginTransmission(IOX_ADDR + x);
                Wire.write(MCP23017_GPPUA);
                Wire.write(0xff & dir);         // Low byte
                Wire.endTransmission();
            
                Wire.beginTransmission(IOX_ADDR + x);
                Wire.write(MCP23017_GPPUB);
                Wire.write(0xff & (dir >> 8));  // High byte
                Wire.endTransmission();
#endif
#ifdef IO_PLOCHER
                Wire.beginTransmission(IO8574_ADDR + x);
                Wire.write(0xff & dir);
                Wire.endTransmission();
                Wire.beginTransmission(IO8574A_ADDR + x);
                Wire.write(0xff & dir);
                Wire.endTransmission();
#endif
            }
        }
    }
}

void readIOXs() {
    for (int bank = 0; bank <= 1; bank++) {
        int channel      = (bank == 0) ? 2 : 3;
        int otherchannel = (bank == 0) ? 3 : 2;
        enableMuxPort(channel);
        disableMuxPort(otherchannel);
        for (int x = 0; x < 7; x++) {
            if (INSTALLED[bank][x]) {
                uint16_t data = 0;
#if defined(DEBUG_IOX)
                Serial.print("IOX: READ bank ");
                Serial.print(bank);
                Serial.print(", board ");
                Serial.print(x);
#endif
#ifdef IOX           
                Wire.beginTransmission(IOX_ADDR + x);
                Wire.write(MCP23017_GPIOA);
                Wire.endTransmission();
                Wire.requestFrom(IOX_ADDR + x, (uint8_t)2, (uint8_t)1);
                data = Wire.read();
                data |= (Wire.read() << 8);
#endif
#ifdef IO_PLOCHER
                byte _data = 0;
                Wire.requestFrom(IO8574_ADDR + x, (uint8_t)1);
                if (Wire.available()) {
                    _data = Wire.read();
                }
                data = _data;
                Wire.requestFrom(IO8574A_ADDR + x, (uint8_t)1);
                if (Wire.available()) {
                    _data = Wire.read();
                }
                data |= (_data<< 8);            
#endif
                INPUTS[bank][x] = data;

#if defined(DEBUG_IOX)
                Serial.print(", data ");
                Serial.println(data);
#endif
            }
        }
    }
}

void writeIOXs() {
    for (int bank = 0; bank <= 1; bank++) {
        int channel      = (bank == 0) ? 2 : 3;
        int otherchannel = (bank == 0) ? 3 : 2;
        enableMuxPort(channel);
        disableMuxPort(otherchannel);
        for (int x = 0; x < 7; x++) {
            if (INSTALLED[bank][x]) {
                uint16_t data = OUTPUTS[bank][x] | CONFIG[bank][x];
#if defined(DEBUG_IOX)
                Serial.print("IOX: WRITE bank ");
                Serial.print(bank);
                Serial.print(", board ");
                Serial.print(x);
                Serial.print(", data ");
                Serial.println(data);
#endif
#ifdef IOX
                Wire.beginTransmission(IOX_ADDR + x);
                Wire.write(MCP23017_GPIOA);
                Wire.write(0xff & data);  //  low byte
                Wire.write(data >> 8);    //  high byte
                Wire.endTransmission();
#endif
#ifdef IO_PLOCHER
                Wire.beginTransmission(IO8574_ADDR + x);
                Wire.write(0xff & data);
                Wire.endTransmission();
                Wire.beginTransmission(IO8574A_ADDR + x);
                Wire.write(0xff & (data >> 8));
                Wire.endTransmission();
#endif
            }
        }
    }
}
