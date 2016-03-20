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
#include "../../../SDFileSystem/Interlocked.h"

// Include C++ headers for test harness.
#include "CppUTest/TestHarness.h"


TEST_GROUP(Interlocked)
{
    void setup()
    {
    }

    void teardown()
    {
    }
};

TEST(Interlocked, InterlockedIncrementDecrement)
{
    uint32_t value = 0;

    // Increment twice.
    LONGS_EQUAL(0, value);
    LONGS_EQUAL(1, interlockedIncrement(&value));
    LONGS_EQUAL(1, value);
    LONGS_EQUAL(2, interlockedIncrement(&value));
    LONGS_EQUAL(2, value);

    // Decrement twice.
    LONGS_EQUAL(1, interlockedDecrement(&value));
    LONGS_EQUAL(1, value);
    LONGS_EQUAL(0, interlockedDecrement(&value));
    LONGS_EQUAL(0, value);
}

TEST(Interlocked, InterlockedAddSubtract)
{
    uint32_t value = 0;

    // Add three times.
    LONGS_EQUAL(0, value);
    LONGS_EQUAL(7, interlockedAdd(&value, 7));
    LONGS_EQUAL(7, value);
    LONGS_EQUAL(14, interlockedAdd(&value, 7));
    LONGS_EQUAL(14, value);
    LONGS_EQUAL(16, interlockedAdd(&value, 2));
    LONGS_EQUAL(16, value);

    // Subtract twice.
    LONGS_EQUAL(8, interlockedSubtract(&value, 8));
    LONGS_EQUAL(8, value);
    LONGS_EQUAL(0, interlockedSubtract(&value, 8));
    LONGS_EQUAL(0, value);
}
