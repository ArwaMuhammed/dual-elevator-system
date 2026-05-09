#include "stm32f401xe.h"
#include "Usart.h"
#include "Gpio.h"
#include "Std_Types.h"

/* =========================================================================
 * USART1 Functions (PA9 / PA10)
 * ========================================================================= */
void Usart1_Init(void) {
    Gpio_Init(GPIO_A, 9, GPIO_AF, GPIO_PUSH_PULL);
    Gpio_Init(GPIO_A, 10, GPIO_AF, GPIO_PUSH_PULL);

    Gpio_SetAF(GPIO_A, 9, GPIO_AF7);
    Gpio_SetAF(GPIO_A, 10, GPIO_AF7);

    USART1->CR1 &= ~(1 << USART_CR1_M_Pos); // 8-bit word length
    USART1->CR2 &= ~(USART_CR2_STOP_Msk);   // 1-stop bit at the end
    USART1->CR1 &= ~(1 << USART_CR1_OVER8_Pos); // 16 over sampling

    USART1->BRR = 0x683; // Baud Rate 9600

    /* Enable Transmission block */
    USART1->CR1 |= (1 << USART_CR1_TE_Pos);
    /* Enable Receive block */
    USART1->CR1 |= (1 << USART_CR1_RE_Pos);
    /* Enable USART1 */
    USART1->CR1 |= (1 << USART_CR1_UE_Pos);
}

uint8 Usart1_TransmitByte(uint8 Byte) {
    if (USART1->SR & USART_SR_TXE_Msk) {
        USART1->DR = Byte;
        while (!(USART1->SR & USART_SR_TC_Msk));
        USART1->SR &= ~(USART_SR_TC_Msk); // Clearing TC bit
        return Tx_OK;
    }
    return Tx_NOK;
}

void Usart1_TransmitString(const char *Str) {
    uint32 i = 0;
    uint8 transmitResult = -1;
    while (Str[i] != '\0') {
        transmitResult = Usart1_TransmitByte(Str[i]);
        if (transmitResult == Tx_OK) {
            i++;
        }
    }
}

uint8 Usart1_RecieveByte(void) {
    while (!(USART1->SR & USART_SR_RXNE_Msk));
    return USART1->DR;
}

/* =========================================================================
 * USART2 Functions (PD5 / PD6)
 * ========================================================================= */
void Usart2_Init(void) {
    /* Configure PD5 (TX) and PD6 (RX) for Alternate Function */
    Gpio_Init(GPIO_D, 5, GPIO_AF, GPIO_PUSH_PULL);
    Gpio_Init(GPIO_D, 6, GPIO_AF, GPIO_PUSH_PULL);

    /* Set Alternate Function 7 (AF7) for USART2 mapping */
    Gpio_SetAF(GPIO_D, 5, GPIO_AF7);
    Gpio_SetAF(GPIO_D, 6, GPIO_AF7);

    USART2->CR1 &= ~(1 << USART_CR1_M_Pos); // 8-bit word length
    USART2->CR2 &= ~(USART_CR2_STOP_Msk);   // 1-stop bit at the end
    USART2->CR1 &= ~(1 << USART_CR1_OVER8_Pos); // 16 over sampling

    /* Assuming 16MHz clock on APB1, same as APB2, keeping the same BRR */
    USART2->BRR = 0x683; // Baud Rate 9600

    /* Enable Transmission block */
    USART2->CR1 |= (1 << USART_CR1_TE_Pos);
    /* Enable Receive block */
    USART2->CR1 |= (1 << USART_CR1_RE_Pos);
    /* Enable USART2 */
    USART2->CR1 |= (1 << USART_CR1_UE_Pos);
}

uint8 Usart2_TransmitByte(uint8 Byte) {
    if (USART2->SR & USART_SR_TXE_Msk) {
        USART2->DR = Byte;
        while (!(USART2->SR & USART_SR_TC_Msk));
        USART2->SR &= ~(USART_SR_TC_Msk); // Clearing TC bit
        return Tx_OK;
    }
    return Tx_NOK;
}

void Usart2_TransmitString(const char *Str) {
    uint32 i = 0;
    uint8 transmitResult = -1;
    while (Str[i] != '\0') {
        transmitResult = Usart2_TransmitByte(Str[i]);
        if (transmitResult == Tx_OK) {
            i++;
        }
    }
}

uint8 Usart2_RecieveByte(void) {
    while (!(USART2->SR & USART_SR_RXNE_Msk));
    return USART2->DR;
}