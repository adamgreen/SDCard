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
// A mock for the real mri.h to provide a dummy for the __debugbreak() function.
#include "mri.h"

uint32_t g_debugBreakCount = 0;


// This implementation just increments g_debugBreakCount each time it is called so that unit tests can query the
// global to determine if it has been called as expected.
void __debugbreak(void)
{
    g_debugBreakCount++;
}
