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

TEST_GROUP_BASE(DiskInit,SDFileSystemBase)
{
};


TEST(DiskInit, VerifyConstructor_SetsUninitStatus_SetsChipSelectHigh_SetsSPIFormat)
{
    validateConstructor();
}

TEST(DiskInit, DiskInit_SuccessfulSDHC)
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
    // ACMD41 input data. Return 0 to indicate not in idle state anymore.
    setupDataForACmd("00");
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
    validateACmd(41, 0x40000000);
    // Should send CMD58 again to read OCR register to determine if the card is high capacity or not.
    validateCmd(58, 0, 4);

    // Should set frequency at end of init process.
    // Verify 25MHz clock rate for SPI.
    CHECK_TRUE(settingsRemaining() >= 1);
    SPIDma::Settings settings = m_sd.spi().getSetting(m_settingsIndex++);
    LONGS_EQUAL(SPIDma::Frequency, settings.type);
    LONGS_EQUAL(25000000, settings.frequency);

    // Verify no longer in NOINIT state.
    LONGS_EQUAL(0, m_sd.disk_status());
    // Verify that card was detected as high capacity where block addresses are SD read/write address.
    LONGS_EQUAL(0, m_sd.blockToAddressShift());
    // Should only loop once watiing for card to not be busy during select().
    LONGS_EQUAL(1, m_sd.maximumWaitWhileBusyTime());
    // Shouldn't have to loop for a valid R1 response.
    LONGS_EQUAL(0, m_sd.maximumWaitForR1ResponseLoopCount());
    // Shouldn't have had to loop for bad CRC.
    LONGS_EQUAL(0, m_sd.maximumCRCRetryCount());
    // Maximum elapsed time for ACMD41 to leave idle state should be short.
    LONGS_EQUAL(1, m_sd.maximumACMD41LoopTime());
    // Maximum elapsed time for receiveDataBlock() should init to 0.
    LONGS_EQUAL(0, m_sd.maximumReceiveDataBlockWaitTime());
    // Maximum read retry count should init to 0.
    LONGS_EQUAL(0, m_sd.maximumReadRetryCount());

}

TEST(DiskInit, DiskInit_SuccessfulSDSCv2)
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
    // ACMD41 input data. Return 0 to indicate not in idle state anymore.
    setupDataForACmd("00");
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
    validateACmd(41, 0x40000000);
    // Should send CMD58 again to read OCR register to determine if the card is high capacity or not.
    validateCmd(58, 0, 4);
    // Should send CMD16 to set the block size to 512 bytes.
    validateCmd(16, 512);

    // Should set frequency at end of init process.
    // Verify 25MHz clock rate for SPI.
    CHECK_TRUE(settingsRemaining() >= 1);
    SPIDma::Settings settings = m_sd.spi().getSetting(m_settingsIndex++);
    LONGS_EQUAL(SPIDma::Frequency, settings.type);
    LONGS_EQUAL(25000000, settings.frequency);

    // Verify no longer in NOINIT state.
    LONGS_EQUAL(0, m_sd.disk_status());
    // Verify that card was detected as low capacity where block addresses need to be converted to 512 bytes per
    // block SD read/write address.
    LONGS_EQUAL(9, m_sd.blockToAddressShift());
    // Should only loop once watiing for card to not be busy during select().
    LONGS_EQUAL(1, m_sd.maximumWaitWhileBusyTime());
    // Shouldn't have to loop for a valid R1 response.
    LONGS_EQUAL(0, m_sd.maximumWaitForR1ResponseLoopCount());
    // Shouldn't have had to loop for bad CRC.
    LONGS_EQUAL(0, m_sd.maximumCRCRetryCount());
    // Maximum elapsed time for ACMD41 to leave idle state should be short.
    LONGS_EQUAL(1, m_sd.maximumACMD41LoopTime());
    // Maximum elapsed time for receiveDataBlock() should init to 0.
    LONGS_EQUAL(0, m_sd.maximumReceiveDataBlockWaitTime());
    // Maximum read retry count should init to 0.
    LONGS_EQUAL(0, m_sd.maximumReadRetryCount());
}

TEST(DiskInit, DiskInit_SuccessfulSDSCv1)
{
    validateConstructor();

    // CMD0 input data.
    setupDataForCmd();
    // CMD59 input data.
    setupDataForCmd();
    // CMD8 input data to indicate illegal command.
    setupDataForCmd("05");
    // CMD58 input data and R3 response (OCR) which is checked for voltage ranges.
    setupDataForCmd();
    m_sd.spi().setInboundFromString("00100000");
    // ACMD41 input data. Return 0 to indicate not in idle state anymore.
    setupDataForACmd("00");
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
    validateCmd(8, 0x1AD);
    // Should send CMD58 to read OCR register and determine voltage levels supported.
    validateCmd(58, 0, 4);
    // Should send ACMD41 (CMD55 + CMD41) to start init process and leave the idle state.
    // The argument to have bit 30 clear for SDv1.
    validateACmd(41, 0x00000000);
    // Should send CMD16 to set the block size to 512 bytes.
    validateCmd(16, 512);

    // Should set frequency at end of init process.
    // Verify 25MHz clock rate for SPI.
    CHECK_TRUE(settingsRemaining() >= 1);
    SPIDma::Settings settings = m_sd.spi().getSetting(m_settingsIndex++);
    LONGS_EQUAL(SPIDma::Frequency, settings.type);
    LONGS_EQUAL(25000000, settings.frequency);

    // Verify no longer in NOINIT state.
    LONGS_EQUAL(0, m_sd.disk_status());
    // Verify that card was detected as low capacity where block addresses need to be converted to 512 bytes per
    // block SD read/write address.
    LONGS_EQUAL(9, m_sd.blockToAddressShift());
    // Should only loop once watiing for card to not be busy during select().
    LONGS_EQUAL(1, m_sd.maximumWaitWhileBusyTime());
    // Shouldn't have to loop for a valid R1 response.
    LONGS_EQUAL(0, m_sd.maximumWaitForR1ResponseLoopCount());
    // Shouldn't have had to loop for bad CRC.
    LONGS_EQUAL(0, m_sd.maximumCRCRetryCount());
    // Maximum elapsed time for ACMD41 to leave idle state should be short.
    LONGS_EQUAL(1, m_sd.maximumACMD41LoopTime());
    // Maximum elapsed time for receiveDataBlock() should init to 0.
    LONGS_EQUAL(0, m_sd.maximumReceiveDataBlockWaitTime());
    // Maximum read retry count should init to 0.
    LONGS_EQUAL(0, m_sd.maximumReadRetryCount());
}


// ************************************
// Exercise select() method code paths.
// ************************************
TEST(DiskInit, DiskInit_RecordMaximumWaitWhileBusyLoopCountDuringSelect_ShouldLoopTwice)
{
    validateConstructor();

    // CMD0 input data.
    setupDataForCmd();

    // CMD59 input data.
    // Force waitWhileBusy() to loop twice for 0xFF.
    m_sd.spi().setInboundFromString("0000FF");
    // Return indicated R1 response.
    m_sd.spi().setInboundFromString("01");

    // CMD8 input data and R7 response.
    setupDataForCmd();
    m_sd.spi().setInboundFromString("000001AD");
    // CMD58 input data and R3 response (OCR) which is checked for voltage ranges.
    setupDataForCmd();
    m_sd.spi().setInboundFromString("00100000");
    // ACMD41 input data. Return 0 to indicate not in idle state anymore.
    setupDataForACmd("00");
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
    // Verify timed out select transaction.
    // Should have set chip select low.
    CHECK_TRUE(settingsRemaining() >= 1);
    SPIDma::Settings settings = m_sd.spi().getSetting(m_settingsIndex++);
    LONGS_EQUAL(SPIDma::ChipSelect, settings.type);
    LONGS_EQUAL(LOW, settings.chipSelect);
    LONGS_EQUAL(m_byteIndex, settings.bytesSentBefore);
    // Should write one 0xFF byte to card to prime it for communication.
    STRCMP_EQUAL("FF", m_sd.spi().getOutboundAsString(m_byteIndex++, 1));
    // Should write 0xFF until 0xFF is received to indicate that the card is no longer busy.
    STRCMP_EQUAL("FFFF", m_sd.spi().getOutboundAsString(m_byteIndex, 2));
    m_byteIndex += 2;
    validateCmdPacket(59, 1);
    validateDeselect();

    // Should send CMD8 to determine if the card is SDv2 or SDv1 card.
    // The argument should be 0x1AD to select 2.7 - 3.6V range and us 0xAD as check pattern.
    validateCmd(8, 0x1AD, 4);
    // Should send CMD58 to read OCR register and determine voltage levels supported.
    validateCmd(58, 0, 4);
    // Should send ACMD41 (CMD55 + CMD41) to start init process and leave the idle state.
    // The argument to have bit 30 set to indicate that this host support high capacity disks.
    validateACmd(41, 0x40000000);
    // Should send CMD58 again to read OCR register to determine if the card is high capacity or not.
    validateCmd(58, 0, 4);

    // Should set frequency at end of init process.
    CHECK_TRUE(settingsRemaining() >= 1);
    // Verify 25MHz clock rate for SPI.
    settings = m_sd.spi().getSetting(m_settingsIndex++);
    LONGS_EQUAL(SPIDma::Frequency, settings.type);
    LONGS_EQUAL(25000000, settings.frequency);

    // Should loop twice waiting for card to not be busy during select().
    LONGS_EQUAL(2, m_sd.maximumWaitWhileBusyTime());
}

TEST(DiskInit, DiskInit_TimeOutTheWaitWhileBusyLoopDuringSelect_ShouldFailAndLogFailure)
{
    validateConstructor();

    // CMD0 input data.
    // select() expects to receive a response which is not 0xFF for the first byte read.
    m_sd.spi().setInboundFromString("00");
   // Force waitWhileBusy() to loop twice, waiting for 0xFF.
    m_sd.spi().setInboundFromString("0000");

    // Set timer to elapse 250 msec / call so that second iteration of waitWhileBusy() should timeout.
    m_sd.timer().setElapsedTimePerCall(250);

        // The disk_initialize call should fail since the CMD0 failed.
        LONGS_EQUAL(STA_NOINIT, m_sd.disk_initialize());

    // Verify 400kHz clock rate for SPI.
    // Verify chip select is set high while 80 > 74 clocks are sent to chip during powerup.
    validate400kHzClockAnd80PrimingClockEdges();

    // Verify timed out select transaction.
    // Should have set chip select low.
    CHECK_TRUE(settingsRemaining() >= 1);
    SPIDma::Settings settings = m_sd.spi().getSetting(m_settingsIndex++);
    LONGS_EQUAL(SPIDma::ChipSelect, settings.type);
    LONGS_EQUAL(LOW, settings.chipSelect);
    LONGS_EQUAL(m_byteIndex, settings.bytesSentBefore);
    // Should write one 0xFF byte to card to prime it for communication.
    STRCMP_EQUAL("FF", m_sd.spi().getOutboundAsString(m_byteIndex++, 1));
    // Should write 0xFF until 0xFF is received to indicate that the card is no longer busy.
    STRCMP_EQUAL("FFFF", m_sd.spi().getOutboundAsString(m_byteIndex, 2));
    m_byteIndex += 2;
    validateDeselect();

    // The 500 msec delay for two cycles should be recorded.
    LONGS_EQUAL(500, m_sd.maximumWaitWhileBusyTime());

    // Verify error log output.
    m_sd.dumpErrorLog(stderr);
    STRCMP_EQUAL("waitWhileBusy(500) - Time out. Response=0x00\n"
                 "select() - 500 msec time out\n"
                 "cmd(CMD0,0,0) - Select timed out\n"
                 "disk_initialize() - CMD0 returned 0xFF. Is card inserted?\n",
                 printfSpy_GetLastOutput());
}



// *******************************************************
// Exercise sendCommandAndGetResponse() method code paths.
// *******************************************************
TEST(DiskInit, DiskInit_MakeSendCommandAndGetResponseLoopOnceForR1Response_ShouldSucceed_Counted)
{
    validateConstructor();

    // CMD0 input data - Force it to loop once and wait for R1 response.
    // select() expects to receive a response which is not 0xFF for the first byte read.
    m_sd.spi().setInboundFromString("00");
    // Return not-busy on first loop in waitForNotBusy().
    m_sd.spi().setInboundFromString("FF");
    // Make sendCommandAndGetResponse() loop once waiting for valid R1 response with high bit clear.
    m_sd.spi().setInboundFromString("8001");

    // CMD59 input data.
    setupDataForCmd();
    // CMD8 input data and R7 response.
    setupDataForCmd();
    m_sd.spi().setInboundFromString("000001AD");
    // CMD58 input data and R3 response (OCR) which is checked for voltage ranges.
    setupDataForCmd();
    m_sd.spi().setInboundFromString("00100000");
    // ACMD41 input data. Return 0 to indicate not in idle state anymore.
    setupDataForACmd("00");
    // CMD58 input data and R3 response (OCR) which is checked for high capacity disk.
    // Return with high capacity (CCS) bit set to indicate SDHC/SDXC disk.
    setupDataForCmd();
    m_sd.spi().setInboundFromString("40000000");

        LONGS_EQUAL(0, m_sd.disk_initialize());

    // Verify 400kHz clock rate for SPI.
    // Verify chip select is set high while 80 > 74 clocks are sent to chip during powerup.
    validate400kHzClockAnd80PrimingClockEdges();

    // Should send CMD0 to reset card into idle state.
    validateSelect();
    validateCmdPacket(0);
    validateFFBytes(1);
    validateDeselect();
    // Should send CMD59 to enable CRC.  The argument should be 0x00000001 to enable it.
    validateCmd(59, 1);
    // Should send CMD8 to determine if the card is SDv2 or SDv1 card.
    // The argument should be 0x1AD to select 2.7 - 3.6V range and us 0xAD as check pattern.
    validateCmd(8, 0x1AD, 4);
    // Should send CMD58 to read OCR register and determine voltage levels supported.
    validateCmd(58, 0, 4);
    // Should send ACMD41 (CMD55 + CMD41) to start init process and leave the idle state.
    // The argument to have bit 30 set to indicate that this host support high capacity disks.
    validateACmd(41, 0x40000000);
    // Should send CMD58 again to read OCR register to determine if the card is high capacity or not.
    validateCmd(58, 0, 4);

    // Should set frequency at end of init process.
    CHECK_TRUE(settingsRemaining() >= 1);
    // Verify 25MHz clock rate for SPI.
    SPIDma::Settings settings = m_sd.spi().getSetting(m_settingsIndex++);
    LONGS_EQUAL(SPIDma::Frequency, settings.type);
    LONGS_EQUAL(25000000, settings.frequency);

    // Make sure that that the fact we looped once with invalid response was recorded.
    LONGS_EQUAL(1, m_sd.maximumWaitForR1ResponseLoopCount());
}

TEST(DiskInit, DiskInit_MakeSendCommandAndGetResponseLoopTooManyTimeForR1Response_ShouldFail_GetLogged_AndCounted)
{
    validateConstructor();

    // CMD0 input data - Force it to loop ten times and wait for R1 response.
    // select() expects to receive a response which is not 0xFF for the first byte read.
    m_sd.spi().setInboundFromString("00");
    // Return not-busy on first loop in waitForNotBusy().
    m_sd.spi().setInboundFromString("FF");
    // Make sendCommandAndGetResponse() loop ten times waiting for valid R1 response with high bit clear.
    m_sd.spi().setInboundFromString("80808080808080808080");

        LONGS_EQUAL(STA_NOINIT, m_sd.disk_initialize());

    // Verify 400kHz clock rate for SPI.
    // Verify chip select is set high while 80 > 74 clocks are sent to chip during powerup.
    validate400kHzClockAnd80PrimingClockEdges();

    // Should send CMD0 to reset card into idle state.
    validateSelect();
    validateCmdPacket(0);
    validateFFBytes(9);
    validateDeselect();

    // Make sure that that the fact we looped ten times was recorded.
    LONGS_EQUAL(10, m_sd.maximumWaitForR1ResponseLoopCount());

    m_sd.dumpErrorLog(stderr);
    STRCMP_EQUAL("sendCommandAndGetResponse(CMD0,0,0) - Timed out waiting for valid R1 response. r1Response=0x80\n"
                 "disk_initialize() - CMD0 returned 0xFF. Is card inserted?\n",
                 printfSpy_GetLastOutput());
}

TEST(DiskInit, DiskInit_MakeSendCommandAndGetResponseLoopOnceForCMDCrcFailure_ShouldSucceed_Counted)
{
    validateConstructor();

    // CMD0 input data - Force it to loop once for CRC error response.
    // select() expects to receive a response which is not 0xFF for the first byte read.
    m_sd.spi().setInboundFromString("00");
    // Return not-busy on first loop in waitForNotBusy().
    m_sd.spi().setInboundFromString("FF");
    // Make sendCommandAndGetResponse() loop once for CRC failure.
    m_sd.spi().setInboundFromString("0801");

    // CMD59 input data.
    setupDataForCmd();
    // CMD8 input data and R7 response.
    setupDataForCmd();
    m_sd.spi().setInboundFromString("000001AD");
    // CMD58 input data and R3 response (OCR) which is checked for voltage ranges.
    setupDataForCmd();
    m_sd.spi().setInboundFromString("00100000");
    // ACMD41 input data. Return 0 to indicate not in idle state anymore.
    setupDataForACmd("00");
    // CMD58 input data and R3 response (OCR) which is checked for high capacity disk.
    // Return with high capacity (CCS) bit set to indicate SDHC/SDXC disk.
    setupDataForCmd();
    m_sd.spi().setInboundFromString("40000000");

        LONGS_EQUAL(0, m_sd.disk_initialize());

    // Verify 400kHz clock rate for SPI.
    // Verify chip select is set high while 80 > 74 clocks are sent to chip during powerup.
    validate400kHzClockAnd80PrimingClockEdges();

    // Should send CMD0 to reset card into idle state.
    validateSelect();
    validateCmdPacket(0);
    validateCmdPacket(0);
    validateDeselect();
    // Should send CMD59 to enable CRC.  The argument should be 0x00000001 to enable it.
    validateCmd(59, 1);
    // Should send CMD8 to determine if the card is SDv2 or SDv1 card.
    // The argument should be 0x1AD to select 2.7 - 3.6V range and us 0xAD as check pattern.
    validateCmd(8, 0x1AD, 4);
    // Should send CMD58 to read OCR register and determine voltage levels supported.
    validateCmd(58, 0, 4);
    // Should send ACMD41 (CMD55 + CMD41) to start init process and leave the idle state.
    // The argument to have bit 30 set to indicate that this host support high capacity disks.
    validateACmd(41, 0x40000000);
    // Should send CMD58 again to read OCR register to determine if the card is high capacity or not.
    validateCmd(58, 0, 4);

    // Should set frequency at end of init process.
    // Verify 25MHz clock rate for SPI.
    CHECK_TRUE(settingsRemaining() >= 1);
    SPIDma::Settings settings = m_sd.spi().getSetting(m_settingsIndex++);
    LONGS_EQUAL(SPIDma::Frequency, settings.type);
    LONGS_EQUAL(25000000, settings.frequency);

    // Didn't send invalid R1 response.
    LONGS_EQUAL(0, m_sd.maximumWaitForR1ResponseLoopCount());
    // Check CRC count.
    LONGS_EQUAL(1, m_sd.maximumCRCRetryCount());
}

TEST(DiskInit, DiskInit_MakeSendCommandAndGetResponseLoopFourTimesToCrcFailure_ShouldFail_GetLogged_Counted)
{
    validateConstructor();

    // CMD0 input data - Force it to loop four times for CRC error response.
    // select() expects to receive a response which is not 0xFF for the first byte read.
    m_sd.spi().setInboundFromString("00");
    // Return not-busy on first loop in waitForNotBusy().
    m_sd.spi().setInboundFromString("FF");
    // Make sendCommandAndGetResponse() loop four times because of CRC failure.
    m_sd.spi().setInboundFromString("08080808");

        LONGS_EQUAL(STA_NOINIT, m_sd.disk_initialize());

    // Verify 400kHz clock rate for SPI.
    // Verify chip select is set high while 80 > 74 clocks are sent to chip during powerup.
    validate400kHzClockAnd80PrimingClockEdges();

    // Should send CMD0 to reset card into idle state.
    validateSelect();
    validateCmdPacket(0);
    validateCmdPacket(0);
    validateCmdPacket(0);
    validateCmdPacket(0);
    validateDeselect();

    // Didn't send invalid R1 response.
    LONGS_EQUAL(0, m_sd.maximumWaitForR1ResponseLoopCount());
    // Check CRC count.
    LONGS_EQUAL(4, m_sd.maximumCRCRetryCount());

    m_sd.dumpErrorLog(stderr);
    STRCMP_EQUAL("sendCommandAndGetResponse(CMD0,0,0) - Failed CRC check 4 times\n"
                 "disk_initialize() - CMD0 returned 0x08. Is card inserted?\n",
                 printfSpy_GetLastOutput());
}

TEST(DiskInit, DiskInit_MakeSendCommandAndGetResponseRetrieveErrorResponse_ShouldFail_GetLogged)
{
    validateConstructor();

    // CMD0 input data.
    // Make sendCommandAndGetResponse() fail for non-CRC error.
    setupDataForCmd("77");

        LONGS_EQUAL(STA_NOINIT, m_sd.disk_initialize());

    // Verify 400kHz clock rate for SPI.
    // Verify chip select is set high while 80 > 74 clocks are sent to chip during powerup.
    validate400kHzClockAnd80PrimingClockEdges();
    // Should send CMD0 to reset card into idle state.
    validateCmd(0);

    // Didn't send invalid R1 response.
    LONGS_EQUAL(0, m_sd.maximumWaitForR1ResponseLoopCount());
    // Check CRC count.
    LONGS_EQUAL(0, m_sd.maximumCRCRetryCount());

    m_sd.dumpErrorLog(stderr);
    STRCMP_EQUAL("disk_initialize() - CMD0 returned 0x77. Is card inserted?\n",
                 printfSpy_GetLastOutput());
}

TEST(DiskInit, DiskInit_MakeSendCommandAndGetResponseRetrieveErrorResponseForCmd55_ShouldFail_GetLogged)
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
    // CMD55 input data - Return error response.
    setupDataForCmd("77");

        LONGS_EQUAL(STA_NOINIT, m_sd.disk_initialize());

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

    // Didn't send invalid R1 response.
    LONGS_EQUAL(0, m_sd.maximumWaitForR1ResponseLoopCount());
    // Check CRC count.
    LONGS_EQUAL(0, m_sd.maximumCRCRetryCount());

    m_sd.dumpErrorLog(stderr);
    STRCMP_EQUAL("sendCommandAndGetResponse(ACMD41,40000000,0) - CMD55 prefix returned 0x77\n"
                 "disk_initialize() - ACMD41 returned 0x77\n",
                 printfSpy_GetLastOutput());
}

TEST(DiskInit, DiskInit_MakeSendCommandAndGetResponseFailSelectAfterCMD55_ShouldFail_GetLogged)
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
    // CMD55 input data - Time out the second select() call, before 41 command gets sent.
    // select() expects to receive a response which is not 0xFF for the first byte read.
    m_sd.spi().setInboundFromString("00");
    // Return not-busy on first loop in waitForNotBusy().
    m_sd.spi().setInboundFromString("FF");
    // Return successful response.
    m_sd.spi().setInboundFromString("00");
    // select() expects to receive a response which is not 0xFF for the first byte read.
    m_sd.spi().setInboundFromString("00");
   // Force waitWhileBusy() to loop twice, waiting for 0xFF.
    m_sd.spi().setInboundFromString("0000");

    // Set timer to elapse 250 msec / call so that second iteration of waitWhileBusy() should timeout.
    m_sd.timer().setElapsedTimePerCall(250);

        LONGS_EQUAL(STA_NOINIT, m_sd.disk_initialize());

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

    // ACMD41 which is going to fail.
    // Should send CMD55
    validateCmd(55);
    // CMD41 part will just get to sending 3 bytes for timing out select() call.
    //    Should have set chip select low.
    //   Assert ChipSelect to LOW.
    CHECK_TRUE(settingsRemaining() >= 1);
    SPIDma::Settings settings = m_sd.spi().getSetting(m_settingsIndex++);
    LONGS_EQUAL(SPIDma::ChipSelect, settings.type);
    LONGS_EQUAL(LOW, settings.chipSelect);
    LONGS_EQUAL(m_byteIndex, settings.bytesSentBefore);
    // Will send 3 0xFF bytes as it times out on select() call.
    STRCMP_EQUAL("FFFFFF", m_sd.spi().getOutboundAsString(m_byteIndex, 3));
    m_byteIndex += 3;
    validateDeselect();
    // An extra deselect occurs after catching the select() failure.  That is ok as it has no negative impact.
    validateDeselect();

    // The 500 msec delay for two cycles should be recorded.
    LONGS_EQUAL(500, m_sd.maximumWaitWhileBusyTime());
    // Didn't send invalid R1 response.
    LONGS_EQUAL(0, m_sd.maximumWaitForR1ResponseLoopCount());
    // Check CRC count.
    LONGS_EQUAL(0, m_sd.maximumCRCRetryCount());

    m_sd.dumpErrorLog(stderr);
    STRCMP_EQUAL("waitWhileBusy(500) - Time out. Response=0x00\n"
                 "select() - 500 msec time out\n"
                 "sendCommandAndGetResponse(ACMD41,40000000,0) - CMD55 prefix select timed out\n"
                 "disk_initialize() - ACMD41 returned 0xFF\n",
                 printfSpy_GetLastOutput());
}


// **************************************************************
// Fail various SD commands during the disk_initialize() process.
// **************************************************************
TEST(DiskInit, DiskInit_FailCMD0ByNotReturningIdleResponse_ShouldFail_GetLogged)
{
    validateConstructor();

    // CMD0 input data
    // select() expects to receive a response which is not 0xFF for the first byte read.
    m_sd.spi().setInboundFromString("00");
    // Return not-busy on first loop in waitForNotBusy().
    m_sd.spi().setInboundFromString("FF");
    // Make sendCommandAndGetResponse() return without idle bit set.
    m_sd.spi().setInboundFromString("00");

        LONGS_EQUAL(STA_NOINIT, m_sd.disk_initialize());

    // Verify 400kHz clock rate for SPI.
    // Verify chip select is set high while 80 > 74 clocks are sent to chip during powerup.
    validate400kHzClockAnd80PrimingClockEdges();
    // Should send CMD0 to reset card into idle state.
    validateCmd(0);

    m_sd.dumpErrorLog(stderr);
    STRCMP_EQUAL("disk_initialize() - CMD0 returned 0x00. Is card inserted?\n",
                 printfSpy_GetLastOutput());
}

TEST(DiskInit, DiskInit_FailCMD59ByNotReturningIdleResponse_ShouldFail_GetLogged)
{
    validateConstructor();

    // CMD0 input data.
    setupDataForCmd();

    // CMD59 input data.
    // select() expects to receive a response which is not 0xFF for the first byte read.
    m_sd.spi().setInboundFromString("00");
    // Return not-busy on first loop in waitForNotBusy().
    m_sd.spi().setInboundFromString("FF");
    // Make sendCommandAndGetResponse() return without idle bit set.
    m_sd.spi().setInboundFromString("00");

        LONGS_EQUAL(STA_NOINIT, m_sd.disk_initialize());

    // Verify 400kHz clock rate for SPI.
    // Verify chip select is set high while 80 > 74 clocks are sent to chip during powerup.
    validate400kHzClockAnd80PrimingClockEdges();
    // Should send CMD0 to reset card into idle state.
    validateCmd(0);
    // Should send CMD59 to enable CRC.  The argument should be 0x00000001 to enable it.
    validateCmd(59, 1);

    m_sd.dumpErrorLog(stderr);
    STRCMP_EQUAL("disk_initialize() - CMD59 returned 0x00\n",
                 printfSpy_GetLastOutput());
}

TEST(DiskInit, DiskInit_FailCMD8ByReturningDifferentVoltageMask_ShouldFail_GetLogged)
{
    validateConstructor();

    // CMD0 input data.
    setupDataForCmd();
    // CMD59 input data.
    setupDataForCmd();

    // CMD8 input data and R7 response with wrong voltage bitmask.
    setupDataForCmd();
    m_sd.spi().setInboundFromString("000000AD");

        LONGS_EQUAL(STA_NOINIT, m_sd.disk_initialize());

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

    m_sd.dumpErrorLog(stderr);
    STRCMP_EQUAL("disk_initialize() - CMD8 returned 0x000000AD (expected 0x000001AD)\n",
                 printfSpy_GetLastOutput());
}

TEST(DiskInit, DiskInit_FailCMD8ByReturningDifferentCheckValue_ShouldFail_GetLogged)
{
    validateConstructor();

    // CMD0 input data.
    setupDataForCmd();
    // CMD59 input data.
    setupDataForCmd();

    // CMD8 input data and R7 response with wrong voltage bitmask.
    setupDataForCmd();
    m_sd.spi().setInboundFromString("000001AC");

        LONGS_EQUAL(STA_NOINIT, m_sd.disk_initialize());

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

    m_sd.dumpErrorLog(stderr);
    STRCMP_EQUAL("disk_initialize() - CMD8 returned 0x000001AC (expected 0x000001AD)\n",
                 printfSpy_GetLastOutput());
}

TEST(DiskInit, DiskInit_FailCMD8ByReturningErrorOtherThanIllegalCommand_ShouldFail_GetLogged)
{
    validateConstructor();

    // CMD0 input data.
    setupDataForCmd();
    // CMD59 input data.
    setupDataForCmd();

    // CMD8 input data with error result.
    // select() expects to receive a response which is not 0xFF for the first byte read.
    m_sd.spi().setInboundFromString("00");
    // Return not-busy on first loop in waitForNotBusy().
    m_sd.spi().setInboundFromString("FF");
    // Make sendCommandAndGetResponse() return with error code.
    m_sd.spi().setInboundFromString("02");

        LONGS_EQUAL(STA_NOINIT, m_sd.disk_initialize());

    // Verify 400kHz clock rate for SPI.
    // Verify chip select is set high while 80 > 74 clocks are sent to chip during powerup.
    validate400kHzClockAnd80PrimingClockEdges();
    // Should send CMD0 to reset card into idle state.
    validateCmd(0);
    // Should send CMD59 to enable CRC.  The argument should be 0x00000001 to enable it.
    validateCmd(59, 1);
    // Should send CMD8 to determine if the card is SDv2 or SDv1 card.
    // The argument should be 0x1AD to select 2.7 - 3.6V range and us 0xAD as check pattern.
    validateCmd(8, 0x1AD);

    m_sd.dumpErrorLog(stderr);
    STRCMP_EQUAL("disk_initialize() - CMD8 returned 0x02\n",
                 printfSpy_GetLastOutput());
}

TEST(DiskInit, DiskInit_FailCMD58ByNotReturningIdleResponse_ShouldFail_GetLogged)
{
    validateConstructor();

    // CMD0 input data.
    setupDataForCmd();
    // CMD59 input data.
    setupDataForCmd();
    // CMD8 input data and R7 response.
    setupDataForCmd();
    m_sd.spi().setInboundFromString("000001AD");

    // CMD58 input data with non-idle response.
    // select() expects to receive a response which is not 0xFF for the first byte read.
    m_sd.spi().setInboundFromString("00");
    // Return not-busy on first loop in waitForNotBusy().
    m_sd.spi().setInboundFromString("FF");
    // Make sendCommandAndGetResponse() return without idle bit set.
    m_sd.spi().setInboundFromString("00");
    // R7 OCR response.
    // UNDONE: Set to a valid power value.
    m_sd.spi().setInboundFromString("00000000");

        LONGS_EQUAL(STA_NOINIT, m_sd.disk_initialize());

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

    m_sd.dumpErrorLog(stderr);
    STRCMP_EQUAL("disk_initialize() - CMD58 returned 0x00 during voltage check\n",
                 printfSpy_GetLastOutput());
}

TEST(DiskInit, DiskInit_FailCMD58ByNotReturningSupportVoltageRange_ShouldFail_GetLogged)
{
    validateConstructor();

    // CMD0 input data.
    setupDataForCmd();
    // CMD59 input data.
    setupDataForCmd();
    // CMD8 input data and R7 response.
    setupDataForCmd();
    m_sd.spi().setInboundFromString("000001AD");

    // CMD58 input data with non-idle response.
    setupDataForCmd("01");
    // R7 OCR response with everything but 3.2-3.3V not valid.
    m_sd.spi().setInboundFromString("01EF8000");

        LONGS_EQUAL(STA_NOINIT, m_sd.disk_initialize());

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

    m_sd.dumpErrorLog(stderr);
    STRCMP_EQUAL("disk_initialize() - CMD58 3.3V not supported. OCR=0x01EF8000\n",
                 printfSpy_GetLastOutput());
}

TEST(DiskInit, DiskInit_MakeACMD41LoopTwiceBeforeLeavingIdleState_ShouldSucceed)
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

    // ACMD41 - Loop twice to get into idle state.
    // Still idle.
    setupDataForACmd("01");
    // Return not idle.
    setupDataForACmd("00");

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
    validateACmd(41, 0x40000000);
    // Should send ACMD41 (CMD55 + CMD41) to start init process and leave the idle state.
    // The argument to have bit 30 set to indicate that this host support high capacity disks.
    validateACmd(41, 0x40000000);
    // Should send CMD58 again to read OCR register to determine if the card is high capacity or not.
    validateCmd(58, 0, 4);

    // Should set frequency at end of init process.
    // Verify 25MHz clock rate for SPI.
    CHECK_TRUE(settingsRemaining() >= 1);
    SPIDma::Settings settings = m_sd.spi().getSetting(m_settingsIndex++);
    LONGS_EQUAL(SPIDma::Frequency, settings.type);
    LONGS_EQUAL(25000000, settings.frequency);

    // Make sure that it has recorded the time for two iterations.
    LONGS_EQUAL(2, m_sd.maximumACMD41LoopTime());

    // Verify no longer in NOINIT state.
    LONGS_EQUAL(0, m_sd.disk_status());
    // Verify that card was detected as high capacity where block addresses are SD read/write address.
    LONGS_EQUAL(0, m_sd.blockToAddressShift());
}

TEST(DiskInit, DiskInit_MakeACMD41TimeOut_ShouldFail_GetLogged)
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

    // ACMD41 - Loop twice to get into idle state.
    // Still idle.
    setupDataForACmd("01");
    // Still idle.
    setupDataForACmd("01");

    // Set timer to elapse 500 msec / call so that second iteration of ACMD41 should timeout.
    m_sd.timerOuter().setElapsedTimePerCall(500);

        LONGS_EQUAL(STA_NOINIT, m_sd.disk_initialize());

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
    validateACmd(41, 0x40000000);
    // Should send ACMD41 (CMD55 + CMD41) to start init process and leave the idle state.
    // The argument to have bit 30 set to indicate that this host support high capacity disks.
    validateACmd(41, 0x40000000);

    // Make sure that it has recorded the time for two iterations.
    LONGS_EQUAL(1000, m_sd.maximumACMD41LoopTime());

    m_sd.dumpErrorLog(stderr);
    STRCMP_EQUAL("disk_initialize() - ACMD41 timed out attempting to leave idle state\n",
                 printfSpy_GetLastOutput());
}

TEST(DiskInit, DiskInit_FailLastCMD58WithError_ShouldFail_GetLogged)
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
    // ACMD41 input data. Return 0 to indicate not in idle state anymore.
    setupDataForACmd("00");

    // CMD58 input data and R3 response (OCR) which is checked for high capacity disk.
    // Fail with error code.
    setupDataForCmd("02");

        LONGS_EQUAL(STA_NOINIT, m_sd.disk_initialize());

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
    validateACmd(41, 0x40000000);
    // Should send CMD58 again to read OCR register to determine if the card is high capacity or not.
    validateCmd(58, 0);

    m_sd.dumpErrorLog(stderr);
    STRCMP_EQUAL("disk_initialize() - CMD58 returned 0x02 during capacity check\n",
                 printfSpy_GetLastOutput());
}

TEST(DiskInit, DiskInit_FailCMD16WithError_ShouldFail_GetLogged)
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
    // ACMD41 input data. Return 0 to indicate not in idle state anymore.
    setupDataForACmd("00");
    // CMD58 input data and R3 response (OCR) which is checked for high capacity disk.
    setupDataForCmd();
    m_sd.spi().setInboundFromString("00000000");
    // CMD16 input data.  Return error.
    setupDataForCmd("02");

        LONGS_EQUAL(STA_NOINIT, m_sd.disk_initialize());

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
    validateACmd(41, 0x40000000);
    // Should send CMD58 again to read OCR register to determine if the card is high capacity or not.
    validateCmd(58, 0, 4);
    // Should send CMD16 to set the block size to 512 bytes.
    validateCmd(16, 512);

    m_sd.dumpErrorLog(stderr);
    STRCMP_EQUAL("disk_initialize() - CMD16 returned 0x02\n",
                 printfSpy_GetLastOutput());
}
