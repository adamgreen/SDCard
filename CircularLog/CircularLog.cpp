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
/* Circular Log to hold recent debug printf() like spew. */
#include <stdio.h>
#include "CircularLog.h"

// Only hook in the printfSpy mocks when building non-ARM unit tests.
#ifndef __ARM_EABI__
    #include <printfSpy.h>
#endif



void CircularLogBase::log(char* lineBuffer, size_t bufferSize, const char* pFormat, va_list args)
{
    vsnprintf(lineBuffer, bufferSize, pFormat, args);

    char* pCurr = lineBuffer;
    while (*pCurr)
    {
        enqueueChar(*pCurr++);
    }
}

void CircularLogBase::enqueueChar(char ch)
{
    *m_pEnqueue = ch;
    advancePointer(m_pEnqueue);
    if (m_pDequeue == m_pEnqueue)
    {
        // Overflowing so advance dequeue pointer and lose one character from oldest part of string.
        advancePointer(m_pDequeue);
    }
}

void CircularLogBase::dump(FILE* pFile)
{
    if (isEmpty())
    {
        return;
    }
    if (doesStringWrapAround())
    {
        fprintf(pFile, "%.*s", m_pEnd - m_pDequeue, m_pDequeue);
        fprintf(pFile, "%.*s", m_pEnqueue - m_pStart, m_pStart);
    }
    else
    {
        fprintf(pFile, "%.*s", m_pEnqueue - m_pDequeue, m_pDequeue);
    }
}

void CircularLogBase::clear()
{
    m_pEnqueue = m_pDequeue = m_pStart;
}
