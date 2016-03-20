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
#include <SingleThreadedCheck.h>
#include <mri.h>

// Include C++ headers for test harness.
#include "CppUTest/TestHarness.h"


TEST_GROUP(SingleThreadedCheck)
{
    void setup()
    {
        g_debugBreakCount = 0;
        SingleThreadedCheck::m_threadCount = 0;
    }

    void teardown()
    {
        g_debugBreakCount = 0;
        SingleThreadedCheck::m_threadCount = 0;
    }
};

TEST(SingleThreadedCheck, SuccessfulConstructDestruct)
{
    LONGS_EQUAL(0, g_debugBreakCount);
    LONGS_EQUAL(0, SingleThreadedCheck::m_threadCount);
    {
        SingleThreadedCheck test;

        LONGS_EQUAL(0, g_debugBreakCount);
        LONGS_EQUAL(1, SingleThreadedCheck::m_threadCount);
    }
    LONGS_EQUAL(0, SingleThreadedCheck::m_threadCount);
    LONGS_EQUAL(0, g_debugBreakCount);
}

TEST(SingleThreadedCheck, SimulateAnotherThreadUsingObject_FailConstructor)
{
    LONGS_EQUAL(0, g_debugBreakCount);
    // Simulate that another thread is already using object by bumping count up to be non-zero.
    SingleThreadedCheck::m_threadCount = 1;
    {
        SingleThreadedCheck test;

        LONGS_EQUAL(1, g_debugBreakCount);
        LONGS_EQUAL(2, SingleThreadedCheck::m_threadCount);
    }
    LONGS_EQUAL(1, SingleThreadedCheck::m_threadCount);
    LONGS_EQUAL(1, g_debugBreakCount);
}
