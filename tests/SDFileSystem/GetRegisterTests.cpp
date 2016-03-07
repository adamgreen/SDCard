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

TEST_GROUP_BASE(GetRegisters,SDFileSystemBase)
{
};


// ***************
// getCID() tests
// ***************
TEST(GetRegisters, GetCID_SuccessfulRead)
{
    uint8_t cid[16];

    initSDHC();

    // CMD10 input data.
    setupDataForCmd("00");
    // 0xFE starts read data block.
    m_sd.spi().setInboundFromString("FE");
    // Data block will contain 16 bytes of 0xAD + valid CRC.
    setupDataBlock(0xAD, 16);

    // Clear buffer to 0x00 before reading into it.
    memset(cid, 0, sizeof(cid));

        LONGS_EQUAL(RES_OK, m_sd.getCID(cid, sizeof(cid)));

    validateSelect();
    // Should send CMD10 to start read.
    validateCmdPacket(10);
    // Should send multiple FF bytes to read in register data block:
    //  1 to read in header.
    //  16 to read data.
    //  2 to read CRC.
    validateFFBytes(1+16+2);
    validateDeselect();

    // Verify that register contents were read into supplied buffer.
    validateBuffer(cid, 16, 0xAD);
}

TEST(GetRegisters, GetCID_FailCommand_ShouldFail)
{
    uint8_t cid[16];

    initSDHC();

    // CMD10 input data.
    setupDataForCmd("04");

    // Clear buffer to 0x00 before reading into it.
    memset(cid, 0, sizeof(cid));

        LONGS_EQUAL(RES_ERROR, m_sd.getCID(cid, sizeof(cid)));

    validateSelect();
    // Should send CMD10 to start read.
    validateCmdPacket(10);
    validateDeselect();

    // Verify that register contents weren't modified.
    validateBuffer(cid, 16, 0x00);

    // Verify error log output.
    m_sd.dumpErrorLog(stderr);
    char expectedOutput[256];
    snprintf(expectedOutput, sizeof(expectedOutput),
             "sendCommandAndReceiveDataBlock(CMD10,0,%08X,16) - CMD10 returned 0x04\n"
             "getCID(%08X,16) - Register read failed\n",
             (uint32_t)(size_t)cid,
             (uint32_t)(size_t)cid);
    STRCMP_EQUAL(expectedOutput, printfSpy_GetLastOutput());
}



// ***************
// getCSD() tests
// ***************
TEST(GetRegisters, GetCSD_SuccessfulRead)
{
    uint8_t csd[16];

    initSDHC();

    // CMD9 input data.
    setupDataForCmd("00");
    // 0xFE starts read data block.
    m_sd.spi().setInboundFromString("FE");
    // Data block will contain 16 bytes of 0xAD + valid CRC.
    setupDataBlock(0xAD, 16);

    // Clear buffer to 0x00 before reading into it.
    memset(csd, 0, sizeof(csd));

        LONGS_EQUAL(RES_OK, m_sd.getCSD(csd, sizeof(csd)));

    validateSelect();
    // Should send CMD9 to start read.
    validateCmdPacket(9);
    // Should send multiple FF bytes to read in register data block:
    //  1 to read in header.
    //  16 to read data.
    //  2 to read CRC.
    validateFFBytes(1+16+2);
    validateDeselect();

    // Verify that register contents were read into supplied buffer.
    validateBuffer(csd, 16, 0xAD);
}

TEST(GetRegisters, GetCSD_FailCommand_ShouldFail)
{
    uint8_t csd[16];

    initSDHC();

    // CMD9 input data.
    setupDataForCmd("04");

    // Clear buffer to 0x00 before reading into it.
    memset(csd, 0, sizeof(csd));

        LONGS_EQUAL(RES_ERROR, m_sd.getCSD(csd, sizeof(csd)));

    validateSelect();
    // Should send CMD9 to start read.
    validateCmdPacket(9);
    validateDeselect();

    // Verify that register contents weren't modified.
    validateBuffer(csd, 16, 0x00);

    // Verify error log output.
    m_sd.dumpErrorLog(stderr);
    char expectedOutput[256];
    snprintf(expectedOutput, sizeof(expectedOutput),
             "sendCommandAndReceiveDataBlock(CMD9,0,%08X,16) - CMD9 returned 0x04\n"
             "getCSD(%08X,16) - Register read failed\n",
             (uint32_t)(size_t)csd,
             (uint32_t)(size_t)csd);
    STRCMP_EQUAL(expectedOutput, printfSpy_GetLastOutput());
}




// ***************
// getOCR() tests
// ***************
TEST(GetRegisters, GetOCR_SuccessfulRead)
{
    uint32_t ocr = 0;

    initSDHC();

    // CMD58 input data with 4-byte OCR value.
    setupDataForCmd("00");
    m_sd.spi().setInboundFromString("12345678");

        LONGS_EQUAL(RES_OK, m_sd.getOCR(&ocr));

    // Should send CMD58 to read OCR.
    validateCmd(58, 0, 4);

    // Verify that register contents were read into supplied buffer.
    LONGS_EQUAL(0x12345678, ocr);
}

TEST(GetRegisters, GetOCR_FailCommand_ShouldFail)
{
    uint32_t ocr = 0;

    initSDHC();

    // CMD58 input data with error response.
    setupDataForCmd("04");

        LONGS_EQUAL(RES_ERROR, m_sd.getOCR(&ocr));

    // Should send CMD58 to read OCR.
    validateCmd(58, 0);

    // Verify that register contents weren't read into supplied buffer.
    LONGS_EQUAL(0, ocr);

    // Verify error log output.
    m_sd.dumpErrorLog(stderr);
    char expectedOutput[256];
    snprintf(expectedOutput, sizeof(expectedOutput),
             "getOCR(%08X) - Register read failed. Response=0x04\n",
             (uint32_t)(size_t)&ocr);
    STRCMP_EQUAL(expectedOutput, printfSpy_GetLastOutput());
}
