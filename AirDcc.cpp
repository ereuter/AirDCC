//------------------------------------------------------------------------
//
// AirDCC Accessory Decoder Example
//
// Copyright (c) 2018 Eric Reuter
//
// This source file is subject of the GNU general public license 2,
// that is available at the world-wide-web at
// http://www.gnu.org/licenses/gpl.txt
//
//------------------------------------------------------------------------
//
// file:      AirDcc.cpp
// author:    Eric Reuter
//
// history:   2018-01-07 Initial Version
//
//------------------------------------------------------------------------
//
// purpose:   Example of a stationary accessory decoder using the AirDCC library.
//            AirDCC employs a Texas Instruments CC1101 RF modem to receive  a
//            modulated DCC stream over a 900 MHz wireless connection.  Channels
//            assignments are compatible with the CVP Airwire throttles.
//
//------------------------------------------------------------------------


#include "Arduino.h"
#include "AirDcc.h"
#include "SPI.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Defines and constants
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define RX      0x34
#define TX      0x35
#define STOP    0x36
#define PATABLE 0x3E
#define CHAN    0x0A


// This string of bytes sets up the CC1101 modem.
const uint8_t initData[48] = {0x40, 0x2E, 0x2E, 0x0D, 0x07, 0xD3, 0x91, 0xFF, 0x04,
                              0x32, 0x00, 0x4B, 0x06, 0x00, 0x22, 0xB7, 0x55, 0x8A,
                              0x93, 0x00, 0x23, 0x3B, 0x50, 0x07, 0x30, 0x18, 0x16,
                              0x6C, 0x03, 0x40, 0x91, 0x87, 0x6B, 0xF8, 0x56, 0x10,
                              0xE9, 0x2A, 0x00, 0x1F, 0x40, 0x00, 0x59, 0x7F, 0x3F,
                              0x81, 0x35, 0x09};

// Channels designations are 0-16.  These are the corresponding values
// for the CC1101.
const uint8_t channels[17] = {0x4B, 0x45, 0x33, 0x27, 0x1B, 0x15, 0x0F, 0x03, 0x5E,
                              0x58, 0x52, 0x3E, 0x39, 0x2C, 0x21, 0x89, 0x37};

// Transmitter power settings are designated 0-10.  These are the corresponding
// PATABLE entries to set these powers.
const uint8_t powers[11] = {0x03, 0x15, 0x1C, 0x27, 0x66, 0x8E, 0x89,
                            0xCD, 0xC4,0xC1, 0xC0};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
AirDcc::AirDcc(uint8_t enable_pin)
{
    _enable_pin = enable_pin;
    pinMode(enable_pin, OUTPUT);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Set up and start the modem in rx or tx.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void AirDcc::startModem(uint8_t channel, bool transmit, uint8_t power)
{
    _channel = channel;
    _transmit = transmit;
    _power = power;

    uint8_t powerCode = 0x89; //use 0x89 for rx mode
    uint8_t channelCode = channels[_channel];
    uint8_t mode = RX;

    if (_transmit){
        mode = TX;
        // If in tx mode, grab the PATABLE value for the power specified
        powerCode=powers[_power];
    }

    // Set enable pin in idle state (high)
    digitalWrite(_enable_pin, HIGH);

    SPISettings settings(8000000, MSBFIRST, SPI_MODE0);

    SPI.begin();
    delay(100);
    SPI.beginTransaction(settings);

    //Stop the modem
    digitalWrite(_enable_pin, LOW);
    SPI.transfer(STOP);
    digitalWrite(_enable_pin, HIGH);

    //Send setup data
    digitalWrite(_enable_pin, LOW);  //enable 1101 communication
    for (int i=0; i < sizeof(initData); i++) SPI.transfer(initData[i]);
    digitalWrite(_enable_pin, HIGH);

    //Write PATBLE (power)
    digitalWrite(_enable_pin, LOW);
    SPI.transfer(PATABLE);
    SPI.transfer(powerCode);
    digitalWrite(_enable_pin, HIGH);

    //Set channel
    digitalWrite(_enable_pin, LOW);
    SPI.transfer(CHAN);
    SPI.transfer(channelCode);
    digitalWrite(_enable_pin, HIGH);

    //Set mode (rx/tx)
    digitalWrite(_enable_pin, LOW);
    SPI.transfer(mode);
    digitalWrite(_enable_pin, HIGH);

    SPI.endTransaction();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Change RF Channel
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void AirDcc::changeChannel(uint8_t channel)
{
    _channel = channel;

    uint8_t channelCode = channels[_channel];
    uint8_t powerCode = powers[_power];
    uint8_t mode = (_transmit) ? TX : RX;

    SPISettings settings(8000000, MSBFIRST, SPI_MODE0);

    SPI.begin();
    SPI.beginTransaction(settings);

    // Stop the modem
    digitalWrite(_enable_pin, LOW);
    SPI.transfer(STOP);
    digitalWrite(_enable_pin, HIGH);

    // Write the power PATABLE
    digitalWrite(_enable_pin, LOW);
    SPI.transfer(PATABLE);
    SPI.transfer(powerCode);  //power setting
    digitalWrite(_enable_pin, HIGH);

    // Set the new channel
    digitalWrite(_enable_pin, LOW);
    SPI.transfer(CHAN);
    SPI.transfer(channelCode);
    digitalWrite(_enable_pin, HIGH);

    // Start the modem
    digitalWrite(_enable_pin, LOW);
    SPI.transfer(mode);
    digitalWrite(_enable_pin, HIGH);

    SPI.endTransaction();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Remind the modem to continue tx or rx.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void AirDcc::keepAlive()
{
    SPISettings settings(8000000, MSBFIRST, SPI_MODE0);

    SPI.begin();
    SPI.beginTransaction(settings);

    digitalWrite(_enable_pin, LOW);

    if (_transmit) SPI.transfer(TX);
    else SPI.transfer(RX);

    digitalWrite(_enable_pin, HIGH);
    SPI.endTransaction();
}
