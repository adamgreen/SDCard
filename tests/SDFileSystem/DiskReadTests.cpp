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
#include "SDFileSystemBaseTests.h"

TEST_GROUP_BASE(DiskRead,SDFileSystemBase)
{
    void setupDataForCmd12(const char* pR1Response = "01" /* No errors & in idle state */)
    {
        // Return extra padding byte.
        m_sd.spi().setInboundFromString("FF");
        // Return indicated R1 response.
        m_sd.spi().setInboundFromString(pR1Response);
    }
};


TEST(DiskRead, DiskRead_AttemptBeforeInit_ShouldFail_GetLogged)
{
    uint8_t buffer[512];

        LONGS_EQUAL(RES_NOTRDY, m_sd.disk_read(buffer, 42, 1));

    // Only the constructor should have generated any SPI traffic.
    validateConstructor();

    m_sd.dumpErrorLog(stderr);
    char expectedOutput[256];
    snprintf(expectedOutput, sizeof(expectedOutput), "disk_read(%08X,42,1) - Attempt to read uninitialized drive\n",
             (uint32_t)(size_t)buffer);
    STRCMP_EQUAL(expectedOutput, printfSpy_GetLastOutput());
}

TEST(DiskRead, DiskRead_AttemptToRead0Blocks_ShouldFail_GetLogged)
{
    uint8_t buffer[512];

    initSDHC();
        LONGS_EQUAL(RES_PARERR, m_sd.disk_read(buffer, 42, 0));

    m_sd.dumpErrorLog(stderr);
    char expectedOutput[256];
    snprintf(expectedOutput, sizeof(expectedOutput), "disk_read(%08X,42,0) - Attempt to read 0 blocks\n",
             (uint32_t)(size_t)buffer);
    STRCMP_EQUAL(expectedOutput, printfSpy_GetLastOutput());
}

TEST(DiskRead, DiskRead_SingleBlockFromSDHC_ShouldSucceed)
{
    uint8_t buffer[512];

    initSDHC();
    // CMD17 input data.
    setupDataForCmd("00");
    // 0xFE starts read data block.
    m_sd.spi().setInboundFromString("FE");
    // Data block will contain 512 bytes of 0xAD + valid CRC.
    setupDataBlock(0xAD, 512);

    // Clear buffer to 0x00 before reading into it.
    memset(buffer, 0, sizeof(buffer));

        LONGS_EQUAL(RES_OK, m_sd.disk_read(buffer, 42, 1));

    validateSelect();
    // Should send CMD17 to start read process.  Argument is block number.
    validateCmdPacket(17, 42);
    // Should send multiple FF bytes to read in data block:
    //  1 to read in header.
    //  512 to read data.
    //  2 to read CRC.
    validateFFBytes(1+512+2);
    validateDeselect();

    // Should have read 0xAD fill into supplied read buffer.
    validateBuffer(buffer, sizeof(buffer), 0xAD);

    // receiveDataBlock() should only loop through once.
    LONGS_EQUAL(1, m_sd.maximumReceiveDataBlockWaitTime());
    // Should have had to make no read retries.
    LONGS_EQUAL(0, m_sd.maximumReadRetryCount());
}

TEST(DiskRead, DiskRead_SingleBlockFromSDSC_ShouldConvertToByteAddress_ShouldSucceed)
{
    uint8_t buffer[512];

    initSDSC();
    // CMD17 input data.
    setupDataForCmd("00");
    // 0xFE starts read data block.
    m_sd.spi().setInboundFromString("FE");
    // Data block will contain 512 bytes of 0xDA + valid CRC.
    setupDataBlock(0xDA, 512);

    // Clear buffer to 0x00 before reading into it.
    memset(buffer, 0, sizeof(buffer));

        LONGS_EQUAL(RES_OK, m_sd.disk_read(buffer, 42, 1));

    validateSelect();
    // Should send CMD17 to start read process.  Argument is byte address.
    // Note that argument for SDSC should be block number * 512.
    validateCmdPacket(17, 42 * 512);
    // Should send multiple FF bytes to read in data block:
    //  1 to read in header.
    //  512 to read data.
    //  2 to read CRC.
    validateFFBytes(1+512+2);
    validateDeselect();

    // Should have read 0xDA fill into supplied read buffer.
    validateBuffer(buffer, sizeof(buffer), 0xDA);

    // receiveDataBlock() should only loop through once.
    LONGS_EQUAL(1, m_sd.maximumReceiveDataBlockWaitTime());
    // Should have had to make no read retries.
    LONGS_EQUAL(0, m_sd.maximumReadRetryCount());
}

TEST(DiskRead, DiskRead_SingleBlock_SelectTimeout_ShouldFail_GetLogged)
{
    uint8_t buffer[512];

    initSDHC();

    // select() expects to receive a response which is not 0xFF for the first byte read.
    m_sd.spi().setInboundFromString("00");
    // Return busy on two loops through waitForNotBusy().
    m_sd.spi().setInboundFromString("0000");

    // Set timer to elapse 250 msec / call so that second iteration of waitWhileBusy() should timeout.
    m_sd.timer().setElapsedTimePerCall(250);

    // Clear buffer to 0x00 before reading into it.
    memset(buffer, 0, sizeof(buffer));

        LONGS_EQUAL(RES_ERROR, m_sd.disk_read(buffer, 42, 1));

    // Assert ChipSelect to LOW.
    CHECK_TRUE(settingsRemaining() >= 1);
    SPIDma::Settings settings = m_sd.spi().getSetting(m_settingsIndex++);
    LONGS_EQUAL(SPIDma::ChipSelect, settings.type);
    LONGS_EQUAL(LOW, settings.chipSelect);
    LONGS_EQUAL(m_byteIndex, settings.bytesSentBefore);
    // Should write one 0xFF byte to card to prime it for communication.
    STRCMP_EQUAL("FF", m_sd.spi().getOutboundAsString(m_byteIndex++, 1));
    // Should write 0xFF until 0xFF is received to indicate that the card is no longer busy.
    STRCMP_EQUAL("FF", m_sd.spi().getOutboundAsString(m_byteIndex++, 1));
    STRCMP_EQUAL("FF", m_sd.spi().getOutboundAsString(m_byteIndex++, 1));
    // Deselect after detecting error.
    validateDeselect();

    // The 500 msec delay for two cycles should be recorded.
    LONGS_EQUAL(500, m_sd.maximumWaitWhileBusyTime());

    // Read buffer shouldn't have changed.
    validateBuffer(buffer, sizeof(buffer), 0x00);

    // Verify error log output.
    m_sd.dumpErrorLog(stderr);
    char expectedOutput[256];
    snprintf(expectedOutput, sizeof(expectedOutput),
             "waitWhileBusy(500) - Time out. Response=0x00\n"
             "select() - 500 msec time out\n"
             "sendCommandAndReceiveDataBlock(CMD17,%X,%X,512) - Select timed out\n"
             "disk_read(%X,42,1) - Read failed\n",
             42, (uint32_t)(size_t)buffer,
             (uint32_t)(size_t)buffer);
    STRCMP_EQUAL(expectedOutput, printfSpy_GetLastOutput());
}

TEST(DiskRead, DiskRead_SingleBlock_CMD17Error_ShouldFail_GetLogged)
{
    uint8_t buffer[512];

    initSDHC();

    // CMD17 input data.  Fail with Illegal command error.
    setupDataForCmd("04");

    // Clear buffer to 0x00 before reading into it.
    memset(buffer, 0, sizeof(buffer));

        LONGS_EQUAL(RES_ERROR, m_sd.disk_read(buffer, 42, 1));

    validateSelect();
    // Should send CMD17 to start read process.  Argument is block number.
    validateCmdPacket(17, 42);
    validateDeselect();

    // Read buffer shouldn't have changed.
    validateBuffer(buffer, sizeof(buffer), 0x00);

    // Verify error log output.
    m_sd.dumpErrorLog(stderr);
    char expectedOutput[256];
    snprintf(expectedOutput, sizeof(expectedOutput),
             "sendCommandAndReceiveDataBlock(CMD17,%X,%X,512) - CMD17 returned 0x04\n"
             "disk_read(%X,42,1) - Read failed\n",
             42, (uint32_t)(size_t)buffer,
             (uint32_t)(size_t)buffer);
    STRCMP_EQUAL(expectedOutput, printfSpy_GetLastOutput());
}

TEST(DiskRead, DiskRead_SingleBlock_ForceReceiveDataBlockLoopTwice_ShouldSucceed_GetRecorded)
{
    uint8_t buffer[512];

    initSDHC();
    // CMD17 input data.
    setupDataForCmd("00");
    // Return 0xFF once to make receiveDataBlock() loop once and then read 0xFE to start ead data block.
    m_sd.spi().setInboundFromString("FFFE");
    // Data block will contain 512 bytes of 0xAD + valid CRC.
    setupDataBlock(0xAD, 512);

    // Clear buffer to 0x00 before reading into it.
    memset(buffer, 0, sizeof(buffer));

        LONGS_EQUAL(RES_OK, m_sd.disk_read(buffer, 42, 1));

    // Successful attempt.
    validateSelect();
    // Should send CMD17 to start read process.  Argument is block number.
    validateCmdPacket(17, 42);
    // Should send multiple FF bytes to read in data block:
    //  2 to read in header.
    //  512 to read data.
    //  2 to read CRC.
    validateFFBytes(2+512+2);
    validateDeselect();

    // Check maximum wait time.
    LONGS_EQUAL(2, m_sd.maximumReceiveDataBlockWaitTime());

    // Should have read 0xAD fill into supplied read buffer.
    validateBuffer(buffer, sizeof(buffer), 0xAD);
}

TEST(DiskRead, DiskRead_SingleBlock_ForceReceiveDataBlockToTimeout_ShouldRetry_Logged_Recorded)
{
    uint8_t buffer[512];

    initSDHC();

    // Time out this attmept.
    // CMD17 input data.
    setupDataForCmd("00");
    // Return 0xFF twice to make receiveDataBlock() loop and timeout.
    m_sd.spi().setInboundFromString("FFFF");

    // Successful attempt.
    // CMD17 input data.
    setupDataForCmd("00");
    // 0xFE starts read data block.
    m_sd.spi().setInboundFromString("FE");
    // Data block will contain 512 bytes of 0xAD + valid CRC.
    setupDataBlock(0xAD, 512);

    // Bump elapsed time so that two loops should lead to timeout.
    m_sd.timer().setElapsedTimePerCall(250);

    // Clear buffer to 0x00 before reading into it.
    memset(buffer, 0, sizeof(buffer));

        LONGS_EQUAL(RES_OK, m_sd.disk_read(buffer, 42, 1));

    // Failed attempt due to time out.
    validateSelect();
    // Should send CMD17 to start read process.  Argument is block number.
    validateCmdPacket(17, 42);
    // Should send 2 FF bytes when timing out the wait for header.
    validateFFBytes(2);
    validateDeselect();

    // Successful attempt.
    validateSelect();
    // Should send CMD17 to start read process.  Argument is block number.
    validateCmdPacket(17, 42);
    // Should send multiple FF bytes to read in data block:
    //  1 to read in header.
    //  512 to read data.
    //  2 to read CRC.
    validateFFBytes(1+512+2);
    validateDeselect();

    // Check for maximum wait time.
    LONGS_EQUAL(500, m_sd.maximumReceiveDataBlockWaitTime());
    // Check for retry count.
    LONGS_EQUAL(1, m_sd.maximumReadRetryCount());
    // Read buffer should contain new data.
    validateBuffer(buffer, sizeof(buffer), 0xAD);

    // Verify error log output.
    m_sd.dumpErrorLog(stderr);
    char expectedOutput[256];
    snprintf(expectedOutput, sizeof(expectedOutput),
             "receiveDataBlock(%08X,512) - Time out after 500ms\n"
             "sendCommandAndReceiveDataBlock(CMD17,%X,%X,512) - receiveDataBlock failed\n",
             (uint32_t)(size_t)buffer,
             42, (uint32_t)(size_t)buffer);
    STRCMP_EQUAL(expectedOutput, printfSpy_GetLastOutput());
}

TEST(DiskRead, DiskRead_SingleBlock_ForceReceiveDataBlockToTimeout3Times_ShouldFail_Logged_Recorded)
{
    uint8_t buffer[512];

    initSDHC();

    // Time out this attmept.
    // CMD17 input data.
    setupDataForCmd("00");
    // Return 0xFF twice to make receiveDataBlock() loop and timeout.
    m_sd.spi().setInboundFromString("FFFF");

    // Time out this attmept.
    // CMD17 input data.
    setupDataForCmd("00");
    // Return 0xFF twice to make receiveDataBlock() loop and timeout.
    m_sd.spi().setInboundFromString("FFFF");

    // Time out this attmept.
    // CMD17 input data.
    setupDataForCmd("00");
    // Return 0xFF twice to make receiveDataBlock() loop and timeout.
    m_sd.spi().setInboundFromString("FFFF");

    // Bump elapsed time so that two loops should lead to timeout.
    m_sd.timer().setElapsedTimePerCall(250);

    // Clear buffer to 0x00 before reading into it.
    memset(buffer, 0, sizeof(buffer));

        LONGS_EQUAL(RES_ERROR, m_sd.disk_read(buffer, 42, 1));

    // Failed attempt due to time out.
    validateSelect();
    // Should send CMD17 to start read process.  Argument is block number.
    validateCmdPacket(17, 42);
    // Should send 2 FF bytes when timing out the wait for header.
    validateFFBytes(2);
    validateDeselect();

    // Failed attempt due to time out.
    validateSelect();
    // Should send CMD17 to start read process.  Argument is block number.
    validateCmdPacket(17, 42);
    // Should send 2 FF bytes when timing out the wait for header.
    validateFFBytes(2);
    validateDeselect();

    // Failed attempt due to time out.
    validateSelect();
    // Should send CMD17 to start read process.  Argument is block number.
    validateCmdPacket(17, 42);
    // Should send 2 FF bytes when timing out the wait for header.
    validateFFBytes(2);
    validateDeselect();
    // When read exits, it will deselect again, which should cause no issues.
    validateDeselect();

    // Check for maximum wait time.
    LONGS_EQUAL(500, m_sd.maximumReceiveDataBlockWaitTime());
    // Check for retry count.
    LONGS_EQUAL(3, m_sd.maximumReadRetryCount());
    // Read buffer should contain original data.
    validateBuffer(buffer, sizeof(buffer), 0x00);

    // Verify error log output.
    m_sd.dumpErrorLog(stderr);
    char expectedOutput[512];
    snprintf(expectedOutput, sizeof(expectedOutput),
             "receiveDataBlock(%08X,512) - Time out after 500ms\n"
             "sendCommandAndReceiveDataBlock(CMD17,%X,%X,512) - receiveDataBlock failed\n"
             "receiveDataBlock(%08X,512) - Time out after 500ms\n"
             "sendCommandAndReceiveDataBlock(CMD17,%X,%X,512) - receiveDataBlock failed\n"
             "receiveDataBlock(%08X,512) - Time out after 500ms\n"
             "sendCommandAndReceiveDataBlock(CMD17,%X,%X,512) - receiveDataBlock failed\n"
             "disk_read(%X,42,1) - Read failed\n",
             (uint32_t)(size_t)buffer,
             42, (uint32_t)(size_t)buffer,
             (uint32_t)(size_t)buffer,
             42, (uint32_t)(size_t)buffer,
             (uint32_t)(size_t)buffer,
             42, (uint32_t)(size_t)buffer,
             (uint32_t)(size_t)buffer);
    STRCMP_EQUAL(expectedOutput, printfSpy_GetLastOutput());
}

TEST(DiskRead, DiskRead_SingleBlock_FailReceiveDataBlockWithInvalidStartToken_ShouldRetry_Logged_Recorded)
{
    uint8_t buffer[512];

    initSDHC();

    // Fail this attmept.
    // CMD17 input data.
    setupDataForCmd("00");
    // Return 0xFD, the incorrect start token.
    m_sd.spi().setInboundFromString("FD");

    // Successful attempt.
    // CMD17 input data.
    setupDataForCmd("00");
    // 0xFE starts read data block.
    m_sd.spi().setInboundFromString("FE");
    // Data block will contain 512 bytes of 0xAD + valid CRC.
    setupDataBlock(0xAD, 512);

    // Clear buffer to 0x00 before reading into it.
    memset(buffer, 0, sizeof(buffer));

        LONGS_EQUAL(RES_OK, m_sd.disk_read(buffer, 42, 1));

    // Failed attempt due to time out.
    validateSelect();
    // Should send CMD17 to start read process.  Argument is block number.
    validateCmdPacket(17, 42);
    // Should send 1 FF byte when reading the invalid header.
    validateFFBytes(1);
    validateDeselect();

    // Successful attempt.
    validateSelect();
    // Should send CMD17 to start read process.  Argument is block number.
    validateCmdPacket(17, 42);
    // Should send multiple FF bytes to read in data block:
    //  1 to read in header.
    //  512 to read data.
    //  2 to read CRC.
    validateFFBytes(1+512+2);
    validateDeselect();

    // Check for maximum wait time.
    LONGS_EQUAL(1, m_sd.maximumReceiveDataBlockWaitTime());
    // Check for retry count.
    LONGS_EQUAL(1, m_sd.maximumReadRetryCount());
    // Read buffer should contain new data.
    validateBuffer(buffer, sizeof(buffer), 0xAD);

    // Verify error log output.
    m_sd.dumpErrorLog(stderr);
    char expectedOutput[256];
    snprintf(expectedOutput, sizeof(expectedOutput),
             "receiveDataBlock(%08X,512) - Expected 0xFE start block token. Response=0xFD\n"
             "sendCommandAndReceiveDataBlock(CMD17,%X,%X,512) - receiveDataBlock failed\n",
             (uint32_t)(size_t)buffer,
             42,(uint32_t)(size_t)buffer);
    STRCMP_EQUAL(expectedOutput, printfSpy_GetLastOutput());
}

TEST(DiskRead, DiskRead_SingleBlock_FailReceiveDataBlockWithInvalidCRC_ShouldRetry_Logged_Recorded)
{
    uint8_t buffer[512];

    initSDHC();

    // Fail this attmept.
    // CMD17 input data.
    setupDataForCmd("00");
    // 0xFE starts read data block.
    m_sd.spi().setInboundFromString("FE");
    // Data block will contain 512 bytes of 0xAD + invalid CRC.
    setupDataBlock(0xAD, 512, "BAAD");

    // Successful attempt.
    // CMD17 input data.
    setupDataForCmd("00");
    // 0xFE starts read data block.
    m_sd.spi().setInboundFromString("FE");
    // Data block will contain 512 bytes of 0xAD + valid CRC.
    setupDataBlock(0xAD, 512);

    // Clear buffer to 0x00 before reading into it.
    memset(buffer, 0, sizeof(buffer));

        LONGS_EQUAL(RES_OK, m_sd.disk_read(buffer, 42, 1));

    // Failed attempt due to CRC.
    validateSelect();
    // Should send CMD17 to start read process.  Argument is block number.
    validateCmdPacket(17, 42);
    // Should send multiple FF bytes to read in data block:
    //  1 to read in header.
    //  512 to read data.
    //  2 to read CRC.
    validateFFBytes(1+512+2);
    validateDeselect();

    // Successful attempt.
    validateSelect();
    // Should send CMD17 to start read process.  Argument is block number.
    validateCmdPacket(17, 42);
    // Should send multiple FF bytes to read in data block:
    //  1 to read in header.
    //  512 to read data.
    //  2 to read CRC.
    validateFFBytes(1+512+2);
    validateDeselect();

    // Check for maximum wait time.
    LONGS_EQUAL(1, m_sd.maximumReceiveDataBlockWaitTime());
    // Check for retry count.
    LONGS_EQUAL(1, m_sd.maximumReadRetryCount());
    // Read buffer should contain new data.
    validateBuffer(buffer, sizeof(buffer), 0xAD);

    // Verify error log output.
    m_sd.dumpErrorLog(stderr);
    char expectedOutput[256];
    snprintf(expectedOutput, sizeof(expectedOutput),
             "receiveDataBlock(%08X,512) - Invalid CRC. Expected=0xBAAD Actual=0x2F29\n"
             "sendCommandAndReceiveDataBlock(CMD17,%X,%X,512) - receiveDataBlock failed\n",
             (uint32_t)(size_t)buffer,
             42, (uint32_t)(size_t)buffer);
    STRCMP_EQUAL(expectedOutput, printfSpy_GetLastOutput());
}

TEST(DiskRead, DiskRead_MultiBlock_FromSDHC_ShouldSucceed)
{
    uint8_t buffer[1024];

    initSDHC();
    // CMD18 input data.
    setupDataForCmd("00");
    // 0xFE starts read data block.
    m_sd.spi().setInboundFromString("FE");
    // Data block will contain 512 bytes of 0xAD + valid CRC.
    setupDataBlock(0xAD, 512);
    // 0xFE starts read data block.
    m_sd.spi().setInboundFromString("FE");
    // Data block will contain 512 bytes of 0xDA + valid CRC.
    setupDataBlock(0xDA, 512);
    // CMD12 input data.
    setupDataForCmd12("00");

    // Clear buffer to 0x00 before reading into it.
    memset(buffer, 0, sizeof(buffer));

        LONGS_EQUAL(RES_OK, m_sd.disk_read(buffer, 42, 2));

    validateSelect();
    // Should send CMD18 to start read process.  Argument is block number.
    validateCmdPacket(18, 42);
    // Should send multiple FF bytes to read in each data block:
    //  1 to read in header.
    //  512 to read data.
    //  2 to read CRC.
    validateFFBytes(2*(1+512+2));
    // Should send CMD12 to stop read process.
    validateCmdPacket(12);
    validateDeselect();

    // Should have read in data via SPI.
    validateBuffer(buffer, 512, 0xAD);
    validateBuffer(buffer + 512, 512, 0xDA);

    // receiveDataBlock() should only loop through once.
    LONGS_EQUAL(1, m_sd.maximumReceiveDataBlockWaitTime());
    // Should have had to make no read retries.
    LONGS_EQUAL(0, m_sd.maximumReadRetryCount());
    // Should have seen no CMD12 padding bytes that actually needed to be discarded.
    LONGS_EQUAL(0, m_sd.cmd12PaddingByteRequiredCount());
}

TEST(DiskRead, DiskRead_MultiBlock_SelectTimeout_ShouldFail_GetLogged)
{
    uint8_t buffer[1024];

    initSDHC();

    // select() expects to receive a response which is not 0xFF for the first byte read.
    m_sd.spi().setInboundFromString("00");
    // Return busy on two loops through waitForNotBusy().
    m_sd.spi().setInboundFromString("0000");

    // Set timer to elapse 250 msec / call so that second iteration of waitWhileBusy() should timeout.
    m_sd.timer().setElapsedTimePerCall(250);

    // Clear buffer to 0x00 before reading into it.
    memset(buffer, 0, sizeof(buffer));

        LONGS_EQUAL(RES_ERROR, m_sd.disk_read(buffer, 42, 2));

    // Assert ChipSelect to LOW.
    CHECK_TRUE(settingsRemaining() >= 1);
    SPIDma::Settings settings = m_sd.spi().getSetting(m_settingsIndex++);
    LONGS_EQUAL(SPIDma::ChipSelect, settings.type);
    LONGS_EQUAL(LOW, settings.chipSelect);
    LONGS_EQUAL(m_byteIndex, settings.bytesSentBefore);
    // Should write one 0xFF byte to card to prime it for communication.
    STRCMP_EQUAL("FF", m_sd.spi().getOutboundAsString(m_byteIndex++, 1));
    // Should write 0xFF until 0xFF is received to indicate that the card is no longer busy.
    STRCMP_EQUAL("FF", m_sd.spi().getOutboundAsString(m_byteIndex++, 1));
    STRCMP_EQUAL("FF", m_sd.spi().getOutboundAsString(m_byteIndex++, 1));
    // Deselect after detecting error.
    validateDeselect();

    // The 500 msec delay for two cycles should be recorded.
    LONGS_EQUAL(500, m_sd.maximumWaitWhileBusyTime());

    // Read buffer shouldn't have changed.
    validateBuffer(buffer, sizeof(buffer), 0x00);

    // Verify error log output.
    m_sd.dumpErrorLog(stderr);
    char expectedOutput[256];
    snprintf(expectedOutput, sizeof(expectedOutput),
             "waitWhileBusy(500) - Time out. Response=0x00\n"
             "select() - 500 msec time out\n"
             "disk_read(%X,42,2) - Select timed out\n",
             (uint32_t)(size_t)buffer);
    STRCMP_EQUAL(expectedOutput, printfSpy_GetLastOutput());
}

TEST(DiskRead, DiskRead_MultiBlock_ForceCmd12PaddingByteToContainStartBitAndErrorCode_ShouldSucceed_ShouldBeCounted)
{
    uint8_t buffer[1024];

    initSDHC();
    // CMD18 input data.
    setupDataForCmd("00");
    // 0xFE starts read data block.
    m_sd.spi().setInboundFromString("FE");
    // Data block will contain 512 bytes of 0xAD + valid CRC.
    setupDataBlock(0xAD, 512);
    // 0xFE starts read data block.
    m_sd.spi().setInboundFromString("FE");
    // Data block will contain 512 bytes of 0xDA + valid CRC.
    setupDataBlock(0xDA, 512);
    // CMD12 input data.
    // Queue up padding byte with start bit cleared.
    m_sd.spi().setInboundFromString("7F00");

    // Clear buffer to 0x00 before reading into it.
    memset(buffer, 0, sizeof(buffer));

        LONGS_EQUAL(RES_OK, m_sd.disk_read(buffer, 42, 2));

    validateSelect();
    // Should send CMD18 to start read process.  Argument is block number.
    validateCmdPacket(18, 42);
    // Should send multiple FF bytes to read in each data block:
    //  1 to read in header.
    //  512 to read data.
    //  2 to read CRC.
    validateFFBytes(2*(1+512+2));
    // Should send CMD12 to stop read process.
    validateCmdPacket(12);
    validateDeselect();

    // Should have read in data via SPI.
    validateBuffer(buffer, 512, 0xAD);
    validateBuffer(buffer + 512, 512, 0xDA);

    // Should count a padding read that was required for CMD12.
    LONGS_EQUAL(1, m_sd.cmd12PaddingByteRequiredCount());
}

TEST(DiskRead, DiskRead_MultiBlock_CMD18Error_ShouldFail_GetLogged)
{
    uint8_t buffer[1024];

    initSDHC();
    // CMD18 input data with error as response code.
    setupDataForCmd("04");

    // Clear buffer to 0x00 before reading into it.
    memset(buffer, 0, sizeof(buffer));

        LONGS_EQUAL(RES_ERROR, m_sd.disk_read(buffer, 42, 2));

    validateSelect();
    // Should send CMD18 to start read process.  Argument is block number.
    validateCmdPacket(18, 42);
    validateDeselect();

    // Should have initial data in data via SPI.
    validateBuffer(buffer, sizeof(buffer), 0x00);

    // Verify error log output.
    m_sd.dumpErrorLog(stderr);
    char expectedOutput[256];
    snprintf(expectedOutput, sizeof(expectedOutput),
             "disk_read(%08X,42,2) - CMD18 returned 0x04\n",
             (uint32_t)(size_t)buffer);
    STRCMP_EQUAL(expectedOutput, printfSpy_GetLastOutput());
}

TEST(DiskRead, DiskRead_MultiBlock_CMD12Error_ShouldFail_GetLogged)
{
    uint8_t buffer[1024];

    initSDHC();
    // CMD18 input data.
    setupDataForCmd("00");
    // 0xFE starts read data block.
    m_sd.spi().setInboundFromString("FE");
    // Data block will contain 512 bytes of 0xAD + valid CRC.
    setupDataBlock(0xAD, 512);
    // 0xFE starts read data block.
    m_sd.spi().setInboundFromString("FE");
    // Data block will contain 512 bytes of 0xDA + valid CRC.
    setupDataBlock(0xDA, 512);
    // CMD12 input data with error as response.
    setupDataForCmd12("04");

    // Clear buffer to 0x00 before reading into it.
    memset(buffer, 0, sizeof(buffer));

        LONGS_EQUAL(RES_ERROR, m_sd.disk_read(buffer, 42, 2));

    validateSelect();
    // Should send CMD18 to start read process.  Argument is block number.
    validateCmdPacket(18, 42);
    // Should send multiple FF bytes to read in each data block:
    //  1 to read in header.
    //  512 to read data.
    //  2 to read CRC.
    validateFFBytes(2*(1+512+2));
    // Should send CMD12 to stop read process.
    validateCmdPacket(12);
    validateDeselect();

    // Should have read in data via SPI by the time the error was encountered.
    validateBuffer(buffer, 512, 0xAD);
    validateBuffer(buffer + 512, 512, 0xDA);

    // Verify error log output.
    m_sd.dumpErrorLog(stderr);
    char expectedOutput[256];
    snprintf(expectedOutput, sizeof(expectedOutput),
             "disk_read(%08X,42,2) - CMD12 returned 0x04\n",
             (uint32_t)(size_t)buffer);
    STRCMP_EQUAL(expectedOutput, printfSpy_GetLastOutput());
}

TEST(DiskRead, DiskRead_MultiBlock_FailDataBlockCrcForEachBlockOnce_ShouldSucceed_GetLogged_GetCounted)
{
    uint8_t buffer[4*512];

    initSDHC();
    // Fail CRC for first block.
    // CMD18 input data.
    setupDataForCmd("00");
    // 0xFE starts read data block.
    m_sd.spi().setInboundFromString("FE");
    // Data block will contain 512 bytes of 0x11 + invalid CRC.
    setupDataBlock(0x11, 512, "BAAD");
    // CMD12 input data.
    setupDataForCmd12("00");

    // Retry of first and second block again.
    // Fail CRC for second block this time.
    // CMD18 input data.
    setupDataForCmd("00");
    // 0xFE starts read data block.
    m_sd.spi().setInboundFromString("FE");
    // Data block will contain 512 bytes of 0x11 + valid CRC.
    setupDataBlock(0x11, 512);
    // 0xFE starts read data block.
    m_sd.spi().setInboundFromString("FE");
    // Data block will contain 512 bytes of 0x22 + invalid CRC.
    setupDataBlock(0x22, 512, "BAAD");
    // CMD12 input data.
    setupDataForCmd12("00");

    // Retry starting from second block.
    // Fail CRC of third block this time.
    // CMD18 input data.
    setupDataForCmd("00");
    // 0xFE starts read data block.
    m_sd.spi().setInboundFromString("FE");
    // Data block will contain 512 bytes of 0x22 + valid CRC.
    setupDataBlock(0x22, 512);
    // 0xFE starts read data block.
    m_sd.spi().setInboundFromString("FE");
    // Data block will contain 512 bytes of 0x33 + invalid CRC.
    setupDataBlock(0x33, 512, "BAAD");
    // CMD12 input data.
    setupDataForCmd12("00");

    // Retry starting from third block.
    // Fail CRC of fourth block this time.
    // CMD18 input data.
    setupDataForCmd("00");
    // 0xFE starts read data block.
    m_sd.spi().setInboundFromString("FE");
    // Data block will contain 512 bytes of 0x33 + valid CRC.
    setupDataBlock(0x33, 512);
    // 0xFE starts read data block.
    m_sd.spi().setInboundFromString("FE");
    // Data block will contain 512 bytes of 0x44 + invalid CRC.
    setupDataBlock(0x44, 512, "BAAD");
    // CMD12 input data.
    setupDataForCmd12("00");

    // Retry starting from fourth block.
    // CMD18 input data.
    setupDataForCmd("00");
    // 0xFE starts read data block.
    m_sd.spi().setInboundFromString("FE");
    // Data block will contain 512 bytes of 0x33 + valid CRC.
    setupDataBlock(0x44, 512);
    // CMD12 input data.
    setupDataForCmd12("00");

    // Clear buffer to 0x00 before reading into it.
    memset(buffer, 0, sizeof(buffer));

        LONGS_EQUAL(RES_OK, m_sd.disk_read(buffer, 42, 4));

    // Failed read of first block.
    validateSelect();
    // Should send CMD18 to start read process.  Argument is block number.
    validateCmdPacket(18, 42);
    // Should send multiple FF bytes to read in each data block:
    //  1 to read in header.
    //  512 to read data.
    //  2 to read CRC.
    validateFFBytes(1*(1+512+2));
    // Should send CMD12 to stop read process.
    validateCmdPacket(12);
    validateDeselect();

    // Retry from failure of first block.
    // Failed read of second block.
    validateSelect();
    // Should send CMD18 to start read process.  Argument is block number.
    validateCmdPacket(18, 42);
    // Should send multiple FF bytes to read in each data block:
    //  1 to read in header.
    //  512 to read data.
    //  2 to read CRC.
    validateFFBytes(2*(1+512+2));
    // Should send CMD12 to stop read process.
    validateCmdPacket(12);
    validateDeselect();

    // Retry from failure of second block.
    // Failed read of third block.
    validateSelect();
    // Should send CMD18 to start read process.  Argument is block number.
    validateCmdPacket(18, 42+1);
    // Should send multiple FF bytes to read in each data block:
    //  1 to read in header.
    //  512 to read data.
    //  2 to read CRC.
    validateFFBytes(2*(1+512+2));
    // Should send CMD12 to stop read process.
    validateCmdPacket(12);
    validateDeselect();

    // Retry from failure of third block.
    // Failed read of fourth block.
    validateSelect();
    // Should send CMD18 to start read process.  Argument is block number.
    validateCmdPacket(18, 42+2);
    // Should send multiple FF bytes to read in each data block:
    //  1 to read in header.
    //  512 to read data.
    //  2 to read CRC.
    validateFFBytes(2*(1+512+2));
    // Should send CMD12 to stop read process.
    validateCmdPacket(12);
    validateDeselect();

    // Retry from failure of last block.
    validateSelect();
    // Should send CMD18 to start read process.  Argument is block number.
    validateCmdPacket(18, 42+3);
    // Should send multiple FF bytes to read in each data block:
    //  1 to read in header.
    //  512 to read data.
    //  2 to read CRC.
    validateFFBytes(1*(1+512+2));
    // Should send CMD12 to stop read process.
    validateCmdPacket(12);
    validateDeselect();

    // Should have read in data via SPI.
    validateBuffer(buffer, 512, 0x11);
    validateBuffer(buffer + 1 * 512, 512, 0x22);
    validateBuffer(buffer + 2 * 512, 512, 0x33);
    validateBuffer(buffer + 3 * 512, 512, 0x44);

    // Only failed CRC check once for any single block.
    LONGS_EQUAL(1, m_sd.maximumReadRetryCount());

    // Verify error log output.
    m_sd.dumpErrorLog(stderr);
    char expectedOutput[1024];
    snprintf(expectedOutput, sizeof(expectedOutput),
             "receiveDataBlock(%08X,512) - Invalid CRC. Expected=0xBAAD Actual=0x3880\n"
             "disk_read(%08X,42,4) - receiveDataBlock failed. block=42\n"
             "receiveDataBlock(%08X,512) - Invalid CRC. Expected=0xBAAD Actual=0x7100\n"
             "disk_read(%08X,42,4) - receiveDataBlock failed. block=43\n"
             "receiveDataBlock(%08X,512) - Invalid CRC. Expected=0xBAAD Actual=0x4980\n"
             "disk_read(%08X,42,4) - receiveDataBlock failed. block=44\n"
             "receiveDataBlock(%08X,512) - Invalid CRC. Expected=0xBAAD Actual=0xE200\n"
             "disk_read(%08X,42,4) - receiveDataBlock failed. block=45\n",
             (uint32_t)(size_t)buffer, (uint32_t)(size_t)buffer,
             (uint32_t)(size_t)buffer + 1*512, (uint32_t)(size_t)buffer,
             (uint32_t)(size_t)buffer + 2*512, (uint32_t)(size_t)buffer,
             (uint32_t)(size_t)buffer + 3*512, (uint32_t)(size_t)buffer);
    STRCMP_EQUAL(expectedOutput, printfSpy_GetLastOutput());
}

TEST(DiskRead, DiskRead_MultiBlock_FailDataBlockCrcForFirstBlockThreeTimes_ShouldFail_GetLogged_GetCounted)
{
    uint8_t buffer[1024];

    initSDHC();

    // Repeat this failing CRC data 3 times.
    // CMD18 input data.
    setupDataForCmd("00");
    // 0xFE starts read data block.
    m_sd.spi().setInboundFromString("FE");
    // Data block will contain 512 bytes of 0xAD + invalid CRC.
    setupDataBlock(0xAD, 512, "BAAD");
    // CMD12 input data.
    setupDataForCmd12("00");
    // CMD18 input data.
    setupDataForCmd("00");
    // 0xFE starts read data block.
    m_sd.spi().setInboundFromString("FE");
    // Data block will contain 512 bytes of 0xAD + invalid CRC.
    setupDataBlock(0xAD, 512, "BAAD");
    // CMD12 input data.
    setupDataForCmd12("00");
    // CMD18 input data.
    setupDataForCmd("00");
    // 0xFE starts read data block.
    m_sd.spi().setInboundFromString("FE");
    // Data block will contain 512 bytes of 0xAD + invalid CRC.
    setupDataBlock(0xAD, 512, "BAAD");
    // CMD12 input data.
    setupDataForCmd12("00");

    // Clear buffer to 0x00 before reading into it.
    memset(buffer, 0, sizeof(buffer));

        LONGS_EQUAL(RES_ERROR, m_sd.disk_read(buffer, 42, 2));

    // Repeat retry of first block read 3 times.
    validateSelect();
    // Should send CMD18 to start read process.  Argument is block number.
    validateCmdPacket(18, 42);
    // Should send multiple FF bytes to read in each data block:
    //  1 to read in header.
    //  512 to read data.
    //  2 to read CRC.
    validateFFBytes(1*(1+512+2));
    // Should send CMD12 to stop read process.
    validateCmdPacket(12);
    validateDeselect();
    // Start next attempt.
    validateSelect();
    // Should send CMD18 to start read process.  Argument is block number.
    validateCmdPacket(18, 42);
    // Should send multiple FF bytes to read in each data block:
    //  1 to read in header.
    //  512 to read data.
    //  2 to read CRC.
    validateFFBytes(1*(1+512+2));
    // Should send CMD12 to stop read process.
    validateCmdPacket(12);
    validateDeselect();
    // Start next attempt.
    validateSelect();
    // Should send CMD18 to start read process.  Argument is block number.
    validateCmdPacket(18, 42);
    // Should send multiple FF bytes to read in each data block:
    //  1 to read in header.
    //  512 to read data.
    //  2 to read CRC.
    validateFFBytes(1*(1+512+2));
    // Should send CMD12 to stop read process.
    validateCmdPacket(12);
    validateDeselect();

    // Will have read in data to first block before encountering CRC error.
    validateBuffer(buffer, 512, 0xAD);
    // Second block read should not have happened though and still contain zero fill.
    validateBuffer(buffer + 512, 512, 0x00);

    // Failed CRC check 3 times on single block.
    LONGS_EQUAL(3, m_sd.maximumReadRetryCount());

    // Verify error log output.
    m_sd.dumpErrorLog(stderr);
    char expectedOutput[1024];
    snprintf(expectedOutput, sizeof(expectedOutput),
             "receiveDataBlock(%08X,512) - Invalid CRC. Expected=0xBAAD Actual=0x2F29\n"
             "disk_read(%08X,42,2) - receiveDataBlock failed. block=42\n"
             "receiveDataBlock(%08X,512) - Invalid CRC. Expected=0xBAAD Actual=0x2F29\n"
             "disk_read(%08X,42,2) - receiveDataBlock failed. block=42\n"
             "receiveDataBlock(%08X,512) - Invalid CRC. Expected=0xBAAD Actual=0x2F29\n"
             "disk_read(%08X,42,2) - receiveDataBlock failed. block=42\n",
             (uint32_t)(size_t)buffer, (uint32_t)(size_t)buffer,
             (uint32_t)(size_t)buffer, (uint32_t)(size_t)buffer,
             (uint32_t)(size_t)buffer, (uint32_t)(size_t)buffer);
    STRCMP_EQUAL(expectedOutput, printfSpy_GetLastOutput());
}
