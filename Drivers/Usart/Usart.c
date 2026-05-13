#include "stm32f401xe.h"
#include "Usart.h"
#include "Gpio.h"
#include "Std_Types.h"
#include "Dma.h"
#include <string.h>

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
 * USART2 Functions (PA2 / PA3)
 * ========================================================================= */
void Usart2_Init(void) {
    /* Configure PA2 (TX) and PA3 (RX) for Alternate Function */
    Gpio_Init(GPIO_A, 2, GPIO_AF, GPIO_PUSH_PULL);
    Gpio_Init(GPIO_A, 3, GPIO_AF, GPIO_PUSH_PULL);

    /* Set Alternate Function 7 (AF7) for USART2 mapping */
    Gpio_SetAF(GPIO_A, 2, GPIO_AF7);
    Gpio_SetAF(GPIO_A, 3, GPIO_AF7);

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

    /* Ensure DMA TX is disabled at init */
    USART2->CR3 &= ~USART_CR3_DMAT;
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

/* =========================================================================
 * USART2 DMA Telemetry (DMA1 Stream6, Channel 4)
 * ========================================================================= */

#define USART2_DMA_TX_STREAM        DMA1_Stream6
#define USART2_DMA_TX_CHANNEL       4U
#define USART2_DMA_BUFFER_SIZE      256U

static volatile boolean Usart2DmaBusy = FALSE;
static char Usart2DmaBuffer[USART2_DMA_BUFFER_SIZE];

void DMA1_Stream6_IRQHandler(void)
{
    /* Transfer Complete for Stream6 is in the HIGH interrupt status register */
    if ((DMA1->HISR & DMA_HISR_TCIF6) != 0U)
    {
        /* Clear all Stream6 flags (TC/HT/TE/DME/FE) */
        DMA1->HIFCR = (DMA_HIFCR_CTCIF6 |
                      DMA_HIFCR_CHTIF6 |
                      DMA_HIFCR_CTEIF6 |
                      DMA_HIFCR_CDMEIF6 |
                      DMA_HIFCR_CFEIF6);

        /* Disable the stream and stop USART DMA requests */
        USART2_DMA_TX_STREAM->CR &= ~DMA_SxCR_EN;
        USART2->CR3 &= ~USART_CR3_DMAT;

        Usart2DmaBusy = FALSE;
    }
    else if ((DMA1->HISR & (DMA_HISR_TEIF6 | DMA_HISR_DMEIF6 | DMA_HISR_FEIF6)) != 0U)
    {
        /* On any error, clear flags and release the busy state */
        DMA1->HIFCR = (DMA_HIFCR_CTCIF6 |
                      DMA_HIFCR_CHTIF6 |
                      DMA_HIFCR_CTEIF6 |
                      DMA_HIFCR_CDMEIF6 |
                      DMA_HIFCR_CFEIF6);

        USART2_DMA_TX_STREAM->CR &= ~DMA_SxCR_EN;
        USART2->CR3 &= ~USART_CR3_DMAT;
        Usart2DmaBusy = FALSE;
    }
}

void Usart2_TransmitStringDMA(const char* Str)
{
    uint16 length;

    if (Str == (void*)0) return;
    if (Usart2DmaBusy == TRUE) return;

    /* Use standard strlen and limit the max size securely */
    length = (uint16)strlen(Str);
    if (length > (USART2_DMA_BUFFER_SIZE - 1U)) {
        length = USART2_DMA_BUFFER_SIZE - 1U;
    }

    if (length == 0U) return;

    memcpy(Usart2DmaBuffer, Str, length);
    Usart2DmaBuffer[length] = '\0';

    Usart2DmaBusy = TRUE;

    Dma_ConfigStream(USART2_DMA_TX_STREAM,
                     USART2_DMA_TX_CHANNEL,
                     DMA_DIR_MEM_TO_PERIPH,
                     (uint32)Usart2DmaBuffer,
                     (uint32)&USART2->DR,
                     length);

    /* Enable transfer complete interrupt */
    USART2_DMA_TX_STREAM->CR |= DMA_SxCR_TCIE;

    /* Enable USART2 DMA transmit requests */
    USART2->CR3 |= USART_CR3_DMAT;

    Dma_StartStream(USART2_DMA_TX_STREAM);
}