#include <EEPROM.h>
#include "hal.h"


/////////////////////////////////////////////////////////////////////////////////////
//Defines:
/////////////////////////////////////////////////////////////////////////////////////

//Messages
const char strHOLD[5] = "HOLD";
const char strRUN[4] = "RUN";
const char strAUTO[5] = "AUTO";
const char strCAL[4] = "CAL";
const char strECAL[17] = "CAL?  B:Yes R:No";
const char strScale0m1[6] = "(0m1)";
const char strScale1m0[6] = "(1m0)";
const char strScale10m[6] = "(10m)";

const char strFullScale[9] = "--------";

/////////////////////////////////////////////////////////////////////////////////////
//Variables:
/////////////////////////////////////////////////////////////////////////////////////
//Display

//General variables       
uint8_t u8CurrentScale = 0;           //Current Scale
uint32_t u32CurrentMeasurement = 0;   //Last conversion value from MCP3422

sInputs_t sInputs;                    //Current Inputs

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
  B00004A_changeScale(u8CurrentScale);
  LCD_Init();
  Wire.setClock(40000);                   //Speed for both display and MCP3422
  MCP3422_StartConversion(MCP3422_CH1);   //Necessary for the reading sequence waiting for the chip for completing a measure
  LCD_TurnOnBacklight;                    //Turn on backlight
  Serial.begin(9600);                     //For debugging
}
/////////////////////////////////////////////////////////////////////////////////////
//Loop..
/////////////////////////////////////////////////////////////////////////////////////
void loop()
{
  updateInputs(sInputs);  //Read pushbuttons
  machine(25u, sInputs);  //State machine
  updateDisplayBuffer(50);  //Update display buffer
}
/////////////////////////////////////////////////////////////////////////////////////
//Functions:
/////////////////////////////////////////////////////////////////////////////////////

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



//Modifies displayBufferA with current measurement and mode
void updateDisplayBuffer(uint16_t u16RefreshTime)
{
  uint8_t u8currentMode = MODERUN;
  static uint64_t u64RefreshTime = 0;

  if(millis() > u64RefreshTime)
  {
    u64RefreshTime = millis() + u16RefreshTime;

    LCD_EraseAll();
    
    if(129999 >= u32CurrentMeasurement)
    {
      writeNNNNNN(u8CurrentScale,u32CurrentMeasurement, 0u, 0u);
      LCD_WriteChar(0xF4, 0u, 7u);
    }
    else
    {
      LCD_WriteString(0u, 0u, strFullScale);
    } 
    switch(u8CurrentScale)
    {
      case 0:
      {
        LCD_WriteString(0u, 11u, strScale0m1);
        break;
      }
      case 1:
      {
        LCD_WriteString(0u, 11u, strScale1m0);
        break;
      }
      case 2:
      {
        LCD_WriteString(0u, 11u, strScale10m);
        break;
      }
    }  
    /*Get current state*/
    u8currentMode = eeGetMode();
    switch(u8currentMode)
    {
      case MODERUN:
      {
        LCD_WriteString(1u, 13u, strRUN);
        break;
      }
      case MODEHOLD:
      {
        LCD_WriteString(1u, 12u, strHOLD);                    
        break;
      }
      case MODEAUTO:
      {
        LCD_WriteString(1u, 13u, strRUN); 
        LCD_WriteString(1u, 8u, strAUTO); 
        break;
      }
      case MODECAL:
      {
        LCD_WriteString(1u, 13u, strCAL); 
        if(millis() & 0x0000000000000080)
        {/*aprox each ~255msec*/
          LCD_WriteChar('*', 1u, 0u);
        }
        else
        {
          LCD_WriteChar('*', 1u, 1u);
        }
        break;
      }
      case MODEECAL:
      {
        LCD_WriteString(1u, 0u, strECAL);
        break;
      }
      default:
      {
        break;
      }
    }

    LCD_UpdateDisplay(); //Write the necessary chars in the display.
  }
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
//and then obtain the median value, after it stores the factor.
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
        B00004A_changeScale(u8CurrentScale);                      //Set the RUN scale in the current sink
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
        B00004A_changeScale(u8CurrentScale);                     
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
