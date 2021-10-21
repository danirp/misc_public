#include <Wire.h>
#include <LiquidCrystal_I2C.h>  //Library: https://bitbucket.org/fmalpartida/new-liquidcrystal/downloads/
#include <EEPROM.h>

/////////////////////////////////////////////////////////////////////////////////////
//Defines:
/////////////////////////////////////////////////////////////////////////////////////

//LCD display parameters
#define N_ROWS 2u
#define N_COLS 16u

//General parameters
#define DEBOUNCEHARDNESS 10u 
#define MODE_T_TEST;
#define USING_CALIBRATION true
//#define CLEAREEATSTART

//Macros
//Scale

//0.0001ohm - 156mA
#define SCALE_01    digitalWrite(10, LOW); \
  digitalWrite(11, HIGH); \
  digitalWrite(12, HIGH); \
  pinMode(10, OUTPUT); \
  pinMode(11, OUTPUT); \
  pinMode(12, OUTPUT)
//0.001ohm - 15.6mA
#define SCALE_1    digitalWrite(11, LOW); \
  digitalWrite(10, HIGH); \
  digitalWrite(12, HIGH); \
  pinMode(10, OUTPUT); \
  pinMode(11, OUTPUT); \
  pinMode(12, OUTPUT)
//0.01ohm - 1.56mA
#define SCALE_10    digitalWrite(12, LOW); \
  digitalWrite(11, HIGH); \
  digitalWrite(10, HIGH); \
  pinMode(10, OUTPUT); \
  pinMode(11, OUTPUT); \
  pinMode(12, OUTPUT)

//Arduino builtin LED
#define STATUS_ON   digitalWrite(LED_BUILTIN, HIGH); \
  pinMode(LED_BUILTIN, OUTPUT)
#define STATUS_OFF  digitalWrite(LED_BUILTIN, LOW); \
  pinMode(LED_BUILTIN, OUTPUT)
//SCALE pushbutton
#define INIT_SCALE    pinMode(2, INPUT)                       //Config SCALE pushbutton pin
#define IS_SCALE_ON   (LOW == digitalRead(2))                 //true if ON pushed
//CAL pushbutton
#define INIT_CAL   pinMode(3, INPUT)                       //Config CAL pushbutton pin
#define IS_CAL_ON  (LOW == digitalRead(3))                 //true if CAL pushed

//MCP3422
#define MCP3422_CH1 0x00
#define MCP3422_CH2 0x01
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

//Messages
const char strHOLD[] = "HOLD";
const char strRUN[] = "RUN";
const char strAUTO[] = "AUTO";
const char strCAL[] = "CAL";
const char strECAL[] = "CAL?  B:Yes R:No";
const char strScale0m1[] = "(0m1)";
const char strScale1m0[] = "(1m0)";
const char strScale10m[] = "(10m)";

const char strFullScale[9] = "--------";
const char strEraseLine[17] = "                ";

//EEPROM Addresses
#define EE_U8MODE       0x0000  /*Current working mode, used in machine(), updateDisplayBuffer()*/
#define EE_U8SCALERUN   0x0002  /*Scale in RUN mode*/
#define EE_U8SCALEHOLD  0x0004  /*Scale in HOLD mode*/
#define EE_U32MEASHOLD  0x0006  /*Last measure stored before entering HOLD mode*/
#define EE_FCCONST0M1   0x0020  /*Calibration constant in 0m1 scale*/
#define EE_FCCONST1M0   0x0030  /*Calibration constant in 1m0 scale*/
#define EE_FCCONST10M   0x0040  /*Calibration constant in 10m scale*/
#define EE_U8CC0M1OK    0x0050  /*Indicator of calibration constant in 0m1 is OK*/
#define EE_U8CC1M0OK    0x0052  /*Indicator of calibration constant in 1m0 is OK*/
#define EE_U8CC10MOK    0x0054  /*Indicator of calibration constant in 10m is OK*/


#define MODEHOLD        0x01
#define MODERUN         0x02
#define MODEECAL        0x03
#define MODECAL         0x04
#define MODEAUTO        0x05
#define CALOKTOKEN      0xAA    /*Token indicating value OK for calibration constants*/

/////////////////////////////////////////////////////////////////////////////////////
//Constants:
/////////////////////////////////////////////////////////////////////////////////////
#define AMTHRESH 1050u 
#define AMTHRESL 95u   
#define AMTIME  1000u

#define RCAL0M1 124750.0
#define RCAL1M0 100000.0
#define RCAL10M 100000.0



/////////////////////////////////////////////////////////////////////////////////////
//Types:
/////////////////////////////////////////////////////////////////////////////////////
//MCP3422 machine states
enum eMCP3422ReadStates_t
{
  eMS_WaitCurrent = 0,
  eMS_WaitVoltage = 1
};

struct sInputs_t 
{
  bool bCal = false;
  bool bScale = false;
};

/////////////////////////////////////////////////////////////////////////////////////
//Variables:
/////////////////////////////////////////////////////////////////////////////////////
//Display
//Double buffer..
char displayBufferA[N_ROWS][N_COLS] =
{ // 0    1    2    3    4    5    6    7    8    9    10   11   12   13   14   15
  {'0', '0', '.', '0', '0', '0', '0', '~', ' ', ' ', ' ', '(', '0', 'm', '1', ')'},
  {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'R', 'U', 'N'}
};
char displayBufferB[N_ROWS][N_COLS] =
{
  {'#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#'},
  {'#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#'}
};

//Display class
LiquidCrystal_I2C lcd(0x3F, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE); // Addr, En, Rw, Rs, d4, d5, d6, d7, backlighpin, polarity (As descrived in the ebay link but with 0x3F address instead of 0x20)

//General variables       
uint8_t u8CurrentScale = 0;         //Current Scale
uint32_t u32CurrentMeasurement = 0;       //Last conversion value from MCP3422

sInputs_t sInputs;


uint64_t u64NextMillis = 0;

/////////////////////////////////////////////////////////////////////////////////////
//First execution:
/////////////////////////////////////////////////////////////////////////////////////
void setup()
{
#ifdef CLEAREEATSTART
  for (int index = 0 ; index < EEPROM.length() ; index++) 
  {
    EEPROM.write(index, 0);
  }
#endif
  if(!eeReadU8(EE_U8SCALERUN , u8CurrentScale))
  {//First time or if scale is corrupted in EEPROM, sets scale 0
    eeStoreU8(EE_U8SCALERUN, 0);
    u8CurrentScale = 0;
  }
  changeScale(u8CurrentScale);
  setupDisplay();
  Wire.setClock(40000);  //Speed for both display and MCP3422
  MCP3422_StartConversion(MCP3422_CH1);
  lcd.setBacklight(HIGH);
  Serial.begin(9600);//For debugging
}
/////////////////////////////////////////////////////////////////////////////////////
//Loop..
/////////////////////////////////////////////////////////////////////////////////////
void loop()
{
  updateInputs(sInputs);  //Read pushbuttons
  machine(25u, sInputs);  //State machine
  updateDisplayBuffer();  //Update display buffer
  updateDisplay(50u);     //Write the necessary chars in the display
}
/////////////////////////////////////////////////////////////////////////////////////
//Functions:
/////////////////////////////////////////////////////////////////////////////////////
//Starts a one shot conversion in MCP3422. Gain and sample rate/nbits are giben in defines.
//u8Channel: Chanel of MCP3422 to be read
//return: true if every thing was ok
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
//Returns the value of the last conversion of the ADC, if the ADC finished the last conversion starts a new one.
//bUsingCalibration: If true the last conversion is multiplied with the calibration factor stored in EEPROM.
//return: Last ADC value, calibrated or not, based on bUsingCalibration.
uint32_t readResistance(bool bUsingCalibration)
{
  static uint32_t u32Resistance = 0;
  float fCalibrationFactor = 0;
  bool bMCP3422ValIsPositive = true;  //Last conversion sign, in this circuit it's always positive
  if(MCP3422_IsDataAndRead(u32Resistance, bMCP3422ValIsPositive))
  {//New conversion available
    if(false == bMCP3422ValIsPositive)
    {
      u32Resistance = 0;
    }
    MCP3422_StartConversion(MCP3422_CH1);   //Restart conversion
    if(true == bUsingCalibration)
    {/*Get the calibration factor for this scale and apply it*/
      fCalibrationFactor = getCalibratedFactor();
      u32Resistance = (uint32_t)(((float)u32Resistance)*fCalibrationFactor);
    }
  }
  return u32Resistance; 
}
//Checks if the last conversion in MCP3422 is finished, if so updates the output value.
//u32Value: Output variable for ADC value.
//bIsPositive: Output variable for sign: true = positive
//return: true if the conversion was finished and there is data available
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

//Conects the mosfets of the current sink to obtain an output current.
//u8Scae: scale: 0: 156mA, 1: 15.6mA, 2: 1.56mA 
void changeScale(uint8_t u8Scale)
{
  switch(u8Scale)
  {
    case 0:
    {
      SCALE_01;
      break;
    }
    case 1:
    {
      SCALE_1;
      break;
    }
    case 2:
    {
      SCALE_10;
      break;
    }
  }
}

//Updates the state and value of pushbuttons
//sInputsUpdate: By ref, structure where the inputs state are stored.
void updateInputs(sInputs_t &sInputsUpdate)
{
  static uint8_t u8SCALEDebounce = 0;     //Debounce counter for On push button
  static uint8_t u8CALDebounce = 0;    //Debounce counter for Off push button
  //Init inputs
  INIT_SCALE;
  INIT_CAL;
  //Debouncing of pushbuttons...
  //Scale
  if (IS_SCALE_ON)
  {
    if (DEBOUNCEHARDNESS <= u8SCALEDebounce)
    { //DEBOUNCEHARDNESS times consecutively read true
      sInputsUpdate.bScale = true;
    }
    else
    {
      u8SCALEDebounce++;
    }
  }
  else
  {
    u8SCALEDebounce = 0;
    sInputsUpdate.bScale = false;
  }
  //Cal
  if (IS_CAL_ON)
  {
    if (DEBOUNCEHARDNESS <= u8CALDebounce)
    { //DEBOUNCEHARDNESS times consecutively read true
      sInputsUpdate.bCal = true;
    }
    else
    {
      u8CALDebounce++;
    }
  }
  else
  {
    u8CALDebounce = 0;
    sInputsUpdate.bCal = false;
  }
}


//Setup LCD.
void setupDisplay(void)
{
  lcd.begin(N_COLS, N_ROWS);
}

//Modifies displayBufferA with current measurement and mode
void updateDisplayBuffer(void)
{
  uint8_t u8currentMode = MODERUN;

  writeString(0u, 0u, strEraseLine);
  writeString(1u, 0u, strEraseLine);
  if(129999 >= u32CurrentMeasurement)
  {
    writeNNNNNN(u8CurrentScale,u32CurrentMeasurement, 0u, 0u);
    displayBufferA[0u][7u] = char(0xF4);
  }
  else
  {
    writeString(0u, 0u, strFullScale);
  } 
  switch(u8CurrentScale)
  {
    case 0:
    {
      writeString(0u, 11u, strScale0m1);
      break;
    }
    case 1:
    {
      writeString(0u, 11u, strScale1m0);
      break;
    }
    case 2:
    {
      writeString(0u, 11u, strScale10m);
      break;
    }
  }  
  /*Get current state*/
  u8currentMode = eeGetMode();
  switch(u8currentMode)
  {
    case MODERUN:
    {
      writeString(1u, 13u, strRUN);
      break;
    }
    case MODEHOLD:
    {
      writeString(1u, 12u, strHOLD);                    
      break;
    }
    case MODEAUTO:
    {
      writeString(1u, 13u, strRUN); 
      writeString(1u, 8u, strAUTO); 
      break;
    }
    case MODECAL:
    {
      writeString(1u, 13u, strCAL); 
      if(millis() & 0x0000000000000080)
      {/*aprox each ~255msec*/
        displayBufferA[1][0] = '*';
        displayBufferA[1][1] = '*';
      }
      else
      {
        displayBufferA[1][2] = '*';
        displayBufferA[1][3] = '*';
      }
      break;
    }
    case MODEECAL:
    {
      writeString(1u, 0u, strECAL);
      break;
    }
    default:
    {
      break;
    }
  }
}
//Browses displayBufferA. If any character is different in displayBufferA than displayBufferB
//writes it to displayBufferB and the LCD. Updates the possition of the cursor when neccesBary.
void updateDisplay(uint16_t u16RefreshTime)
{
  uint8_t u8Rows = 0;
  uint8_t u8Columns = 0;
  bool bMustSetCuror = true;
  static uint64_t u64RefreshTime = 0;

  if(millis() > u64RefreshTime)
  {
    u64RefreshTime = millis() + u16RefreshTime;
    for (u8Rows = 0; u8Rows < N_ROWS; u8Rows++)
    {
      for (u8Columns = 0; u8Columns < N_COLS; u8Columns++)
      {
        //Enters here for each character in displayBufferA
        if (displayBufferA[u8Rows][u8Columns] != displayBufferB[u8Rows][u8Columns])
        {
          //A new character must be written
          if (true == bMustSetCuror)
          {
            //Cursor must be updated because last character was not written
            lcd.setCursor(u8Columns, u8Rows);
          }
          bMustSetCuror = false;
          lcd.write(displayBufferA[u8Rows][u8Columns]);
          displayBufferB[u8Rows][u8Columns] = displayBufferA[u8Rows][u8Columns];
        }
        else
        {
          //No character to be written
          bMustSetCuror = true;
        }
      }
      //Update cursor in new row
      bMustSetCuror = true;
    }
  }
}
//Writes a string starting at the specified position.
//u8Line: Line.
//u8Column: Column.
//strText: text to write must be a c string with null terminator.
void writeString(uint8_t u8Line, uint8_t u8Column, const char* strText)
{
  uint8_t u8Index = 0;    
  while((strText[u8Index] != '\0') && (N_COLS > (u8Column + u8Index)))
  {
    displayBufferA[u8Line][u8Column + u8Index] = strText[u8Index];
    u8Index++;
  }
}

//Writes a NNNNNN fixed dot number in displayBufferA. The dot is located depending on the scale
//in NN.NNNN if scale == 0, in NNN.NNN if scale == 1, in NNNN.NN if scale = 2.
//u8Scale: current scale.
//u32Value: Number to be written, 0 = 000000 to 999999 = 999999.
//u8Line: Start possition line in displayBufferA.
//u8Column: Start possition column in displayBufferA.
void writeNNNNNN(uint8_t u8Scale, uint32_t u32Value, uint8_t u8Line, uint8_t u8Column)
{
  uint8_t u8tmp;

  //Limits
  if(999999u < u32Value)
  {
    u32Value = 999999u;
  }
  if(u8Scale > 2u)
  {
    u8Scale = 2u;  
  }
  displayBufferA[u8Line][u8Column + 6u] = (u32Value % 10) + 0x30;  //6th digit
  u8tmp = 6u;
  do
  {
    u8tmp--;
    if((u8Scale + 2u) == u8tmp)
    {
      displayBufferA[u8Line][u8Column + u8tmp] = '.';                     //Dot
    }
    else
    {
      u32Value /= 10u;
      displayBufferA[u8Line][u8Column + u8tmp] = (u32Value % 10) + 0x30;  //next digit
    }
  }
  while(u8tmp > 0);
}

//Stores a byte in EEPROM and its complementary value in the next position
//eeaddr: EEPROM address
//u8Value: Value to store
void eeStoreU8(int eeaddr, uint8_t u8Value)
{
  EEPROM.write(eeaddr, u8Value);
  EEPROM.write(eeaddr + 1u, ~u8Value);
}
//Reads a byte from EEPROM and checks if the next byte is the complementary
//eeaddr: EEPROM address
//u8Value: Read value, by reference.
//return: True if the next position in EEPROM had the complementary value
bool eeReadU8(int eeaddr, uint8_t &u8Value)
{
  uint8_t u8CheckValue = 0;
  u8Value = EEPROM.read(eeaddr);
  u8CheckValue = EEPROM.read(eeaddr + 1u);  
  u8CheckValue = ~u8CheckValue;
  return u8Value == u8CheckValue; //True if checksum was ok
}
//Stores a uint32_t in EEPROM and its complementary value in the next position
//eeaddr: EEPROM address
//u32Value: Value to store
void eeStoreU32(int eeaddr, uint32_t u32Value)
{
  EEPROM.put(eeaddr, u32Value);
  EEPROM.put(eeaddr + 4u, ~u32Value);
}
//Reads a uint32_t from EEPROM and checks if the next uint32_t is the complementary
//eeaddr: EEPROM address
//u32Value: Read value, by reference.
//return: True if the next position in EEPROM had the complementary value
bool eeReadU32(int eeaddr, uint32_t &u32Value)
{
  uint32_t u32CheckValue = 0;
  EEPROM.get(eeaddr, u32Value);
  EEPROM.get(eeaddr + 4u, u32CheckValue);  
  u32CheckValue = ~u32CheckValue;
  return u32Value == u32CheckValue; //True if checksum was ok
}
//Stores a float in EEPROM.
//eeaddr: EEPROM address.
//fValue: Value to store.
void eeStoreFloat(int eeaddr, float fValue)
{
  EEPROM.put(eeaddr, fValue);
}
//Reads a float from EEPROM.
//eeaddr: EEPROM address.
//return: The value stored in eeprom.
float eeReadFloat(int eeaddr)
{
  float fValue;
  EEPROM.get(eeaddr, fValue);
  return fValue;
}
//Return true if the mode stored in EEPROM is HOLD, if the mode.
//position is corrupted, puts the EEPROM mode in RUN.
bool eeIsHoldMode(void)
{
  bool bRet = false;
  uint8_t u8Mode;
  if(eeReadU8(EE_U8MODE, u8Mode))
  {
    if(MODEHOLD == u8Mode)
    {
      bRet = true;
    }
  }
  else
  {
    eeStoreU8(EE_U8MODE, MODERUN);
  }
  return bRet;
}
//Sets the mode
//u8Mode: Mode RUN: u8Mode = MODERUN, Mode HOLD: u8Mode = MODEHOLD
void eeSetMode(uint8_t u8Mode)
{
  eeStoreU8(EE_U8MODE, u8Mode);
}
//Return the current mode stored in EEPROM, if corrupted sets mode RUN
//return: Mode RUN: u8Mode = MODERUN, Mode HOLD: u8Mode = MODEHOLD, etc..
uint8_t eeGetMode(void)
{
  uint8_t u8Ret = MODERUN;
  uint8_t u8Mode;
  
  if(eeReadU8(EE_U8MODE, u8Mode))
  {
    u8Ret = u8Mode;
  }
  else
  {
    eeStoreU8(EE_U8MODE, MODERUN);
  }
  return u8Ret;
}
//Based on the current scale and the current measured value of resistance returns
//the next scale. AMTHRESH and AMTHRESL must be defined.
//u8Scale: Current scale.
//u32Measurement: Last resistance value measured.
//Returns: Next suitable scale.
uint8_t getNextScaleAuto(uint8_t u8Scale, uint32_t u32Measurement)
{
  uint8_t u8RetScale = u8Scale;
  if(u32CurrentMeasurement > (AMTHRESH))
  {
    if(u8CurrentScale == 0)
    {
      u8RetScale = 1;
    }  
    else if(u8CurrentScale == 1)
    {
      u8RetScale = 2;
    }
  }
  else if(u32CurrentMeasurement < (AMTHRESL))
  {
    if(u8CurrentScale == 2)
    {
      u8RetScale = 1;
    }  
    else if(u8CurrentScale == 1)
    {
      u8RetScale = 0;
    }         
  }  
  return u8RetScale;
}
//Swap two bytes
void swap(uint32_t *xp, uint32_t *yp)
{
    uint32_t temp = *xp;
    *xp = *yp;
    *yp = temp;
}
 
//Bubble sort of an uint32_t array. Uses swap().
//arr[]: array to be sorted.
//n: number of elements of arr[].
void bubbleSort(uint32_t arr[], uint16_t n)
{
   uint16_t i, j;
   for (i = 0; i < n-1; i++)
  {      
    // Last i elements are already in place   
    for (j = 0; j < n-i-1; j++) 
    {
       if (arr[j] > arr[j+1])
       {
          swap(&arr[j], &arr[j+1]);
       }
    }
  }
}

//Generates the calibration factor and stores it in EEPROM for a given resistance 
//value, taking into account the current scale. Updates the "EEPROM updated tokens".
//u32Resistance: Resistance value to be used to generate de calibration factor.
void setCalibratedFactor(uint32_t u32Resistance)
{
  float fConstant = 0;
  eeReadU8(EE_U8SCALERUN, u8CurrentScale);  /*Read the scale*/
  switch(u8CurrentScale)                    /*Read the values for this scale*/
  {
    case 0u:
    {
      fConstant = RCAL0M1/((float)u32Resistance);
      eeStoreU8(EE_U8CC0M1OK , CALOKTOKEN);
      eeStoreFloat(EE_FCCONST0M1, fConstant);
      break;
    }
    case 1u:
    {
      fConstant = RCAL1M0/((float)u32Resistance);
      eeStoreU8(EE_U8CC1M0OK , CALOKTOKEN);
      eeStoreFloat(EE_FCCONST1M0, fConstant);
      break;
    }
    case 2u:
    {
      fConstant = RCAL10M/((float)u32Resistance);
      eeStoreU8(EE_U8CC10MOK , CALOKTOKEN);
      eeStoreFloat(EE_FCCONST10M, fConstant);
      break;
    }
    default:
    {
        break;
    }
  }
}
//Returns the calibration factor for the current scale.
float getCalibratedFactor(void)
{
  uint8_t u8CurrentScale = 0;
  uint8_t u8Token = 0;
  float fValue = 0;
  float fRetVal = 1.0;
  eeReadU8(EE_U8SCALERUN, u8CurrentScale);  /*Read the scale*/
  switch(u8CurrentScale)                    /*Read the values for this scale*/
  {
    case 0u:
    {
        eeReadU8(EE_U8CC0M1OK, u8Token);
        fValue = eeReadFloat(EE_FCCONST0M1);
        break;
    }
    case 1u:
    {
        eeReadU8(EE_U8CC1M0OK, u8Token);
        fValue = eeReadFloat(EE_FCCONST1M0);
        break;
    }
    case 2u:
    {
        eeReadU8(EE_U8CC10MOK, u8Token);
        fValue = eeReadFloat(EE_FCCONST10M);
        Serial.println(fValue);
        break;
    }
    default:
    {
        break;
    }
  }
  if(CALOKTOKEN == u8Token)
  {
    fRetVal = fValue; /*If token OK, load the factor*/
  }
  return fRetVal;
}
//"Calibration machine", if bStart == true, starts the sequence. While the calibration is
//going on the return value is always false. The function must be called periodically until
//the return value is true. The function gets 5 values of the resistance in the current scale
//and then obtain the median value, after it 
bool calibrate(bool bStart)
{
  static uint32_t au32CalValues[5u];
  static uint64_t u64CalTimer = 0;
  static uint8_t u8CalValuesIndex = 0;
  bool bEnded = false;

  if(true == bStart)
  {/*Initialize*/
      u64CalTimer = millis() + 500u;
      u8CalValuesIndex = 0;
  }
  if(5u <= u8CalValuesIndex)
  {/*Calibration ended*/
    bEnded = true;    
  }
  else
  {
    if(millis() > u64CalTimer)
    {/*Next step*/
      u64CalTimer = millis() + 500u;
      au32CalValues[u8CalValuesIndex] = readResistance(false); /*Get a new value without calibration*/
      u8CalValuesIndex++;
      if(5u <= u8CalValuesIndex)
      { /*All the values gathered*/
        bEnded = true;
        bubbleSort(au32CalValues, 5); /*Sort the values*/
        setCalibratedFactor(au32CalValues[2]); /*Calculate and store the calibration factor*/
      }
    }
  }
  return bEnded;
}
//Main state machine
//u16Tick: Execution tick.
//sInputs_t: Last value of pushbuttons.
void machine(uint16_t u16Tick, sInputs_t &sInputValues)
{
  static uint64_t u64NextTick = 0;
  static bool bLastScale = false;
  static bool bLastCal = false; 
  static uint64_t u64TimerAutoScale = 0;
  static uint64_t u64TimerCalibration = 0;
  uint8_t u8State = MODERUN;
  uint8_t u8RunScale = 0;
  static uint8_t u8CalCounter = 0;
  
  if(millis() > u64NextTick)
  {
    u64NextTick = millis() + u16Tick;
    /*Get current state*/
    u8State = eeGetMode();
    switch(u8State)
    {
      case MODEHOLD:
      {
        eeReadU8(EE_U8SCALEHOLD, u8CurrentScale);         //Read the HOLD scale
        eeReadU32(EE_U32MEASHOLD, u32CurrentMeasurement); //Read the HOLD measurement 

        if((false == sInputValues.bScale) && (true == bLastScale))
        {
          eeSetMode(MODERUN);
        }
        break;
      }
      case MODERUN:
      {
        eeReadU8(EE_U8SCALERUN, u8CurrentScale);          //Read the RUN scale
        changeScale(u8CurrentScale);                      //Set the RUN scale in the current sink
        //Read the ADC if a new measurement is available. 
        u32CurrentMeasurement = readResistance(USING_CALIBRATION);

        if((true == sInputValues.bScale) && (false == bLastScale))
        {
          
        }
        if(false == sInputValues.bScale)
        {
          u64TimerCalibration = millis() + 2000;
        }
        if((millis() > u64TimerCalibration) && (true == sInputValues.bScale))
        {
          u8CalCounter = 0; /*test*/
          eeSetMode(MODEECAL);
        }
        if((false == sInputValues.bScale) && (true == bLastScale))
        {
          if(true == eeReadU8(EE_U8SCALERUN , u8RunScale))   //Read the latest scale value
          { //Stored value was OK
            u8RunScale++;                           //Increment 0 - 1 - 2
            if(2u < u8RunScale)
            {
              eeSetMode(MODEAUTO);
              u8CurrentScale = 0;
              u8RunScale = 0;
            }     
          }
          else
          { //Stored value was Corruped
            u8RunScale = 0;
          }
          eeStoreU8(EE_U8SCALERUN , u8RunScale);  //Store the new value of scale
        }
        if(true == sInputValues.bCal)
        {
          if(false == eeReadU8(EE_U8SCALERUN , u8RunScale))
          {
            eeStoreU8(EE_U8SCALERUN , 0);
          }
          eeStoreU8(EE_U8SCALEHOLD, u8RunScale);
          eeStoreU32(EE_U32MEASHOLD, u32CurrentMeasurement);
          eeSetMode(MODEHOLD);
        }
        break;
      }
      case MODEECAL:
      {
        if((true == sInputValues.bScale) && (false == bLastScale))
        {
          calibrate(true);
          eeSetMode(MODECAL);
        }
        if((false == sInputValues.bCal) && (true == bLastCal))
        {
          eeSetMode(MODERUN);
        }
        break;
      }
      case MODECAL:
      {
        if(true == calibrate(false))
        {
          eeSetMode(MODERUN);
        }
        break;
      }
      case MODEAUTO:
      {
        changeScale(u8CurrentScale);                     
        //Read the ADC if a new measurement is available. 
        u32CurrentMeasurement = readResistance(USING_CALIBRATION);

        if(millis() > u64TimerAutoScale)
        {
          u64TimerAutoScale =  millis() + AMTIME;
          u8CurrentScale = getNextScaleAuto(u8CurrentScale, u32CurrentMeasurement);
        }

        if((false == sInputValues.bScale) && (true == bLastScale))
        {
          eeSetMode(MODERUN);
        }
        bLastScale = sInputValues.bScale;
        
        break;
      }
      default:
      {
        eeSetMode(MODERUN);
        break;
      }
    }
    bLastScale = sInputValues.bScale;
    bLastCal = sInputValues.bCal;
  }
}
//EOF
