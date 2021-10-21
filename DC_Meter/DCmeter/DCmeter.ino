#include <Wire.h> 
#include <LiquidCrystal_I2C.h>  //Library: https://bitbucket.org/fmalpartida/new-liquidcrystal/downloads/
                                //Display: http://www.ebay.com/itm/Yellow-2004-20X4-IIC-I2C-TWI-Character-LCD-Display-Module-For-Arduino-/172181792364?hash=item2816d5a66c:g:VooAAOSwMVFXIC-3

/////////////////////////////////////////////////////////////////////////////////////
//Defines:
/////////////////////////////////////////////////////////////////////////////////////
//Conditional build:
#define LCD4x20
//#define LCD2x16

//LCD display parameters
#ifdef LCD4x20
  #define N_ROWS 4u
  #define N_COLS 20u
#endif //LCD4x20 
#ifdef LCD2x16
  #define N_ROWS 2u
  #define N_COLS 16u
#endif //LCD4x20
#define LCD_TBL 5000u     //Time that backlight remain active

//#define TURN_OFF_BACKLIGHT

//General parameters
#define MAXVP 2600u           //Maximum OV in 1/100V units
#define MAXCP 3100u           //Maximum OC in 1/1000A units
#define DEBOUNCEHARDNESS 10u  

//Macros
//Relay
#define RELAY_ON    digitalWrite(2, HIGH); \
                    pinMode(2, OUTPUT)
#define RELAY_OFF   digitalWrite(2, LOW); \
                    pinMode(2, OUTPUT)
#define IS_RELAY_ON (HIGH == digitalRead(2))

//Aruduino builtin LED                    
#define STATUS_ON   digitalWrite(LED_BUILTIN, HIGH); \
                    pinMode(LED_BUILTIN, OUTPUT)
#define STATUS_OFF  digitalWrite(LED_BUILTIN, LOW); \
                    pinMode(LED_BUILTIN, OUTPUT)
//MODE Pin                    
#define INIT_MODE   pinMode(12, INPUT)                      //Config MODE pin
#define IS_MODE_CV  (HIGH == digitalRead(12))               //true if mode CV
//ON pushbutton
#define INIT_PON    pinMode(4, INPUT)                       //Config ON pushbutton pin
#define IS_PON_ON   (LOW == digitalRead(4))                 //true if ON pushed
//OFF pushbutton
#define INIT_POFF   pinMode(7, INPUT)                       //Config OFF pushbutton pin
#define IS_POFF_ON  (LOW == digitalRead(7))                 //true if OFF pushed
//OC potentiometer
#define READ_ADC_CP() analogRead(0)
//OV potentiometer
#define READ_ADC_VP() analogRead(1)

//MCP3422
#define MCP3422_CH1 0x00
#define MCP3422_CH2 0x01
//#define MCP3422_SR 0x00 //240 SPS (12bit)
#define MCP3422_SR 0x01 //60 SPS (14bit)
//#define MCP3422_SR 0x02 //15 SPS (16bit)
//#define MCP3422_SR 0x03 //3.75 SPS (18bit)
#define MCP3422_GAIN 0x00 //x1
//#define MCP3422_GAIN 0x01 //x2
//#define MCP3422_GAIN 0x02 //x4
//#define MCP3422_GAIN 0x03 //x8
#define MCP3422_CR_STARTONESHOT 0x80 // /RDY bit = 1, /O/C bit = 0
#define MCP3422_CR_READY 0x80        // /RDY bit mask
#define MCP3422_NCH 2u    //Number of channels available
#define MCP3422_ADD 0x68  //Slave address
#define MCP3422_TP 5u     //Number of msec between pollings to MCP3422
/////////////////////////////////////////////////////////////////////////////////////
//Constants:
/////////////////////////////////////////////////////////////////////////////////////
//Special chars for progress bar
const uint8_t a6x8u8ProgressBarChars[6][8] = 
{
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00},
  {0x00, 0x00, 0x10, 0x10, 0x10, 0x15, 0x00, 0x00},
  {0x00, 0x00, 0x08, 0x08, 0x08, 0x1D, 0x00, 0x00},
  {0x00, 0x00, 0x04, 0x04, 0x04, 0x15, 0x00, 0x00},
  {0x00, 0x00, 0x02, 0x02, 0x02, 0x17, 0x00, 0x00},
  {0x00, 0x00, 0x01, 0x01, 0x01, 0x15, 0x00, 0x00}
};
//Status messages
const char strStatusCC_ON[6]  = {"CC ON"};
const char strStatusCV_ON[6]  = {"CV ON"};
const char strStatusOFF[4]    = {"OFF"};
const char strStatusOV_OFF[7] = {"OV OFF"};
const char strStatusOC_OFF[7] = {"OC OFF"};
/////////////////////////////////////////////////////////////////////////////////////
//Types:
/////////////////////////////////////////////////////////////////////////////////////
//MCP3422 machine states
enum eMCP3422ReadStates_t 
{ 
  eMS_WaitCurrent = 0,
  eMS_WaitVoltage = 1
};
//Status values
enum eStatusValues_t 
{ 
  eStatus_CC_ON = 0,
  eStatus_CV_ON = 1,
  eStatus_OFF = 2,
  eStatus_OV_OFF = 3,
  eStatus_OC_OFF = 4
};
/////////////////////////////////////////////////////////////////////////////////////
//Variables:
/////////////////////////////////////////////////////////////////////////////////////
//Display
//Double buffer..
#ifdef LCD4x20
char displayBufferA[N_ROWS][N_COLS] =  
{
  {'V', 'O', '=', '0', '0', '.', '0', '0', 'V', ' ', ' ', 'C', 'O', '=', '0', '.', '0', '0', '0', 'A'},
  {'0', '0', '0', '0', '0', '0', '0', '0', '0', ' ', ' ', '0', '0', '0', '0', '0', '0', '0', '0', '0'},
  {'O', 'V', 'P', '=', '0', '.', '0', '0', 'V', ' ', ' ', 'O', 'C', 'P', '=', '0', '.', '0', '0', 'A'},
  {'S', 'T', 'A', 'T', 'U', 'S', ':', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'O', 'F', 'F'}
};   
char displayBufferB[N_ROWS][N_COLS]=   
{
  {'#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#'},
  {'#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#'},
  {'#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#'},
  {'#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#'}
};
#endif //LCD4x20
#ifdef LCD2x16
char displayBufferA[N_ROWS][N_COLS] =  
{// 0    1    2    3    4    5    6    7    8    9    10   11   12   13   14   15
  {'V', '=', '0', '0', '.', '0', '0', ' ', '0', '0', '0', '0', '0', '0', '0', '0'},
  {'C', '=', '0', '.', '0', '0', '0', ' ', '0', '0', '0', '0', '0', '0', '0', '0'}
};   
char displayBufferB[N_ROWS][N_COLS]=   
{
  {'#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#'},
  {'#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#'}
};
#endif //LCD4x20

//Display class
#ifdef LCD4x20
LiquidCrystal_I2C lcd(0x3F, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE); // Addr, En, Rw, Rs, d4, d5, d6, d7, backlighpin, polarity (As descrived in the ebay link but with 0x3F address instead of 0x20)
#endif //LCD2x16
#ifdef LCD2x16
LiquidCrystal_I2C lcd(0x3F, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE); // Addr, En, Rw, Rs, d4, d5, d6, d7, backlighpin, polarity (As descrived in the ebay link but with 0x3F address instead of 0x20)
#endif //LCD2x16

uint32_t u32BackLightTimer = 0; //Timer for backlight
bool bBackLightOn = false;      //Backlight state

//MCP3422 state machine
uint32_t u32MCP3422Timer = 0; //Polling timer
eMCP3422ReadStates_t MCP3422States = eMS_WaitCurrent;
//Status value
eStatusValues_t eStatusValue = eStatus_OFF;

//General variables
uint16_t u16VOValue = 0; //Voltage output value
uint16_t u16COValue = 0; //Current output value
uint16_t u16VPValue = 0; //Voltage protect value
uint16_t u16CPValue = 0; //Current protect value
uint16_t a3u16VP[3u] = {0, 0, 0}; //Array for filtering voltage protect
uint16_t a3u16CP[3u] = {0, 0, 0}; //Array for filtering current protect
uint8_t u8Index_a3u16VP = 0; //Index of a3u16VP 
uint8_t u8Index_a3u16CP = 0; //Index of a3u16CP 
bool bOnPressed = false;      //State of On push button
bool bLastOnPressed = false;  //Last state of On push button, used for flange detection
bool bOffPressed = false;     //State of Off push button
bool bLastOffPressed = false; //Last state of Off push button, used for flange detection
uint8_t u8OnDebounce = 0;     //Debounce counter for On push button
uint8_t u8OffDebounce = 0;    //Debounce counter for Off push button
bool bMODECV = false;         //Mode of power supply, true: constant voltage, false: constant current
uint32_t u32MCP3422Value = 0;       //Last conversion value from MCP3422
bool bMCP3422ValIsPositive = true;  //Last conversion sign, in this circuit it's always positive
bool bValuesOK = false;             //Indicates in the main loop if the last execution of MCP3422_Machine updated u16VOValue and u16COValue 
/////////////////////////////////////////////////////////////////////////////////////
//First execution:
/////////////////////////////////////////////////////////////////////////////////////
void setup()
{
  RELAY_OFF;
  setupDisplay();
  Wire.setClock(400000);  //Speed for both display and MCP3422
  restartBackLight();
  MCP3422_MachineInit();
}
/////////////////////////////////////////////////////////////////////////////////////
//Loop..
/////////////////////////////////////////////////////////////////////////////////////
void loop() 
{
  updateInputs(); 
  refreshBackLight();
  bValuesOK  = MCP3422_Machine();
  if(true == bValuesOK)
  {
    updateStatus();
    updateDisplayBuffer();
    updateDisplay();
  }
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
  if((MCP3422_NCH - 1u) < u8Channel)
  {
    bIsOK = false;
  }
  if(true == bIsOK)
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

  if(MCP3422_CR_READY != (MCP3422_CR_READY & a4u8RecData[3]))
  {//RDY bit = 0 -> conversion finished, data is valid
    bIsData = true;
  }
  if(true == bIsData)
  {
    //Formatting according to page 22 of datasheet
    //Mode 12 bit
#if(0x00 == MCP3422_SR) 
    if(0x08 == (0x08 & a4u8RecData[0]))
    {
      bIsPositive = false;
    }
    else
    {
      bIsPositive = true;  
    }
    u32Value = ((a4u8RecData[0] & 0x07) << 8u) | a4u8RecData[1];
    if(false == bIsPositive)
    {
        u32Value = ~u32Value - 1; //Two's complement
    }
#endif //(0x00 == MCP3422_SR) 
    //Mode 14 bit
#if(0x01 == MCP3422_SR) 
    if(0x20 == (0x20 & a4u8RecData[0]))
    {
      bIsPositive = false;
    }
    else
    {
      bIsPositive = true;  
    }
    u32Value = ((a4u8RecData[0] & 0x1F) << 8u) | a4u8RecData[1];
    if(false == bIsPositive)
    {
        u32Value = ~u32Value - 1; //Two's complement
    }
#endif //(0x01 == MCP3422_SR) 
    
    //Mode 16 bit
#if(0x02 == MCP3422_SR) 
    if(0x80 == (0x80 & a4u8RecData[0]))
    {
      bIsPositive = false;
    }
    else
    {
      bIsPositive = true;  
    }
    u32Value = ((a4u8RecData[0] & 0x7F) << 8u) | a4u8RecData[1];
    if(false == bIsPositive)
    {
        u32Value = ~u32Value - 1; //Two's complement
    }
#endif //(0x02 == MCP3422_SR) 
    //Mode 18 bit
#if(0x03 == MCP3422_SR) 
    if(0x02 == (0x02 & a4u8RecData[0]))
    {
      bIsPositive = false;
    }
    else
    {
      bIsPositive = true;  
    }
    u32Value = ((a4u8RecData[0] & 0x01) << 16u) | (a4u8RecData[1] << 8u) | a4u8RecData[2];
    if(false == bIsPositive)
    {
        u32Value = ~u32Value - 1; //Two's complement
    }
#endif //(0x03 == MCP3422_SR)      
  }
  return bIsData;
}
//Inits the MCP3422 state machine, must be executed before the periodic execution of MCP3422_Machine
void MCP3422_MachineInit(void)
{
  MCP3422_StartConversion(MCP3422_CH1);   //Start reading the output current value
  MCP3422States = eMS_WaitCurrent;        //Next state, wait for current to be read
  u32MCP3422Timer = millis();                //Update the counter for first timming
}
//Checks if the last conversion in MCP3422 is finished, if so updates the output value.
//u32Value: Output variable for ADC value.
//bIsPositive: Output variable for sign: true = positive
//return: true if the conversion of voltage and current was finished and there is data available
bool MCP3422_Machine(void)
{
  bool bCurrentVoltageMeasured = false;
  if(millis() > (u32MCP3422Timer + MCP3422_TP))
  {//MCP3422_TP msec elapsed
    u32MCP3422Timer = millis();
    switch(MCP3422States)
    {
      case eMS_WaitCurrent:
      {
        if(MCP3422_IsDataAndRead(u32MCP3422Value, bMCP3422ValIsPositive))
        {//Conversion ended,  value and next state
          u16COValue = (uint16_t)(u32MCP3422Value >> 1);  //Save output current value (dividing by 2 gets 1/1000A units)
          MCP3422_StartConversion(MCP3422_CH2);           //Start reading the output voltage value
          MCP3422States = eMS_WaitVoltage;                //Next state, wait for voltage to be read
        }
        break;
      }
      case eMS_WaitVoltage:
      {
        if(MCP3422_IsDataAndRead(u32MCP3422Value, bMCP3422ValIsPositive))
        {//Conversion ended, update value and next state  
          u16VOValue = (uint16_t)(u32MCP3422Value >> 1);  //Save output voltage value (dividing by 2 gets 1/100V units)
          MCP3422_StartConversion(MCP3422_CH1),           //Start reading the output current value
          MCP3422States = eMS_WaitCurrent;                //Next state, wait for current to be read
          bCurrentVoltageMeasured = true;                 //in this execution u16VOValue and u16COValue are fresh
        }
        break;
      }
    }
  }
  return bCurrentVoltageMeasured;
}
//Updates the state and value of all inputs except MCP3422  
void updateInputs(void)
{
  //Init inputs
  INIT_MODE;
  INIT_PON;
  INIT_POFF;
  //Debouncing of pushbuttons...
  //On
  if(IS_PON_ON)
  {
    if(DEBOUNCEHARDNESS <= u8OnDebounce)
    {//DEBOUNCEHARDNESS times consecutively read true
      bOnPressed = true;
    }
    else
    {
      u8OnDebounce++;  
    }
  }
  else
  {
    u8OnDebounce = 0;
    bOnPressed = false;
  }
  //Off
  if(IS_POFF_ON)
  {
    if(DEBOUNCEHARDNESS <= u8OffDebounce)
    {//DEBOUNCEHARDNESS times consecutively read true
      bOffPressed = true;
    }
    else
    {
      u8OffDebounce++;  
    }
  }
  else
  {
    u8OffDebounce = 0;
    bOffPressed = false;
  }
  //Update mode CC or CV directly reading the input pin
  if(IS_MODE_CV)
  {
    bMODECV = true;   
  }
  else
  {
    bMODECV = false;   
  }
  //Voltage protect. Read the arudino adc input, scale it to MAXVP, update the circular stack of three with the last value.
  a3u16VP[u8Index_a3u16VP] = (uint16_t)((float)(READ_ADC_VP()) * (((float)(MAXVP))/965.0)); //Analog in travel ~(0 - 965)lsb. At 965 -> MAXVP
  if(MAXVP < a3u16VP[u8Index_a3u16VP]) //Limit
  {
    a3u16VP[u8Index_a3u16VP] = MAXVP;
  }
  u8Index_a3u16VP ++;
  if(3u < u8Index_a3u16VP)
  {
    u8Index_a3u16VP = 0;
  }
  //Voltage protect. Read the mid point from the circular stack.
  u16VPValue = getMidPointOfThreeuInt16(a3u16VP);
  
  //Current protect. Read the arudino adc input, scale it to MAXCP, update the circular stack of three with the last value.
  a3u16CP[u8Index_a3u16CP] = (uint16_t)((float)(READ_ADC_CP()) * (((float)(MAXCP))/965.0)); //Analog in travel ~(0 - 965)lsb. At 965 -> MAXCP
  if(MAXCP < a3u16CP[u8Index_a3u16CP]) //Limit
  {
    a3u16CP[u8Index_a3u16CP] = MAXCP;
  }
  u8Index_a3u16CP ++;
  if(3u < u8Index_a3u16CP)
  {
    u8Index_a3u16CP = 0;
  }  
  //Current protect. Read the mid point from the circular stack. 
  u16CPValue = getMidPointOfThreeuInt16(a3u16CP);
}
//Setup LCD i2c address, LCD format, set up progress bar chars.
void setupDisplay(void)
{
  uint8_t charIndex = 0;
  lcd.begin(N_COLS, N_ROWS); 
  //Write progress bar chars.
  for(charIndex = 0; charIndex < 6; charIndex++)
  {
    lcd.createChar(charIndex, (uint8_t *)a6x8u8ProgressBarChars[charIndex]);
  }
}
//Turns the backlight on if it was off and reload the timing
void restartBackLight(void)
{
  if(false == bBackLightOn)
  {//Turn on backlight if it was off
    lcd.setBacklight(HIGH);  
  }
  bBackLightOn = true;
  //Reload the timer
  u32BackLightTimer = millis() + LCD_TBL;
}
//Automatic turn off backlight, must be executed periodically
void refreshBackLight(void)
{
  if(true == bBackLightOn)
  {
    if(millis() > u32BackLightTimer)
    {//Turn off backlight if it was on and the time elapsed
      bBackLightOn = false;  
#ifdef TURN_OFF_BACKLIGHT      
      lcd.setBacklight(LOW);
#endif      
    }
  }
}
//Updates eStatusValue, turns on or off the relay and modifies displayBufferA
//depending on current values of measurements.
void updateStatus(void)
{
  eStatusValues_t eLastStatusValue = eStatusValue;
  if((false == bLastOffPressed) && (true == bOffPressed))
  {//Rising flange in Off pushbutton
    RELAY_OFF;   //Turn off output
    eStatusValue = eStatus_OFF;
    restartBackLight(); //Backlight on with any pulsation of on or off
  }
  else if((false == bLastOnPressed) && (true == bOnPressed))
  {//Rising flange in On pushbutton
    RELAY_ON;   //Turn on output
    restartBackLight(); //Backlight on with any pulsation of on or off
  }
  bLastOnPressed = bOnPressed;    //Update last state
  bLastOffPressed = bOffPressed;  //Update last state

  if(u16VOValue > u16VPValue)
  {//Overvoltage
    eStatusValue = eStatus_OV_OFF;
    RELAY_OFF;   //Turn off output
  }
  if(u16COValue > u16CPValue)
  {//Overvoltage
    eStatusValue = eStatus_OC_OFF;
    RELAY_OFF;   //Turn off output
  }
  if(IS_RELAY_ON)
  {//CC or CV
    if(bMODECV)
    {
      eStatusValue = eStatus_CV_ON;
    }
    else
    {
      eStatusValue = eStatus_CC_ON;
    }
  }
  //Write message in displayBufferA
  switch(eStatusValue)
  {
    case eStatus_OFF:
    {
      writeStringStatus(strStatusOFF);
      break;
    }
    case eStatus_CV_ON:
    {
      writeStringStatus(strStatusCV_ON);
      break;
    }
    case eStatus_CC_ON:
    {
      writeStringStatus(strStatusCC_ON);
      break;
    }
    case eStatus_OV_OFF:
    {
      writeStringStatus(strStatusOV_OFF);
      break;
    }
    case eStatus_OC_OFF:
    {
      writeStringStatus(strStatusOC_OFF);
      break;
    }
  }   
  if(eLastStatusValue != eStatusValue)
  {//Backligh on if there has been a change in status
    restartBackLight();
  }
}
//Modifies displayBufferA with current values of measurements
void updateDisplayBuffer(void)
{
    writeN_DOT_NNN(u16COValue, 0u, 14u);
    writeNN_DOT_NN(u16VOValue, 0u, 3u);
    writeN_DOT_NN(u16CPValue/10u, 2u, 15u);
    writeNN_DOT_N(u16VPValue/10u, 2u, 4u); 
    drawProgressBar(u16CPValue, u16COValue, 1u, 11u, 9u);
    drawProgressBar(u16VPValue, u16VOValue, 1u, 0u, 9u);
}
//Browses displayBufferA. If any character is different in displayBufferA than displayBufferB 
//writes it to displayBufferB and the LCD. Updates the possition of the cursor when neccesBary.
void updateDisplay(void)
{
  uint8_t u8Rows = 0;
  uint8_t u8Columns = 0;
  bool bMustSetCuror = true;
  for(u8Rows = 0; u8Rows < N_ROWS; u8Rows++)
  {
    for(u8Columns = 0; u8Columns < N_COLS; u8Columns++)
    {
      //Enters here for each character in displayBufferA
      if(displayBufferA[u8Rows][u8Columns] != displayBufferB[u8Rows][u8Columns])
      {
        //A new character must be written
        if(true == bMustSetCuror)
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
//Writes strText right justified in the last line.
//strText: text to write must be a c string with null terminator. 
void writeStringStatus(const char* strText)
{
  uint8_t u8Size = 0;
  uint8_t u8DisplayPos = 7u; //Next char after STATUS:
  //1º Clear the text
  for(; N_COLS > u8DisplayPos; u8DisplayPos++)
  {
    displayBufferA[3u][u8DisplayPos] = ' ';
  }
  //2º Write the text
  u8DisplayPos = 19u;         //Starts at the end of the line
  u8Size = strlen(strText);   //Get the size of the string
  do
  {
    u8Size--;
    displayBufferA[3u][u8DisplayPos] = strText[u8Size];
    u8DisplayPos--;
  }while((0 != u8Size) && (7u <= u8DisplayPos)); //Ends at the end of string or before reaching "STATUS:" string
}
//Writes a N.NNN fixed dot number in displayBufferA.
//u16Value: Number to be written, 0 = 0.000 to 9999 = 9.999.
//u8Line: Start possition line in displayBufferA.
//u8Column: Start possition column in displayBufferA.
void writeN_DOT_NNN(uint16_t u16Value, uint8_t u8Line, uint8_t u8Column)
{
  //Limit
  if(9999u < u16Value)
  {
    u16Value = 9999u;
  }
  displayBufferA[u8Line][u8Column + 4] = (u16Value % 10) + 0x30;  //4º digit
  u16Value /= 10u;
  displayBufferA[u8Line][u8Column + 3] = (u16Value % 10) + 0x30;  //3º digit
  u16Value /= 10u;
  displayBufferA[u8Line][u8Column + 2] = (u16Value % 10) + 0x30;  //2º digit
  u16Value /= 10u;
  displayBufferA[u8Line][u8Column + 1] = '.';                     //Dot
  displayBufferA[u8Line][u8Column] = (u16Value % 10) + 0x30;      //1º digit
}
//Writes a NN.NN fixed dot number in displayBufferA.
//u16Value: Number to be written, 0 = 00.00 to 9999 = 99.99.
//u8Line: Start possition line in displayBufferA.
//u8Column: Start possition column in displayBufferA.
void writeNN_DOT_NN(uint16_t u16Value, uint8_t u8Line, uint8_t u8Column)
{
  //Limit
  if(9999u < u16Value)
  {
    u16Value = 9999u;
  }
  displayBufferA[u8Line][u8Column + 4] = (u16Value % 10) + 0x30;  //4º digit
  u16Value /= 10u;
  displayBufferA[u8Line][u8Column + 3] = (u16Value % 10) + 0x30;  //3º digit
  u16Value /= 10u;
  displayBufferA[u8Line][u8Column + 2] = '.';                     //Dot
  displayBufferA[u8Line][u8Column + 1] = (u16Value % 10) + 0x30;  //2º digit
  u16Value /= 10u;
  displayBufferA[u8Line][u8Column] = (u16Value % 10) + 0x30;      //1º digit
}
//Writes a NN.N fixed dot number in displayBufferA.
//u16Value: Number to be written, 0 = 00.0 to 999 = 99.9.
//u8Line: Start possition line in displayBufferA.
//u8Column: Start possition column in displayBufferA.
void writeNN_DOT_N(uint16_t u16Value, uint8_t u8Line, uint8_t u8Column)
{
  //Limit
  if(999u < u16Value)
  {
    u16Value = 999u;
  }
  displayBufferA[u8Line][u8Column + 3] = (u16Value % 10) + 0x30;  //3º digit
  u16Value /= 10u;
  displayBufferA[u8Line][u8Column + 2] = '.';                     //Dot
  displayBufferA[u8Line][u8Column + 1] = (u16Value % 10) + 0x30;  //2º digit
  u16Value /= 10u;
  displayBufferA[u8Line][u8Column] = (u16Value % 10) + 0x30;      //1º digit
  u16Value /= 10u;
}
//Writes a N.NN fixed dot number in displayBufferA.
//u16Value: Number to be written, 0 = 0.00 to 999 = 9.99.
//u8Line: Start possition line in displayBufferA.
//u8Column: Start possition column in displayBufferA.
void writeN_DOT_NN(uint16_t u16Value, uint8_t u8Line, uint8_t u8Column)
{
  //Limit
  if(999u < u16Value)
  {
    u16Value = 999u;
  }
  displayBufferA[u8Line][u8Column + 3] = (u16Value % 10) + 0x30;  //3º digit
  u16Value /= 10u;
  displayBufferA[u8Line][u8Column + 2] = (u16Value % 10) + 0x30;  //2º digit
  u16Value /= 10u;
  displayBufferA[u8Line][u8Column + 1] = '.';                     //Dot
  displayBufferA[u8Line][u8Column] = (u16Value % 10) + 0x30;      //1º digit
  u16Value /= 10u;
}

//Draws a progress bar in displayBufferA.
//u16Top: Full scale value.
//u16Value: Progress relative to u16Top.
//u8Line: Start possition line in displayBufferA.
//u8Column: Start possition column in displayBufferA.
//u8Size: Number of digits used by the progress bar.
void drawProgressBar(uint16_t u16Top, uint16_t u16Value, uint8_t u8Line, uint8_t u8Column, uint8_t u8Size)
{
  // Progress bar characters:
  // 0      1      2      3      4      5
  //                                       
  // 
  //        X       X       X       X       X
  //        X       X       X       X       X
  //        X       X       X       X       X
  // X X X  X X X  X X X  X X X  X X X  X X X
  // 
  //
  uint16_t u16NumberOfPositions = 0;
  uint16_t u16Position = 0; //Position within the current number of positions
  uint8_t u8Digit = 0; //Column where the value is
  uint8_t u8CharIndex = 0; //Position in the digit - special char to put in u8Digit
  uint8_t u8ColIndex = 0; //Column counter

  u16NumberOfPositions = u8Size * 5u; //There are five positions in each character
  u16Position = (uint16_t)(((float)u16Value * (float)u16NumberOfPositions)/(float)u16Top); //Position within the current number of positions
  u8Digit = (uint8_t)(u16Position / 5u); //Column where the value is
  u8CharIndex = 1u + (uint8_t)(u16Position % 5u); //Position in the digit - special char to put in u8Digit
  if((u8Digit >= u8Size) || (0 == u16Top))
  {//Limit for operations with top lower than value
    u8Digit = u8Size - 1;
    u8CharIndex = 5u;
  }
  //First, set the 0 char (flat line) across whole progress bar
  for(u8ColIndex = u8Column; u8ColIndex < (u8Column + u8Size); u8ColIndex++)
  {
    displayBufferA[u8Line][u8ColIndex] = char(0);
  }
  //Second, write the charcter 1, 2, 3, 4 o 5 in the specific position
  displayBufferA [u8Line][u8Column + u8Digit] = char(u8CharIndex);
}
//Returns the mid point of a array of three uint16_t. Useful to reduce noise..
//pa3u16Data input data 
uint16_t getMidPointOfThreeuInt16(uint16_t *pa3u16Data)
{
  uint16_t u16RetVal = 0;
  if(pa3u16Data[0] < pa3u16Data[1])
  {
    if(pa3u16Data[1] < pa3u16Data[2])
    {
      u16RetVal = pa3u16Data[1];
    }
    else
    {
      if(pa3u16Data[0] < pa3u16Data[2])
      {
        u16RetVal = pa3u16Data[2];
      }
      else
      {
        u16RetVal = pa3u16Data[0];
      }         
    }    
  }
  else
  {
    if(pa3u16Data[1] < pa3u16Data[2])
    {
      if(pa3u16Data[0] < pa3u16Data[2])
      {
        u16RetVal = pa3u16Data[0];
      }
      else
      {
        u16RetVal = pa3u16Data[2];
      }       
    }
    else
    {
      u16RetVal = pa3u16Data[1];
    }     
  }
  return u16RetVal;
}

//EOF
