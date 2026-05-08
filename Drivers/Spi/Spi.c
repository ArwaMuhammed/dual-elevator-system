#include "stm32f401xe.h"
#include "Gpio.h"
#include "Spi.h"

void Spi1_Init(uint8 MasterSlave, uint8 ClkPol, uint8 ClkPhase)
{
  Gpio_Init(GPIO_B, 3, GPIO_AF, GPIO_PUSH_PULL); /* SCK  */
  Gpio_Init(GPIO_B, 4, GPIO_AF, GPIO_PUSH_PULL); /* MISO */
  Gpio_Init(GPIO_B, 5, GPIO_AF, GPIO_PUSH_PULL); /* MOSI */

  Gpio_SetAF(GPIO_B, 3, GPIO_AF5);
  Gpio_SetAF(GPIO_B, 4, GPIO_AF5);
  Gpio_SetAF(GPIO_B, 5, GPIO_AF5);

  SPI1->CR1 = 0U;

  SPI1->CR1 |= (1U << SPI_CR1_SSM_Pos);
  SPI1->CR1 |= (1U << SPI_CR1_SSI_Pos);

  SPI1->CR1 |= ((uint32)MasterSlave << SPI_CR1_MSTR_Pos);
  SPI1->CR1 |= ((uint32)ClkPol      << SPI_CR1_CPOL_Pos);
  SPI1->CR1 |= ((uint32)ClkPhase    << SPI_CR1_CPHA_Pos);

  SPI1->CR1 &= ~(0x7U << SPI_CR1_BR_Pos);
  SPI1->CR1 |=  (0x3U << SPI_CR1_BR_Pos);

  SPI1->CR1 |= (1U << SPI_CR1_SPE_Pos);
}

uint8 Spi1_TransmitReceiveByte(uint8 TxData, uint8 *RxData)
{
  while (!(SPI1->SR & (1U << SPI_SR_TXE_Pos)))
  {
  }

  *((volatile uint8 *)&SPI1->DR) = TxData;

  while (!(SPI1->SR & (1U << SPI_SR_RXNE_Pos)))
  {
  }

  *RxData = *((volatile uint8 *)&SPI1->DR);

  while (SPI1->SR & (1U << SPI_SR_BSY_Pos))
  {
  }

  return SPI_OK;
}

uint8 Spi1_TransmitReceiveBuffer(uint8 *TxBuffer, uint8 *RxBuffer, uint8 Length)
{
  uint8 i;

  for (i = 0U; i < Length; i++)
  {
    if (Spi1_TransmitReceiveByte(TxBuffer[i], &RxBuffer[i]) != SPI_OK)
    {
      return SPI_NOK;
    }
  }

  return SPI_OK;
}

uint8 Spi1_SlavePreloadByte(uint8 TxData)
{
  if (SPI1->SR & (1U << SPI_SR_TXE_Pos))
  {
    *((volatile uint8 *)&SPI1->DR) = TxData;
    return SPI_OK;
  }

  return SPI_NOK;
}

uint8 Spi1_SlaveReadByte(uint8 *RxData)
{
  if (SPI1->SR & (1U << SPI_SR_RXNE_Pos))
  {
    *RxData = *((volatile uint8 *)&SPI1->DR);
    return SPI_OK;
  }

  return SPI_NOK;
}