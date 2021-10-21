#include "MCP3422.h"
#include <Wire.h>
#include "hal.h"

//MCP3422

//#define MCP3422_SR 0x00 //240 SPS (12bit)
//#define MCP3422_SR 0x01 //60 SPS (14bit)
//#define MCP3422_SR 0x02 //15 SPS (16bit)
#define MCP3422_SR 0x03 //3.75 SPS (18bit)
#define MCP3422_GAIN 0x00 //x1
//#define MCP3422_GAIN 0x01 //x2
//#define MCP3422_GAIN 0x02 //x4
//#define MCP3422_GAIN 0x03 //x8
#define MCP3422_CR_STARTONESHOT 0x80 // /RDY bit = 1, /O/C bit = 0
#define MCP3422_CR_READY 0x80        // /RDY bit mask
#define MCP3422_NCH 2u    //Number of channels available
#define MCP3422_ADD 0x68  //Slave address


bool MCP3422_StartConversion(uint8_t u8Channel)
{
  bool bIsOK = true;
  uint8_t u8ConfigRegister = MCP3422_CR_STARTONESHOT | (MCP3422_SR << 2u) | MCP3422_GAIN; //Set defined parameters
  //Check channel range
  if ((MCP3422_NCH - 1u) < u8Channel)
  {
    bIsOK = false;
  }
  if (true == bIsOK)
  {
    //Set channel
    u8ConfigRegister |= (u8Channel << 5u);
    //Write Config Register
    //Wire.begin();
    Wire.beginTransmission(MCP3422_ADD);
    Wire.write(u8ConfigRegister);
    if (0 != Wire.endTransmission()) //0 = success in Wire library
    {
      bIsOK = false;
    }
  }
  return bIsOK;
}

bool MCP3422_IsDataAndRead(uint32_t &u32Value, bool &bIsPositive)
{
  bool bIsData = false;
  uint8_t u8ByteCounter = 0;
  uint8_t a4u8RecData[4];
  //Wire.begin();
  Wire.requestFrom(MCP3422_ADD, 4u);
  a4u8RecData[3] |= MCP3422_CR_READY;   //Force to NOT ready
  for (u8ByteCounter = 0; 4u > u8ByteCounter; u8ByteCounter++)
  {
    a4u8RecData[u8ByteCounter] = Wire.read();
  }

  if (MCP3422_CR_READY != (MCP3422_CR_READY & a4u8RecData[3]))
  { //RDY bit = 0 -> conversion finished, data is valid
    bIsData = true;
  }
  if (true == bIsData)
  {
    //Formatting according to page 22 of datasheet
    //Mode 12 bit
#if(0x00 == MCP3422_SR)
    if (0x08 == (0x08 & a4u8RecData[0]))
    {
      bIsPositive = false;
    }
    else
    {
      bIsPositive = true;
    }
    u32Value = ((a4u8RecData[0] & 0x07) << 8u) | a4u8RecData[1];
    if (false == bIsPositive)
    {
      u32Value = ~u32Value - 1; //Two's complement
    }
#endif //(0x00 == MCP3422_SR) 
    //Mode 14 bit
#if(0x01 == MCP3422_SR)
    if (0x20 == (0x20 & a4u8RecData[0]))
    {
      bIsPositive = false;
    }
    else
    {
      bIsPositive = true;
    }
    u32Value = ((a4u8RecData[0] & 0x1F) << 8u) | a4u8RecData[1];
    if (false == bIsPositive)
    {
      u32Value = ~u32Value - 1; //Two's complement
    }
#endif //(0x01 == MCP3422_SR) 

    //Mode 16 bit
#if(0x02 == MCP3422_SR)
    if (0x80 == (0x80 & a4u8RecData[0]))
    {
      bIsPositive = false;
    }
    else
    {
      bIsPositive = true;
    }
    u32Value = ((a4u8RecData[0] & 0x7F) << 8u) | a4u8RecData[1];
    if (false == bIsPositive)
    {
      u32Value = ~u32Value - 1; //Two's complement
    }
#endif //(0x02 == MCP3422_SR) 
    //Mode 18 bit
#if(0x03 == MCP3422_SR)
    if (0x02 == (0x02 & a4u8RecData[0]))
    {
      bIsPositive = false;
    }
    else
    {
      bIsPositive = true;
    }
    u32Value = ((uint32_t)(a4u8RecData[0] & 0x01) << 16u) | ((uint32_t)a4u8RecData[1] << 8u) | a4u8RecData[2];
    if (false == bIsPositive)
    {
      u32Value = ~u32Value - 1; //Two's complement
    }
#endif //(0x03 == MCP3422_SR)      
  }
  return bIsData;
}
