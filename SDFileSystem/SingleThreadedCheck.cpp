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
/* Class to check that only a single thread is accessing an object. */
#include <mri.h>
#include "SingleThreadedCheck.h"
#include "Interlocked.h"


// Static Members.
volatile uint32_t SingleThreadedCheck::m_threadCount = 0;


SingleThreadedCheck::SingleThreadedCheck()
{
    // Increment counter when this thread enters scope.
    uint32_t newThreadCount = interlockedIncrement(&m_threadCount);
    if (newThreadCount != 1)
    {
        // More than 1 thread is attempting to use the object at a time.
        __debugbreak();
    }
}

SingleThreadedCheck::~SingleThreadedCheck()
{
    // Decrement counter when this thread leaves scope.
    interlockedDecrement(&m_threadCount);
}
