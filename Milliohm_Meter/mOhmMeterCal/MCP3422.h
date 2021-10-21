#ifndef __MCP3422__
#define __MCP3422__

#define MCP3422_CH1 0x00
#define MCP3422_CH2 0x01


//Starts a one shot conversion in MCP3422. Gain and sample rate/nbits are giben in defines.
//u8Channel: Chanel of MCP3422 to be read
//return: true if every thing was ok
bool MCP3422_StartConversion(uint8_t u8Channel);

//Checks if the last conversion in MCP3422 is finished, if so updates the output value.
//u32Value: Output variable for ADC value.
//bIsPositive: Output variable for sign: true = positive
//return: true if the conversion was finished and there is data available
bool MCP3422_IsDataAndRead(uint32_t &u32Value, bool &bIsPositive);

#endif //#ifndef __MCP3422__
//EOF

