#ifndef __LCD__
#define __LCD__




//Setup LCD.
void LCD_ALFA_Init(void);
void LCD_TurnOnBacklight(void);
void LCD_TurnOffBacklight(void);
void LCD_EraseLine(uint8_t u8Row);
void LCD_EraseAll(void);
void LCD_WriteChar(char cChar, uint8_t u8Row, uint8_t u8Column);
void LCD_WriteString(uint8_t u8Line, uint8_t u8Column, const char* strText);
void LCD_UpdateDisplay(void);
#endif //#define __LCD__
//EOF

