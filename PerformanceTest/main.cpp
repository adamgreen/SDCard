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
/* Performance test for file I/O. */
#include <errno.h>
#include <mbed.h>
#include <SDFileSystem.h>
#include <SDTestLib.h>

// Set to 1 to test on CoCoCartridge which uses different pins for SPI.
#define COCO_CARTRIDGE 0

#if COCO_CARTRIDGE
    static SDFileSystem g_sd(P1_24, P1_23, P1_20, P1_21, "sd");
#else
    static SDFileSystem g_sd(p5, p6, p7, p8, "sd");
#endif // COCO_CARTRIDGE


int main()
{
    static const char         testFilename[] = "/sd/sdtst.bin";
    static const unsigned int testFileSize = 10 * 1024 * 1024;
    static Timer              timer;
    FILE*                     pFile = NULL;
    size_t                    bytesTransferred = 0;
    size_t                    i = 0;
    int                       seekResult = -1;
    static __attribute((section("AHBSRAM0"),aligned)) unsigned char buffer[16 * 1024];
    static __attribute((section("AHBSRAM1"),aligned)) char          cache[16 * 1024];


    // Remove the test file if it already exists.
    printf("Cleanup from previous test run.\n");
    remove(testFilename);
    checkSdLog(&g_sd);

    // Dump information about card that might make it easier to interpret performance data later.
    dumpCID(&g_sd);
    dumpOCR(&g_sd);
    dumpCSD(&g_sd);

    printf("Performing write test of %u bytes...\n", testFileSize);
    memset(buffer, 0x55, sizeof(buffer));
    pFile = fopen(testFilename, "w");
    checkSdLog(&g_sd);
    if (!pFile)
    {
        fprintf(stderr, "error: Failed to create %s - %d\n", testFilename, errno);
        testExit(&g_sd, -1);
    }
    setvbuf(pFile, cache, _IOFBF, sizeof(cache));

    timer.start();
    for (i = 0 ; i < testFileSize / sizeof(buffer) ; i++)
    {
        bytesTransferred = fwrite(buffer, 1, sizeof(buffer), pFile);
        checkSdLog(&g_sd);
        if (bytesTransferred != sizeof(buffer))
        {
            fprintf(stderr, "error: Failed to write to %s - %d\n", testFilename, errno);
            break;
        }
    }
    unsigned int totalTicks = (unsigned int)timer.read_ms();
    unsigned int totalBytes = ftell(pFile);
    fclose(pFile);
    checkSdLog(&g_sd);

    float writeRate = (totalBytes / (totalTicks / 1000.0f)) / (1000.0f * 1000.0f);
    printf("    %.2f MB/second.\n", writeRate);


    printf("Performing read test of %u bytes...\n", testFileSize);
    pFile = fopen(testFilename, "r");
    checkSdLog(&g_sd);
    if (!pFile)
    {
        fprintf(stderr, "error: Failed to open %s - %d\n", testFilename, errno);
        testExit(&g_sd, -1);
    }
    setvbuf(pFile, cache, _IOFBF, sizeof(cache));

    timer.reset();
    for (;;)
    {
        bytesTransferred = fread(buffer, 1, sizeof(buffer), pFile);
        checkSdLog(&g_sd);
        if (bytesTransferred != sizeof(buffer))
        {
            if (ferror(pFile))
            {
                fprintf(stderr, "error: Failed to read from %s - %d\n", testFilename, errno);
                testExit(&g_sd, -1);
            }
            else
            {
                break;
            }
        }
    }
    totalTicks = (unsigned int)timer.read_ms();
    totalBytes = ftell(pFile);

    float readRate = (totalBytes / (totalTicks / 1000.0f)) / (1000.0f * 1000.0f);
    printf("    %.2f MB/second.\n", readRate);


    printf("Validating data on disk.\n");
    seekResult = fseek(pFile, 0, SEEK_SET);
    checkSdLog(&g_sd);
    if (seekResult)
    {
        fprintf(stderr, "error: Failed to seek to beginning of file - %d\n", errno);
        testExit(&g_sd, -1);
    }

    for (;;)
    {
        unsigned int   j;
        unsigned char* pCurr;

        memset(buffer, 0xaa, sizeof(buffer));
        bytesTransferred = fread(buffer, 1, sizeof(buffer), pFile);
        checkSdLog(&g_sd);
        if (bytesTransferred != sizeof(buffer) && ferror(pFile))
        {
            fprintf(stderr, "error: Failed to read from %s - %d\n", testFilename, errno);
            testExit(&g_sd, -1);
        }

        for (j = 0, pCurr = buffer ; j < bytesTransferred ; j++)
        {
            unsigned char byte = *pCurr++;
            if (byte != 0x55)
            {
                fprintf(stderr, "error: Unexpected read byte (0x%02X) encountered.\n", byte);
                testExit(&g_sd, -1);
            }
        }

        if (bytesTransferred != sizeof(buffer))
            break;
    }
    totalBytes = ftell(pFile);
    printf("Validated %u bytes.\n", totalBytes);
    fclose(pFile);

    printf("Removing test file.\n");
    int removeResult = remove(testFilename);
    checkSdLog(&g_sd);
    if (removeResult)
    {
        fprintf(stderr, "error: remove() failed - %d\n", errno);
        testExit(&g_sd, -1);
    }


    dumpSdCounters(&g_sd);
    printf("Test Completed!\n");

    return 0;
}
