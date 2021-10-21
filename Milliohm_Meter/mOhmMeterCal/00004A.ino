#include "00004A.h"

//Conects the mosfets of the current sink to obtain an output current.
//u8Scae: scale: 0: 156mA, 1: 15.6mA, 2: 1.56mA 
void B00004A_changeScale(uint8_t u8Scale)
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
