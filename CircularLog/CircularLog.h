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
#ifndef CIRCULAR_LOG_H_
#define CIRCULAR_LOG_H_

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

class CircularLogBase
{
public:
    void dump(FILE* pFile);
    void clear();
    bool isEmpty()
    {
        return m_pEnqueue == m_pDequeue;
    }

protected:
    CircularLogBase()
    {
    }

    void log(char* lineBuffer, size_t bufferSize, const char* pFormat, va_list args);

    void enqueueChar(char ch);
    void advancePointer(char*& p)
    {
        p++;
        if (p == m_pEnd)
        {
            p = m_pStart;
        }
    }
    bool doesStringWrapAround()
    {
        return m_pDequeue > m_pEnqueue;
    }

    char* m_pStart;
    char* m_pEnd;
    char* m_pEnqueue;
    char* m_pDequeue;
};


template <size_t SIZE, size_t MAX_LINE>
class CircularLog : public CircularLogBase
{
public:
    CircularLog()
    {
        assert ( SIZE > MAX_LINE );

        m_pStart = m_buffer;
        m_pEnd = m_pStart + sizeof(m_buffer);
        clear();
    }

    void log(const char* pFormat, ...)
    {
        char lineBuffer[MAX_LINE];

        va_list args;
        va_start(args, pFormat);
            CircularLogBase::log(lineBuffer, sizeof(lineBuffer), pFormat, args);
        va_end(args);
    }

protected:
    char  m_buffer[SIZE];
};

#endif /* CIRCULAR_LOG_H_ */
