#include "stm32f401xe.h"
#include "Gpio.h"
#include "Spi.h"

void Spi1_Init(uint8 MasterSlave, uint8 ClkPol, uint8 ClkPhase)
{
    /* Initialize SCK (PB3), MISO (PB4), and MOSI (PB5) */
    Gpio_Init(GPIO_B, 3, GPIO_AF, GPIO_PUSH_PULL);
    Gpio_Init(GPIO_B, 4, GPIO_AF, GPIO_PUSH_PULL);
    Gpio_Init(GPIO_B, 5, GPIO_AF, GPIO_PUSH_PULL);

    Gpio_SetAF(GPIO_B, 3, GPIO_AF5);
    Gpio_SetAF(GPIO_B, 4, GPIO_AF5);
    Gpio_SetAF(GPIO_B, 5, GPIO_AF5);

    SPI1->CR1 = 0U;

    if (MasterSlave == SPI_MASTER)
    {
        /* Master uses software NSS (controls Slave's PA4 using standard GPIO) */
        SPI1->CR1 |= (1U << SPI_CR1_SSM_Pos);
        SPI1->CR1 |= (1U << SPI_CR1_SSI_Pos);
    }
    else
    {
        /* SLAVE MODE: Hardware NSS on PA4 */
        Gpio_Init(GPIO_A, 4, GPIO_AF, GPIO_PUSH_PULL);
        Gpio_SetAF(GPIO_A, 4, GPIO_AF5);

        SPI1->CR1 &= ~(1U << SPI_CR1_SSM_Pos);
        SPI1->CR1 &= ~(1U << SPI_CR1_SSI_Pos);

        /* 100% NON-BLOCKING: Enable RXNE Interrupt */
        SPI1->CR2 |= (1U << 6);
        NVIC_EnableIRQ(SPI1_IRQn);
        NVIC_SetPriority(SPI1_IRQn, 1);
    }

    SPI1->CR1 &= ~(1U << SPI_CR1_MSTR_Pos);
    SPI1->CR1 |= ((uint32)MasterSlave << SPI_CR1_MSTR_Pos);

    SPI1->CR1 &= ~(1U << SPI_CR1_CPOL_Pos);
    SPI1->CR1 |= ((uint32)ClkPol      << SPI_CR1_CPOL_Pos);

    SPI1->CR1 &= ~(1U << SPI_CR1_CPHA_Pos);
    SPI1->CR1 |= ((uint32)ClkPhase    << SPI_CR1_CPHA_Pos);

    SPI1->CR1 &= ~(0x7U << SPI_CR1_BR_Pos);
    SPI1->CR1 |=  (0x3U << SPI_CR1_BR_Pos);

    SPI1->CR1 |= (1U << SPI_CR1_SPE_Pos);
}
