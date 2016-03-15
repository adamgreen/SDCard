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
/* Soak test for file I/O. */
#include <assert.h>
#include <errno.h>
#include <mbed.h>
#include <SDFileSystem.h>
#include <SDTestLib.h>

#if (!MRI_ENABLE)
    static Serial       g_pc(USBTX, USBRX);
    #define SHOULD_STOP_TEST g_pc.readable()
    #define CLEAR_INPUT_QUEUE() while(g_pc.readable()) g_pc.getc()
#else
    volatile int        g_stopTest = 0;
    #define SHOULD_STOP_TEST g_stopTest
    #define CLEAR_INPUT_QUEUE()
#endif

static SDFileSystem g_sd(p5, p6, p7, p8, "sd");
static DigitalOut   g_led1(LED1);


int main()
{
    static const char testFilename[] = "/sd/soak.tst";
    static __attribute((section("AHBSRAM0"),aligned)) uint8_t buffer[16 * 1024];
    static __attribute((section("AHBSRAM1"),aligned)) char    cache[16 * 1024];
    static uint32_t*   pBufferEnd = (uint32_t*)(buffer + sizeof(buffer));

    // Start the timer and use microsecond count for user input as seed.
    static Timer timer;
    timer.start();

    // Remove the test file if it already exists.
    printf("\n\nSDCard Soak Test\n");
    printf("Cleanup from previous test run.\n");
    remove(testFilename);
    checkSdLog(&g_sd);
    printf("\n");


    // Ask the user to pick the size of the file used for testing.
    unsigned long sizeInMB = 0;
    while (1)
    {
        printf("How large should the test file be in MB (100MB is default): ");
        char lineBuffer[64];
        fgets(lineBuffer, sizeof(lineBuffer), stdin);
        sizeInMB = strtoul(lineBuffer, NULL, 0);
        if (sizeInMB == 0)
        {
            sizeInMB = 100;
        }

        if (sizeInMB < 2048)
        {
            break;
        }
        printf("Input must be smaller than 2048 (2GB).\n");
    }
    uint32_t sizeInBytes = 1024 * 1024 * sizeInMB;
    uint32_t sizeInBlocks = sizeInBytes / 512;

    // The seed will be based on the time it took the user to input the desired test size.
    uint32_t seed = timer.read_us();
    srand(seed);

    // Dump information about card that might make it easier to interpret data later.
    printf("\n");
    dumpCID(&g_sd);
    dumpOCR(&g_sd);
    dumpCSD(&g_sd);



    printf("Creating %lu MB test file...\n", sizeInMB);
    FILE* pFile = fopen(testFilename, "w");
    checkSdLog(&g_sd);
    if (!pFile)
    {
        fprintf(stderr, "error: Failed to create %s - %d\n", testFilename, errno);
        testExit(&g_sd, -1);
    }
    setvbuf(pFile, cache, _IOFBF, sizeof(cache));

    timer.reset();
    uint32_t* pCurr = (uint32_t*)buffer;
    for (uint32_t block = 0 ; block < sizeInBlocks ; block++)
    {
        uint32_t blockSeed = block ^ seed;

        for (size_t i = 0 ; i < 512 / sizeof(uint32_t) ; i++)
        {
            uint32_t wordSeed = i | (i << 8) | (i << 16) | (i << 24);
            *pCurr++ = blockSeed ^ wordSeed;
        }

        if (pCurr >= pBufferEnd)
        {
            size_t bytesTransferred = fwrite(buffer, 1, sizeof(buffer), pFile);
            checkSdLog(&g_sd);
            if (bytesTransferred != sizeof(buffer))
            {
                fprintf(stderr, "error: Failed to write to %s - %d\n", testFilename, errno);
                break;
            }
            pCurr = (uint32_t*)buffer;
        }

        // Blink LED1 to let user know that we are still running.
        if (timer.read_ms() >= 250)
        {
            g_led1 = !g_led1;
            timer.reset();
        }
    }
    fclose(pFile);
    checkSdLog(&g_sd);
    pFile = NULL;



    printf("The following soak test will run until you press a key to stop it.\n");
    printf("LED1 will blink while the test is progressing smoothly.\n");
    printf("Starting soak test now...\n");
    CLEAR_INPUT_QUEUE();
    timer.reset();
    while (!SHOULD_STOP_TEST)
    {
        // Randomly select the nature of the next read.
        uint32_t startOffset = (rand() << 16 | rand()) % sizeInBytes;
        uint32_t maxReadSize = sizeInBytes - startOffset;
        uint32_t readSize = rand() % sizeof(buffer);
        if (readSize > maxReadSize)
        {
            readSize = maxReadSize;
        }

        // Blink LED1 to let user know that we are still running.
        if (timer.read_ms() >= 500)
        {
            g_led1 = !g_led1;
            timer.reset();
        }

        // Open the file when not already open.
        if (!pFile)
        {
            pFile = fopen(testFilename, "r");
            checkSdLog(&g_sd);
            if (!pFile)
            {
                fprintf(stderr, "error: Failed to open %s - %d\n", testFilename, errno);
                testExit(&g_sd, -1);
            }
            setvbuf(pFile, cache, _IOFBF, sizeof(cache));
        }

        // Seek to desired location.
        int seekResult = fseek(pFile, startOffset, SEEK_SET);
        checkSdLog(&g_sd);
        if (seekResult)
        {
            fprintf(stderr, "error: Failed to seek to %lu in %s - %d\n", startOffset, testFilename, seekResult);
            testExit(&g_sd, -1);
        }

        // Issue read.
        size_t bytesTransferred = fread(buffer, 1, readSize, pFile);
        checkSdLog(&g_sd);
        if (bytesTransferred != readSize)
        {
            fprintf(stderr, "error: Failed to read from %s - %u\n", testFilename, bytesTransferred);
            testExit(&g_sd, -1);
        }

        // Vallidate the data.
        size_t          bytesToValidate = bytesTransferred;
        uint32_t        currOffset = startOffset;
        const uint32_t* pCurr = (const uint32_t*)buffer;
        while (bytesToValidate > 0)
        {
            uint32_t blockIndex = currOffset / 512;
            uint32_t byteIndex = currOffset % 512;
            uint32_t wordIndex = byteIndex / 4;
            uint32_t blockSeed = blockIndex ^ seed;

            // Handle case where first byte is not word aligned.
            uint32_t offsetInWord = byteIndex % 4;
            if (offsetInWord != 0)
            {
                uint32_t wordSeed = wordIndex | (wordIndex << 8) | (wordIndex << 16) | (wordIndex << 24);
                uint32_t expectedValue = blockSeed ^ wordSeed;

                const uint8_t* p1 = (const uint8_t*)&expectedValue + offsetInWord;
                const uint8_t* p2 = (const uint8_t*)pCurr;
                while (offsetInWord++ != 4 && bytesToValidate > 0)
                {
                    uint8_t expectedByte = *p1++;
                    uint8_t actualByte = *p2++;
                    if (expectedByte != actualByte)
                    {
                        fprintf(stderr, "error: Read mismatch @ %lu. Actual:0x%02X Expected:0x%02X\n",
                                currOffset, actualByte, expectedByte);
                        testExit(&g_sd, -1);
                    }
                    currOffset++;
                    bytesToValidate--;
                    byteIndex++;
                }
                pCurr = (const uint32_t*)p2;
                wordIndex++;
            }

            // Now check a word at time.
            assert ( bytesToValidate == 0 || (currOffset & 3) == 0 );
            assert ( bytesToValidate == 0 || (byteIndex & 3) == 0 );
            while (byteIndex < 512 && bytesToValidate >= sizeof(uint32_t))
            {
                uint32_t wordSeed = wordIndex | (wordIndex << 8) | (wordIndex << 16) | (wordIndex << 24);
                uint32_t expectedWord = blockSeed ^ wordSeed;
                uint32_t actualWord = *pCurr;
                if (actualWord != expectedWord)
                {
                    fprintf(stderr, "error: Read mismatch @ %lu. Actual:0x%08lX Expected:0x%08lX\n",
                            currOffset, actualWord, expectedWord);
                    testExit(&g_sd, -1);
                }

                pCurr++;
                currOffset += sizeof(uint32_t);
                bytesToValidate -= sizeof(uint32_t);
                byteIndex += sizeof(uint32_t);
                wordIndex++;
            }

            // Handle case where last bytes don't make up full word.
            if (byteIndex < 512 && bytesToValidate > 0)
            {
                uint32_t wordSeed = wordIndex | (wordIndex << 8) | (wordIndex << 16) | (wordIndex << 24);
                uint32_t expectedValue = blockSeed ^ wordSeed;

                const uint8_t* p1 = (const uint8_t*)&expectedValue;
                const uint8_t* p2 = (const uint8_t*)pCurr;
                while (bytesToValidate > 0)
                {
                    uint8_t expectedByte = *p1++;
                    uint8_t actualByte = *p2++;
                    if (expectedByte != actualByte)
                    {
                        fprintf(stderr, "error: Read mismatch @ %lu. Actual:0x%02X Expected:0x%02X\n",
                                currOffset, actualByte, expectedByte);
                        testExit(&g_sd, -1);
                    }
                    currOffset++;
                    bytesToValidate--;
                    byteIndex++;
                }
                pCurr = (const uint32_t*)p2;
            }
        }
        assert ( (uint32_t)((const uint8_t*)pCurr - buffer) == readSize );

        // Should close the file sometimes.
        if ((rand() & 0xFF) == 0xFF)
        {
            fclose(pFile);
            checkSdLog(&g_sd);
            pFile = NULL;
        }
    }
    if (pFile)
    {
        fclose(pFile);
        checkSdLog(&g_sd);
        pFile = NULL;
    }

    printf("Removing test file.\n");
    int removeResult = remove(testFilename);
    checkSdLog(&g_sd);
    if (removeResult)
    {
        fprintf(stderr, "error: remove() failed - %d\n", removeResult);
        testExit(&g_sd, -1);
    }

    dumpSdCounters(&g_sd);
    printf("Test Completed!\n");

    return 0;
}
