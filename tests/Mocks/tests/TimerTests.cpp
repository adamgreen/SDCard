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
#include <Timer.h>

// Include C++ headers for test harness.
#include "CppUTest/TestHarness.h"


TEST_GROUP(Timer)
{
    void setup()
    {
    }

    void teardown()
    {
    }
};


TEST(Timer, ReadTimeTwiceBeforeStartingTimer_ShouldReturn0BothTimes)
{
    Timer timer;

    LONGS_EQUAL(0, timer.read_ms());
    LONGS_EQUAL(0, timer.read_ms());
}

TEST(Timer, CheckTimeAfterCallingStartAndThenStop_TimeShouldOnlyProgressInOneMillisecondIncWhenRunning)
{
    Timer timer;

    timer.start();
    LONGS_EQUAL(1, timer.read_ms());
    LONGS_EQUAL(2, timer.read_ms());
    timer.stop();
    LONGS_EQUAL(2, timer.read_ms());
}

TEST(Timer, CheckTimeBeforeAndAfterReset_ShouldResetTime)
{
    Timer timer;

    timer.start();
    LONGS_EQUAL(1, timer.read_ms());
    LONGS_EQUAL(2, timer.read_ms());
    LONGS_EQUAL(3, timer.read_ms());
    timer.reset();
    LONGS_EQUAL(1, timer.read_ms());
}

TEST(Timer, CheckTimeAfterCallingStart_UseNonDefaultIncrementAmount)
{
    Timer timer;

    timer.setElapsedTimePerCall(10);
    timer.start();
    LONGS_EQUAL(10, timer.read_ms());
    LONGS_EQUAL(20, timer.read_ms());
    LONGS_EQUAL(30, timer.read_ms());
}
