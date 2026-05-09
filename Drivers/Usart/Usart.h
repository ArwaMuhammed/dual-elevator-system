#ifndef USART_H
#define USART_H
#include "Std_Types.h"

#define Tx_OK    0U
#define Tx_NOK   1U

/* USART1 Prototypes */
void Usart1_Init(void);
uint8 Usart1_TransmitByte(uint8 Byte);
uint8 Usart1_RecieveByte(void);
void Usart1_TransmitString(const char* Str);

/* USART2 Prototypes */
void Usart2_Init(void);
uint8 Usart2_TransmitByte(uint8 Byte);
uint8 Usart2_RecieveByte(void);
void Usart2_TransmitString(const char* Str);
void Usart2_TransmitStringDMA(const char* Str);
#endif /* USART_H */