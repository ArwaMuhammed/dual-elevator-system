#ifndef SPI_H
#define SPI_H

#include "Std_Types.h"

#define SPI_SLAVE   0U
#define SPI_MASTER  1U

#define SPI_IDLE_LOW   0U
#define SPI_IDLE_HIGH  1U

#define SPI_SAMPLE_FIRST_TRANSITION   0U
#define SPI_SAMPLE_SECOND_TRANSITION  1U

#define SPI_OK   0U
#define SPI_NOK  1U

void Spi1_Init(uint8 MasterSlave, uint8 ClkPol, uint8 ClkPhase);
uint8 Spi1_TransmitReceiveByte(uint8 TxData, uint8 *RxData);
uint8 Spi1_TransmitReceiveBuffer(uint8 *TxBuffer, uint8 *RxBuffer, uint8 Length);

uint8 Spi1_SlavePreloadByte(uint8 TxData);
uint8 Spi1_SlaveReadByte(uint8 *RxData);

#endif