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

TEST_GROUP_BASE(DiskWrite,SDFileSystemBase)
{
    void setupDataForCmd12(const char* pR1Response = "01" /* No errors & in idle state */)
    {
        // select() expects to receive a response which is not 0xFF for the first byte read.
        m_sd.spi().setInboundFromString("00");
        // Return not-busy on first loop in waitForNotBusy().
        m_sd.spi().setInboundFromString("FF");
        // Return extra padding byte.
        m_sd.spi().setInboundFromString("FF");
        // Return indicated R1 response.
        m_sd.spi().setInboundFromString(pR1Response);
    }
};


TEST(DiskWrite, DiskWrite_AttemptBeforeInit_ShouldFail_GetLogged)
{
    uint8_t buffer[512];

        LONGS_EQUAL(RES_NOTRDY, m_sd.disk_write(buffer, 42, 1));

    // Only the constructor should have generated any SPI traffic.
    validateConstructor();

    m_sd.dumpErrorLog(stderr);
    char expectedOutput[256];
    snprintf(expectedOutput, sizeof(expectedOutput), "disk_write(%08X,42,1) - Attempt to write uninitialized drive\n",
             (uint32_t)(size_t)buffer);
    STRCMP_EQUAL(expectedOutput, printfSpy_GetLastOutput());
}

TEST(DiskWrite, DiskWrite_AttemptToWrite0Blocks_ShouldFail_GetLogged)
{
    uint8_t buffer[512];

    initSDHC();
        LONGS_EQUAL(RES_PARERR, m_sd.disk_write(buffer, 42, 0));

    m_sd.dumpErrorLog(stderr);
    char expectedOutput[256];
    snprintf(expectedOutput, sizeof(expectedOutput), "disk_write(%08X,42,0) - Attempt to write 0 blocks\n",
             (uint32_t)(size_t)buffer);
    STRCMP_EQUAL(expectedOutput, printfSpy_GetLastOutput());
}

TEST(DiskWrite, DiskWrite_SingleBlockFromSDHC_ShouldSucceed)
{
    uint8_t buffer[512];

    initSDHC();
    // CMD24 input data.
    setupDataForCmd("00");
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return successful write response token.
    m_sd.spi().setInboundFromString("05");
    // CMD13 input data with successful R2 response.
    setupDataForCmd("00");
    m_sd.spi().setInboundFromString("00");

    // Fill the write buffer with data to write.
    memset(buffer, 0xAD, sizeof(buffer));

        LONGS_EQUAL(RES_OK, m_sd.disk_write(buffer, 42, 1));

    validateSelect();
    // Should send CMD24 to start write process.  Argument is block number.
    validateCmdPacket(24, 42);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFE, 0xAD);
    // Deselect as the write is now complete.
    validateDeselect();
    // Should send CMD13 to get R2 write status.
    validateCmd(13, 0, 1);

    // Should have required no retries.
    LONGS_EQUAL(0, m_sd.maximumWriteRetryCount());
}

TEST(DiskWrite, DiskWrite_SingleBlockFromSDSC_ShouldConvertToByteAddress_ShouldSucceed)
{
    uint8_t buffer[512];

    initSDSC();
    // CMD24 input data.
    setupDataForCmd("00");
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return successful write response token with don't care bits set high.
    m_sd.spi().setInboundFromString("E5");
    // CMD13 input data with successful R2 response.
    setupDataForCmd("00");
    m_sd.spi().setInboundFromString("00");

    // Fill the write buffer with data to write.
    memset(buffer, 0xAD, sizeof(buffer));

        LONGS_EQUAL(RES_OK, m_sd.disk_write(buffer, 42, 1));

    validateSelect();
    // Should send CMD24 to start write process.  Argument is byte address.
    // Note that argument for SDSC should be block number * 512.
    validateCmdPacket(24, 42 * 512);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFE, 0xAD);
    // Deselect as the write is now complete.
    validateDeselect();
    // Should send CMD13 to get R2 write status.
    validateCmd(13, 0, 1);

    // Should have required no retries.
    LONGS_EQUAL(0, m_sd.maximumWriteRetryCount());
}

TEST(DiskWrite, DiskWrite_SingleBlock_SelectTimeout_ShouldFail_GetLogged)
{
    uint8_t buffer[512];

    initSDHC();

    // select() expects to receive a response which is not 0xFF for the first byte read.
    m_sd.spi().setInboundFromString("00");
    // Return busy on two loops through waitForNotBusy().
    m_sd.spi().setInboundFromString("0000");

    // Set timer to elapse 250 msec / call so that second iteration of waitWhileBusy() should timeout.
    m_sd.timer().setElapsedTimePerCall(250);

    // Fill the write buffer with data to write.
    memset(buffer, 0xAD, sizeof(buffer));

        LONGS_EQUAL(RES_ERROR, m_sd.disk_write(buffer, 42, 1));

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

    // Verify error log output.
    m_sd.dumpErrorLog(stderr);
    char expectedOutput[256];
    snprintf(expectedOutput, sizeof(expectedOutput),
             "waitWhileBusy(500) - Time out. Response=0x00\n"
             "select() - 500 msec time out\n"
             "disk_write(%X,42,1) - Select timed out\n",
             (uint32_t)(size_t)buffer);
    STRCMP_EQUAL(expectedOutput, printfSpy_GetLastOutput());
}

TEST(DiskWrite, DiskWrite_SingleBlock_CMD24Error_ShouldFail_GetLogged)
{
    uint8_t buffer[512];

    initSDHC();
    // CMD24 input data.  Fail with Illegal command error.
    setupDataForCmd("04");

    // Fill the write buffer with data to write.
    memset(buffer, 0xAD, sizeof(buffer));

        LONGS_EQUAL(RES_ERROR, m_sd.disk_write(buffer, 42, 1));

    validateSelect();
    // Should send CMD24 to start write process.  Argument is block number.
    validateCmdPacket(24, 42);
    validateDeselect();

    // Verify error log output.
    m_sd.dumpErrorLog(stderr);
    char expectedOutput[256];
    snprintf(expectedOutput, sizeof(expectedOutput),
             "disk_write(%X,42,1) - CMD24 returned 0x04\n",
             (uint32_t)(size_t)buffer);
    STRCMP_EQUAL(expectedOutput, printfSpy_GetLastOutput());
}

TEST(DiskWrite, DiskWrite_SingleBlock_ForceWaitWhileBusyToTimeout_ShouldRetry_Logged_Recorded)
{
    uint8_t buffer[512];

    initSDHC();
    // CMD24 input data.
    setupDataForCmd("00");
    // Keep returning busy in waitWhileBusy().
    m_sd.spi().setInboundFromString("0000");

    // Retry which should be successful.
    // CMD24 input data.
    setupDataForCmd("00");
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return successful write response token.
    m_sd.spi().setInboundFromString("05");
    // CMD13 input data with successful R2 response.
    setupDataForCmd("00");
    m_sd.spi().setInboundFromString("00");

    // Bump elapsed time so that two loops should lead to timeout.
    m_sd.timer().setElapsedTimePerCall(250);

    // Fill the write buffer with data to write.
    memset(buffer, 0xAD, sizeof(buffer));

        LONGS_EQUAL(RES_OK, m_sd.disk_write(buffer, 42, 1));

    // First failed attempt.
    validateSelect();
    // Should send CMD24 to start write process.  Argument is block number.
    validateCmdPacket(24, 42);
    // Should have sent two 0xFF byte in waitWhileBusy().
    validateFFBytes(2);
    // Deselect as the write is now complete.
    validateDeselect();

    // Successful retry.
    validateSelect();
    // Should send CMD24 to start write process.  Argument is block number.
    validateCmdPacket(24, 42);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFE, 0xAD);
    // Deselect as the write is now complete.
    validateDeselect();
    // Should send CMD13 to get R2 write status.
    validateCmd(13, 0, 1);

    // Check for retry count.
    LONGS_EQUAL(1, m_sd.maximumWriteRetryCount());
    // The 500 msec delay for two cycles should be recorded.
    LONGS_EQUAL(500, m_sd.maximumWaitWhileBusyTime());

    // Verify error log output.
    m_sd.dumpErrorLog(stderr);
    char expectedOutput[256];
    snprintf(expectedOutput, sizeof(expectedOutput),
             "waitWhileBusy(500) - Time out. Response=0x00\n"
             "transmitDataBlock(FE,%08X,512) - Time out after 500ms\n"
             "disk_write(%X,42,1) - transmitDataBlock failed\n",
             (uint32_t)(size_t)buffer,
             (uint32_t)(size_t)buffer);
    STRCMP_EQUAL(expectedOutput, printfSpy_GetLastOutput());
}

TEST(DiskWrite, DiskWrite_SingleBlock_FailTransmitDataBlockWithInvalidCRC_ShouldRetry_Logged_Recorded)
{
    uint8_t buffer[512];

    initSDHC();

    // Fail first attempt with CRC error.
    // CMD24 input data.
    setupDataForCmd("00");
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return failure write response token for CRC error.
    m_sd.spi().setInboundFromString("0B");

    // Successful attempt.
    // CMD24 input data.
    setupDataForCmd("00");
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return successful write response token.
    m_sd.spi().setInboundFromString("05");
    // CMD13 input data with successful R2 response.
    setupDataForCmd("00");
    m_sd.spi().setInboundFromString("00");

    // Fill the write buffer with data to write.
    memset(buffer, 0xAD, sizeof(buffer));

        LONGS_EQUAL(RES_OK, m_sd.disk_write(buffer, 42, 1));

    // Failed first attempt with CRC error.
    validateSelect();
    // Should send CMD24 to start write process.  Argument is block number.
    validateCmdPacket(24, 42);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFE, 0xAD);
    // Deselect as the write is now complete.
    validateDeselect();

    // Successful on retry.
    validateSelect();
    // Should send CMD24 to start write process.  Argument is block number.
    validateCmdPacket(24, 42);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFE, 0xAD);
    // Deselect as the write is now complete.
    validateDeselect();
    // Should send CMD13 to get R2 write status.
    validateCmd(13, 0, 1);

    // Should have required 1 retry.
    LONGS_EQUAL(1, m_sd.maximumWriteRetryCount());

    // Verify error log output.
    m_sd.dumpErrorLog(stderr);
    char expectedOutput[256];
    snprintf(expectedOutput, sizeof(expectedOutput),
             "transmitDataBlock(FE,%08X,512) - Data Response=0x0B\n"
             "disk_write(%X,42,1) - transmitDataBlock failed\n",
             (uint32_t)(size_t)buffer,
             (uint32_t)(size_t)buffer);
    STRCMP_EQUAL(expectedOutput, printfSpy_GetLastOutput());
}

TEST(DiskWrite, DiskWrite_SingleBlock_ForceTransmitDataBlockToFailCRC3Times_ShouldFail_Logged_Recorded)
{
    uint8_t buffer[512];

    initSDHC();

    // Fail this attempt with CRC error.
    // CMD24 input data.
    setupDataForCmd("00");
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return failure write response token for CRC error.
    m_sd.spi().setInboundFromString("0B");

    // Fail this attempt with CRC error.
    // CMD24 input data.
    setupDataForCmd("00");
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return failure write response token for CRC error.
    m_sd.spi().setInboundFromString("0B");

    // Fail this attempt with CRC error.
    // CMD24 input data.
    setupDataForCmd("00");
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return failure write response token for CRC error.
    m_sd.spi().setInboundFromString("0B");

    // Fill the write buffer with data to write.
    memset(buffer, 0xAD, sizeof(buffer));

        LONGS_EQUAL(RES_ERROR, m_sd.disk_write(buffer, 42, 1));

    // Failed attempt with CRC error.
    validateSelect();
    // Should send CMD24 to start write process.  Argument is block number.
    validateCmdPacket(24, 42);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFE, 0xAD);
    // Deselect as the write is now complete.
    validateDeselect();

    // Failed attempt with CRC error.
    validateSelect();
    // Should send CMD24 to start write process.  Argument is block number.
    validateCmdPacket(24, 42);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFE, 0xAD);
    // Deselect as the write is now complete.
    validateDeselect();

    // Failed attempt with CRC error.
    validateSelect();
    // Should send CMD24 to start write process.  Argument is block number.
    validateCmdPacket(24, 42);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFE, 0xAD);
    // Deselect as the write is now complete.
    validateDeselect();

    // Should have required 3 retries.
    LONGS_EQUAL(3, m_sd.maximumWriteRetryCount());

    // Verify error log output.
    m_sd.dumpErrorLog(stderr);
    char expectedOutput[512];
    snprintf(expectedOutput, sizeof(expectedOutput),
             "transmitDataBlock(FE,%08X,512) - Data Response=0x0B\n"
             "disk_write(%X,42,1) - transmitDataBlock failed\n"
             "transmitDataBlock(FE,%08X,512) - Data Response=0x0B\n"
             "disk_write(%X,42,1) - transmitDataBlock failed\n"
             "transmitDataBlock(FE,%08X,512) - Data Response=0x0B\n"
             "disk_write(%X,42,1) - transmitDataBlock failed\n",
             (uint32_t)(size_t)buffer, (uint32_t)(size_t)buffer,
             (uint32_t)(size_t)buffer, (uint32_t)(size_t)buffer,
             (uint32_t)(size_t)buffer, (uint32_t)(size_t)buffer);
    STRCMP_EQUAL(expectedOutput, printfSpy_GetLastOutput());
}

TEST(DiskWrite, DiskWrite_SingleBlock_CMD13R1ResponseError_ShouldFail_GetLogged)
{
    uint8_t buffer[512];

    initSDHC();
    // CMD24 input data.
    setupDataForCmd("00");
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return successful write response token.
    m_sd.spi().setInboundFromString("05");
    // CMD13 input data with error in R1 response.
    setupDataForCmd("04");

    // Fill the write buffer with data to write.
    memset(buffer, 0xAD, sizeof(buffer));

        LONGS_EQUAL(RES_ERROR, m_sd.disk_write(buffer, 42, 1));

    validateSelect();
    // Should send CMD24 to start write process.  Argument is block number.
    validateCmdPacket(24, 42);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFE, 0xAD);
    // Deselect as the write is now complete.
    validateDeselect();
    // Should send CMD13 but fail before asking for R2 response byte.
    validateCmd(13, 0);

    // Should have performed no retries.
    LONGS_EQUAL(0, m_sd.maximumWriteRetryCount());

    // Verify error log output.
    m_sd.dumpErrorLog(stderr);
    char expectedOutput[256];
    snprintf(expectedOutput, sizeof(expectedOutput),
             "disk_write(%X,42,1) - CMD13 failed. r1Response=0x04\n",
             (uint32_t)(size_t)buffer);
    STRCMP_EQUAL(expectedOutput, printfSpy_GetLastOutput());
}

TEST(DiskWrite, DiskWrite_SingleBlock_CMD13R2ResponseError_ShouldFail_GetLogged)
{
    uint8_t buffer[512];

    initSDHC();
    // CMD24 input data.
    setupDataForCmd("00");
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return successful write response token.
    m_sd.spi().setInboundFromString("05");
    // CMD13 input data with error R2 response.
    setupDataForCmd("00");
    m_sd.spi().setInboundFromString("02");

    // Fill the write buffer with data to write.
    memset(buffer, 0xAD, sizeof(buffer));

        LONGS_EQUAL(RES_ERROR, m_sd.disk_write(buffer, 42, 1));

    validateSelect();
    // Should send CMD24 to start write process.  Argument is block number.
    validateCmdPacket(24, 42);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFE, 0xAD);
    // Deselect as the write is now complete.
    validateDeselect();
    // Should send CMD13 to get R2 write status.
    validateCmd(13, 0, 1);

    // Should have required no retries.
    LONGS_EQUAL(0, m_sd.maximumWriteRetryCount());

    // Verify error log output.
    m_sd.dumpErrorLog(stderr);
    char expectedOutput[256];
    snprintf(expectedOutput, sizeof(expectedOutput),
             "disk_write(%X,42,1) - CMD13 failed. Status=0x02\n",
             (uint32_t)(size_t)buffer);
    STRCMP_EQUAL(expectedOutput, printfSpy_GetLastOutput());
}

TEST(DiskWrite, DiskWrite_MultiBlock_ToSDHC_ShouldSucceed)
{
    uint8_t buffer[2*512];

    initSDHC();

    // ACMD23 input data.
    setupDataForACmd("00");
    // CMD25 input data.
    setupDataForCmd("00");

    // First block.
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return successful write response token.
    m_sd.spi().setInboundFromString("05");
    // Second block.
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return successful write response token.
    m_sd.spi().setInboundFromString("05");
    // Sending of stop transmission token.
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");

    // CMD13 input data with successful R2 response.
    setupDataForCmd("00");
    m_sd.spi().setInboundFromString("00");

    // Fill the write buffer with data to write.
    memset(buffer, 0xAD, 512);
    memset(buffer+512, 0xDA, 512);

        LONGS_EQUAL(RES_OK, m_sd.disk_write(buffer, 42, 2));

    // Should send ACDM23 to start write process.  Argument is block count.
    validateACmd(23, 2);
    validateSelect();
    // Should send CMD25 to start write process.  Argument is block number.
    validateCmdPacket(25, 42);

    // First block.
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFC, 0xAD);
    // Second block.
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFC, 0xDA);

    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send stop transmission token.
    STRCMP_EQUAL("FD", m_sd.spi().getOutboundAsString(m_byteIndex++, 1));
    // Deselect as the write is now complete.
    validateDeselect();
    // Should send CMD13 to get R2 write status.
    validateCmd(13, 0, 1);

    // Should have required no retries.
    LONGS_EQUAL(0, m_sd.maximumWriteRetryCount());
}

TEST(DiskWrite, DiskWrite_MultiBlock_ToSDSC_ShouldSucceed)
{
    uint8_t buffer[2*512];

    initSDSC();

    // ACMD23 input data.
    setupDataForACmd("00");
    // CMD25 input data.
    setupDataForCmd("00");

    // First block.
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return successful write response token.
    m_sd.spi().setInboundFromString("05");
    // Second block.
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return successful write response token.
    m_sd.spi().setInboundFromString("05");
    // Sending of stop transmission token.
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");

    // CMD13 input data with successful R2 response.
    setupDataForCmd("00");
    m_sd.spi().setInboundFromString("00");

    // Fill the write buffer with data to write.
    memset(buffer, 0xAD, 512);
    memset(buffer+512, 0xDA, 512);

        LONGS_EQUAL(RES_OK, m_sd.disk_write(buffer, 42, 2));

    // Should send ACDM23 to start write process.  Argument is block count.
    validateACmd(23, 2);
    validateSelect();
    // Should send CMD25 to start write process.  Argument is byte address for SDSC.
    validateCmdPacket(25, 42*512);

    // First block.
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFC, 0xAD);
    // Second block.
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFC, 0xDA);

    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send stop transmission token.
    STRCMP_EQUAL("FD", m_sd.spi().getOutboundAsString(m_byteIndex++, 1));
    // Deselect as the write is now complete.
    validateDeselect();
    // Should send CMD13 to get R2 write status.
    validateCmd(13, 0, 1);

    // Should have required no retries.
    LONGS_EQUAL(0, m_sd.maximumWriteRetryCount());
}

TEST(DiskWrite, DiskWrite_MultiBlock_FailACMD23_ShouldIgnoreError_ShouldSucceed)
{
    uint8_t buffer[2*512];

    initSDHC();

    // ACMD23 input data. Fail with non-CRC error.
    setupDataForCmd("00"); // CMD55 prefix.
    setupDataForCmd("04");
    // CMD25 input data.
    setupDataForCmd("00");

    // First block.
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return successful write response token.
    m_sd.spi().setInboundFromString("05");
    // Second block.
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return successful write response token.
    m_sd.spi().setInboundFromString("05");
    // Sending of stop transmission token.
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");

    // CMD13 input data with successful R2 response.
    setupDataForCmd("00");
    m_sd.spi().setInboundFromString("00");

    // Fill the write buffer with data to write.
    memset(buffer, 0xAD, 512);
    memset(buffer+512, 0xDA, 512);

        LONGS_EQUAL(RES_OK, m_sd.disk_write(buffer, 42, 2));

    // Should send ACDM23 to start write process.  Argument is block count.
    validateACmd(23, 2);
    validateSelect();
    // Should send CMD25 to start write process.  Argument is block number.
    validateCmdPacket(25, 42);

    // First block.
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFC, 0xAD);
    // Second block.
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFC, 0xDA);

    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send stop transmission token.
    STRCMP_EQUAL("FD", m_sd.spi().getOutboundAsString(m_byteIndex++, 1));
    // Deselect as the write is now complete.
    validateDeselect();
    // Should send CMD13 to get R2 write status.
    validateCmd(13, 0, 1);

    // Should have required no retries.
    LONGS_EQUAL(0, m_sd.maximumWriteRetryCount());

    // Should have logged no errors.
    m_sd.dumpErrorLog(stderr);
    STRCMP_EQUAL("", printfSpy_GetLastOutput());
}

TEST(DiskWrite, DiskWrite_MultiBlock_SelectTimeout_ShouldFail_GetLogged)
{
    uint8_t buffer[2*512];

    initSDHC();

    // ACMD23 input data.
    setupDataForACmd("00");
    // select() expects to receive a response which is not 0xFF for the first byte read.
    m_sd.spi().setInboundFromString("00");
    // Return busy on two loops through waitForNotBusy().
    m_sd.spi().setInboundFromString("0000");

    // Set timer to elapse 250 msec / call so that second iteration of waitWhileBusy() should timeout.
    m_sd.timer().setElapsedTimePerCall(250);

    // Fill the write buffer with data to write.
    memset(buffer, 0xAD, 512);
    memset(buffer+512, 0xDA, 512);

        LONGS_EQUAL(RES_ERROR, m_sd.disk_write(buffer, 42, 2));

    // Should send ACDM23 to start write process.  Argument is block count.
    validateACmd(23, 2);
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

    // Verify error log output.
    m_sd.dumpErrorLog(stderr);
    char expectedOutput[256];
    snprintf(expectedOutput, sizeof(expectedOutput),
             "waitWhileBusy(500) - Time out. Response=0x00\n"
             "select() - 500 msec time out\n"
             "disk_write(%X,42,2) - Select timed out\n",
             (uint32_t)(size_t)buffer);
    STRCMP_EQUAL(expectedOutput, printfSpy_GetLastOutput());
}

TEST(DiskWrite, DiskWrite_MultiBlock_CMD25Error_ShouldFail_GetLogged)
{
    uint8_t buffer[2*512];

    initSDHC();

    // ACMD23 input data.
    setupDataForACmd("00");
    // CMD25 with error as response code.
    setupDataForCmd("04");

    // Fill the write buffer with data to write.
    memset(buffer, 0xAD, 512);
    memset(buffer+512, 0xDA, 512);

        LONGS_EQUAL(RES_ERROR, m_sd.disk_write(buffer, 42, 2));

    // Should send ACDM23 to start write process.  Argument is block count.
    validateACmd(23, 2);
    validateSelect();
    // Should send CMD25 to start write process.  Argument is block number.
    validateCmdPacket(25, 42);
    // Deselect as the write is now complete.
    validateDeselect();

    // Should have required no retries.
    LONGS_EQUAL(0, m_sd.maximumWriteRetryCount());

    // Verify error log output.
    m_sd.dumpErrorLog(stderr);
    char expectedOutput[256];
    snprintf(expectedOutput, sizeof(expectedOutput),
             "disk_write(%X,42,2) - CMD25 returned 0x04\n",
             (uint32_t)(size_t)buffer);
    STRCMP_EQUAL(expectedOutput, printfSpy_GetLastOutput());
}

TEST(DiskWrite, DiskWrite_MultiBlock_FailDataBlockCrcOnceOnEachBlock_ShouldRetryAndSucceed_GetLogged_GetCounted)
{
    uint8_t buffer[4*512];

    initSDHC();

    // ACMD23 input data.
    setupDataForACmd("00");
    // CMD25 input data.
    setupDataForCmd("00");
    // First block.
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return CRC error for write response token.
    m_sd.spi().setInboundFromString("0B");
    // CMD12 input data.
    setupDataForCmd12("00");

    // Retry from first block.
    // Fail on second block.
    // ACMD23 input data.
    setupDataForACmd("00");
    // CMD25 input data.
    setupDataForCmd("00");
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return successful write response token.
    m_sd.spi().setInboundFromString("05");
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return CRC error for write response token.
    m_sd.spi().setInboundFromString("0B");
    // CMD12 input data.
    setupDataForCmd12("00");

    // Retry from second block.
    // Fail on third block.
    // ACMD23 input data.
    setupDataForACmd("00");
    // CMD25 input data.
    setupDataForCmd("00");
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return successful write response token.
    m_sd.spi().setInboundFromString("05");
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return CRC error for write response token.
    m_sd.spi().setInboundFromString("0B");
    // CMD12 input data.
    setupDataForCmd12("00");

    // Retry from third block.
    // Fail on fourth block.
    // ACMD23 input data.
    setupDataForACmd("00");
    // CMD25 input data.
    setupDataForCmd("00");
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return successful write response token.
    m_sd.spi().setInboundFromString("05");
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return CRC error for write response token.
    m_sd.spi().setInboundFromString("0B");
    // CMD12 input data.
    setupDataForCmd12("00");

    // Retry from fourth block.
    // Will finish successfully.
    // ACMD23 input data.
    setupDataForACmd("00");
    // CMD25 input data.
    setupDataForCmd("00");
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return successful write response token.
    m_sd.spi().setInboundFromString("05");
    // Sending of stop transmission token.
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");

    // CMD13 input data with successful R2 response.
    setupDataForCmd("00");
    m_sd.spi().setInboundFromString("00");

    // Fill the write buffer with data to write.
    memset(buffer, 0x11, 512);
    memset(buffer + 1*512, 0x22, 512);
    memset(buffer + 2*512, 0x33, 512);
    memset(buffer + 3*512, 0x44, 512);

        LONGS_EQUAL(RES_OK, m_sd.disk_write(buffer, 42, 4));

    // First attempt which will fail CRC on first block.
    // Should send ACDM23 to start write process.  Argument is block count.
    validateACmd(23, 4);
    validateSelect();
    // Should send CMD25 to start write process.  Argument is block number.
    validateCmdPacket(25, 42);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFC, 0x11);
    // Should send CMD12 to stop write because of the error.
    validateDeselect();
    validateCmd(12, 0);

    // Retry from first block.
    // Fail on second block.
    // Should send ACDM23 to start write process.  Argument is block count.
    validateACmd(23, 4);
    validateSelect();
    // Should send CMD25 to start write process.  Argument is block number.
    validateCmdPacket(25, 42);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFC, 0x11);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFC, 0x22);
    // Should send CMD12 to stop write because of the error.
    validateDeselect();
    validateCmd(12, 0);

    // Retry from second block.
    // Fail on third block.
    // Should send ACDM23 to start write process.  Argument is block count.
    validateACmd(23, 3);
    validateSelect();
    // Should send CMD25 to start write process.  Argument is block number.
    validateCmdPacket(25, 43);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFC, 0x22);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFC, 0x33);
    // Should send CMD12 to stop write because of the error.
    validateDeselect();
    validateCmd(12, 0);

    // Retry from third block.
    // Fail on fourth block.
    // Should send ACDM23 to start write process.  Argument is block count.
    validateACmd(23, 2);
    validateSelect();
    // Should send CMD25 to start write process.  Argument is block number.
    validateCmdPacket(25, 44);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFC, 0x33);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFC, 0x44);
    // Should send CMD12 to stop write because of the error.
    validateDeselect();
    validateCmd(12, 0);

    // Retry from fourth block and complete successfully.
    // Should send ACDM23 to start write process.  Argument is block count.
    validateACmd(23, 1);
    validateSelect();
    // Should send CMD25 to start write process.  Argument is block number.
    validateCmdPacket(25, 45);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFC, 0x44);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send stop transmission token.
    STRCMP_EQUAL("FD", m_sd.spi().getOutboundAsString(m_byteIndex++, 1));
    // Deselect as the write is now complete.
    validateDeselect();
    // Should send CMD13 to get R2 write status.
    validateCmd(13, 0, 1);

    // Should have required a maximum of 1 retry for any single block.
    LONGS_EQUAL(1, m_sd.maximumWriteRetryCount());

    // Verify error log output.
    m_sd.dumpErrorLog(stderr);
    char expectedOutput[1024];
    snprintf(expectedOutput, sizeof(expectedOutput),
             "transmitDataBlock(FC,%08X,512) - Data Response=0x0B\n"
             "disk_write(%08X,42,4) - transmitDataBlock failed. block=42\n"
             "transmitDataBlock(FC,%08X,512) - Data Response=0x0B\n"
             "disk_write(%08X,42,4) - transmitDataBlock failed. block=43\n"
             "transmitDataBlock(FC,%08X,512) - Data Response=0x0B\n"
             "disk_write(%08X,42,4) - transmitDataBlock failed. block=44\n"
             "transmitDataBlock(FC,%08X,512) - Data Response=0x0B\n"
             "disk_write(%08X,42,4) - transmitDataBlock failed. block=45\n",
             (uint32_t)(size_t)buffer, (uint32_t)(size_t)buffer,
             (uint32_t)(size_t)buffer + 1*512, (uint32_t)(size_t)buffer,
             (uint32_t)(size_t)buffer + 2*512, (uint32_t)(size_t)buffer,
             (uint32_t)(size_t)buffer + 3*512, (uint32_t)(size_t)buffer);
    STRCMP_EQUAL(expectedOutput, printfSpy_GetLastOutput());
}

// Fail CRC on one block 3 times, should fail.
TEST(DiskWrite, DiskWrite_MultiBlock_FailDataBlockCrcForFirstBlockThreeTimes_ShouldFail_GetLogged_GetCounted)
{
    uint8_t buffer[2*512];

    initSDHC();

    // First failed read of first block.
    // ACMD23 input data.
    setupDataForACmd("00");
    // CMD25 input data.
    setupDataForCmd("00");
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return CRC error for write response token.
    m_sd.spi().setInboundFromString("0B");
    // CMD12 input data.
    setupDataForCmd12("00");

    // Second failed read of first block.
    // ACMD23 input data.
    setupDataForACmd("00");
    // CMD25 input data.
    setupDataForCmd("00");
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return CRC error for write response token.
    m_sd.spi().setInboundFromString("0B");
    // CMD12 input data.
    setupDataForCmd12("00");

    // Third failed read of first block.
    // ACMD23 input data.
    setupDataForACmd("00");
    // CMD25 input data.
    setupDataForCmd("00");
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return CRC error for write response token.
    m_sd.spi().setInboundFromString("0B");
    // CMD12 input data.
    setupDataForCmd12("00");

    // Fill the write buffer with data to write.
    memset(buffer, 0xAD, 512);
    memset(buffer+512, 0xDA, 512);

        LONGS_EQUAL(RES_ERROR, m_sd.disk_write(buffer, 42, 2));

    // Fail read on first block.
    // Should send ACDM23 to start write process.  Argument is block count.
    validateACmd(23, 2);
    validateSelect();
    // Should send CMD25 to start write process.  Argument is block number.
    validateCmdPacket(25, 42);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFC, 0xAD);
    // Should send CMD12 to stop write because of the error.
    validateDeselect();
    validateCmd(12, 0);

    // Fail read on first block.
    // Should send ACDM23 to start write process.  Argument is block count.
    validateACmd(23, 2);
    validateSelect();
    // Should send CMD25 to start write process.  Argument is block number.
    validateCmdPacket(25, 42);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFC, 0xAD);
    // Should send CMD12 to stop write because of the error.
    validateDeselect();
    validateCmd(12, 0);

    // Fail read on first block.
    // Should send ACDM23 to start write process.  Argument is block count.
    validateACmd(23, 2);
    validateSelect();
    // Should send CMD25 to start write process.  Argument is block number.
    validateCmdPacket(25, 42);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFC, 0xAD);
    // Should send CMD12 to stop write because of the error.
    validateDeselect();
    validateCmd(12, 0);

    // Should have required the maximum 3 retries.
    LONGS_EQUAL(3, m_sd.maximumWriteRetryCount());

    // Verify error log output.
    m_sd.dumpErrorLog(stderr);
    char expectedOutput[1024];
    snprintf(expectedOutput, sizeof(expectedOutput),
             "transmitDataBlock(FC,%08X,512) - Data Response=0x0B\n"
             "disk_write(%08X,42,2) - transmitDataBlock failed. block=42\n"
             "transmitDataBlock(FC,%08X,512) - Data Response=0x0B\n"
             "disk_write(%08X,42,2) - transmitDataBlock failed. block=42\n"
             "transmitDataBlock(FC,%08X,512) - Data Response=0x0B\n"
             "disk_write(%08X,42,2) - transmitDataBlock failed. block=42\n",
             (uint32_t)(size_t)buffer, (uint32_t)(size_t)buffer,
             (uint32_t)(size_t)buffer, (uint32_t)(size_t)buffer,
             (uint32_t)(size_t)buffer, (uint32_t)(size_t)buffer);
    STRCMP_EQUAL(expectedOutput, printfSpy_GetLastOutput());
}

TEST(DiskWrite, DiskWrite_MultiBlock_FailDataBlockWithWriteError_RewindBlockPointerForRetry_ShouldSucceed_GetLogged_GetCounted)
{
    uint8_t buffer[4*512];

    initSDHC();

    // On 3rd block write, fail with a write error (not CRC).
    // Return that only 1 block was successful.
    // Driver should retry write from second block which requires rewinding the block pointer back 1 additional block.
    // ACMD23 input data.
    setupDataForACmd("00");
    // CMD25 input data.
    setupDataForCmd("00");
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return successful write response token.
    m_sd.spi().setInboundFromString("05");
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return successful write response token.
    m_sd.spi().setInboundFromString("05");
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return write error for write response token.
    m_sd.spi().setInboundFromString("0D");
    // CMD12 input data.
    setupDataForCmd12("00");
    // ACMD22
    setupDataForACmd("00");
    // 0xFE starts read data block.
    m_sd.spi().setInboundFromString("FE");
    // Return that just 1 block was written successfully.
    setupDataBlock(1);

    // Retry from second block.
    // ACMD23 input data.
    setupDataForACmd("00");
    // CMD25 input data.
    setupDataForCmd("00");
    // Second block.
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return successful write response token.
    m_sd.spi().setInboundFromString("05");
    // Third block.
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return successful write response token.
    m_sd.spi().setInboundFromString("05");
    // Fourth block.
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return successful write response token.
    m_sd.spi().setInboundFromString("05");
    // Sending of stop transmission token.
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // CMD13 input data with successful R2 response.
    setupDataForCmd("00");
    m_sd.spi().setInboundFromString("00");

    // Fill the write buffer with data to write.
    memset(buffer, 0x11, 512);
    memset(buffer + 1*512, 0x22, 512);
    memset(buffer + 2*512, 0x33, 512);
    memset(buffer + 3*512, 0x44, 512);

        LONGS_EQUAL(RES_OK, m_sd.disk_write(buffer, 42, 4));

    // First attempt which will fail with write error on third block.
    // Should send ACDM23 to start write process.  Argument is block count.
    validateACmd(23, 4);
    validateSelect();
    // Should send CMD25 to start write process.  Argument is block number.
    validateCmdPacket(25, 42);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFC, 0x11);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFC, 0x22);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFC, 0x33);
    // Should send CMD12 to stop write because of the error.
    validateDeselect();
    validateCmd(12, 0);
    // Should send ACMD22 (CMD55 + CMD22) to determine blocks written.
    validateCmd(55);
    validateSelect();
    validateCmdPacket(22);
    // Should send multiple FF bytes to read in data block:
    //  1 to read in header.
    //  4 to read data.
    //  2 to read CRC.
    validateFFBytes(1+4+2);
    validateDeselect();

    // Retry from second block and complete successfully.
    // Should send ACDM23 to start write process.  Argument is block count.
    validateACmd(23, 3);
    validateSelect();
    // Should send CMD25 to start write process.  Argument is block number.
    validateCmdPacket(25, 43);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFC, 0x22);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFC, 0x33);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFC, 0x44);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send stop transmission token.
    STRCMP_EQUAL("FD", m_sd.spi().getOutboundAsString(m_byteIndex++, 1));
    // Deselect as the write is now complete.
    validateDeselect();
    // Should send CMD13 to get R2 write status.
    validateCmd(13, 0, 1);

    // Should have required a maximum of 1 retry for any single block.
    LONGS_EQUAL(1, m_sd.maximumWriteRetryCount());

    // Verify error log output.
    m_sd.dumpErrorLog(stderr);
    char expectedOutput[1024];
    snprintf(expectedOutput, sizeof(expectedOutput),
             "transmitDataBlock(FC,%08X,512) - Data Response=0x0D\n"
             "disk_write(%08X,42,4) - transmitDataBlock failed. block=44\n",
             (uint32_t)(size_t)buffer + 2*512, (uint32_t)(size_t)buffer);
    STRCMP_EQUAL(expectedOutput, printfSpy_GetLastOutput());
}

TEST(DiskWrite, DiskWrite_MultiBlock_FailACMD22DuringWriteFailureRecovery_ShouldFail_GetLogged_GetCounted)
{
    uint8_t buffer[2*512];

    initSDHC();

    // On second block write, fail with a write error (not CRC).
    // Fail the subsequent ACMD22 call which attempt to figure out how many blocks were actually written.
    // ACMD23 input data.
    setupDataForACmd("00");
    // CMD25 input data.
    setupDataForCmd("00");
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return successful write response token.
    m_sd.spi().setInboundFromString("05");
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return successful write response token.
    m_sd.spi().setInboundFromString("0D");
    // CMD12 input data.
    setupDataForCmd12("00");
    // ACMD22
    setupDataForCmd("00");
    setupDataForCmd("04");

    // Fill the write buffer with data to write.
    memset(buffer, 0x11, 512);
    memset(buffer + 1*512, 0x22, 512);

        LONGS_EQUAL(RES_ERROR, m_sd.disk_write(buffer, 42, 2));

    // Should send ACDM23 to start write process.  Argument is block count.
    validateACmd(23, 2);
    validateSelect();
    // Should send CMD25 to start write process.  Argument is block number.
    validateCmdPacket(25, 42);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFC, 0x11);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFC, 0x22);
    // Should send CMD12 to stop write because of the error.
    validateDeselect();
    validateCmd(12, 0);
    // Should send ACMD22 (CMD55 + CMD22) to determine blocks written.
    validateCmd(55);
    validateSelect();
    validateCmdPacket(22);
    validateDeselect();

    // Should have required a maximum of 1 retry for any single block.
    LONGS_EQUAL(1, m_sd.maximumWriteRetryCount());

    // Verify error log output.
    m_sd.dumpErrorLog(stderr);
    char expectedOutput[1024];
    snprintf(expectedOutput, sizeof(expectedOutput),
             "transmitDataBlock(FC,%08X,512) - Data Response=0x0D\n"
             "disk_write(%08X,42,2) - transmitDataBlock failed. block=43\n"
             "sendCommandAndReceiveDataBlock(ACMD22,0,X,4) - ACMD22 returned 0x04\n"
             "disk_write(%08X,42,2) - Failed to retrieve written block count.\n",
             (uint32_t)(size_t)buffer + 1*512,
             (uint32_t)(size_t)buffer,
             (uint32_t)(size_t)buffer);

    // Replace internal buffer address in sendCommandAndReceiveDataBlock log entry with an X.
    char actualOutput[1024];
    strcpy(actualOutput, printfSpy_GetLastOutput());
    const char searchString[] = "sendCommandAndReceiveDataBlock(ACMD22,0,";
    char* pAddress = strstr(actualOutput, searchString) + sizeof(searchString) - 1;
    char* pComma = strchr(pAddress+1, ',');
    *pAddress = 'X';
    memmove(pAddress+1, pComma, strlen(pComma)+1);

    STRCMP_EQUAL(expectedOutput, actualOutput);
}

TEST(DiskWrite, DiskWrite_MultiBlock_ReturnTooLargeBlockCountInACMD22Call_ShouldTreatAsZeroBlocks_Succeed_GetLogged_GetCounted)
{
    uint8_t buffer[2*512];

    initSDHC();

    // ACMD23 input data.
    setupDataForACmd("00");
    // CMD25 input data.
    setupDataForCmd("00");
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return successful write response token.
    m_sd.spi().setInboundFromString("05");
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return successful write response token.
    m_sd.spi().setInboundFromString("0D");
    // CMD12 input data.
    setupDataForCmd12("00");
    // ACMD22
    setupDataForACmd("00");
    // 0xFE starts read data block.
    m_sd.spi().setInboundFromString("FE");
    // Return that 3 blocks were written which is larger than 2 requested.
    setupDataBlock(3);

    // Retry from first block.
    // ACMD23 input data.
    setupDataForACmd("00");
    // CMD25 input data.
    setupDataForCmd("00");
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return successful write response token.
    m_sd.spi().setInboundFromString("05");
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return successful write response token.
    m_sd.spi().setInboundFromString("05");
    // Sending of stop transmission token.
    // Return not-busy on first loop in waitWhileBusy().
    m_sd.spi().setInboundFromString("FF");
    // CMD13 input data with successful R2 response.
    setupDataForCmd("00");
    m_sd.spi().setInboundFromString("00");

    // Fill the write buffer with data to write.
    memset(buffer, 0x11, 512);
    memset(buffer + 1*512, 0x22, 512);

        LONGS_EQUAL(RES_OK, m_sd.disk_write(buffer, 42, 2));

    // Should send ACDM23 to start write process.  Argument is block count.
    validateACmd(23, 2);
    // First attempt which will fail with write error on second block
    validateSelect();
    // Should send CMD25 to start write process.  Argument is block number.
    validateCmdPacket(25, 42);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFC, 0x11);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFC, 0x22);
    // Should send CMD12 to stop write because of the error.
    validateDeselect();
    validateCmd(12, 0);
    // Should send ACMD22 (CMD55 + CMD22) to determine blocks written.
    validateCmd(55);
    validateSelect();
    validateCmdPacket(22);
    // Should send multiple FF bytes to read in data block:
    //  1 to read in header.
    //  4 to read data.
    //  2 to read CRC.
    validateFFBytes(1+4+2);
    validateDeselect();

    // Should send ACDM23 to start write process.  Argument is block count.
    validateACmd(23, 2);
    // Retry from first block and complete successfully.
    validateSelect();
    // Should send CMD25 to start write process.  Argument is block number.
    validateCmdPacket(25, 42);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFC, 0x11);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send start block token, buffer data, and CRC.
    validateDataBlock(0xFC, 0x22);
    // Should have sent one 0xFF byte in waitWhileBusy().
    validateFFBytes(1);
    // Should send stop transmission token.
    STRCMP_EQUAL("FD", m_sd.spi().getOutboundAsString(m_byteIndex++, 1));
    // Deselect as the write is now complete.
    validateDeselect();
    // Should send CMD13 to get R2 write status.
    validateCmd(13, 0, 1);

    // Should have required a maximum of 1 retry for any single block.
    LONGS_EQUAL(1, m_sd.maximumWriteRetryCount());

    // Verify error log output.
    m_sd.dumpErrorLog(stderr);
    char expectedOutput[1024];
    snprintf(expectedOutput, sizeof(expectedOutput),
             "transmitDataBlock(FC,%08X,512) - Data Response=0x0D\n"
             "disk_write(%08X,42,2) - transmitDataBlock failed. block=43\n",
             (uint32_t)(size_t)buffer + 1*512, (uint32_t)(size_t)buffer);
    STRCMP_EQUAL(expectedOutput, printfSpy_GetLastOutput());
}
