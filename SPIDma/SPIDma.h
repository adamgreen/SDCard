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
#ifndef SPI_DMA_H_
#define SPI_DMA_H_

// Only need to set this 1 when running LoopbackTest. It allows the test to peek in and see what reads were discarded.
#define SPIDMA_LOOP_BACK_TEST 0


#include <mbed.h>
#include "GPDMA.h"


class SPIDma : public SPI
{
public:
    SPIDma(PinName mosi, PinName miso, PinName sclk, PinName ssel = NC, int sselInitVal = 1);
    ~SPIDma();

    // Override some of the existing SPI methods for customization.
    void format(int bits, int mode = 0);
    void frequency(int hz = 1000000);

    // Methods that are specific to SPIDma functionality.
    //  Sets the state of the ssel pin.
    void setChipSelect(int state);
    //  Perform a blocking write to the MOSI and read from MISO. Doesn't take advantage of FIFO.
    int  exchange(int data);
    //  Perform a multi-byte read/write using DMA. It is blocking but higher priority interrupts have less impact on
    //  throughput since it takes advantage of DMA and the CPU is just waiting for that to complete.
    //  NOTE: Only supports 8-bit element transfers currently.
    void transfer(const void* pvWrite, size_t writeCount, void* pvRead, size_t readCount);
    //  This is a non-blocking write. The corresponding MOSI data is ignored.
    void send(int data);
    // Waits for all data in the transmit FIFO to be completely sent before returning.
    void waitForCompletion();

#if SPIDMA_LOOP_BACK_TEST
public:
    bool isDiscardedQueueEmpty()
    {
        return (m_enqueue == m_dequeue);
    }
    int dequeueDiscardedRead()
    {
        if (isDiscardedQueueEmpty())
        {
            return -1;
        }
        int retVal = m_discardedQueue[m_dequeue];
        m_dequeue = (m_dequeue + 1) & (sizeof(m_discardedQueue)/sizeof(m_discardedQueue[0]) - 1);
        return retVal;
    }

protected:
    void enqueueDiscardedRead(int value)
    {
        size_t nextIndex = (m_enqueue + 1) & (sizeof(m_discardedQueue)/sizeof(m_discardedQueue[0]) - 1);
        if (nextIndex == m_dequeue)
            // Queue is full.
            return;
        m_discardedQueue[m_enqueue] = value;
        m_enqueue = nextIndex;
    }

    int    m_discardedQueue[512];
    size_t m_enqueue;
    size_t m_dequeue;
#endif // SPIDMA_LOOP_BACK_TEST

private:
    // NOTE: This method shouldn't be called in this implementation. Use exchange() instead.
    virtual int write(int value);

protected:
    void readDiscardedNonBlocking();
    void readDiscardedBlocking();
    int  isReadable();
    int  sspRead();
    void sspWrite(int value);
    int  isWriteable();
    void completeDiscardedReads();
    bool isBusy();

    LPC_GPDMACH_TypeDef*    m_pChannelRx;
    LPC_GPDMACH_TypeDef*    m_pChannelTx;
    DigitalOut              m_cs;
    int                     m_readsToDiscard;
    uint32_t                m_channelRx;
    uint32_t                m_channelTx;
    uint32_t                m_sspRx;
    uint32_t                m_sspTx;
};

#endif /* SPI_DMA_H_ */
