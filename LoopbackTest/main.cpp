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
/* Loopback tests for the SPIDma class. */
#include <SPIDma.h>


// States for chip select pin.
#define HIGH 1
#define LOW  0


// Function prototypes.
static void printTestResult(bool testResult);
static void printFinalTestResults();


uint32_t g_totalTestCases = 0;
uint32_t g_failingTestCases = 0;


int main(void)
{
    SPIDma     spi(p5, p6, p7, p8, 1);
    DigitalIn  cs(p9);
    bool       testResult = false;

    printf("\n\nSPIDma Loopback Test\n");
    printf("Make sure that your mbed-LPC1768 is wired up with the following pin loopback connections:\n");
    printf("    p5 - p6  Loopback MOSI to MISO\n");
    printf("    p7 - p8  Loopback the chip select pin for validation\n\n");

    printf("Press ENTER to start test.\n");
    char dummyBuffer[5];
    fgets(dummyBuffer, sizeof(dummyBuffer), stdin);

    // Test chip select functionality.
    printf("Verify chip select initializes to HIGH...");
    testResult = (cs == HIGH);
    printTestResult(testResult);

    printf("Verify m_spi.setChipSelect(LOW)...");
    spi.setChipSelect(LOW);
    testResult = (cs == LOW);
    printTestResult(testResult);

    printf("Verify m_spi.setChipSelect(HIGH)...");
    spi.setChipSelect(HIGH);
    testResult = (cs == HIGH);
    printTestResult(testResult);


    // Just use one fixed SPI configuration for this test.
    // Use a low frequency since that keeps the transmit FIFO filled during non-blocking sends.
    spi.format(8, 0);
    spi.frequency(10000);

    // exchange() tests.
    printf("Verify m_spi.exchange()...");
    testResult = true;
    for (int i = 0 ; i < 256 ; i++)
    {
        int byteReceived = spi.exchange(i);
        if (byteReceived != i)
        {
            printf("\nactual: %d expected: %d   ", byteReceived, i);
            testResult = false;
        }
    }
    // Verify that the 256 bytes were counted.
    if (spi.getByteCount() != 256)
    {
        printf("\ngetByteCount() returned: %lu expected: 256   ", spi.getByteCount());
        testResult = false;
    }
    printTestResult(testResult);

    // send() tests.
    printf("Verify m_spi.send()...");
    testResult = true;
    spi.resetByteCount();
    for (int i = 0 ; i < 256 ; i++)
    {
        spi.send(i);
    }
    // The FIFO is only 8 deep so most of the discarded reads should already been on the test queue.
    if (spi.isDiscardedQueueEmpty())
    {
        printf("\nDidn't expect discard queue to be empty after 256 byte send().  ");
        testResult = false;
    }
    // An exchange() will force all of the remaining discarded reads to be placed in the test queue.
    int byteReceived = spi.exchange(0x80);
    if (byteReceived != 0x80)
    {
        printf("\nexchange()-> actual: %d expected: %d   ", byteReceived, 0x80);
        testResult = false;
    }
    // Verify that the 256+1 bytes were counted.
    if (spi.getByteCount() != 256 + 1)
    {
        printf("\ngetByteCount() returned: %lu expected: 257   ", spi.getByteCount());
        testResult = false;
    }
    // Verify that the expected read values were discarded.
    for (int i = 0 ; i < 256 ; i++)
    {
        if (spi.isDiscardedQueueEmpty())
        {
            printf("\nDidn't expect discard queue to be empty.  ");
            testResult = false;
        }
        int discardedByte = spi.dequeueDiscardedRead();
        if (discardedByte != i)
        {
            printf("\nactual: %d expected: %d   ", discardedByte, i);
            testResult = false;
        }
    }
    if (!spi.isDiscardedQueueEmpty())
    {
        printf("\nExpected discard queue to now be empty.  ");
        testResult = false;
    }
    printTestResult(testResult);

    // transfer() tests.
    printf("Verify m_spi.transfer() with valid read & write buffers...");
    testResult = true;
    spi.resetByteCount();
    uint8_t writeBuffer[256];
    uint8_t readBuffer[256];
    memset(readBuffer, 0xAD, sizeof(readBuffer));
    for (int i = 0 ; i < 256 ; i++)
    {
        writeBuffer[i] = 255 - i;
    }
    spi.transfer(writeBuffer, sizeof(writeBuffer), readBuffer, sizeof(readBuffer));
    if (spi.getByteCount() != 256)
    {
        printf("\ngetByteCount() returned: %lu expected: 256   ", spi.getByteCount());
        testResult = false;
    }
    for (int i = 0 ; i < 256 ; i++)
    {
        int expectedByte = 255 - i;
        if (readBuffer[i] != expectedByte)
        {
            printf("\nactual: %d expected: %d   ", readBuffer[i], expectedByte);
            testResult = false;
        }
    }
    printTestResult(testResult);

    printf("Verify m_spi.transfer() with valid read buffer & single byte write buffer...");
    testResult = true;
    spi.resetByteCount();
    memset(readBuffer, 0xAD, sizeof(readBuffer));
    memset(writeBuffer, 0x5A, sizeof(writeBuffer));
    writeBuffer[0] = 0xDA;
    spi.transfer(writeBuffer, 1, readBuffer, sizeof(readBuffer));
    if (spi.getByteCount() != 256)
    {
        printf("\ngetByteCount() returned: %lu expected: 256   ", spi.getByteCount());
        testResult = false;
    }
    for (int i = 0 ; i < 256 ; i++)
    {
        if (readBuffer[i] != 0xDA)
        {
            printf("\nactual: %d expected: 0xDA   ", readBuffer[i]);
            testResult = false;
        }
    }
    printTestResult(testResult);

    printf("Verify m_spi.transfer() with valid write buffer & single byte read buffer...");
    testResult = true;
    spi.resetByteCount();
    memset(readBuffer, 0xAD, sizeof(readBuffer));
    for (int i = 0 ; i < 256 ; i++)
    {
        writeBuffer[i] = 255 - i;
    }
    spi.transfer(writeBuffer, sizeof(writeBuffer), readBuffer, 1);
    if (spi.getByteCount() != 256)
    {
        printf("\ngetByteCount() returned: %lu expected: 256   ", spi.getByteCount());
        testResult = false;
    }
    for (int i = 0 ; i < 256 ; i++)
    {
        int expectedByte = (i == 0) ? 0 : 0xAD;
        if (readBuffer[i] != expectedByte)
        {
            printf("\nactual: %d expected: %d   ", readBuffer[i], expectedByte);
            testResult = false;
        }
    }
    printTestResult(testResult);

    printf("Verify m_spi.transfer() with valid write buffer & NULL read buffer...");
    testResult = true;
    spi.resetByteCount();
    for (int i = 0 ; i < 256 ; i++)
    {
        writeBuffer[i] = 255 - i;
    }
    spi.transfer(writeBuffer, sizeof(writeBuffer), NULL, 0);
    if (spi.getByteCount() != 256)
    {
        printf("\ngetByteCount() returned: %lu expected: 256   ", spi.getByteCount());
        testResult = false;
    }
    printTestResult(testResult);

    printf("Verify m_spi.transfer() with full-sized read buffer right after send()...");
    testResult = true;
    spi.resetByteCount();
    memset(readBuffer, 0xAD, sizeof(readBuffer));
    for (int i = 0 ; i < 256 ; i++)
    {
        writeBuffer[i] = 255 - i;
    }
    spi.send(0x5A);
    spi.send(0xA5);
    spi.transfer(writeBuffer, sizeof(writeBuffer), readBuffer, sizeof(readBuffer));
    if (spi.getByteCount() != 256 + 2)
    {
        printf("\ngetByteCount() returned: %lu expected: 258   ", spi.getByteCount());
        testResult = false;
    }
    for (int i = 0 ; i < 256 ; i++)
    {
        int expectedByte = 255 - i;
        if (readBuffer[i] != expectedByte)
        {
            printf("\nactual: %d expected: %d   ", readBuffer[i], expectedByte);
            testResult = false;
        }
    }
    printTestResult(testResult);

    printf("Verify m_spi.transfer() with single byte read buffer after send()...");
    testResult = true;
    spi.resetByteCount();
    memset(readBuffer, 0xAD, sizeof(readBuffer));
    for (int i = 0 ; i < 256 ; i++)
    {
        writeBuffer[i] = 255 - i;
    }
    spi.send(0x5A);
    spi.send(0xA5);
    spi.transfer(writeBuffer, sizeof(writeBuffer), readBuffer, 1);
    if (spi.getByteCount() != 256 + 2)
    {
        printf("\ngetByteCount() returned: %lu expected: 258   ", spi.getByteCount());
        testResult = false;
    }
    for (int i = 0 ; i < 256 ; i++)
    {
        int expectedByte = (i == 0) ? 0 : 0xAD;
        if (readBuffer[i] != expectedByte)
        {
            printf("\nactual: %d expected: %d   ", readBuffer[i], expectedByte);
            testResult = false;
        }
    }
    printTestResult(testResult);

    printf("Verify m_spi.transfer() with single byte read buffer after send() followed by exchange()...");
    testResult = true;
    spi.resetByteCount();
    memset(readBuffer, 0xAD, sizeof(readBuffer));
    for (int i = 0 ; i < 256 ; i++)
    {
        writeBuffer[i] = 255 - i;
    }
    spi.send(0x5A);
    spi.send(0xA5);
    spi.transfer(writeBuffer, sizeof(writeBuffer), readBuffer, 1);
    if (spi.getByteCount() != 256 + 2)
    {
        printf("\ngetByteCount() returned: %lu expected: 258   ", spi.getByteCount());
        testResult = false;
    }
    for (int i = 0 ; i < 256 ; i++)
    {
        int expectedByte = (i == 0) ? 0 : 0xAD;
        if (readBuffer[i] != expectedByte)
        {
            printf("\nactual: %d expected: %d   ", readBuffer[i], expectedByte);
            testResult = false;
        }
    }
    spi.exchange(0xFF);
    printTestResult(testResult);


    printFinalTestResults();
    return 0;
}

static void printTestResult(bool testResult)
{
    printf("%s\n", testResult ? "Pass" : "Failure");
    g_totalTestCases++;
    if (!testResult)
    {
        g_failingTestCases++;
    }
}

static void printFinalTestResults()
{
    printf("\n");
    printf("Failing Tests: %lu %s\n", g_failingTestCases, (g_failingTestCases > 0) ? "**" : "");
    printf("Passing Tests: %lu\n", g_totalTestCases - g_failingTestCases);
    printf("  Total Tests: %lu\n", g_totalTestCases);
}