/**
* Dma.c
 *
 * Generic DMA HAL Driver
 */

#include "Dma.h"

void Dma_Init(void) {
    /* Clock enablement is typically handled in Rcc.c,
       but you can leave this empty or add specific init logic later. */
}

void Dma_ConfigStream(DMA_Stream_TypeDef *Stream,
                      uint8 Channel,
                      DmaDirection_t Direction,
                      uint32 SourceAddr,
                      uint32 DestAddr,
                      uint16 DataLength)
{
    /* 1. Disable the stream before configuring */
    Stream->CR &= ~DMA_SxCR_EN;
    while (Stream->CR & DMA_SxCR_EN); /* Wait until hardware confirms it's off */

    /* 2. Clear previous configuration */
    Stream->CR = 0;

    /* 3. Select Channel */
    Stream->CR |= ((uint32)Channel << DMA_SxCR_CHSEL_Pos);

    /* 4. Set Direction */
    Stream->CR |= ((uint32)Direction << DMA_SxCR_DIR_Pos);

    /* 5. Enable Memory Increment Mode (so it reads the string array one byte at a time) */
    Stream->CR |= DMA_SxCR_MINC;

    /* 6. Set Addresses based on direction */
    if (Direction == DMA_DIR_MEM_TO_PERIPH) {
        Stream->PAR  = DestAddr;
        Stream->M0AR = SourceAddr;
    } else {
        Stream->PAR  = SourceAddr;
        Stream->M0AR = DestAddr;
    }

    /* 7. Set Data Length */
    Stream->NDTR = DataLength;

    /* 8. Clear all interrupt flags for both DMA1 and DMA2 to ensure a clean start */
    /* (Brute force clear for simplicity in this bare-metal application) */
    if ((uint32)Stream >= (uint32)DMA2) {
        DMA2->HIFCR = 0x0F7D0F7D;
        DMA2->LIFCR = 0x0F7D0F7D;
    } else {
        DMA1->HIFCR = 0x0F7D0F7D;
        DMA1->LIFCR = 0x0F7D0F7D;
    }
}

void Dma_StartStream(DMA_Stream_TypeDef *Stream) {
    /* Fire the DMA transfer */
    Stream->CR |= DMA_SxCR_EN;
}