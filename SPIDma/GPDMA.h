/* Copyright (C) 2016  Adam Green (https://github.com/adamgreen)

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#ifndef DMA_H
#define DMA_H

#define DMACCxCONTROL_TRANSFER_SIZE_MASK    0xFFF
#define DMACCxCONTROL_SBSIZE_SHIFT          12
#define DMACCxCONTROL_DBSIZE_SHIFT          15
#define DMACCxCONTROL_BURSTSIZE_1           0
#define DMACCxCONTROL_BURSTSIZE_4           1
#define DMACCxCONTROL_BURSTSIZE_8           2
#define DMACCxCONTROL_BURSTSIZE_16          3
#define DMACCxCONTROL_BURSTSIZE_32          4
#define DMACCxCONTROL_BURSTSIZE_64          5
#define DMACCxCONTROL_BURSTSIZE_128         6
#define DMACCxCONTROL_BURSTSIZE_256         7
#define DMACCxCONTROL_I                     (1 << 31)
#define DMACCxCONTROL_SI                    (1 << 26)
#define DMACCxCONTROL_DI                    (1 << 27)
#define DMACCxCONFIG_ENABLE                 (1 << 0)
#define DMACCxCONFIG_SRC_PERIPHERAL_SHIFT   1
#define DMACCxCONFIG_DEST_PERIPHERAL_SHIFT  6
#define DMACCxCONFIG_TRANSFER_TYPE_SHIFT    11
#define DMACCxCONFIG_IE                     (1 << 14)
#define DMACCxCONFIG_ITC                    (1 << 15)
#define DMACCxCONFIG_ACTIVE                 (1 << 17)
#define DMACCxCONFIG_HALT                   (1 << 18)
#define DMA_PERIPHERAL_SSP0_TX              0
#define DMA_PERIPHERAL_SSP0_RX              1
#define DMA_PERIPHERAL_SSP1_TX              2
#define DMA_PERIPHERAL_SSP1_RX              3
#define DMA_PERIPHERAL_ADC                  4
#define DMA_PERIPHERAL_I2S0                 5
#define DMA_PERIPHERAL_I2S1                 6
#define DMA_PERIPHERAL_DAC                  7
#define DMA_PERIPHERAL_UART0TX_MAT0_0       8
#define DMA_PERIPHERAL_UART0RX_MAT0_1       9
#define DMA_PERIPHERAL_UART1TX_MAT1_0       10
#define DMA_PERIPHERAL_UART1RX_MAT1_1       11
#define DMA_PERIPHERAL_UART2TX_MAT2_0       12
#define DMA_PERIPHERAL_UART2RX_MAT2_1       13
#define DMA_PERIPHERAL_UART3TX_MAT3_0       14
#define DMA_PERIPHERAL_UART3RX_MAT3_1       15
#define DMACCxCONFIG_TRANSFER_TYPE_M2M      (0 << DMACCxCONFIG_TRANSFER_TYPE_SHIFT)
#define DMACCxCONFIG_TRANSFER_TYPE_M2P      (1 << DMACCxCONFIG_TRANSFER_TYPE_SHIFT)
#define DMACCxCONFIG_TRANSFER_TYPE_P2M      (2 << DMACCxCONFIG_TRANSFER_TYPE_SHIFT)
#define DMACCxCONFIG_TRANSFER_TYPE_P2P      (3 << DMACCxCONFIG_TRANSFER_TYPE_SHIFT)

typedef enum
{
    GPDMA_CHANNEL0 = 0,
    GPDMA_CHANNEL1 = 1,
    GPDMA_CHANNEL2 = 2,
    GPDMA_CHANNEL3 = 3,
    GPDMA_CHANNEL4 = 4,
    GPDMA_CHANNEL5 = 5,
    GPDMA_CHANNEL6 = 6,
    GPDMA_CHANNEL7 = 7,
    GPDMA_CHANNEL_LOWEST = GPDMA_CHANNEL7,
    GPDMA_CHANNEL_HIGHEST = GPDMA_CHANNEL0,
    GPDMA_CHANNEL_MEM2MEM = GPDMA_CHANNEL_LOWEST,
    GPDMA_CHANNEL_HIGH = -1,                        // Search from 0 up until find unused channel.
    GPDMA_CHANNEL_LOW = 0x7FFFFFFF                  // Search from 6 down until find unused channel.
} DmaDesiredChannel;

static __INLINE void enableGpdmaPower(void)
{
    LPC_SC->PCONP |= (1 << 29);
}

static __INLINE void enableGpdmaInLittleEndianMode(void)
{
    LPC_GPDMA->DMACConfig = 1;
}



#ifdef __cplusplus
extern "C"
{
#endif


extern uint32_t g_dmaChannelsInUse;

int                  allocateDmaChannel(DmaDesiredChannel desiredChannel);
void                 freeDmaChannel(int channel);
LPC_GPDMACH_TypeDef* dmaChannelFromIndex(int index);


#ifdef __cplusplus
}
#endif

#endif /* DMA_H */
