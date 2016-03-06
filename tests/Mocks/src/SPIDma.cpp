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
// Mock to simulate SPIDma that would run on real hardware. It records outbound SPI traffic and plays back test
// provided inbound SPI traffic.
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "SPIDma.h"

// How much to grow the output buffer each time we run out of space.
#define OUT_GROW 1024
// How much to grow the settings array each time we run out of space.
#define SETTINGS_GROW 2


SPIDma::SPIDma(PinName mosi, PinName miso, PinName sclk, PinName ssel /* = 0 */, int sselInitVal /* = 1 */)
{
    m_pOutBuffer = NULL;
    m_pOutCurr = NULL;
    m_pInBuffer = NULL;
    m_pInCurr = NULL;
    m_pStringBuffer = NULL;
    m_pSettings = NULL;
    m_pSettingsCurr = NULL;
    m_outAlloc = 0;
    m_inAlloc = 0;
    m_stringAlloc = 0;
    m_settingsAlloc = 0;
    memset(&m_settings, 0, sizeof(m_settings));
    if (ssel > 0)
    {
        setChipSelect(sselInitVal);
    }
}

SPIDma::~SPIDma()
{
    free(m_pOutBuffer);
    m_pOutCurr = NULL;
    m_outAlloc = 0;
    free(m_pStringBuffer);
    m_pStringBuffer = NULL;
    m_stringAlloc = 0;
    free(m_pInBuffer);
    m_pInBuffer = NULL;
    m_pInCurr = NULL;
    m_inAlloc = 0;
    free(m_pSettings);
    m_pSettings = NULL;
    m_pSettingsCurr = NULL;
    m_settingsAlloc = 0;
}

void SPIDma::setChipSelect(int state)
{
    m_settings.type = ChipSelect;
    m_settings.chipSelect = state;
    m_settings.bytesSentBefore = m_pOutCurr - m_pOutBuffer;

    recordLatestSetting();
}

void SPIDma::format(int bits, int mode /* = 0 */)
{
    m_settings.type = Format;
    m_settings.bits = bits;
    m_settings.mode = mode;
    m_settings.bytesSentBefore = m_pOutCurr - m_pOutBuffer;

    recordLatestSetting();
}

void SPIDma::recordLatestSetting()
{
    size_t settingCount = m_pSettingsCurr - m_pSettings;
    if (settingCount >= m_settingsAlloc)
    {
        // Need to grow the array used to store outbound data.
        size_t newSize = m_settingsAlloc + SETTINGS_GROW;

        // This is test code and doesn't run in production so don't worry about alloc failure.
        Settings* pRealloc = (Settings*)realloc(m_pSettings, sizeof(*pRealloc) * newSize);
        assert ( pRealloc );
        m_pSettings = pRealloc;
        m_pSettingsCurr = pRealloc + settingCount;
        m_settingsAlloc = newSize;
    }
    *m_pSettingsCurr++ = m_settings;
}

void SPIDma::frequency(int hz /* = 1000000 */)
{
    m_settings.type = Frequency;
    m_settings.frequency = hz;
    m_settings.bytesSentBefore = m_pOutCurr - m_pOutBuffer;

    recordLatestSetting();
}

void SPIDma::send(int data)
{
    int bytesUsed = m_pOutCurr - m_pOutBuffer;
    if ((size_t)bytesUsed >= m_outAlloc)
    {
        // Need to grow the buffer used to store outbound data.
        size_t newSize = m_outAlloc + OUT_GROW;

        // This is test only code and doesn't run in production so don't worry about alloc failure.
        uint8_t* pRealloc = (uint8_t*)realloc(m_pOutBuffer, newSize);
        assert ( pRealloc );
        m_pOutBuffer = pRealloc;
        m_pOutCurr = pRealloc + bytesUsed;
        m_outAlloc = newSize;
    }

    *m_pOutCurr++ = data;
}

int  SPIDma::exchange(int data)
{
    send(data);

    int ret = 0xBD;
    assert ( !isInboundBufferEmpty() );
    if (!isInboundBufferEmpty())
        ret = *m_pInCurr++;
    return ret;
}

void SPIDma::transfer(const void* pvWrite, size_t writeSize, void* pvRead, size_t readSize)
{
    const uint8_t* pWrite = (const uint8_t*)pvWrite;
    uint8_t*       pRead = (uint8_t*)pvRead;
    size_t         transferSize = (writeSize > readSize) ? writeSize : readSize;
    int            readIncrement = (readSize > 1) ? 1 : 0;
    int            writeIncrement = (writeSize > 1) ? 1 : 0;

    while (transferSize--)
    {
        if (pRead)
        {
            *pRead = exchange(*pWrite);
            pRead += readIncrement;
        }
        else
        {
            send(*pWrite);
        }
        pWrite += writeIncrement;
    }
}

const char* SPIDma::getOutboundAsString(int start /* = 0 */, int count /* = -1 */)
{
    static const char hexDigits[] = "0123456789ABCDEF";
    int outBytes = m_pOutCurr - m_pOutBuffer;

    if (start < 0 || start >= outBytes)
    {
        // Attempt to read past end.
        return "";
    }

    if (count == -1)
    {
        // All remaining bytes.
        count = outBytes - start;
    }

    // Require 2 hex digits per byte + 1 nul terminator.
    size_t charRequired = 2 * count + 1;
    if (charRequired > m_stringAlloc)
    {
        // This is test only code and doesn't run in production so don't worry about alloc failure.
        char* pRealloc = (char*)realloc(m_pStringBuffer, charRequired);
        assert ( pRealloc );
        m_pStringBuffer = pRealloc;
        m_stringAlloc = charRequired;
    }

    uint8_t* pRead = m_pOutBuffer + start;
    char* pWrite = m_pStringBuffer;
    for (int i = 0 ; i < count ; i++)
    {
        uint8_t byte = *pRead++;
        *pWrite++ = hexDigits[byte >> 4];
        *pWrite++ = hexDigits[byte & 0xF];
    }
    *pWrite = '\0';

    return m_pStringBuffer;
}

void SPIDma::setInboundFromString(const char* pData)
{
    size_t len = strlen(pData);
    // There are two hex digits per byte.
    size_t bytesToAdd = len / 2;
    size_t newLength = m_inAlloc + bytesToAdd;
    int currOffset = m_pInCurr - m_pInBuffer;

    // This is test only code and doesn't run in production so don't worry about alloc failure.
    uint8_t* pRealloc = (uint8_t*)realloc(m_pInBuffer, newLength);
    assert ( pRealloc );
    m_pInBuffer = pRealloc;
    m_pInCurr = pRealloc + currOffset;
    uint8_t* pNewBytes = pRealloc + m_inAlloc;
    m_inAlloc = newLength;

    const char* pRead = pData;
    uint8_t* pWrite = pNewBytes;
    for (size_t i = 0 ; i < bytesToAdd ; i++)
    {
        uint8_t byte;

        byte  = hexToNibble(*pRead++) << 4;
        byte |= hexToNibble(*pRead++);
        *pWrite++ = byte;
    }
}

uint32_t SPIDma::hexToNibble(char digit)
{
    uint32_t ret = 0;

    digit = tolower(digit);

    // The test code shouldn't pass in such invalid digits and this isn't used in production so assert is enough.
    assert ( (digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f') );
    if (digit >= '0' && digit <= '9')
        ret = digit - '0';
    if (digit >= 'a' && digit <= 'f')
        ret = digit - 'a' + 10;

    return ret;
}

bool SPIDma::isInboundBufferEmpty()
{
    return m_pInCurr >= m_pInBuffer + m_inAlloc;
}

size_t SPIDma::getSettingsCount()
{
    return m_pSettingsCurr - m_pSettings;
}

SPIDma::Settings SPIDma::getSetting(size_t index)
{
    assert ( (int)index < m_pSettingsCurr - m_pSettings );
    return m_pSettings[index];
}
