
#ifndef InverterMessage_h
#define InverterMessage_h

#include "Arduino.h"

#define DEBUG 0

#define PV_VOLTAGE_1 13
#define PV_CURRENT_1 15
#define PV_VOLTAGE_2 17
#define PV_CURRENT_2 19
#define PV_POWER_1 21
#define PV_POWER_2 23

#define AC_VOLTAGE_1 31
#define AC_CURRENT_1 33
#define AC_VOLTAGE_2 35
#define AC_CURRENT_2 37
#define AC_VOLTAGE_3 39
#define AC_CURRENT_4 41


#define MODULE_TEMP 55

#define TOTAL_PRODUCTION 43
#define TODAY_PRODUCTION 51

class InverterMessage
{
private:
    byte messageData[220];
    float getShort(uint8_t begin, uint16_t divider);
    float getShort(uint8_t begin) {
        return this->getShort(begin, 10);
    }
    float getLong(uint8_t begin, uint16_t divider);
    float getLong(uint8_t begin) {
        return this->getLong(begin, 10);
    }
public:
    InverterMessage(byte message[], uint8 size);
    ~InverterMessage();
    float getTotalEnergy();
    float getTodayEnergy();
    float getPVVoltage(uint8_t stringID);
    float getPVCurrent(uint8_t stringID);
    float getPVPower(uint8_t stringID);
    float getACVoltage(uint8_t phase);
    float getACCurrent(uint8_t phase);
    float getModuleTemp();
};

#endif