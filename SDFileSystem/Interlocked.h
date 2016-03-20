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
#ifndef INTERLOCK_H__
#define INTERLOCK_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

uint32_t interlockedIncrement(volatile uint32_t* pValue);
uint32_t interlockedDecrement(volatile uint32_t* pValue);
uint32_t interlockedAdd(volatile uint32_t* pVal1, uint32_t val2);
uint32_t interlockedSubtract(volatile uint32_t* pVal1, uint32_t val2);

#ifdef __cplusplus
}
#endif

#endif // INTERLOCK_H__
