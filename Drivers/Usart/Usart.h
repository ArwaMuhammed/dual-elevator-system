/**
 * Usart.h
 *
 *  Created on: Wed May 24 2023
 *  Author    : Abdullah Darwish
 */

#ifndef USART_H
#define USART_H
#include "Std_Types.h"


#define Tx_OK    0U
#define Tx_NOK   1U

void Usart1_Init(void);

uint8 Usart1_TransmitByte(uint8 Byte);

uint8 Usart1_RecieveByte(void);

void Usart1_TransmitString(const char* Str);

#endif /* USART_H */
