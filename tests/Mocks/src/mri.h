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
#ifndef MRI_H
#define MRI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif


void __debugbreak(void);
extern uint32_t g_debugBreakCount;


#ifdef __cplusplus
}
#endif

#endif // MRI_H
