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

TEST_GROUP_BASE(ErrorLog,SDFileSystemBase)
{
};


TEST(ErrorLog, IsErrorLogEmpty_ClearErrorLog)
{
    validateConstructor();

    // CMD0 input data.
    // Make sendCommandAndGetResponse() fail for non-CRC error.
    setupDataForCmd("77");

    // Error log should be empty before calling init.
    CHECK_TRUE(m_sd.isErrorLogEmpty());

        LONGS_EQUAL(STA_NOINIT, m_sd.disk_initialize());

    // Verify that it is no longer empty.
    CHECK_FALSE(m_sd.isErrorLogEmpty());
    m_sd.dumpErrorLog(stderr);
    STRCMP_EQUAL("disk_initialize() - CMD0 returned 0x77. Is card inserted?\n",
                 printfSpy_GetLastOutput());

    // Verify that is it empty again after clearing.
    m_sd.clearErrorLog();
    CHECK_TRUE(m_sd.isErrorLogEmpty());

    // Verify 400kHz clock rate for SPI.
    // Verify chip select is set high while 80 > 74 clocks are sent to chip during powerup.
    validate400kHzClockAnd80PrimingClockEdges();
    // Should send CMD0 to reset card into idle state.
    validateCmd(0);

    // Didn't send invalid R1 response.
    LONGS_EQUAL(0, m_sd.maximumWaitForR1ResponseLoopCount());
    // Check CRC count.
    LONGS_EQUAL(0, m_sd.maximumCRCRetryCount());
}
