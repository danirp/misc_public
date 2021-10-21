#ifndef __00004A__
#define __00004A__

/////////////////////////////////////////////////////////////////////////////////////
//Parameters:
/////////////////////////////////////////////////////////////////////////////////////
//LCD display parameters
#define N_ROWS 2u
#define N_COLS 16u

//General parameters
#define DEBOUNCEHARDNESS 10u      
#define MODE_T_TEST             
#define USING_CALIBRATION true  
//#define CLEAREEATSTART        

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

//EEPROM Tokens
#define MODEHOLD        0x01
#define MODERUN         0x02
#define MODEECAL        0x03
#define MODECAL         0x04
#define MODEAUTO        0x05
#define CALOKTOKEN      0xAA    /*Token indicating value OK for calibration constants*/

//Auto Scale parameters
#define AMTHRESH 1050u 
#define AMTHRESL 95u   
#define AMTIME  1000u

#define RCAL0M1 124750.0
#define RCAL1M0 100000.0
#define RCAL10M 100000.0

/////////////////////////////////////////////////////////////////////////////////////
//Types:
/////////////////////////////////////////////////////////////////////////////////////

struct sInputs_t 
{
  bool bCal = false;
  bool bScale = false;
};
/////////////////////////////////////////////////////////////////////////////////////
//Pin Macros:
/////////////////////////////////////////////////////////////////////////////////////
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

//TDA1311A
#define TDA1311A_SETWS  digitalWrite(3, HIGH); \
  pinMode(3, OUTPUT)
#define TDA1311A_CLRWS  digitalWrite(3, LOW); \
  pinMode(3, OUTPUT)
#define TDA1311A_ENABLE  digitalWrite(A6, LOW); \
  pinMode(A6, OUTPUT)
#define TDA1311A_DISABLE  digitalWrite(A6, HIGH); \
  pinMode(A6, OUTPUT)
  
  



/////////////////////////////////////////////////////////////////////////////////////
//Prototypes:
/////////////////////////////////////////////////////////////////////////////////////
void B00004A_changeScale(uint8_t u8Scale);




#endif //#ifndef __00004A__

