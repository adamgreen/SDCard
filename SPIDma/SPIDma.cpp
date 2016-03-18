/* Copyright 2016 Adam Green (http://mbed.org/users/AdamGreen/)

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
// Class to expose greater SPI functionality than the stock SPI class from the mbed SDK:
// * A transfer() method which utilizes DMA to reduce CPU overhead.
// * Separate send() and exchange() methods so that a user only needs to block on SPI reads as needed. The mbed SDK
//   version always blocks and waits for each byte to go over the wire, not taking advantage of the FIFO.
#include <assert.h>
#include "SPIDma.h"
#include "GPDMA.h"


// The LPC17xx has an 8 element FIFO.
#define SPI_FIFO_SIZE 8


SPIDma::SPIDma(PinName mosi, PinName miso, PinName sclk, PinName ssel /* = NC */, int sselInitVal /* = 1 */)
    : SPI(mosi, miso, sclk, NC), m_cs(ssel, sselInitVal)
{
    m_readsToDiscard = 0;
    m_byteCount = 0;

    // Setup GPDMA module.
    enableGpdmaPower();
    enableGpdmaInLittleEndianMode();

    m_channelTx = allocateDmaChannel(GPDMA_CHANNEL_LOW);
    m_channelRx = allocateDmaChannel(GPDMA_CHANNEL_LOW);
    m_pChannelRx = dmaChannelFromIndex(m_channelRx);
    m_pChannelTx = dmaChannelFromIndex(m_channelTx);
    m_sspRx = (_spi.spi == (LPC_SSP_TypeDef*)SPI_1) ? DMA_PERIPHERAL_SSP1_RX : DMA_PERIPHERAL_SSP0_RX;
    m_sspTx = (_spi.spi == (LPC_SSP_TypeDef*)SPI_1) ? DMA_PERIPHERAL_SSP1_TX : DMA_PERIPHERAL_SSP0_TX;

#if SPIDMA_LOOP_BACK_TEST
    m_enqueue = m_dequeue = 0;
    memset(m_discardedQueue, -1, sizeof(m_discardedQueue));
#endif // SPIDMA_LOOP_BACK_TEST
}

SPIDma::~SPIDma()
{
    freeDmaChannel(m_channelTx);
    freeDmaChannel(m_channelRx);
}

void SPIDma::format(int bits, int mode /* = 0 */)
{
    // I have only implemented the DMA transfer routine to support 8-bit elements.
    assert ( bits == 8 );

    waitForCompletion();
    SPI::format(bits, mode);
}

void SPIDma::frequency(int hz /* = 1000000*/)
{
    waitForCompletion();
    SPI::frequency(hz);
}

int SPIDma::write(int value)
{
    // Don't use this method with SPIDma.
    assert ( false );

    return -1;
}

void SPIDma::setChipSelect(int state)
{
    waitForCompletion();
    m_cs = state;
}

void SPIDma::send(int data)
{
    readDiscardedNonBlocking();
    if (m_readsToDiscard >= SPI_FIFO_SIZE)
    {
        assert ( m_readsToDiscard == SPI_FIFO_SIZE );
        readDiscardedBlocking();
    }
    m_readsToDiscard++;
    m_byteCount++;
    sspWrite(data);
}

void SPIDma::readDiscardedNonBlocking()
{
    // Keep reading discarded values until there are no more or the read would block.
    while (m_readsToDiscard > 0 && isReadable())
    {
        int discarded = sspRead();
        m_readsToDiscard--;
#if SPIDMA_LOOP_BACK_TEST
        enqueueDiscardedRead(discarded);
#else
        (void)discarded;
#endif // SPIDMA_LOOP_BACK_TEST
    }
}

void SPIDma::readDiscardedBlocking()
{
    // Block to read one discarded value from the FIFO.
    int discarded = sspRead();
    m_readsToDiscard--;
#if SPIDMA_LOOP_BACK_TEST
    enqueueDiscardedRead(discarded);
#else
    (void)discarded;
#endif // SPIDMA_LOOP_BACK_TEST
}

int SPIDma::isReadable()
{
    return _spi.spi->SR & (1 << 2);
}

int SPIDma::sspRead()
{
    while (!isReadable())
    {
    }
    return _spi.spi->DR;
}

void SPIDma::sspWrite(int value)
{
    while (!isWriteable())
    {
    }
    _spi.spi->DR = value;
}

int SPIDma::isWriteable()
{
    return _spi.spi->SR & (1 << 1);
}

int  SPIDma::exchange(int data)
{
    completeDiscardedReads();
    m_byteCount++;
    sspWrite(data);
    return sspRead();
}

void SPIDma::completeDiscardedReads()
{
    while (m_readsToDiscard > 0)
    {
        readDiscardedBlocking();
    }
}

bool SPIDma::transfer(const void* pvWrite, size_t writeCount, void* pvRead, size_t readCount)
{
    size_t                transferCount = (writeCount > readCount) ? writeCount : readCount;
    size_t                actualReadCount = transferCount;
    int                   readIncrement = (readCount > 1 && pvRead) ? 1 : 0;
    int                   writeIncrement = (writeCount > 1) ? 1 : 0;
    uint32_t              dummyRead = 0;
    bool                  retVal = true;

    // If complete read buffer then we should first pre-fetch any discarded reads so that they don't end up in pvRead.
    if (readCount == transferCount)
    {
        completeDiscardedReads();
    }
    else if (m_readsToDiscard > 0)
    {
        // Just keeps reading into the same byte so don't busy wait, just add to DMA receive count.
        assert ( readIncrement == 0 );
        actualReadCount += m_readsToDiscard;
        m_readsToDiscard = 0;
    }
    m_byteCount += transferCount;

    // Must specify a buffer containing what should be written to SPI.
    // If writeCount is 1 then the single element will be repeatedly sent for each element read.
    assert ( pvWrite && writeCount > 0 );
    // If pvRead is NULL then we will use dummyRead for discarded reads.  The readCount has to be <= 1 though.
    assert ( pvRead || readCount <= 1 );

    // Make sure that the Rx FIFO hasn't already overflown.
    assert ( (_spi.spi->RIS & (1 << 0)) == 0 );

    // Clear error and terminal complete interrupts for both channels.
    uint32_t channelsMask = (1 << m_channelRx) | (1 << m_channelTx);
    LPC_GPDMA->DMACIntTCClear = channelsMask;
    LPC_GPDMA->DMACIntErrClr  = channelsMask;

    // Prep channel to receive the incoming bytes from the SPI device.
    m_pChannelRx->DMACCSrcAddr  = (uint32_t)&_spi.spi->DR;
    m_pChannelRx->DMACCDestAddr = (uint32_t)(pvRead ? pvRead : &dummyRead);
    m_pChannelRx->DMACCLLI      = 0;
    m_pChannelRx->DMACCControl  = DMACCxCONTROL_I |
                                (readIncrement ? DMACCxCONTROL_DI : 0) |
                                (DMACCxCONTROL_BURSTSIZE_4 << DMACCxCONTROL_SBSIZE_SHIFT) |
                                (DMACCxCONTROL_BURSTSIZE_4 << DMACCxCONTROL_DBSIZE_SHIFT) |
                                (actualReadCount & DMACCxCONTROL_TRANSFER_SIZE_MASK);

    // Prep channel to send bytes to the SPI device.
    m_pChannelTx->DMACCSrcAddr  = (uint32_t)pvWrite;
    m_pChannelTx->DMACCDestAddr = (uint32_t)&_spi.spi->DR;
    m_pChannelTx->DMACCLLI      = 0;
    m_pChannelTx->DMACCControl  = DMACCxCONTROL_I |
                     (writeIncrement ? DMACCxCONTROL_SI : 0) |
                     (DMACCxCONTROL_BURSTSIZE_4 << DMACCxCONTROL_SBSIZE_SHIFT) |
                     (DMACCxCONTROL_BURSTSIZE_4 << DMACCxCONTROL_DBSIZE_SHIFT) |
                     (transferCount & DMACCxCONTROL_TRANSFER_SIZE_MASK);

    // Enable receive and transmit channels.
    m_pChannelRx->DMACCConfig = DMACCxCONFIG_ENABLE |
                   (m_sspRx << DMACCxCONFIG_SRC_PERIPHERAL_SHIFT) |
                   DMACCxCONFIG_TRANSFER_TYPE_P2M |
                   DMACCxCONFIG_IE |
                   DMACCxCONFIG_ITC;
    m_pChannelTx->DMACCConfig = DMACCxCONFIG_ENABLE |
                   (m_sspTx << DMACCxCONFIG_DEST_PERIPHERAL_SHIFT) |
                   DMACCxCONFIG_TRANSFER_TYPE_M2P |
                   DMACCxCONFIG_IE |
                   DMACCxCONFIG_ITC;

    // Turn on DMA requests in SSP.
    _spi.spi->DMACR = 0x3;

    // Wait for the DMA transmit to complete.
    while ((LPC_GPDMA->DMACIntStat & (1 << m_channelTx)) == 0)
    {
    }

    // Wait for the DMA receive to complete. End early if Rx FIFO overflowed.
    uint32_t iteration = 0;
    while ((LPC_GPDMA->DMACIntStat & (1 << m_channelRx)) == 0)
    {
        // Check for Rx FIFO overflow every so often. Don't do it all the time since reading SPI peripheral registers
        // too often will slow down the DMA operations on the same peripheral.
        if ((++iteration & (16 - 1)) == 0 && _spi.spi->RIS & (1 << 0))
        {
            // Turn off DMA requests in SSP.
            _spi.spi->DMACR = 0x0;

            // Halt the Rx DMA channel.
            m_pChannelRx->DMACCConfig = DMACCxCONFIG_HALT;
            while (m_pChannelRx->DMACCConfig & DMACCxCONFIG_ACTIVE)
            {
            }

            // Flush any remaining Rx FIFO data.
            waitForCompletion();
            while (isReadable())
            {
                sspRead();
            }

            // Clear the Rx overflow error.
            _spi.spi->ICR = 1 << 0;
            retVal = false;
            break;
        }
    }

    // Turn off DMA requests in SSP.
    _spi.spi->DMACR = 0x0;

    return retVal;
}

void SPIDma::waitForCompletion()
{
    while (isBusy())
    {
    }
    completeDiscardedReads();
}

bool SPIDma::isBusy()
{
    return _spi.spi->SR & (1 << 4);
}

uint32_t SPIDma::getByteCount()
{
    return m_byteCount;
}

void SPIDma::resetByteCount()
{
    m_byteCount = 0;
}
