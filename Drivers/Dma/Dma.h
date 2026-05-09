//
// Created by hp on 09/05/2026.
//

#ifndef STM32_TEMPLATE_DMA_H
#define STM32_TEMPLATE_DMA_H
#include "stm32f401xe.h"
#include "Std_Types.h"

typedef enum {
    DMA_DIR_PERIPH_TO_MEM = 0U,
    DMA_DIR_MEM_TO_PERIPH = 1U,
    DMA_DIR_MEM_TO_MEM    = 2U
} DmaDirection_t;

/* Initializes the DMA subsystem (if needed) */
void Dma_Init(void);

/* Configures a generic DMA stream */
void Dma_ConfigStream(DMA_Stream_TypeDef *Stream,
                      uint8 Channel,
                      DmaDirection_t Direction,
                      uint32 SourceAddr,
                      uint32 DestAddr,
                      uint16 DataLength);

/* Starts the DMA transfer */
void Dma_StartStream(DMA_Stream_TypeDef *Stream);
#endif //STM32_TEMPLATE_DMA_H