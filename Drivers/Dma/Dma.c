// #include "Dma.h"
//
// void Dma_Init(void) {
//     /* Clock enablement is typically handled in Rcc.c,
//        but you can leave this empty or add specific init logic later. */
// }
//
// void Dma_ConfigStream(DMA_Stream_TypeDef *Stream,
//                       uint8 Channel,
//                       DmaDirection_t Direction,
//                       uint32 SourceAddr,
//                       uint32 DestAddr,
//                       uint16 DataLength)
// {
//     /* 1. Disable the stream before configuring */
//     Stream->CR &= ~DMA_SxCR_EN;
//     while (Stream->CR & DMA_SxCR_EN); /* Wait until hardware confirms it's off */
//
//     /* 2. Clear previous configuration */
//     Stream->CR = 0;
//
//     /* 3. Select Channel */
//     Stream->CR |= ((uint32)Channel << DMA_SxCR_CHSEL_Pos);
//
//     /* 4. Set Direction */
//     Stream->CR |= ((uint32)Direction << DMA_SxCR_DIR_Pos);
//
//     /* 5. Enable Memory Increment Mode (so it reads the string array one byte at a time) */
//     Stream->CR |= DMA_SxCR_MINC;
//
//     /* 6. Set Addresses based on direction */
//     if (Direction == DMA_DIR_MEM_TO_PERIPH) {
//         Stream->PAR  = DestAddr;
//         Stream->M0AR = SourceAddr;
//     } else {
//         Stream->PAR  = SourceAddr;
//         Stream->M0AR = DestAddr;
//     }
//
//     /* 7. Set Data Length */
//     Stream->NDTR = DataLength;
//
//     /* 8. Clear all interrupt flags for both DMA1 and DMA2 to ensure a clean start */
//     /* (Brute force clear for simplicity in this bare-metal application) */
//     if ((uint32)Stream >= (uint32)DMA2) {
//         DMA2->HIFCR = 0x0F7D0F7D;
//         DMA2->LIFCR = 0x0F7D0F7D;
//     } else {
//         DMA1->HIFCR = 0x0F7D0F7D;
//         DMA1->LIFCR = 0x0F7D0F7D;
//     }
// }
//
// void Dma_StartStream(DMA_Stream_TypeDef *Stream) {
//     /* Fire the DMA transfer */
//     Stream->CR |= DMA_SxCR_EN;
// }
#include "Dma.h"

/* ── Stream6-specific flag masks (from Table 37, Image 2) ── */
#define DMA1_S6_ALL_FLAGS   (DMA_HIFCR_CTCIF6  | \
                             DMA_HIFCR_CHTIF6  | \
                             DMA_HIFCR_CTEIF6  | \
                             DMA_HIFCR_CDMEIF6 | \
                             DMA_HIFCR_CFEIF6)

void Dma_Init(void) { /* Clock enabled in Rcc_Init */ }

void Dma_ConfigStream(DMA_Stream_TypeDef *Stream,
                      uint8              Channel,
                      DmaDirection_t     Direction,
                      uint32             SourceAddr,
                      uint32             DestAddr,
                      uint16             DataLength)
{
    /* 1. Disable stream and wait for HW confirmation */
    Stream->CR &= ~DMA_SxCR_EN;
    while (Stream->CR & DMA_SxCR_EN);

    /* 2. Clear ONLY Stream6 flags — don't touch other streams */
    if (Stream == DMA1_Stream6) {
        DMA1->HIFCR = DMA1_S6_ALL_FLAGS;
    }
    /* Add else-if blocks here for other streams as needed */

    /* 3. Reset CR fully */
    Stream->CR = 0U;

    /* 4. Channel selection */
    Stream->CR |= ((uint32)Channel << DMA_SxCR_CHSEL_Pos);

    /* 5. Data sizes: Byte→Byte (MSIZE=00, PSIZE=00) — explicit */
    Stream->CR &= ~(DMA_SxCR_MSIZE | DMA_SxCR_PSIZE); // 8-bit both

    /* 6. Direction */
    Stream->CR |= ((uint32)Direction << DMA_SxCR_DIR_Pos);

    /* 7. Memory increment ON, Peripheral increment OFF */
    Stream->CR |= DMA_SxCR_MINC;
    Stream->CR &= ~DMA_SxCR_PINC;

    /* 8. Addresses */
    if (Direction == DMA_DIR_MEM_TO_PERIPH) {
        Stream->PAR  = DestAddr;    /* USART2->DR */
        Stream->M0AR = SourceAddr;  /* Buffer     */
    } else {
        Stream->PAR  = SourceAddr;
        Stream->M0AR = DestAddr;
    }

    /* 9. Transfer length */
    Stream->NDTR = DataLength;

    /* 10. Disable direct mode (use FIFO) to avoid DMEIF issues
           FS[1:0]=01 (1/4 full), DMDIS=1                       */
    Stream->FCR  = DMA_SxFCR_DMDIS | (0x1U << DMA_SxFCR_FTH_Pos);
}

void Dma_StartStream(DMA_Stream_TypeDef *Stream)
{
    Stream->CR |= DMA_SxCR_EN;
}