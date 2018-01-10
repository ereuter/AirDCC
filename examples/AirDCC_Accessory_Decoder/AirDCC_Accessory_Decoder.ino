
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
// file:      AirDCC_Accessory_Decoder.ino
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
//            In this version, the channel and accessory addresses are hard coded.
//            Future goal is to use CVs for this.
//
// operation: Assign the accessory addresses and pins you want to use in the
//            ConfigureDecoder function.  There are separate pins assigned for
//            on, off, and a servo output.  You can also set positions for the
//            servo in each state.
//
//            Whenever a state is changed, it is written to EEPROM, and restored
//            at startup.
//
//            The AirDCC library communicates with the CC1101 over the SPI bus.
//            The CC1101 is put into asynchronous mode at the specified frequency.
//            The DCC stream then appears at the GDO pin on the CC1101.  This is
//            fed to Pin 2 on the Arduio, and the decoder operation is then
//            identical to a hardwired isntallation.
//
// required:  You will need to install the DCC_Decoder library, available in
//            the Arduino IDE's Library Manager.  Other required libraries
//            are built in.  The AirDCC library may be used in conjuction with
//            any standard DCC library.
//
// hardware:  The CC1101 module should be connected to the Arduino as follows:
//            Arduino                 CC1101
//            MOSI (pin 11)           MOSI  (SPI)
//            SCK  (pin 13)           SCK   (SPI)
//            Pin 4 (Slave enable)    CSN   (SPI) (this can be assigned to any pin)
//            Pin 2 (DCC in)          GD0
//------------------------------------------------------------------------

#include <DCC_Decoder.h>
#include <SPI.h>
#include <AirDcc.h>
#include <Servo.h>
#include <EEPROM.h>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Defines and structures
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define kDCC_INTERRUPT            0  // INT0 = Pin 2
#define NUM_ADDRESSES             4  // Number of addresses to decode

typedef struct
{
    int   address;      // Accessory address
    bool  state;        // State of accessory 1=on, 0=off
    int   offPin;       // Arduino output pins to drive
    int   onPin;
    int   servoPin;     // Pin for servo control
    int   servoOffPos;  // Position of servo in off state (in degrees)
    int   servoOnPos;   // Position of servo in on state (in degrees)
} DCCAccessoryAddress;

DCCAccessoryAddress gAddresses[NUM_ADDRESSES];  //number of addresses below

Servo servo[NUM_ADDRESSES];  // Create servo instances

unsigned long timer;  // Timer value for modem keepAlive

// Setup RF modem
const int enablePin = 10;   // Slave enable line for CC1101
int channel = 6;            // RF channel (frequency)
int tx = false;             // We are receiving
int power = 0;              // Power is irrelevant when receiving
AirDcc   Modem(enablePin) ; // Instantiate modem

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Decoder Init
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ConfigureDecoder()
{
    gAddresses[0].address =     1;
    gAddresses[0].offPin =      18;
    gAddresses[0].onPin =       19;
    gAddresses[0].servoPin =    14;
    gAddresses[0].servoOffPos = 45;
    gAddresses[0].servoOnPos =  135;

    gAddresses[1].address =     2;
    gAddresses[1].offPin =      3;
    gAddresses[1].onPin =       4;
    gAddresses[1].servoPin =    15;
    gAddresses[1].servoOffPos = 45;
    gAddresses[1].servoOnPos =  135;

    gAddresses[2].address =     3;
    gAddresses[2].offPin =      5;
    gAddresses[2].onPin =       6;
    gAddresses[2].servoPin =    16;
    gAddresses[2].servoOffPos = 45;
    gAddresses[2].servoOnPos =  135;

    gAddresses[3].address =     4;
    gAddresses[3].offPin =      8;
    gAddresses[3].onPin =       9;
    gAddresses[3].servoPin =    17;
    gAddresses[3].servoOffPos = 45;
    gAddresses[3].servoOnPos =  135;

        // Setup outputs
    for(int i=0; i<(int)(sizeof(gAddresses)/sizeof(gAddresses[0])); i++)
    {
      pinMode( gAddresses[i].offPin,   OUTPUT );
      pinMode( gAddresses[i].onPin,    OUTPUT );
      pinMode( gAddresses[i].servoPin, OUTPUT );

        //attach all servos
      servo[i].attach(gAddresses[i].servoPin);

        // Print setup data to console
      Serial.print("Addr:");
      Serial.print(gAddresses[i].address);
      Serial.print(" Off Pin:");
      Serial.print(gAddresses[i].offPin);
      Serial.print(" On Pin:");
      Serial.print(gAddresses[i].onPin);
      Serial.print(" Servo Pin:");
      Serial.print(gAddresses[i].servoPin);
      Serial.print(" Off Position:");
      Serial.print(gAddresses[i].servoOffPos);
      Serial.print(" On Position:");
      Serial.println(gAddresses[i].servoOnPos);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Set the outputs and save state to EEPROM
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SetOutputs(int addr)
{
  if(gAddresses[addr].state)
  {
    digitalWrite(gAddresses[addr].offPin, LOW);
    digitalWrite(gAddresses[addr].onPin, HIGH);
    servo[addr].write(gAddresses[addr].servoOnPos);
  }
  else
  {
    digitalWrite(gAddresses[addr].onPin, LOW);
    digitalWrite(gAddresses[addr].offPin, HIGH);
    servo[addr].write(gAddresses[addr].servoOffPos);
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Restore states from EEPROM at startup
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void RestoreStates()
{
  for(int i=0; i<(int)(sizeof(gAddresses)/sizeof(gAddresses[0])); i++)
  {
    gAddresses[i].state = EEPROM.read(i);
    SetOutputs(i);
    Serial.print(gAddresses[i].address);
    Serial.print(":");
    Serial.println(gAddresses[i].state);
    delay(500);
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Basic accessory packet handler
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void BasicAccDecoderPacket_Handler(int address, bool activate, byte data)
{
        // Convert NMRA packet address format to human address
    address -= 1;
    address *= 4;
    address += 1;
    address += (data & 0x06) >> 1;

       // Determine whether activate bit and bit 0 of data bit match
    bool state = (activate == (data & 0x01)) ? 0 : 1;
    Serial.print("Accessory ");
    Serial.print(address);
    Serial.println((state==1) ? " on" : " off");

    for(int i=0; i<(int)(sizeof(gAddresses)/sizeof(gAddresses[0])); i++)
    {
      if (address == gAddresses[i].address)
      {
        gAddresses[i].state = state;
        SetOutputs(i);
        EEPROM.update(i, state);
      }
    }

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup()
{
   Serial.begin(115200);
   Serial.println("DCC Wireless Accessory Decoder");
   DCC.SetBasicAccessoryDecoderPacketHandler(BasicAccDecoderPacket_Handler, true);
   ConfigureDecoder();
   delay(100);  // Delay for serial buffer to clear
   DCC.SetupDecoder( 0x00, 0x00, kDCC_INTERRUPT );  // (mfgID, mfgVers, interrupt)
   Serial.print("RF Channel:");
   Serial.println(channel);
   Modem.startModem(channel, tx, power);
   Serial.println("Restoring states:");
   RestoreStates();
   timer = millis();  // Load timer with current time
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Main loop
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void loop()
{
    DCC.loop();

    // Remind the modem to keep modeming every 500 ms
    if (millis() - timer > 500)
    {
      Modem.keepAlive();
      timer = millis();
    }
}
