//
//  AirDcc.h
//
//
//  Created by Eric Reuter on 11/7/17.
//
//

#ifndef airdcc_h
#define airdcc_h


#include "Arduino.h"
#include "SPI.h"

class AirDcc{

public:
    AirDcc(uint8_t enable_pin);
    void startModem(uint8_t channel, boolean transmit, uint8_t power);
    void changeChannel(uint8_t channel);
    void keepAlive();

private:
    uint8_t _enable_pin;
    uint8_t _transmit;
    uint8_t _channel;
    uint8_t _power;
};


#endif
