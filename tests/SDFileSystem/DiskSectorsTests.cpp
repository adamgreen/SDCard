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

TEST_GROUP_BASE(DiskSectors,SDFileSystemBase)
{
};


TEST(DiskSectors, DiskSectors_AttemptBeforeInit_ShouldFail_GetLogged)
{
    LONGS_EQUAL(0, m_sd.disk_sectors());

    // Only the constructor should have generated any SPI traffic.
    validateConstructor();

    m_sd.dumpErrorLog(stderr);
    STRCMP_EQUAL("disk_sectors() - Attempt to query uninitialized drive\n", printfSpy_GetLastOutput());
}

TEST(DiskSectors, DiskSectors_SDv1_ShouldSucceed)
{
    initSDHC();

    // CMD9 input data.
    setupDataForCmd("00");
    // 0xFE starts read data block.
    m_sd.spi().setInboundFromString("FE");
    // Data block will contain 16 bytes of 0x3F + valid CRC.
    setupDataBlock(0x3F, 16);

    uint8_t csd[16];
    memset(csd, 0x3F, sizeof(csd));
    uint32_t expectedSectorCount = ((csd[8] >> 6) + ((uint32_t)csd[7] << 2) + ((uint32_t)(csd[6] & 3) << 10) + 1) <<
                                   (((csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2) - 9);
        LONGS_EQUAL(expectedSectorCount, m_sd.disk_sectors());

    validateSelect();
    // Should send CMD9 to start read.
    validateCmdPacket(9);
    // Should send multiple FF bytes to read in register data block:
    //  1 to read in header.
    //  16 to read data.
    //  2 to read CRC.
    validateFFBytes(1+16+2);
    validateDeselect();
}

TEST(DiskSectors, DiskSectors_SDv2_ShouldSucceed)
{
    initSDHC();

    // CMD9 input data.
    setupDataForCmd("00");
    // 0xFE starts read data block.
    m_sd.spi().setInboundFromString("FE");
    // Data block will contain 16 bytes of 0x3F + valid CRC.
    setupDataBlock(0x7F, 16);

    uint8_t csd[16];
    memset(csd, 0x7F, sizeof(csd));
    uint32_t expectedSectorCount = csd[9] + ((uint32_t)csd[8] << 8) + ((uint32_t)(csd[7] & 63) << 16) + 1;
        LONGS_EQUAL(expectedSectorCount, m_sd.disk_sectors());

    validateSelect();
    // Should send CMD9 to start read.
    validateCmdPacket(9);
    // Should send multiple FF bytes to read in register data block:
    //  1 to read in header.
    //  16 to read data.
    //  2 to read CRC.
    validateFFBytes(1+16+2);
    validateDeselect();
}

TEST(DiskSectors, DiskSectors_FailCMD9_ShouldFail_Log)
{
    initSDHC();

    // CMD9 input data.
    setupDataForCmd("04");

        LONGS_EQUAL(0, m_sd.disk_sectors());

    validateSelect();
    // Should send CMD9 to start read.
    validateCmdPacket(9);
    validateDeselect();

    // Verify error log output.
    // Just verify the last line as others contain a pointer that I don't know.
    m_sd.dumpErrorLog(stderr);
    static const char expectedOutput[] = "disk_sectors() - Failed to read CSD\n";
    const char* pActualOutput = printfSpy_GetLastOutput();
    STRCMP_EQUAL(expectedOutput, pActualOutput + strlen(pActualOutput) - (sizeof(expectedOutput) - 1));
}
