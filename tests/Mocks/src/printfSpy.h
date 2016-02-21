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
/* Module for spying on printf output from code under test. */
#ifndef _PRINTF_SPY_H_
#define _PRINTF_SPY_H_

#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* Pointer to *printf routines which can intercepted by this module. */
extern int (*hook_printf)(const char* pFormat, ...);
extern int (*hook_fprintf)(FILE* pFile, const char* pFormat, ...);

void        printfSpy_Hook(size_t BufferSize);
void        printfSpy_Unhook(void);

const char* printfSpy_GetLastOutput(void);
const char* printfSpy_GetPreviousOutput(void);
const char* printfSpy_GetLastErrorOutput(void);
FILE*       printfSpy_GetLastFile(void);
size_t      printfSpy_GetCallCount(void);

#ifdef __cplusplus
}
#endif


#undef  printf
#define printf hook_printf

#undef fprintf
#define fprintf hook_fprintf


#endif /* _PRINTF_SPY_H_ */
