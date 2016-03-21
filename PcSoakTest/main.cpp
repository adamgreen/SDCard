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
#include <stdio.h>


static void displayUsage()
{
    printf("Usage: PcSoakTest testFilename\n");
}


int main(int argc, const char** argv)
{
    const char* testFilename = NULL;
    uint8_t     buffer[16 * 1024];
    uint32_t*   pBufferEnd = (uint32_t*)(buffer + sizeof(buffer));

    // Expect one command line argument for the name of the test file.
    if (argc != 2)
    {
        displayUsage();
        exit(-1);
    }
    testFilename = argv[1];

    // Remove the test file if it already exists.
    printf("\n\nSDCard Soak Test\n");
    printf("Cleanup from previous test run.\n");
    remove(testFilename);
    printf("\n");


    // Ask the user to pick the size of the file used for testing.
    unsigned long sizeInMB = 10;
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

    // Using a fixed seed for the PC version.
    uint32_t seed = 0xBAADF00D;
    srand(seed);



    printf("Creating %lu MB test file...\n", sizeInMB);
    FILE* pFile = fopen(testFilename, "w");
    if (!pFile)
    {
        fprintf(stderr, "error: Failed to create %s - %d\n", testFilename, errno);
        exit(-1);
    }

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
            if (bytesTransferred != sizeof(buffer))
            {
                fprintf(stderr, "error: Failed to write to %s - %d\n", testFilename, errno);
                break;
            }
            pCurr = (uint32_t*)buffer;
        }
    }
    fclose(pFile);
    pFile = NULL;



    printf("Starting soak test now...\n");
    while (1)
    {
        // Randomly select the nature of the next read.
        uint32_t startOffset = (rand() << 16 | rand()) % sizeInBytes;
        uint32_t maxReadSize = sizeInBytes - startOffset;
        uint32_t readSize = rand() % sizeof(buffer);
        if (readSize > maxReadSize)
        {
            readSize = maxReadSize;
        }

        // Open the file when not already open.
        if (!pFile)
        {
            pFile = fopen(testFilename, "r");
            if (!pFile)
            {
                fprintf(stderr, "error: Failed to open %s - %d\n", testFilename, errno);
                exit(-1);
            }
        }

        // Seek to desired location.
        int seekResult = fseek(pFile, startOffset, SEEK_SET);
        if (seekResult)
        {
            fprintf(stderr, "error: Failed to seek to %u in %s - %d\n", startOffset, testFilename, seekResult);
            exit(-1);
        }

        // Issue read.
        size_t bytesTransferred = fread(buffer, 1, readSize, pFile);
        if (bytesTransferred != readSize)
        {
            fprintf(stderr, "error: Failed to read from %s - %lu\n", testFilename, bytesTransferred);
            exit(-1);
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
                        fprintf(stderr, "error: Read mismatch @ %u. Actual:0x%02X Expected:0x%02X\n",
                                currOffset, actualByte, expectedByte);
                        exit(-1);
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
                    fprintf(stderr, "error: Read mismatch @ %u. Actual:0x%08X Expected:0x%08X\n",
                            currOffset, actualWord, expectedWord);
                    exit(-1);
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
                        fprintf(stderr, "error: Read mismatch @ %u. Actual:0x%02X Expected:0x%02X\n",
                                currOffset, actualByte, expectedByte);
                        exit(-1);
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
            pFile = NULL;
        }
    }
    if (pFile)
    {
        fclose(pFile);
        pFile = NULL;
    }

    printf("Removing test file.\n");
    int removeResult = remove(testFilename);
    if (removeResult)
    {
        fprintf(stderr, "error: remove() failed - %d\n", removeResult);
        exit(-1);
    }

    printf("Test Completed!\n");

    return 0;
}
