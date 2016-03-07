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
// Mock to simulate mbed's Timer class.  It will just return a fixed amount of elapsed time between each call to
// read_ms().
#include "Timer.h"

Timer::Timer()
{
    m_isRunning = false;
    m_currTime = 0;
    m_elapsedTimePerCall = 1;
}

int Timer::read_ms()
{
    if (m_isRunning)
    {
        m_currTime += m_elapsedTimePerCall;
    }
    return m_currTime;
}

void Timer::start()
{
    m_isRunning = true;
}

void Timer::stop()
{
    m_isRunning = false;
}

void Timer::reset()
{
    m_currTime = 0;
}

void Timer::setElapsedTimePerCall(int amount)
{
    m_elapsedTimePerCall = amount;
}
