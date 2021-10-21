#include "LCD.h"
#include "hal.h"
#include <LiquidCrystal_I2C.h>  //Library: https://bitbucket.org/fmalpartida/new-liquidcrystal/downloads/
/////////////////////////////////////////////////////////////////////////////////////
//Variables:
/////////////////////////////////////////////////////////////////////////////////////
//Display class
static LiquidCrystal_I2C lcd(0x3F, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE); // Addr, En, Rw, Rs, d4, d5, d6, d7, backlighpin, polarity (As descrived in the ebay link but with 0x3F address instead of 0x20)

//Double buffer.
static char displayBufferA[N_ROWS][N_COLS];
static char displayBufferB[N_ROWS][N_COLS];

/////////////////////////////////////////////////////////////////////////////////////
//Functions:
/////////////////////////////////////////////////////////////////////////////////////

//Setup LCD.
void LCD_Init(void)
{
  uint8_t u8Rows = 0;
  uint8_t u8Columns = 0;
  for (u8Rows = 0; u8Rows < N_ROWS; u8Rows++)
  {
    for (u8Columns = 0; u8Columns < N_COLS; u8Columns++)
    {
      displayBufferA[u8Rows][u8Columns] = ' ';
      displayBufferB[u8Rows][u8Columns] = '#';
    }
  }

  lcd.begin(N_COLS, N_ROWS);
}

void LCD_TurnOnBacklight(void)
{
  lcd.setBacklight(HIGH);
}

void LCD_TurnOffBacklight(void)
{
  lcd.setBacklight(LOW);
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

void LCD_EraseLine(uint8_t u8Row)
{
  if(u8Row < N_ROWS)
  {
    uint8_t u8Columns = 0;
    for (u8Columns = 0; u8Columns < N_COLS; u8Columns++)
    {
      displayBufferA[u8Row][u8Columns] = ' ';
    }
  }
}

void LCD_EraseAll(void)
{
  uint8_t u8Rows = 0;
  uint8_t u8Columns = 0;

  for (u8Rows = 0; u8Rows < N_ROWS; u8Rows++)
  {
    for (u8Columns = 0; u8Columns < N_COLS; u8Columns++)
    {
      displayBufferA[u8Rows][u8Columns] = ' ';
    }
  }
}

void LCD_WriteChar(char cChar, uint8_t u8Row, uint8_t u8Column)
{
  if((u8Row < N_ROWS)&&(u8Column < N_COLS))
  {
     displayBufferA[u8Row][u8Column] = cChar;
  }
}
//Writes a string starting at the specified position.
//u8Line: Line.
//u8Column: Column.
//strText: text to write must be a c string with null terminator.
void LCD_WriteString(uint8_t u8Line, uint8_t u8Column, const char* strText)
{
  uint8_t u8Index = 0;    
  while((strText[u8Index] != '\0') && (N_COLS > (u8Column + u8Index)))
  {
    displayBufferA[u8Line][u8Column + u8Index] = strText[u8Index];
    u8Index++;
  }
}
//Browses displayBufferA. If any character is different in displayBufferA than displayBufferB
//writes it to displayBufferB and the LCD. Updates the possition of the cursor when neccesBary.
void LCD_UpdateDisplay(void)
{
  uint8_t u8Rows = 0;
  uint8_t u8Columns = 0;
  bool bMustSetCuror = true;

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




