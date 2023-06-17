/*=============================================================================
File Name           :
Project File Number : v 0.07b
Project Name        : SMS alarm Samsø 4 x BWE632
Author              :
Start Date          :
Chip                : STM32f401
Copyright(c) 2022, All Rights Reserved.
-------------------------------------------------------------------------------
Description:
-----------------------------------------------------------------------------*/
/***************************   Include Files   *******************************/
#include <Arduino.h>
#include <IWatchdog.h>
#include <stdio.h>
#include <Timers.h>
#include <AsyncSMS.h>
#include <string.h>
#include <Wire.h>
#include "CRC8.h"
#include "CRC.h" //https://crccalc.com/
//#include <mytest.h>
/***************************   Defines   *************************************/
#define EE24C04P0 0x50 // Address of 24LC04 "block 0"
#define EE24C04P1 0x51 // Address of 24LC04 "block 1"
#define PCFI2CADR 0x20 // Address of PCF
Timers timer;
/***************************   Flags and bit *********************************/
bool newSms = false;
bool sec1 = false;
bool masterSmsNo = true;
bool smsSendNow = false;
bool delSms = false;
/***************************   sbit ******************************************/
/***************************   Macros   **************************************/
/***************************   Data Types   **********************************/
/***************************   Local Variables   *****************************/
u_int32_t deleteSmsAfterSend = millis();
u_int16_t smsCount = 1;
u_int16_t dayCounter = 0;
u_int8_t dataPcf = 87;
String smsMsg;
char timeBuffer[24];
char recSmsNumber[12];
char sendSmsMumber[12];
char recSmsMessage[30];
char henrik[12] = "+4522360040";
u_int8_t phNo0[17] = {"0"}; // Master no
u_int8_t phNo1[17] = {"1"};
u_int8_t phNo2[17] = {"2"};
u_int8_t phNo3[17] = {"3"};
u_int8_t phNo4[17] = {"4"};
u_int8_t eeSetting[17] = {"5"};
u_int8_t h; 
u_int8_t m; 
u_int8_t s;
/*****************************************************************************/
/***************************   Enum   ****************************************/
/***************************   Constants   ***********************************/
/***************************   Global Variables   ****************************/
/*****************************************************************************/
//            SDA  SCL
// TwoWire Wire1(PB4, PA8); // add by wire I2C 3
TwoWire Wire1(PB7, PB6); // Onboard PCF and EEprom AT24C04
//                     RX   TX
HardwareSerial Serial2(PA3, PA2);
AsyncSMS smsHelper(&Serial1, 57600);
/***************************   Function Prototypes   *************************/
void setup(void);
void loop(void);
void setupStm(void);
void readInPhNo(void);
void sendSmsTxt(void);
void readPcf8574(void);
void messageReceived(char *number, char *message);
void testPcfInput(void);
void newSmsRecived(void);
void sendSmsAlarm(void);
void sendEeSmsNo(void);
void retStatusSms(void);
void smsWriteEeprom(void);
void makeSmsTxtMsgSms(u_int8_t dataPcf);
void writeEeprom(int16_t addressI2C, u_int16_t eeAddress,
                 u_int8_t *data, u_int16_t numChars);
void readEeprom(int16_t addressI2C, u_int16_t eeAddress,
                u_int8_t *data, u_int16_t numChars);
void readMagnaAlarm(void);
void smsRunDays(void);
void eraseUsedEeprom(void);
void SettingInEeprom(void);
void sendStatusDaysReset(void);
byte bcdToByte(u_int8_t bcdHigh, u_int8_t bcdLow);
void updateClock(void);
/******************************************************************************
Function name : void setup()
         Type : PRIVATE
Description   : Run after start/reset
Notes :
******************************************************************************/
void setup()
{
   setupStm();
   eraseUsedEeprom(); // press buttom PA0 to erase EEprom or recive @* from "henrik"
   readInPhNo();      // read from EEprom
   smsHelper.init();
   smsHelper.smsReceived = *messageReceived;
   if (phNo0[0] != '+')
   {
      masterSmsNo = false;
   }

//  
   Serial2.print("Start UP mS:- ");
   Serial2.println(millis());
   sendStatusDaysReset(); // send sms with number of reset
   timer.start(5000); //time in ms
   IWatchdog.begin(10000000); // time in uS = 10 sec.
} // END setup
/******************************************************************************
Function name : void Loop()
         Type : PRIVATE
Description   :
Notes :
******************************************************************************/
void loop()
{
/***************************   Local Variables   *****************************/
static u_int32_t WaitSMStoSend = millis();
   smsHelper.process(); // call in main loop
   if ((smsSendNow) && (millis() - WaitSMStoSend >= 100))
   {
      sendSmsAlarm();
      WaitSMStoSend = millis();
   }
   readPcf8574();
   testPcfInput();
   newSmsRecived();
   if(eeSetting[1] == '1')
   { 
      smsRunDays();  // make so can turn on and off
   }
   //IWatchdog.reload();
   if (timer.available()) // just to make sure that Timer lib working after 50 days
   {
      timer.stop();
      IWatchdog.reload();
      timer.start(5000);
   }
   if((millis() - deleteSmsAfterSend >= 60000) && (delSms))
   {
      smsHelper.deleteAllSMS();
      //smsHelper.deleteSendSMS(); // give some error
      //smsHelper.deleteReadSMS();
      delSms = false;
   }
} // END loop
/******************************************************************************
Function name : void SetupStm32(void)
         Type :
Description   :
Notes :
******************************************************************************/
void setupStm(void)
{
   Serial2.begin(115200);
   while (!Serial2)
      ;
   Wire1.begin();
   pinMode(PA4, INPUT_PULLUP); // Magna3 alarm
   pinMode(PA0, INPUT_PULLUP); // Reset buttom
   pinMode(PC13, OUTPUT);      // sets the digital pin 13 as output
   digitalWrite(PC13, HIGH);
   // analogReadResolution(12);
   // delay(100);
} // END setupStm
/******************************************************************************
Function name : void
         Type :
Description   :
Notes :
******************************************************************************/
void readInPhNo(void)
{
/***************************   Local Variables   *****************************/   
uint8_t *data;
u_int8_t eeReadError = 0;
   readEeprom(EE24C04P0, 0x00, phNo0, 16); // master no
   data = (uint8_t *)&phNo0[0];
   if (phNo0[15] != crc8(data, 15, 0x07, 0x00, 0x00, false, false))
   {
      eeReadError = eeReadError + 1;
   }
   readEeprom(EE24C04P0, 0x10, phNo1, 16); // Phone no 1
   data = (uint8_t *)&phNo1[0];
   if (phNo1[15] != crc8(data, 15, 0x07, 0x00, 0x00, false, false))
   {
      eeReadError = eeReadError + 3;
   }
   readEeprom(EE24C04P0, 0x20, phNo2, 16); // Phone no 2
   data = (uint8_t *)&phNo2[0];
   if (phNo2[15] != crc8(data, 15, 0x07, 0x00, 0x00, false, false))
   {
      eeReadError = eeReadError + 5;
   }
   readEeprom(EE24C04P0, 0x30, phNo3, 16); // Phone no 3
   data = (uint8_t *)&phNo3[0];
   if (phNo3[15] != crc8(data, 15, 0x07, 0x00, 0x00, false, false))
   {
      eeReadError = eeReadError + 11;
   }
   readEeprom(EE24C04P0, 0x40, phNo4, 16); // Phone no 4
   data = (uint8_t *)&phNo4[0];
   if (phNo4[15] != crc8(data, 15, 0x07, 0x00, 0x00, false, false))
   {
      eeReadError = eeReadError + 12;
   }
   readEeprom(EE24C04P1, 0x10, eeSetting, 16); // Setting from EEprom
   eeSetting[0] = eeSetting[0] + 1;
   eeSetting[1] = '1';
   writeEeprom(EE24C04P1, 0x10, eeSetting, 16);
   Serial2.print("CRC8 Error: ");
   Serial2.println(eeReadError);
   if (eeReadError != 0)
   {
      smsMsg = "EEprom read ERROR!\n";
      smsMsg += eeReadError;
      smsMsg += "\n";
      strncpy(sendSmsMumber, henrik ,11);
      sendSmsTxt();
   }
}
/******************************************************************************
Function name : void sendSmsTxt(void)
         Type :
Description   :
Notes :
******************************************************************************/
void sendSmsTxt(void)
/***************************   Local Variables   *****************************/
{
   //   Serial2.print(millis());
   //   Serial2.print(" - ");
   digitalWrite(PC13, LOW);
   smsCount++;
   //smsHelper.send(sendSmsMumber, (char *)smsMsg.c_str(), smsMsg.length());
   //   Serial2.write(sendSmsMumber);
   //   Serial2.println("");
   Serial2.write(smsMsg.c_str(), smsMsg.length());
   Serial2.println("");
   deleteSmsAfterSend = millis(); // set 60 (59) sec. after sms to send
   digitalWrite(PC13, HIGH);
   //   Serial2.println(millis());
} // end
/******************************************************************************
Function name : void readPcf8574(void)
         Type :
Description   :
Notes :
******************************************************************************/
void readPcf8574(void)
{
/***************************   Local Variables   *****************************/
static u_int32_t readInterval = millis();
static u_int8_t read3Times = 0;
static u_int8_t inp[3] = {0xFF, 0xFF, 0xFF};
static u_int8_t i = 0;
static u_int8_t dataPcfRead = 0;
   if (sec1)
   {
      if (millis() - readInterval >= 3) // read in 3 times to get 2 correct
      {
         readInterval = millis();
         Wire1.beginTransmission(PCFI2CADR);
         Wire1.endTransmission();
         Wire1.requestFrom(PCFI2CADR, 1); // 1 byte from PCF
      }
      if (Wire1.available())
      {
         dataPcfRead = Wire1.read();
         read3Times++;
         inp[i++] = dataPcfRead;
         if (read3Times >= 3)
         {
            dataPcf = (inp[0] & inp[1]) & inp[2];
            i = 0;
            sec1 = false;
            read3Times = 0;
            readMagnaAlarm();
         }
      }
   }
}
/******************************************************************************
Function name : void messageReceived(char * number, char * message)
         Type :
Description   :
Notes :
*******************************************************************************/
void messageReceived(char *number, char *message)
{
   // Do something with your message
   Serial2.println("Message received");
   Serial2.println(number);
   Serial2.println(message);
   strncpy(recSmsNumber, number ,11);
   strncpy(recSmsMessage, message, 20);
   newSms = true;
}
/******************************************************************************
Function name : void testPcfInput(void)
         Type : FSM
Description   :
Notes :
******************************************************************************/
void testPcfInput(void)
{
/***************************   Local Variables   *****************************/
static u_int32_t ReadPCFinterval = 0;
u_int32_t mili_time = 0;
static u_int8_t dataPCFold = 27;
static u_int8_t LastDataSend = 156;
static u_int32_t Readcount = 0;
#define DEBOUNCE_TIME 10 // in Sec.
static bool WaitForData = false;
   //
   if (WaitForData)
   {
      WaitForData = false;
      if (dataPcf != dataPCFold)
      {
         if (dataPcf == LastDataSend)
         {
            Readcount = DEBOUNCE_TIME;
         }
         else
         {
            Readcount = 0;
         }
         dataPCFold = dataPcf;
      }
      else if (Readcount < DEBOUNCE_TIME)
      {
         Readcount++;
         if (Readcount == DEBOUNCE_TIME)
         {
            makeSmsTxtMsgSms(dataPcf);
            smsSendNow = true;
            LastDataSend = dataPcf;
         }
      }
   }
   else // WaitForData = false
   {
      mili_time = millis();
      if (mili_time - ReadPCFinterval >= 1000)
      {
         ReadPCFinterval = mili_time;
         // Read interval 1000 = 1 sek.
         updateClock();
         sec1 = true;
         WaitForData = true;
      }
   }
}
/******************************************************************************
Function name : void newSmsRecived(void)
         Type :
Description   :
Notes :
******************************************************************************/
void newSmsRecived(void)
{
   if (newSms)
   {
      if (recSmsMessage[0] == '#') // set new numers in EEprom and ram
      {
         smsWriteEeprom();
      }
      else if (recSmsMessage[0] == '?') // get status send back to asking number
      {
         retStatusSms();
      }
      else if (recSmsMessage[0] == '$') // send number and setting back to master and "henrik"
      {
         sendEeSmsNo();
      }
      if ((masterSmsNo == false) && (strncmp(recSmsNumber, recSmsMessage, 11) == 0)) // set the master number after reset or new device
      {
         u_int8_t *data;
         strncpy((char *)phNo0, recSmsMessage, 11);
         data = (uint8_t *)&phNo0[0];
         phNo0[15] = crc8(data, 15, 0x07, 0x00, 0x00, false, false);
         writeEeprom(EE24C04P0, 0x00, phNo0, 16);
         masterSmsNo = true;
      }
      if ((recSmsMessage[0] == '@') && (recSmsNumber, (char *)henrik, 11)) // check number == "henrik" in Function and pin & *
      {
         SettingInEeprom();
      }
      newSms = false;
   }
}
/******************************************************************************
Function name : void sendSmsAlarm(void)
         Type : FSM
Description   : sendSmsTxt();
Notes :
******************************************************************************/
void sendSmsAlarm(void)
{
/***************************   Local Variables   *****************************/
static u_int8_t sendAlarmState = 0;
   switch (sendAlarmState)
   {
   case 0:
      smsSendNow = true;
      // Move to next state
      deleteSmsAfterSend = millis(); // set 15 (14) sec. after first to sms to send
      sendAlarmState = 1;
      break;
   case 1:
      if (phNo0[0] == '+') // send always to master if exist
      {
         strncpy(sendSmsMumber, (char *)phNo0, 11);
         sendSmsTxt();
      }
      // Move to next state
      sendAlarmState = 2;
      break;
   case 2:
      if (phNo1[13] == '1') // to phone no 1
      {
         strncpy(sendSmsMumber, (char *)phNo1, 11);
         sendSmsTxt();
      }
      // Move to next state
      sendAlarmState = 3;
      break;
   case 3:
      if (phNo2[13] == '1') // to phone no 2
      {
         strncpy(sendSmsMumber, (char *)phNo2, 11);
         sendSmsTxt();
      }
      // Move to next state
      sendAlarmState = 4;
      break;
   case 4:
      if (phNo3[13] == '1') // to phone no 3
      {
         strncpy(sendSmsMumber, (char *)phNo3, 11);
         sendSmsTxt();
      }
      // Move to next state
      sendAlarmState = 5;
      break;
   case 5:
      if (phNo4[13] == '1') // to phone no 4
      {
         strncpy(sendSmsMumber, (char *)phNo4, 11);
         sendSmsTxt();
      }
      // Move to next state
      sendAlarmState = 6;
      break;
   case 6:
      smsSendNow = false;
      // Move to next state      
      sendAlarmState = 0; // reset to state 0
      delSms = true;
      break;
   default:
      break;
   }
}
/******************************************************************************
Function name : void sendEeSmsNo(void)
         Type :
Description   :
Notes :
******************************************************************************/
void sendEeSmsNo(void)
{
   if ((strcmp(recSmsNumber, henrik) == 0) ||
       (strcmp(recSmsNumber, (char *)phNo0) == 0)) // equal 0 ;-)
   {
      smsMsg = "Aktive Numre\n";
      smsMsg += (char *)phNo0;
      smsMsg += ("\n");
      smsMsg += (char *)phNo1;
      smsMsg += ("\n");
      smsMsg += (char *)phNo2;
      smsMsg += ("\n");
      smsMsg += (char *)phNo3;
      smsMsg += ("\n");
      smsMsg += (char *)phNo4;
      strncpy(sendSmsMumber, recSmsNumber ,11);
      sendSmsTxt();
   }
}
/******************************************************************************
Function name : void retStatusSms(void)
         Type :
Description   :
Notes :
******************************************************************************/
void retStatusSms(void)
{
   if ((strncmp(recSmsNumber, (char *)phNo0, 11) == 0) ||
       (strncmp(recSmsNumber, (char *)phNo1, 11) == 0) ||
       (strncmp(recSmsNumber, (char *)phNo2, 11) == 0) ||
       (strncmp(recSmsNumber, (char *)phNo3, 11) == 0) ||
       (strncmp(recSmsNumber, (char *)phNo4, 11) == 0)) // equal 0 ;-)
   {
      makeSmsTxtMsgSms(dataPcf);
      strncpy(sendSmsMumber, recSmsNumber ,11);
      sendSmsTxt();
      delSms = true;
      deleteSmsAfterSend = millis(); // set 60 (59) sec. after sms to send
   }
}
/******************************************************************************
Function name : void smsWriteEeprom(void)
         Type :
Description   :
Notes : https://crccalc.com/
******************************************************************************/
void smsWriteEeprom(void)
{
/***************************   Local Variables   *****************************/
u_int8_t *data;
   if ((strcmp(recSmsNumber, henrik) == 0) ||
       (strcmp(recSmsNumber, (char *)phNo0) == 0)) // equal 0 ;-) from master or "henrik"
   {
      if (recSmsMessage[13] == '1')
      {
         strncpy((char *)phNo1, recSmsMessage + 1, 16);
         data = (uint8_t *)&phNo1[0];
         phNo1[15] = crc8(data, 15, 0x07, 0x00, 0x00, false, false);
         writeEeprom(EE24C04P0, 0x10, phNo1, 16);
      }
      else if (recSmsMessage[13] == '2')
      {
         strncpy((char *)phNo2, recSmsMessage + 1, 16);
         data = (uint8_t *)&phNo2[0];
         phNo2[15] = crc8(data, 15, 0x07, 0x00, 0x00, false, false);
         writeEeprom(EE24C04P0, 0x20, phNo2, 16);
      }
      else if (recSmsMessage[13] == '3')
      {
         strncpy((char *)phNo3, recSmsMessage + 1, 16);
         data = (uint8_t *)&phNo3[0];
         phNo3[15] = crc8(data, 15, 0x07, 0x00, 0x00, false, false);
         writeEeprom(EE24C04P0, 0x30, phNo3, 16);
      }
      else if (recSmsMessage[13] == '4')
      {
         strncpy((char *)phNo4, recSmsMessage + 1, 16);
         data = (uint8_t *)&phNo4[0];
         phNo4[15] = crc8(data, 15, 0x07, 0x00, 0x00, false, false);
         writeEeprom(EE24C04P0, 0x40, phNo4, 16);
      }
   }
}
/******************************************************************************
Function name : void makeSmsTxtMsgSms(u_int8_t dataPcf)
         Type :
Description   :
Notes :
******************************************************************************/
void makeSmsTxtMsgSms(u_int8_t dataPcf)
{
   smsMsg = "Samsoe 4 x BWE 632\n";
   smsMsg += "Zone 1: ";    // Zone 1
   if (bitRead(dataPcf, 0)) // test Zone 1 for low pellets
   {
      smsMsg += "OK\n";
   }
   else
   {
      smsMsg += "LAV\n";
   }
   smsMsg += "Zone 2: ";    // Zone 2
   if (bitRead(dataPcf, 1)) // test Zone 2 for low pellets
   {
      smsMsg += "OK\n";
   }
   else
   {
      smsMsg += "LAV\n";
   }
   smsMsg += "Zone 3: ";    // Zone 3
   if (bitRead(dataPcf, 2)) // test Zone 3 for low pellets
   {
      smsMsg += "OK\n";
   }
   else
   {
      smsMsg += "LAV\n";
   }
   smsMsg += "Zone 4: ";    // Zone 4
   if (bitRead(dataPcf, 3)) // test Zone 4 for low pellets
   {
      smsMsg += "OK\n";
   }
   else
   {
      smsMsg += "LAV\n";
   }
   smsMsg += "Fase 1: ";    // Fase 1
   if (bitRead(dataPcf, 4)) // test Fase 1 for power
   {
      smsMsg += "ALARM\n";
   }
   else
   {
      smsMsg += "OK\n";
   }
   smsMsg += "Fase 2: ";    // Fase 2
   if (bitRead(dataPcf, 5)) // test Fase 2 for power
   {
      smsMsg += "ALARM\n";
   }
   else
   {
      smsMsg += "OK\n";
   }
   smsMsg += "Fase 3: ";    // Fase 3
   if (bitRead(dataPcf, 6)) // test Fase 3 for power
   {
      smsMsg += "ALARM\n";
   }
   else
   {
      smsMsg += "OK\n";
   }
   smsMsg += "Magna3: "; // Test Magna pump for ERROR
   if (bitRead(dataPcf, 7))
   {
      smsMsg += "OK\n";
   }
   else
   {
      smsMsg += "ALARM\n";
   }
//   smsMsg += "Tek: ";
//   smsMsg += smsCount;
//   smsMsg += " - ";
//   smsMsg += eeSetting[0];
//   smsMsg += " - ";
//   smsMsg += eeSetting[1];
//      smsMsg += "\n";
} // end check input
/******************************************************************************
Function name : void writeEeprom(int16_t addressI2C, u_int16_t eeAddress,
                                  u_int8_t* data, u_int16_t numChars)
         Type :
Description   :
Notes :
******************************************************************************/
void writeEeprom(int16_t addressI2C, u_int16_t eeAddress,
                 u_int8_t *data, u_int16_t numChars)
{
/***************************   Local Variables   *****************************/   
u_int8_t i = 0;
   Wire1.beginTransmission(addressI2C);
   Wire1.write(eeAddress);
   Wire1.write(data, numChars);
   Wire1.endTransmission(true);
   delay(5); // Not the right solution. but since it is only written when changing the user
   Serial2.println("Write");
}
/******************************************************************************
Function name : void readEeprom(int16_t addressI2C, u_int16_t eeAddress,
                                 u_int8_t* data, u_int16_t numChars)
         Type :
Description   :
Notes :
******************************************************************************/
void readEeprom(int16_t addressI2C, u_int16_t eeAddress,
                u_int8_t *data, u_int16_t numChars)
{
/***************************   Local Variables   *****************************/   
u_int8_t i = 0;
   Wire1.beginTransmission(addressI2C);
   Wire1.write(eeAddress); // LSB in 24c04 only one address byte use addressI2C 0x51 for "page 2"
   Wire1.endTransmission();
   Wire1.requestFrom(addressI2C, numChars);
   while (Wire1.available())
   {
      data[i++] = Wire1.read(); // recive all byte in rxbuffer
   }
}
/******************************************************************************
Function name : void readMagnaAlarm(void)
         Type :
Description   :
Notes :
******************************************************************************/
void readMagnaAlarm(void)
{
   bitWrite(dataPcf, 7, digitalRead(PA4)); // set alarm on LOW
}
/******************************************************************************
Function name : void smsRunDays(void)
         Type :
Description   :
Notes :
******************************************************************************/
void smsRunDays(void)
{
/***************************   Local Variables   *****************************/
static u_int32_t daysRunNoReset = millis();
#define DAYSRUNNING 86400000    // 24 hour

   if (millis() - daysRunNoReset >= DAYSRUNNING)
   {
      dayCounter = dayCounter + 1;
      sendStatusDaysReset();
      daysRunNoReset = millis();
   }
}
/******************************************************************************
Function name : void eraseUsedEeprom(void)
         Type :
Description   :
Notes : // if buttom preset on boot in 5 sec.
******************************************************************************/
void eraseUsedEeprom(void)
{
/***************************   Local Variables   *****************************/   
char DeleteEE[17] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
u_int8_t *data;
   if ((digitalRead(PA0) == LOW) || (recSmsMessage[0] == '@'))
   {
      digitalWrite(PC13, LOW);
      delay(5000); // test buttom press > 5 sec.
      if ((digitalRead(PA0) == LOW) || (recSmsMessage[0] == '@'))
      {
         strncpy((char *)phNo0, DeleteEE, 16);
         data = (uint8_t *)&phNo0[0];
         phNo0[15] = crc8(data, 15, 0x07, 0x00, 0x00, false, false);
         writeEeprom(EE24C04P0, 0x00, phNo0, 16);

         strncpy((char *)phNo1, DeleteEE, 16);
         data = (uint8_t *)&phNo1[0];
         phNo1[15] = crc8(data, 15, 0x07, 0x00, 0x00, false, false);
         writeEeprom(EE24C04P0, 0x10, phNo1, 16);

         strncpy((char *)phNo2, DeleteEE, 16);
         data = (uint8_t *)&phNo2[0];
         phNo2[15] = crc8(data, 15, 0x07, 0x00, 0x00, false, false);
         writeEeprom(EE24C04P0, 0x20, phNo2, 16);

         strncpy((char *)phNo3, DeleteEE, 16);
         data = (uint8_t *)&phNo3[0];
         phNo3[15] = crc8(data, 15, 0x07, 0x00, 0x00, false, false);
         writeEeprom(EE24C04P0, 0x30, phNo3, 16);

         strncpy((char *)phNo4, DeleteEE, 16);
         data = (uint8_t *)&phNo4[0];
         phNo4[15] = crc8(data, 15, 0x07, 0x00, 0x00, false, false);
         writeEeprom(EE24C04P0, 0x40, phNo4, 16);

         strncpy((char *)eeSetting, DeleteEE, 16);
         // eeSetting[0] = 0x00; // start with 1 after boot (255 = 0 on first boot)
         writeEeprom(EE24C04P1, 0x10, eeSetting, 16);
      }
      digitalWrite(PC13, HIGH);
   }
}
/******************************************************************************
Function name : void SettingInEeprom(void)
         Type :
Description   :
Notes :
******************************************************************************/
void SettingInEeprom(void)
{
   if (recSmsMessage[1] == 'C')
   {
      eeSetting[1] = recSmsMessage[2]; // do not save in EEprom 
   }
   if (recSmsMessage[1] == '*')
   {
      eraseUsedEeprom();
   }
   if (recSmsMessage[1] == 'R')
   {
   NVIC_SystemReset();
   }
}
/******************************************************************************
Function name : void sendStatusDaysReset(void)
         Type :
Description   :
Notes :
******************************************************************************/
void sendStatusDaysReset(void)
{
   smsMsg = "Antal dage i drift: ";
   smsMsg += dayCounter;
   smsMsg += "\n";
   smsMsg += "Antal Reset: ";
   smsMsg += eeSetting[0];
   smsMsg += "\n";
   strncpy(sendSmsMumber, henrik ,11);
   sendSmsTxt();
}
/******************************************************************************
Function name : byte bcdToByte(byte bcdHigh, byte bcdLow)
         Type :
Description   :
Notes :
******************************************************************************/
byte bcdToByte(u_int8_t bcdHigh, u_int8_t bcdLow) 
{
  // Convert the high BCD digit to its decimal equivalent
  u_int8_t decimalHigh = (bcdHigh >> 4) * 10 + (bcdHigh & 0x0F);
  
  // Convert the low BCD digit to its decimal equivalent
  u_int8_t decimalLow = (bcdLow >> 4) * 10 + (bcdLow & 0x0F);
  
  // Combine the decimal digits into one byte
  u_int8_t result = (decimalHigh * 10) + decimalLow;
  
  return result;
}
/******************************************************************************
Function name : void updateClock(void)
         Type :
Description   :
Notes :
******************************************************************************/
void updateClock()
{
static u_int32_t updateTheTime = millis();   
u_int8_t bcdHigh;
u_int8_t bcdLow;   

   Serial2.printf("%01d:%01d:%01d\n", h, m, s);
   if((millis() - updateTheTime >= 60000))
   { 
      updateTheTime = millis();
      smsHelper.readGSMTime();
      Serial2.println("update");
      Serial2.println(timeBuffer);
      bcdHigh = (timeBuffer[10] - 48); // Timer
      bcdLow = (timeBuffer[11] - 48); 
      u_int8_t hx = bcdToByte(bcdHigh, bcdLow);
      bcdHigh = (timeBuffer[13] - 48); // Minutter
      bcdLow = (timeBuffer[14] - 48); 
      u_int8_t mx = bcdToByte(bcdHigh, bcdLow);
      bcdHigh = (timeBuffer[16] - 48); // Sekunder
      bcdLow = (timeBuffer[17] - 48); 
      u_int8_t sx = bcdToByte(bcdHigh, bcdLow);
      bcdHigh = (timeBuffer[1] - 48); // Year
      bcdLow = (timeBuffer[2] - 48); 
      u_int8_t y = bcdToByte(bcdHigh, bcdLow);
      if(y < 22) // test if time error on gsm then dont update time
      {
         Serial2.println("update error");
      }
      else
      {
         h = hx;
         m = mx + 1; // let smsHelper.readGSMTime() have some time to update > 20 sec.
         s = sx + 1;
      }
   }
            s++;
         if (s >= 60) {
               m++;
               s = 0;
         }
         if (m >= 60) {
               h++;
               m = 0;
         }
         if (h >= 24) {
               h = 0;
         }
}