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
// Include headers from C modules under test.
extern "C"
{
    #include "printfSpy.h"
}

// Include C++ headers for test harness.
#include "CppUTest/TestHarness.h"


const char g_HelloWorld[] = "Hello World!\n";


TEST_GROUP(printfSpy)
{
    int m_Result;

    void setup()
    {
        m_Result = -1;
    }

    void teardown()
    {
        printfSpy_Unhook();
    }

    void printfCheck(int ExpectedLength, const char* pExpectedString)
    {
        LONGS_EQUAL(ExpectedLength, m_Result);
        STRCMP_EQUAL(pExpectedString, printfSpy_GetLastOutput());
        STRCMP_EQUAL("", printfSpy_GetPreviousOutput());
        LONGS_EQUAL(1, printfSpy_GetCallCount());
        POINTERS_EQUAL(stdout, printfSpy_GetLastFile());
    }

    char* CreateCheckBuffer(size_t BufferSize)
    {
        char* pCheckString = NULL;

        pCheckString = (char*)malloc(BufferSize + 1);
        CHECK(NULL != pCheckString);

        strncpy(pCheckString, g_HelloWorld, BufferSize);
        pCheckString[BufferSize] = '\0';

        return pCheckString;
    }

    void printfCheckHelloWorldWithBufferOfSize(size_t BufferSize)
    {
        printfSpy_Hook(BufferSize);
        m_Result = hook_printf(g_HelloWorld);

        char* pCheckString = CreateCheckBuffer(BufferSize);

        printfCheck(sizeof(g_HelloWorld)-1, pCheckString);

        free(pCheckString);
    }
};


TEST(printfSpy, BufferSize0)
{
    printfCheckHelloWorldWithBufferOfSize(0);
}


TEST(printfSpy, BufferSize1)
{
    printfCheckHelloWorldWithBufferOfSize(1);
}

TEST(printfSpy, BufferSizeMinus1)
{
    printfCheckHelloWorldWithBufferOfSize(sizeof(g_HelloWorld)-2);
}

TEST(printfSpy, BufferSizeExact)
{
    printfCheckHelloWorldWithBufferOfSize(sizeof(g_HelloWorld)-1);
}

TEST(printfSpy, BufferSizePlus1)
{
    printfCheckHelloWorldWithBufferOfSize(sizeof(g_HelloWorld));
}

TEST(printfSpy, WithFormatting)
{
    printfSpy_Hook(10);
    m_Result = hook_printf("Hello %s\n", "World");

    printfCheck(12, "Hello Worl");
}

TEST(printfSpy, TwoCall)
{
    printfSpy_Hook(10);
    hook_printf("Line 1\r\n");
    hook_printf("Line 2\r\n");
    LONGS_EQUAL(2, printfSpy_GetCallCount());
    STRCMP_EQUAL("Line 2\r\n", printfSpy_GetLastOutput());
    STRCMP_EQUAL("Line 1\r\n", printfSpy_GetPreviousOutput());
}

TEST(printfSpy, SendToStdErr)
{
    printfSpy_Hook(10);
    POINTERS_EQUAL(NULL, printfSpy_GetLastFile());
    hook_fprintf(stderr, "Line 1\r\n");
    POINTERS_EQUAL(stderr, printfSpy_GetLastFile());
    STRCMP_EQUAL("Line 1\r\n", printfSpy_GetLastOutput());
    STRCMP_EQUAL("Line 1\r\n", printfSpy_GetLastErrorOutput());
}
