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
/* Library of routines to dump information about SD cards under test. */
#include <mbed.h>
#include "SDTestLib.h"


// Function Prototypes.
static void dumpCSDv1(SDFileSystem* pSD, uint8_t* pCSD);
static void dumpCSDv2(SDFileSystem* pSD, uint8_t* pCSD);


void checkSdLog(SDFileSystem* pSD)
{
#if SDFILESYSTEM_ENABLE_ERROR_LOG
    if (pSD->isErrorLogEmpty())
    {
        return;
    }

    fprintf(stderr, "**SDFileSystem internal errors**\n");
    pSD->dumpErrorLog(stderr);
    pSD->clearErrorLog();
#endif // SDFILESYSTEM_ENABLE_ERROR_LOG`
}

void dumpOCR(SDFileSystem* pSD)
{
    uint32_t ocr = 0;

    printf("Dumping SD OCR register contents.\n");

    pSD->getOCR(&ocr);
    checkSdLog(pSD);

    printf("  OCR = 0x%08lX\n", ocr);
    printf("          Card Power Up Status: %d\n", (ocr & (1 << 31)) ? 1 : 0);
    printf("          Card Capacity Status: %d\n", (ocr & (1 << 30)) ? 1 : 0);
    printf("            UHS-II Card Status: %d\n", (ocr & (1 << 29)) ? 1 : 0);
    printf("    Switching to 1.8V Accepted: %d\n", (ocr & (1 << 24)) ? 1 : 0);
    float voltage = 2.7f;
    for (int i = 15 ; i <= 23 ; i++)
    {
        uint32_t mask = 1 << i;
        printf("                    %.1f - %.1fV: %d\n", voltage, voltage + 0.1f, (ocr & mask) ? 1 : 0);
        voltage += 0.1f;
    }
}

void dumpCID(SDFileSystem* pSD)
{
    static const char* months[16] =
    {
        "???",
        "January",
        "February",
        "March",
        "April",
        "May",
        "June",
        "July",
        "August",
        "September",
        "October",
        "November",
        "December",
        "???", "???", "???"
    };
    uint8_t cid[16];

    printf("Dumping SD CID register contents.\n");

    pSD->getCID(cid, sizeof(cid));
    checkSdLog(pSD);
    uint32_t productRevision = pSD->extractBits(cid, sizeof(cid), 56, 63);
    uint32_t year = 2000 + pSD->extractBits(cid, sizeof(cid), 12, 19);
    uint32_t month = pSD->extractBits(cid, sizeof(cid), 8, 11);

    printf("  CID = ");
    for (size_t i = 0 ; i < sizeof(cid) ; i++)
    {
        printf("0x%02X ", cid[i]);
    }
    printf("\n");

    printf("          Manufacturer ID: 0x%02lX\n", pSD->extractBits(cid, sizeof(cid), 120, 127));
    printf("                   OEM ID: %.2s\n", &cid[1]);
    printf("             Product Name: %.5s\n", &cid[3]);
    printf("         Product Revision: %lu.%lu\n", productRevision >> 4, productRevision & 0xF);
    printf("    Product Serial Number: 0x%08lX\n", pSD->extractBits(cid, sizeof(cid), 24, 55));
    printf("       Manufacturing Date: %s %lu\n", months[month], year);
    printf("                 Checksum: 0x%02lX\n", pSD->extractBits(cid, sizeof(cid), 1, 7));
}

void dumpCSD(SDFileSystem* pSD)
{
    uint8_t csd[16];

    printf("Dumping SD CSD register contents.\n");

    pSD->getCSD(csd, sizeof(csd));
    checkSdLog(pSD);

    printf("  CSD = ");
    for (size_t i = 0 ; i < sizeof(csd) ; i++)
    {
        printf("0x%02X ", csd[i]);
    }
    printf("\n");

    uint32_t csdStructure = pSD->extractBits(csd, sizeof(csd), 126, 127);
    switch (csdStructure)
    {
    case 0:
        dumpCSDv1(pSD, csd);
        break;
    case 1:
        dumpCSDv2(pSD, csd);
        break;
    default:
        printf("    Unknown CSD_STRUCTURE value: %lu\n", csdStructure);
        break;
    }
}

static void dumpCSDv1(SDFileSystem* pSD, uint8_t* pCSD)
{
    static const struct
    {
        const char* pName;
        float       val;
    } timeUnits[8] =
    {   { "ns", 1.0f },
        { "ns", 10.0f },
        { "ns", 100.0f },
        { "us", 1.0f },
        { "us", 10.0f },
        { "us", 100.0f },
        { "ms", 1.0f },
        { "ms", 10.0f } };
    static const float timeValues[16] = { 0.0f, 1.0f, 1.2f, 1.3f, 1.5f, 2.0f, 2.5f, 3.0f,
                                          3.5f, 4.0f, 4.5f, 5.0f, 5.5f, 6.0f, 7.0f, 8.0f };
    static const float minCurrents[8] = { 0.5f, 1.0f, 5.0f, 10.0f, 25.0f, 35.0f, 60.0f, 100.0f };
    static const float maxCurrents[8] = { 1.0f, 5.0f, 10.0f, 25.0f, 35.0f, 45.0f, 80.0f, 200.0f };

    uint32_t TAAC = pSD->extractBits(pCSD, 16, 112, 119);
    uint32_t unitIndex = TAAC & 0x7;
    uint32_t timeIndex = (TAAC >> 3) & 0xF;
    float    taacVal = timeUnits[unitIndex].val * timeValues[timeIndex];
    uint32_t TRAN_SPEED = pSD->extractBits(pCSD, 16, 96, 103);
    float    tranSpeed = 0.1f * pow(10.0f, TRAN_SPEED & 0x7) * timeValues[(TRAN_SPEED >> 3) & 0xF];
    uint32_t C_SIZE = pSD->extractBits(pCSD, 16, 62,73);
    uint32_t C_SIZE_MULT = pSD->extractBits(pCSD, 16, 47, 49);
    uint32_t READ_BL_LEN = pSD->extractBits(pCSD, 16, 80, 83);
    uint32_t diskSize = (C_SIZE + 1)  << (C_SIZE_MULT + 2 + READ_BL_LEN);


    printf("    CSD Version: 1.0\n");

    printf("                  Data Read Access-Time: %.1f %s\n", taacVal, timeUnits[unitIndex].pName);
    printf("    Data Read Access-Time in CLK cycles: %lu\n", pSD->extractBits(pCSD, 16, 104, 111) * 100);
    printf("                      Max Transfer Rate: %.1fMHz\n", tranSpeed);
    uint32_t CCC = pSD->extractBits(pCSD, 16, 84, 95);
    for (int i = 0 ; i <= 11 ; i++)
    {
        uint32_t mask = 1 << i;
        printf("                  Card Command Class %2d: %s\n", i, (CCC & mask) ? "yes" : "no");
    }
    printf("             Max Read Data Block Length: %u\n", 1 << pSD->extractBits(pCSD, 16, 80, 83));
    printf("        Partial Blocks for Read Allowed: %s\n", pSD->extractBits(pCSD, 16, 79, 79) ? "yes" : "no");
    printf("               Write Block Misalignment: %s\n", pSD->extractBits(pCSD, 16, 78, 78) ? "yes" : "no");
    printf("                Read Block Misalignment: %s\n", pSD->extractBits(pCSD, 16, 77, 77) ? "yes" : "no");
    printf("                        DSR Implemented: %s\n", pSD->extractBits(pCSD, 16, 76, 76) ? "yes" : "no");
    printf("                            Device Size: %lu (%lu bytes)\n", C_SIZE + 1, diskSize);
    printf("             Max Read Current @ VDD min: %.2f mA\n", minCurrents[pSD->extractBits(pCSD, 16, 59, 61)]);
    printf("             Max Read Current @ VDD max: %.2f mA\n", maxCurrents[pSD->extractBits(pCSD, 16, 56, 58)]);
    printf("            Max Write Current @ VDD min: %.2f mA\n", minCurrents[pSD->extractBits(pCSD, 16, 53, 55)]);
    printf("            Max Write Current @ VDD max: %.2f mA\n", maxCurrents[pSD->extractBits(pCSD, 16, 50, 52)]);
    printf("                 Device Size Multiplier: %u\n", 1 << (C_SIZE_MULT + 2));
    printf("              Erase Single Block Enable: %s\n", pSD->extractBits(pCSD, 16, 46, 46) ? "512 bytes" : "SECTOR_SIZE");
    printf("        Erase Sector Size (SECTOR_SIZE): %lu\n", pSD->extractBits(pCSD, 16, 39, 45) + 1);
    printf("               Write Protect Group Size: %lu\n", pSD->extractBits(pCSD, 16, 32, 38) + 1);
    printf("             Write Protect Group Enable: %s\n", pSD->extractBits(pCSD, 16, 31, 31) ? "yes" : "no");
    printf("                     Write Speed Factor: %u\n", 1 << pSD->extractBits(pCSD, 16, 26, 28));
    printf("            Max Write Data Block Length: %u\n", 1 << pSD->extractBits(pCSD, 16, 22, 25));
    printf("       Partial Blocks for Write Allowed: %s\n", pSD->extractBits(pCSD, 16, 21, 21) ? "yes" : "no");
    printf("                      File Format Group: %lu\n", pSD->extractBits(pCSD, 16, 15, 15));
    printf("                              Copy Flag: %s\n", pSD->extractBits(pCSD, 16, 14, 14) ? "copy" : "original");
    printf("             Permanent Write Protection: %lu\n", pSD->extractBits(pCSD, 16, 13, 13));
    printf("             Temporary Write Protection: %lu\n", pSD->extractBits(pCSD, 16, 12, 12));
    printf("                            File Format: %lu\n", pSD->extractBits(pCSD, 16, 10, 11));
    printf("                                    CRC: 0x%02lX\n", pSD->extractBits(pCSD, 16, 1, 7));
}

static void dumpCSDv2(SDFileSystem* pSD, uint8_t* pCSD)
{
    static const float timeValues[16] = { 0.0f, 1.0f, 1.2f, 1.3f, 1.5f, 2.0f, 2.5f, 3.0f,
                                         3.5f, 4.0f, 4.5f, 5.0f, 5.5f, 6.0f, 7.0f, 8.0f };
    uint32_t TAAC = pSD->extractBits(pCSD, 16, 112, 119);
    uint32_t TRAN_SPEED = pSD->extractBits(pCSD, 16, 96, 103);
    float tranSpeed = 0.1f * pow(10.0f, TRAN_SPEED & 0x7) * timeValues[(TRAN_SPEED >> 3) & 0xF];

    printf("    CSD Version: 2.0\n");
    printf("                  Data Read Access-Time: 0x%02lX %s\n", TAAC, (TAAC == 0x0E) ? "(1ms)" : "");
    printf("    Data Read Access-Time in CLK cycles: %lu\n", pSD->extractBits(pCSD, 16, 104, 111));
    printf("                      Max Transfer Rate: %.1fMHz\n", tranSpeed);
    uint32_t CCC = pSD->extractBits(pCSD, 16, 84, 95);
    for (int i = 0 ; i <= 11 ; i++)
    {
        uint32_t mask = 1 << i;
        printf("                  Card Command Class %2d: %s\n", i, (CCC & mask) ? "yes" : "no");
    }
    printf("             Max Read Data Block Length: %u\n", 1 << pSD->extractBits(pCSD, 16, 80, 83));
    printf("        Partial Blocks for Read Allowed: %s\n", pSD->extractBits(pCSD, 16, 79, 79) ? "yes" : "no");
    printf("               Write Block Misalignment: %s\n", pSD->extractBits(pCSD, 16, 78, 78) ? "yes" : "no");
    printf("                Read Block Misalignment: %s\n", pSD->extractBits(pCSD, 16, 77, 77) ? "yes" : "no");
    printf("                        DSR Implemented: %s\n", pSD->extractBits(pCSD, 16, 76, 76) ? "yes" : "no");
    printf("                            Device Size: %llu bytes\n", (uint64_t)((pSD->extractBits(pCSD, 16, 48, 69) + 1) << 10) * 512ULL);
    printf("              Erase Single Block Enable: %s\n", pSD->extractBits(pCSD, 16, 46, 46) ? "512 bytes" : "SECTOR_SIZE");
    printf("        Erase Sector Size (SECTOR_SIZE): %lu\n", pSD->extractBits(pCSD, 16, 39, 45) + 1);
    printf("               Write Protect Group Size: %lu\n", pSD->extractBits(pCSD, 16, 32, 38));
    printf("             Write Protect Group Enable: %s\n", pSD->extractBits(pCSD, 16, 31, 31) ? "yes" : "no");
    printf("                     Write Speed Factor: %u\n", 1 << pSD->extractBits(pCSD, 16, 26, 28));
    printf("            Max Write Data Block Length: %u\n", 1 << pSD->extractBits(pCSD, 16, 22, 25));
    printf("       Partial Blocks for Write Allowed: %s\n", pSD->extractBits(pCSD, 16, 21, 21) ? "yes" : "no");
    printf("                      File Format Group: %lu\n", pSD->extractBits(pCSD, 16, 15, 15));
    printf("                              Copy Flag: %s\n", pSD->extractBits(pCSD, 16, 14, 14) ? "copy" : "original");
    printf("             Permanent Write Protection: %lu\n", pSD->extractBits(pCSD, 16, 13, 13));
    printf("             Temporary Write Protection: %lu\n", pSD->extractBits(pCSD, 16, 12, 12));
    printf("                            File Format: %lu\n", pSD->extractBits(pCSD, 16, 10, 11));
    printf("                                    CRC: 0x%02lX\n", pSD->extractBits(pCSD, 16, 1, 7));
}

void testExit(SDFileSystem* pSD, int retVal)
{
    dumpSdCounters(pSD);
    exit(retVal);
}

void dumpSdCounters(SDFileSystem* pSD)
{
    uint32_t counter = 0;

    #define DUMP_COUNTER(COUNTER,IGNORE_VAL) \
        counter = pSD->COUNTER(); \
        if (counter != IGNORE_VAL) \
        { \
            printf("    " #COUNTER " = %lu\n", counter); \
        }

    printf("SD Card Driver Counters\n");

    DUMP_COUNTER(selectFirstExchangeRequiredCount, 0);
    DUMP_COUNTER(maximumWaitWhileBusyTime, 0);
    DUMP_COUNTER(maximumWaitForR1ResponseLoopCount, 0);
    DUMP_COUNTER(maximumCRCRetryCount, 0);
    DUMP_COUNTER(maximumACMD41LoopTime, 0);
    DUMP_COUNTER(maximumReceiveDataBlockWaitTime, 0);
    DUMP_COUNTER(maximumReadRetryCount, 0);
    DUMP_COUNTER(cmd12PaddingByteRequiredCount, 0);
    DUMP_COUNTER(maximumWriteRetryCount, 0);
    DUMP_COUNTER(cmdCrcErrorCount, 0);
    DUMP_COUNTER(receiveTimeoutCount, 0);
    DUMP_COUNTER(receiveBadTokenCount, 0);
    DUMP_COUNTER(receiveTransferFailCount, 0);
    DUMP_COUNTER(receiveCrcErrorCount, 0);
    DUMP_COUNTER(transmitTimeoutCount, 0);
    DUMP_COUNTER(transmitTransferFailCount, 0);
    DUMP_COUNTER(transmitResponseErrorCount, 0);

}
