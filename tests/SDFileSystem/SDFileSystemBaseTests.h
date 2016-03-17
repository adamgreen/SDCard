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
#ifndef _SD_FILE_SYSTEM_BASE_TESTS_H_
#define _SD_FILE_SYSTEM_BASE_TESTS_H_

#include <string.h>
#include <SDFileSystem.h>
#include <SDCRC.h>
#include <diskio.h>

#include <printfSpy.h>

// Include C++ headers for test harness.
#include "CppUTest/TestHarness.h"

// Derive a class from SDFileSystem to expose protected fields for testing.
class TestSDFileSystem : public SDFileSystem
{
public:
    TestSDFileSystem(PinName mosi, PinName miso, PinName sclk, PinName cs, const char* name)
        : SDFileSystem(mosi, miso, sclk, cs, name)
    {
        m_keepSpiExchangePerSecond = false;
    }

    SPIDma& spi()
    {
        return m_spi;
    }

    uint32_t blockToAddressShift()
    {
        return m_blockToAddressShift;
    }

    uint32_t spiBytesPerSecond()
    {
        return m_spiBytesPerSecond;
    }

    void setSpiBytesPerSecond(uint32_t spiExchanges)
    {
        m_spiBytesPerSecond = spiExchanges;
        m_keepSpiExchangePerSecond = true;
    }

    virtual void setCurrentFrequency(uint32_t spiFrequency)
    {
        uint32_t old = m_spiBytesPerSecond;
        SDFileSystem::setCurrentFrequency(spiFrequency);
        if (m_keepSpiExchangePerSecond)
        {
            m_spiBytesPerSecond = old;
        }
    }



protected:
    bool m_keepSpiExchangePerSecond;
};

#define HIGH 1
#define LOW  0


class SDFileSystemBase : public Utest
{
protected:
    SDFileSystemBase()
        : m_sd(1, 2, 3, 4, "sd")
    {
    }

    void setup()
    {
        m_settingsIndex = 0;
        m_byteIndex = 0;
        printfSpy_Hook(1024);
    }

    void teardown()
    {
        // Make sure that all SPI output was verified.
        LONGS_EQUAL(0, settingsRemaining());
        STRCMP_EQUAL("", m_sd.spi().getOutboundAsString(m_byteIndex, 1));
        // Make sure that all SPI test input was consumed.
        CHECK_TRUE(m_sd.spi().isInboundBufferEmpty());
        printfSpy_Unhook();
    }

    void validateConstructor()
    {
        // Verify initial noinit state.
        LONGS_EQUAL(STA_NOINIT, m_sd.disk_status());

        // Should have updated chip select and format settings.
        CHECK_TRUE(settingsRemaining() >= 2);

        // Verify that chip select is initialized to HIGH (1) state.
        SPIDma::Settings settings = m_sd.spi().getSetting(m_settingsIndex++);
        LONGS_EQUAL(SPIDma::ChipSelect, settings.type);
        LONGS_EQUAL(HIGH, settings.chipSelect);

        // Verify that SPI format was set to 8 bits and mode 0 (polarity/phase).
        settings = m_sd.spi().getSetting(m_settingsIndex++);
        LONGS_EQUAL(SPIDma::Format, settings.type);
        LONGS_EQUAL(8, settings.bits);
        LONGS_EQUAL(0, settings.mode);
        LONGS_EQUAL(0, settings.bytesSentBefore);

        // Verify that card blockToAddress conversion is intialized to 0.
        LONGS_EQUAL(0, m_sd.blockToAddressShift());

        // SPI exchanges per second should be initialized to 0.
        LONGS_EQUAL(0, m_sd.spiBytesPerSecond());
    }

    size_t settingsRemaining()
    {
        return m_sd.spi().getSettingsCount() - m_settingsIndex;
    }

    void setupDataForCmd(const char* pR1Response = "01" /* No errors & in idle state */)
    {
        // select() expects to receive a response which is not 0xFF for the first byte read.
        m_sd.spi().setInboundFromString("00");
        // Return not-busy on first loop in waitForNotBusy().
        m_sd.spi().setInboundFromString("FF");
        // Return indicated R1 response.
        m_sd.spi().setInboundFromString(pR1Response);
    }

    void setupDataForACmd(const char* pR1Response = "01" /* No errors & in idle state */)
    {
        // CMD55 + ACMD code
        setupDataForCmd(pR1Response);
        setupDataForCmd(pR1Response);
    }

    void validate400kHzClockAnd80PrimingClockEdges()
    {
        // Should set frequency and chip select settings at beginning on init process.
        CHECK_TRUE(settingsRemaining() >= 2);

        // Verify 400kHz clock rate for SPI.
        SPIDma::Settings settings = m_sd.spi().getSetting(m_settingsIndex++);
        LONGS_EQUAL(SPIDma::Frequency, settings.type);
        LONGS_EQUAL(400000, settings.frequency);

        // Verify chip select is set high while 80 > 74 clocks are sent to chip during powerup.
        settings = m_sd.spi().getSetting(m_settingsIndex++);
        LONGS_EQUAL(SPIDma::ChipSelect, settings.type);
        LONGS_EQUAL(HIGH, settings.chipSelect);
        STRCMP_EQUAL("FFFFFFFFFFFFFFFF", m_sd.spi().getOutboundAsString(m_byteIndex, 8));
        m_byteIndex += 8;
    }

    void validateCmd(uint8_t expectedCommand, uint32_t expectedArgument = 0, size_t extraResponseBytes=0)
    {
        // Should have set chip select low and then back to high again.
        CHECK_TRUE(settingsRemaining() >= 2);

        validateSelect();
        validateCmdPacket(expectedCommand, expectedArgument, extraResponseBytes);
        validateDeselect();
    }

    void validateACmd(uint8_t expectedCommand, uint32_t expectedArgument = 0, size_t extraResponseBytes=0)
    {
        validateCmd(55);
        validateCmd(expectedCommand, expectedArgument, extraResponseBytes);
    }

    void validateSelect()
    {
        // Should have set chip select low.
        CHECK_TRUE(settingsRemaining() >= 1);

        // Verify select() sequence:
        //  Assert ChipSelect to LOW.
        //  Write 0xFF out SPI to prime card for next command.
        //  Continue to keep writing 0xFF and reading returned token until SPI input no longer registers as busy (0x00).
        //      This will timeout at 500 msec.
        // Assert ChipSelect to LOW.
        SPIDma::Settings settings = m_sd.spi().getSetting(m_settingsIndex++);
        LONGS_EQUAL(SPIDma::ChipSelect, settings.type);
        LONGS_EQUAL(LOW, settings.chipSelect);
        LONGS_EQUAL(m_byteIndex, settings.bytesSentBefore);
        // Should write one 0xFF byte to card to prime it for communication.
        STRCMP_EQUAL("FF", m_sd.spi().getOutboundAsString(m_byteIndex++, 1));
        // Should write 0xFF until 0xFF is received to indicate that the card is no longer busy.
        STRCMP_EQUAL("FF", m_sd.spi().getOutboundAsString(m_byteIndex++, 1));
    }

    void validateCmdPacket(uint8_t expectedCommand, uint32_t expectedArgument = 0, size_t extraResponseBytes=0)
    {
        // Should send 48-bit command packet.
        uint8_t expectedPacket[6] = {0x40 | expectedCommand,
                                     expectedArgument >> 24,
                                     expectedArgument >> 16,
                                     expectedArgument >> 8,
                                     expectedArgument,
                                     0x01};
        expectedPacket[5] |= SDCRC::crc7(expectedPacket, 5) << 1;
        char expectedString[12+1];
        snprintf(expectedString, sizeof(expectedString), "%02X%02X%02X%02X%02X%02X",
                 expectedPacket[0], expectedPacket[1],
                 expectedPacket[2], expectedPacket[3],
                 expectedPacket[4], expectedPacket[5]);
        STRCMP_EQUAL(expectedString, m_sd.spi().getOutboundAsString(m_byteIndex, 6));
        m_byteIndex += 6;

        // CMD12 should perform an extra SPI padding exchange.
        if (expectedCommand == 12)
        {
            STRCMP_EQUAL("FF", m_sd.spi().getOutboundAsString(m_byteIndex++, 1));
        }

        // Should write 0xFF once to obtain non-error R1 response.
        STRCMP_EQUAL("FF", m_sd.spi().getOutboundAsString(m_byteIndex++, 1));

        // Will write 0xFF for the extra response bytes (R7, R2, etc).
        if (extraResponseBytes > 0)
        {
            char extraBytes[extraResponseBytes*2 + 1];
            memset(extraBytes, 'F', sizeof(extraBytes) - 1);
            extraBytes[sizeof(extraBytes) - 1] = '\0';
            STRCMP_EQUAL(extraBytes, m_sd.spi().getOutboundAsString(m_byteIndex, extraResponseBytes));
            m_byteIndex += extraResponseBytes;
        }
    }

    void validateDeselect()
    {
        // Should have set chip select high.
        CHECK_TRUE(settingsRemaining() >= 1);

        // Verify deselect() sequence.
        // Should deassert chip select back to high state now that command is completed.
        SPIDma::Settings settings = m_sd.spi().getSetting(m_settingsIndex++);
        LONGS_EQUAL(SPIDma::ChipSelect, settings.type);
        LONGS_EQUAL(HIGH, settings.chipSelect);
        LONGS_EQUAL(m_byteIndex, settings.bytesSentBefore);
        // Should send 8 additional clock edges (1-byte) after completing instruction with MOSI high.
        STRCMP_EQUAL("FF", m_sd.spi().getOutboundAsString(m_byteIndex++, 1));
    }

    static const char m_hexDigits[];

    void setupDataBlock(uint8_t fillByte, uint32_t size, const char* pCRC = NULL)
    {
        assert ( !pCRC || strlen(pCRC) == 4 );

        // Allocate buffer large enough for 2 hex digits/byte + 4 bytes for CRC + 1 nul terminator.
        char* pAlloc = (char*)malloc(size * 2 + 4 + 1);

        // Calculate the CRC if none was provided.
        char crcCalculated[5];
        if (!pCRC)
        {
            memset(pAlloc, fillByte, size);
            uint16_t crc = SDCRC::crc16((uint8_t*)pAlloc, size);
            snprintf(crcCalculated, sizeof(crcCalculated), "%04X", crc);
            pCRC = crcCalculated;
        }

        // Fill the buffer with supplied hex digits.
        char highNibble = m_hexDigits[fillByte >> 4];
        char lowNibble = m_hexDigits[fillByte & 0xF];
        char* pCurr = pAlloc;
        while (size--)
        {
            *pCurr++ = highNibble;
            *pCurr++ = lowNibble;
        }

        // Append the CRC.
        strcpy(pCurr, pCRC);

        // Store in SPI mock.
        m_sd.spi().setInboundFromString(pAlloc);

        free(pAlloc);
    }

    void setupDataBlock(uint32_t data, const char* pCRC = NULL)
    {
        size_t size = sizeof(data);
        assert ( !pCRC || strlen(pCRC) == 4 );

        // Allocate buffer large enough for 2 hex digits/byte + 4 bytes for CRC + 1 nul terminator.
        char* pAlloc = (char*)malloc(size * 2 + 4 + 1);

        // Calculate the CRC if none was provided.
        char crcCalculated[5];
        if (!pCRC)
        {
            pAlloc[0] = data >> 24;
            pAlloc[1] = data >> 16;
            pAlloc[2] = data >> 8;
            pAlloc[3] = data;
            uint16_t crc = SDCRC::crc16((uint8_t*)pAlloc, size);
            snprintf(crcCalculated, sizeof(crcCalculated), "%04X", crc);
            pCRC = crcCalculated;
        }

        // Fill the buffer with supplied hex digits.
        char* pCurr = pAlloc;
        uint32_t shift = 24;
        for (int i = 0 ; i < 4 ; i++)
        {
            uint8_t byte = data >> shift;
            *pCurr++ = m_hexDigits[byte >> 4];
            *pCurr++ = m_hexDigits[byte & 0xF];
            shift -= 8;
        }

        // Append the CRC.
        strcpy(pCurr, pCRC);

        // Store in SPI mock.
        m_sd.spi().setInboundFromString(pAlloc);

        free(pAlloc);
    }

    void validateDataBlock(uint8_t tokenByte, uint8_t fillByte)
    {
        // Need room for 2 hex digits per byte and a nul terminator.
        // Need a byte for:
        //  Block token byte
        //  512 buffer bytes
        //  2 CRC bytes
        char buffer[2*(1 + 512 + 2) + 1];

        // First fill buffer with raw bytes to calculate CRC.
        memset(buffer, fillByte, 512);
        uint16_t crc = SDCRC::crc16((uint8_t*)buffer, 512);

        // Place block token byte first.
        char* pCurr = buffer;
        *pCurr++ = m_hexDigits[tokenByte >> 4];
        *pCurr++ = m_hexDigits[tokenByte & 0xF];

        // Fill the buffer with supplied hex digits.
        char highNibble = m_hexDigits[fillByte >> 4];
        char lowNibble = m_hexDigits[fillByte & 0xF];
        for (int i = 0 ; i < 512 ; i++)
        {
            *pCurr++ = highNibble;
            *pCurr++ = lowNibble;
        }

        // Append the CRC.
        snprintf(pCurr, 5, "%04X", crc);

        STRCMP_EQUAL(buffer, m_sd.spi().getOutboundAsString(m_byteIndex, 1 + 512 + 2));
        m_byteIndex += 1 + 512 + 2;

        // Should have sent one 0xFF byte to retrieve write response token.
        validateFFBytes(1);
    }

    void validateFFBytes(size_t size)
    {
        // Allocate buffer large enough for 2 hex digits/byte + 1 nul terminator.
        char* pExpected = (char*)malloc(size * 2 + 1);

        // Fill the buffer with supplied hex digits.
        memset(pExpected, 'F', size * 2);
        pExpected[size * 2] = '\0';

        STRCMP_EQUAL(pExpected, m_sd.spi().getOutboundAsString(m_byteIndex, size));
        m_byteIndex += size;

        free(pExpected);
    }

    void validateBuffer(uint8_t* pBuffer, size_t bufferSize, uint8_t expectedFill)
    {
        // Allocate buffer large enough for 2 hex digits/byte + 1 nul terminator.
        char* pExpected = (char*)malloc(bufferSize * 2 + 1);
        char* pActual = (char*)malloc(bufferSize * 2 + 1);

        // Fill the buffer with supplied hex digits.
        static const char hexDigits[] = "0123456789ABCDEF";
        char highNibble = hexDigits[expectedFill >> 4];
        char lowNibble = hexDigits[expectedFill & 0xF];
        char* p1 = pExpected;
        char* p2 = pActual;
        while (bufferSize--)
        {
            *p1++ = highNibble;
            *p1++ = lowNibble;

            uint8_t curr = *pBuffer++;
            *p2++ = hexDigits[curr >> 4];
            *p2++ = hexDigits[curr & 0xF];
        }
        *p1 = '\0';
        *p2 = '\0';

        STRCMP_EQUAL(pExpected, pActual);
        free(pActual);
        free(pExpected);
    }


    void initSDHC()
    {
        validateConstructor();

        // CMD0 input data.
        setupDataForCmd();
        // CMD59 input data.
        setupDataForCmd();
        // CMD8 input data and R7 response.
        setupDataForCmd();
        m_sd.spi().setInboundFromString("000001AD");
        // CMD58 input data and R3 response (OCR) which is checked for voltage ranges.
        setupDataForCmd();
        m_sd.spi().setInboundFromString("00100000");
        // ACMD41
        // CMD55 input data.
        setupDataForCmd();
        // CMD41 (actually ACMD41 since it was preceded by CMD55. Return 0 to indicate not in idle state anymore.
        setupDataForCmd("00");
        // CMD58 input data and R3 response (OCR) which is checked for high capacity disk.
        // Return with high capacity (CCS) bit set to indicate SDHC/SDXC disk.
        setupDataForCmd();
        m_sd.spi().setInboundFromString("40000000");

        LONGS_EQUAL(0, m_sd.disk_initialize());

        // Verify 400kHz clock rate for SPI.
        // Verify chip select is set high while 80 > 74 clocks are sent to chip during powerup.
        validate400kHzClockAnd80PrimingClockEdges();

        // Should send CMD0 to reset card into idle state.
        validateCmd(0);
        // Should send CMD59 to enable CRC.  The argument should be 0x00000001 to enable it.
        validateCmd(59, 1);
        // Should send CMD8 to determine if the card is SDv2 or SDv1 card.
        // The argument should be 0x1AD to select 2.7 - 3.6V range and us 0xAD as check pattern.
        validateCmd(8, 0x1AD, 4);
        // Should send CMD58 to read OCR register and determine voltage levels supported.
        validateCmd(58, 0, 4);
        // Should send ACMD41 (CMD55 + CMD41) to start init process and leave the idle state.
        // The argument to have bit 30 set to indicate that this host support high capacity disks.
        validateCmd(55);
        validateCmd(41, 0x40000000);
        // Should send CMD58 again to read OCR register to determine if the card is high capacity or not.
        validateCmd(58, 0, 4);

        // Should set frequency at end of init process.
        CHECK_TRUE(settingsRemaining() >= 1);

        // Verify 25MHz clock rate for SPI.
        SPIDma::Settings settings = m_sd.spi().getSetting(m_settingsIndex++);
        LONGS_EQUAL(SPIDma::Frequency, settings.type);
        LONGS_EQUAL(25000000, settings.frequency);

        // Verify no longer in NOINIT state.
        LONGS_EQUAL(0, m_sd.disk_status());
        // Verify that card was detected as high capacity where block addresses are SD read/write address.
        LONGS_EQUAL(0, m_sd.blockToAddressShift());
    }

    void initSDSC()
    {
       validateConstructor();

        // CMD0 input data.
        setupDataForCmd();
        // CMD59 input data.
        setupDataForCmd();
        // CMD8 input data and R7 response.
        setupDataForCmd();
        m_sd.spi().setInboundFromString("000001AD");
        // CMD58 input data and R3 response (OCR) which is checked for voltage ranges.
        setupDataForCmd();
        m_sd.spi().setInboundFromString("00100000");
        // ACMD41
        // CMD55 input data.
        setupDataForCmd();
        // CMD41 (actually ACMD41 since it was preceded by CMD55. Return 0 to indicate not in idle state anymore.
        setupDataForCmd("00");
        // CMD58 input data and R3 response (OCR) which is checked for high capacity disk.
        setupDataForCmd();
        m_sd.spi().setInboundFromString("00000000");
        // CMD16 input data.
        setupDataForCmd();

        LONGS_EQUAL(0, m_sd.disk_initialize());

        // Verify 400kHz clock rate for SPI.
        // Verify chip select is set high while 80 > 74 clocks are sent to chip during powerup.
        validate400kHzClockAnd80PrimingClockEdges();

        // Should send CMD0 to reset card into idle state.
        validateCmd(0);
        // Should send CMD59 to enable CRC.  The argument should be 0x00000001 to enable it.
        validateCmd(59, 1);
        // Should send CMD8 to determine if the card is SDv2 or SDv1 card.
        // The argument should be 0x1AD to select 2.7 - 3.6V range and us 0xAD as check pattern.
        validateCmd(8, 0x1AD, 4);
        // Should send CMD58 to read OCR register and determine voltage levels supported.
        validateCmd(58, 0, 4);
        // Should send ACMD41 (CMD55 + CMD41) to start init process and leave the idle state.
        // The argument to have bit 30 set to indicate that this host support high capacity disks.
        validateCmd(55);
        validateCmd(41, 0x40000000);
        // Should send CMD58 again to read OCR register to determine if the card is high capacity or not.
        validateCmd(58, 0, 4);
        // Should send CMD16 to set the block size to 512 bytes.
        validateCmd(16, 512);

        // Should set frequency at end of init process.
        CHECK_TRUE(settingsRemaining() >= 1);

        // Verify 25MHz clock rate for SPI.
        SPIDma::Settings settings = m_sd.spi().getSetting(m_settingsIndex++);
        LONGS_EQUAL(SPIDma::Frequency, settings.type);
        LONGS_EQUAL(25000000, settings.frequency);

        // Verify no longer in NOINIT state.
        LONGS_EQUAL(0, m_sd.disk_status());
        // Verify that card was detected as low capacity where block addresses need to be converted to 512 bytes per
        // block SD read/write address.
        LONGS_EQUAL(9, m_sd.blockToAddressShift());
    }

    TestSDFileSystem m_sd;
    size_t           m_settingsIndex;
    size_t           m_byteIndex;
};

#endif // _SD_FILE_SYSTEM_BASE_TESTS_H_
