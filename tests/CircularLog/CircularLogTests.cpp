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
#include <string.h>
#include <limits.h>
#include <CircularLog.h>
#include <printfSpy.h>

// Include C++ headers for test harness.
#include "CppUTest/TestHarness.h"

TEST_GROUP(CircularLog)
{
    void setup()
    {
        printfSpy_Hook(1024);
    }

    void teardown()
    {
        printfSpy_Unhook();
    }
};

TEST(CircularLog, WriteEmptyLog)
{
    CircularLog<1,0> log;

    log.dump(stderr);

    LONGS_EQUAL(0, printfSpy_GetCallCount());
}

TEST(CircularLog, LogOneShortEntry)
{
    CircularLog<9,8> log;

    log.log("Test %d\n", 1);
    log.dump(stderr);

    LONGS_EQUAL(1, printfSpy_GetCallCount());
    STRCMP_EQUAL("Test 1\n", printfSpy_GetLastOutput());
    POINTERS_EQUAL(stderr, printfSpy_GetLastFile());
}

TEST(CircularLog, LogOneShortEntry_WriteToStdOut)
{
    CircularLog<9,8> log;

    log.log("Test %d\n", 1);
    log.dump(stdout);

    LONGS_EQUAL(1, printfSpy_GetCallCount());
    STRCMP_EQUAL("Test 1\n", printfSpy_GetLastOutput());
    POINTERS_EQUAL(stdout, printfSpy_GetLastFile());
}

TEST(CircularLog, LogOneEntryTooLongForLineBuffer_ShouldTruncateAtEnd)
{
    CircularLog<9,7> log;

    log.log("Test %d\n", 1);
    log.dump(stderr);

    LONGS_EQUAL(1, printfSpy_GetCallCount());
    STRCMP_EQUAL("Test 1", printfSpy_GetLastOutput());
    POINTERS_EQUAL(stderr, printfSpy_GetLastFile());
}

TEST(CircularLog, LogTwoShortEntriesWhichFitInBuffer)
{
    CircularLog<15,8> log;

    log.log("Test %d\n", 1);
    log.log("Test %d\n", 2);
    log.dump(stderr);

    LONGS_EQUAL(1, printfSpy_GetCallCount());
    STRCMP_EQUAL("Test 1\nTest 2\n", printfSpy_GetLastOutput());
    POINTERS_EQUAL(stderr, printfSpy_GetLastFile());
}

TEST(CircularLog, LogTwoEntriesWhichOverflowByOneChar_ExpectFirstEntryToBeTruncatedBy1Char)
{
    CircularLog<14,8> log;

    log.log("Test %d\n", 1);
    log.log("Test %d\n", 2);
    log.dump(stderr);

    LONGS_EQUAL(2, printfSpy_GetCallCount());
    STRCMP_EQUAL("est 1\nTest 2\n", printfSpy_GetPreviousOutput());
    STRCMP_EQUAL("", printfSpy_GetLastOutput());
    POINTERS_EQUAL(stderr, printfSpy_GetLastFile());
}

TEST(CircularLog, LogTwoEntriesWhichOverflowByTwoChars_ExpectFirstEntryToBeTruncatedBy2Char)
{
    CircularLog<13,8> log;

    log.log("Test %d\n", 1);
    log.log("Test %d\n", 2);
    log.dump(stderr);

    LONGS_EQUAL(2, printfSpy_GetCallCount());
    STRCMP_EQUAL("st 1\nTest 2", printfSpy_GetPreviousOutput());
    STRCMP_EQUAL("\n", printfSpy_GetLastOutput());
    POINTERS_EQUAL(stderr, printfSpy_GetLastFile());
}

TEST(CircularLog, LogThreeEntriesWhichOverflowToEraseFirstOneCompletely)
{
    CircularLog<15,8> log;

    log.log("Test %d\n", 1);
    log.log("Test %d\n", 2);
    log.log("Test %d\n", 3);
    log.dump(stderr);

    LONGS_EQUAL(2, printfSpy_GetCallCount());
    STRCMP_EQUAL("Test 2\nT", printfSpy_GetPreviousOutput());
    STRCMP_EQUAL("est 3\n", printfSpy_GetLastOutput());
    POINTERS_EQUAL(stderr, printfSpy_GetLastFile());
}

TEST(CircularLog, LogOneShortEntryAndThenClearLog)
{
    CircularLog<9,8> log;

    log.log("Test %d\n", 1);
    log.clear();
    log.dump(stderr);
    LONGS_EQUAL(0, printfSpy_GetCallCount());
}

TEST(CircularLog, IsEmptyOnNewLog_ShouldReturnTrue)
{
    CircularLog<10,5> log;

    CHECK_TRUE(log.isEmpty());
}

TEST(CircularLog, IsEmptyOnOneByteInLog_ShouldReturnFalse)
{
    CircularLog<10,5> log;

    log.log("\n");
    CHECK_FALSE(log.isEmpty());
}

TEST(CircularLog, IsEmptyAfterClear_ShouldReturnTrue)
{
    CircularLog<10,5> log;

    log.log("\n");
    log.clear();
    CHECK_TRUE(log.isEmpty());
}