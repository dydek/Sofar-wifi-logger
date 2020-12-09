#include "InverterMessage.h"
#include <Arduino.h>

InverterMessage::InverterMessage(byte message[], uint8 size)
{
    memcpy(
        this->messageData,
        message + 27,
        sizeof(message[0]) * (size - 27));
}

InverterMessage::~InverterMessage()
{
}

float InverterMessage::getShort(uint8_t begin, uint16_t divider)
{
    uint16_t num = 0;
    num = this->messageData[begin + 1];
    num |= this->messageData[begin] << 8;
    if (num > 32767)
    {
        return float(-(65536 - num)) / divider;
    }
    return float(num) / divider;
}

float InverterMessage::getLong(uint8_t begin, uint16_t divider)
{
    uint32_t num = 0;
    num = this->messageData[begin + 3];
    num |= this->messageData[begin + 2] << 8;
    num |= this->messageData[begin + 1] << 16;
    num |= this->messageData[begin] << 24;
    return float(num) / divider;
}

float InverterMessage::getTotalEnergy()
{
    return this->getLong(TOTAL_PRODUCTION, 1);
}

float InverterMessage::getTodayEnergy()
{
    return this->getShort(TODAY_PRODUCTION, 100);
}

float InverterMessage::getPVVoltage(uint8_t stringID)
{
    if (stringID == 1)
    {
        return this->getShort(PV_VOLTAGE_1);
    }
    else
    {
        return this->getShort(PV_VOLTAGE_2);
    }
}

float InverterMessage::getPVCurrent(uint8_t stringID)
{
    if (stringID == 1)
    {
        return this->getShort(PV_CURRENT_1, 100);
    }
    else
    {
        return this->getShort(PV_CURRENT_2, 100);
    }
}

float InverterMessage::getPVPower(uint8_t stringID)
{
    if (stringID == 1)
    {
        return this->getShort(PV_POWER_1, 100);
    }
    else
    {
        return this->getShort(PV_POWER_2, 100);
    }
}

float InverterMessage::getModuleTemp()
{
    return this->getShort(MODULE_TEMP, 10);
}