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
/*
    SD Card based file system using DMA based SPI.
    No MMC support as most modern embedded projects use a uSD slot which won't fit MMC media anyway.

    This code is derived from multiple sources:
    SD Specifications Part 1 Physical Layer Simplified Specification Version 4.10
      (https://www.sdcard.org/downloads/pls/pdf/part1_410.pdf)
    Neil Thiessen's update SDFileSystem driver for mbed.
      (https://developer.mbed.org/users/neilt6/code/SDFileSystem/file/d10a519c0910/SDFileSystem.cpp)
    The lpc176x sample from ChaN's, the creator of the FatFS driver, ffsample.zip archive of samples.
      (http://elm-chan.org/fsw/ff/ffsample.zip)
*/
#ifndef SD_FILE_SYSTEM_H
#define SD_FILE_SYSTEM_H

#include <FATFileSystem.h>
#include <SPIDma.h>
#include <CircularLog.h>
#include <stdint.h>

class SDFileSystem : public FATFileSystem
{
public:
    SDFileSystem(PinName mosi, PinName miso, PinName sclk, PinName cs, const char* name);

    // Implementation of FATFileSystem file I/O interface.
    virtual int disk_initialize();
    virtual int disk_status();
    virtual int disk_read(uint8_t* buffer, uint32_t block_number, uint32_t count);
    virtual int disk_write(const uint8_t* buffer, uint32_t block_number, uint32_t count);
    virtual int disk_sync();
    virtual uint32_t disk_sectors();

    // Accessors for SD registers.
    int getCID(uint8_t* pCID, size_t cidSize);
    int getCSD(uint8_t* pCSD, size_t csdSize);
    int getOCR(uint32_t* pOCR);

    // Utility function for extracting bitfields from a byte array (ie. SD Registers).
    static uint32_t extractBits(const uint8_t* p, size_t size, uint32_t lowBit, uint32_t highBit);

    // *** Accessors for diagnostic information. ***
    // The following methods provide access to the circular log file of error text.
    void dumpErrorLog(FILE* pFile)
    {
        m_log.dump(pFile);
    }
    bool isErrorLogEmpty()
    {
        return m_log.isEmpty();
    }
    void clearErrorLog()
    {
        m_log.clear();
    }

    // Count how many times the first SPI exchange in select() was actually required.
    uint32_t selectFirstExchangeRequiredCount()
    {
        return m_selectFirstExchangeRequiredCount;
    }
    // The maximum amount of time waitWhileBusy() loops waiting for device to not be busy writing.
    uint32_t maximumWaitWhileBusyTime()
    {
        return m_maximumWaitWhileBusyTime;
    }
    // The maximum number of times getCommandAndReturnResponse() loops waiting for valid R1 response.
    uint32_t maximumWaitForR1ResponseLoopCount()
    {
        return m_maximumWaitForR1ResponseLoopCount;
    }
    // The maximum number of times a command was retried because of CRC errors.
    uint32_t maximumCRCRetryCount()
    {
        return m_maximumCRCRetryCount;
    }
    // The maximum time it has taken ACMD41 to leave idle state.
    uint32_t maximumACMD41LoopTime()
    {
        return m_maximumACMD41LoopTime;
    }
    // The maximum time it has taken receiveDataBlock() to receive block header byte.
    uint32_t maximumReceiveDataBlockWaitTime()
    {
        return m_maximumReceiveDataBlockWaitTime;
    }
    // The maximum number of read retries for a single block because of receiveDataBlock() failure.
    // Possible causes:
    //  Timed out waiting for data block start token.
    //  Didn't receive expected data block start token.
    //  Datablock failed CRC.
    uint32_t maximumReadRetryCount()
    {
        return m_maximumReadRetryCount;
    }
    // Count how many times the extra SPI exchange for CMD12 was probably required.
    uint32_t cmd12PaddingByteRequiredCount()
    {
        return m_cmd12PaddingByteRequiredCount;
    }
    // The maximum number of write retries for a single block because of transmitDataBlock() failure.
    // Possible causes:
    //  Timed out waiting for card to exit busy state.
    //  Datablock failed CRC.
    uint32_t maximumWriteRetryCount()
    {
        return m_maximumWriteRetryCount;
    }

protected:
    uint8_t     cmd(uint8_t cmd, uint32_t argument = 0, uint32_t* pResponse = NULL);
    bool        select();
    void        deselect();
    bool        waitWhileBusy(uint32_t msecTimeout);
    uint8_t     sendCommandAndGetResponse(uint8_t cmd, uint32_t argument = 0, uint32_t* pResponse = NULL);
    int         sendCommandAndReceiveDataBlock(uint8_t cmd, uint32_t cmdArgument, uint8_t* pBuffer, size_t bufferSize);
    bool        receiveDataBlock(uint8_t* pBuffer, size_t bufferSize);
    uint8_t     transmitDataBlock(uint8_t blockToken, const uint8_t* pBuffer, size_t bufferSize);

    static const char* cmdToString(uint8_t cmd);

    SPIDma                 m_spi;
    Timer                  m_timer;
    Timer                  m_timerOuter;
    int                    m_status;
    uint32_t               m_blockToAddressShift;

    CircularLog<1024, 256> m_log;                                   // Diagnostic
    uint32_t               m_selectFirstExchangeRequiredCount;      // Diagnostic
    uint32_t               m_maximumWaitWhileBusyTime;              // Diagnostic
    uint32_t               m_maximumWaitForR1ResponseLoopCount;     // Diagnostic
    uint32_t               m_maximumCRCRetryCount;                  // Diagnostic
    uint32_t               m_maximumACMD41LoopTime;                 // Diagnostic
    uint32_t               m_maximumReceiveDataBlockWaitTime;       // Diagnostic
    uint32_t               m_maximumReadRetryCount;                 // Diagnostic
    uint32_t               m_cmd12PaddingByteRequiredCount;         // Diagnostic
    uint32_t               m_maximumWriteRetryCount;                // Diagnostic
};

#endif // SD_FILE_SYSTEM_H
