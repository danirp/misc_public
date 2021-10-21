

void TDA1311A_Init(void)
{
  SPI.begin();      /*SPI port initialization*/
  TDA1311A_SETWS;   
  TDA1311A_DISABLE;
}

void TDA1311A_SetValues(uint16_t u16L, uint16_t u16R)
{
  TDA1311A_ENABLE;
  TDA1311A_CLRWS; 
  SPI.transfer16(u16L);
  TDA1311A_SETWS; 
  SPI.transfer16(u16R);
  TDA1311A_DISABLE;
}


  
  
  // Setup SPI
  SPI.begin();                              // SPI port initialisieren

/* ******************* Die folgenden 3 Funktionsaufrufe werden durch SPI.beginTransaction (..) ersetzt ************* 
  // SPI.setBitOrder(MSBFIRST);             // Wähle SPI 1 Modus (Flankenauswahl) bit order, MSB zuerst
  // SPI.setDataMode(SPI_MODE0);            // Wähle SPI data mode 0
  // SPI.setClockDivider(SPI_CLOCK_DIV1);   // Slow speed (72MHz / 16 = 4.5 MHz SPI_1 speed) nur für STM32F103 nötig
  ******************************************************************************************************************* */

      SPI.beginTransaction (SPISettings (15800000, MSBFIRST, SPI_MODE0)); // Die Zahl gibt die max. Clock des SPI
                                                                          // Bausteines, hier des DAC, in Hz an.
  
      pinMode(WS, OUTPUT);       // D3 Pin zur Kanalauswahl "WS" als Output definieren
