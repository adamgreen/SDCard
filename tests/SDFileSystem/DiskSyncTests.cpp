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

TEST_GROUP_BASE(DiskSync,SDFileSystemBase)
{
};


TEST(DiskSync, DiskSync_ShouldSucceed)
{
    initSDHC();

    // select() expects to receive a response which is not 0xFF for the first byte read.
    m_sd.spi().setInboundFromString("00");
    // Return not-busy on first loop in waitForNotBusy().
    m_sd.spi().setInboundFromString("FF");

        LONGS_EQUAL(RES_OK, m_sd.disk_sync());

    // Should have set chip select low and then back to high again.
    validateSelect();
    validateDeselect();
}

TEST(DiskSync, DiskSync_SelectTimeout_ShouldFail_GetLogged)
{
    initSDHC();

    // select() expects to receive a response which is not 0xFF for the first byte read.
    m_sd.spi().setInboundFromString("00");
    // Return busy on two loops through waitForNotBusy().
    m_sd.spi().setInboundFromString("0000");

    // Set timer to elapse 250 msec / call so that second iteration of waitWhileBusy() should timeout.
    m_sd.timer().setElapsedTimePerCall(250);

        LONGS_EQUAL(RES_ERROR, m_sd.disk_sync());

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
    // Should still deselect after detecting error.
    validateDeselect();

    // The 500 msec delay for two cycles should be recorded.
    LONGS_EQUAL(500, m_sd.maximumWaitWhileBusyTime());

    // Verify error log output.
    m_sd.dumpErrorLog(stderr);
    STRCMP_EQUAL("waitWhileBusy(500) - Time out. Response=0x00\n"
                 "select() - 500 msec time out\n"
                 "disk_sync() - Failed waiting for not busy\n",
                 printfSpy_GetLastOutput());
}
