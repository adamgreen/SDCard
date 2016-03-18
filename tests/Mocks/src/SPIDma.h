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
#ifndef SPI_DMA_H_
#define SPI_DMA_H_

// Define this here for PC based unit testing so that we don't need to use mbed provided ones.
typedef uint32_t PinName;

class SPIDma
{
public:
    SPIDma(PinName mosi, PinName miso, PinName sclk, PinName ssel = 0, int sselInitVal = 1);
    ~SPIDma();

    // Mimic routines that exist in real SPIDma implementation.
    void format(int bits, int mode = 0);
    void frequency(int hz = 1000000);

    void setChipSelect(int state);

    void send(int data);
    int  exchange(int data);
    bool transfer(const void* pvWrite, size_t writeSize, void* pvRead, size_t readSize);

    uint32_t getByteCount();
    void     resetByteCount();


    // Routines & structures just used for unit testing.
    enum SettingType
    {
        Frequency = 1,
        Format,
        ChipSelect
    };
    struct Settings
    {
        SettingType type;
        size_t      bytesSentBefore;
        int         frequency;
        int         bits;
        int         mode;
        int         chipSelect;
    };

    const char* getOutboundAsString(int start = 0, int count = -1);
    void        setInboundFromString(const char* pData);
    bool        isInboundBufferEmpty();
    size_t      getSettingsCount();
    Settings    getSetting(size_t index);
    void        failTransferCall(uint32_t callToFail, uint32_t failRepeatCount = 1);

protected:
    static uint32_t hexToNibble(char digit);
    void            recordLatestSetting();

    uint8_t*  m_pOutBuffer;
    uint8_t*  m_pOutCurr;
    uint8_t*  m_pInBuffer;
    uint8_t*  m_pInCurr;
    char*     m_pStringBuffer;
    Settings* m_pSettings;
    Settings* m_pSettingsCurr;
    size_t    m_outAlloc;
    size_t    m_inAlloc;
    size_t    m_stringAlloc;
    size_t    m_settingsAlloc;
    Settings  m_settings;
    uint32_t  m_byteCount;
    uint32_t  m_transferCall;
    uint32_t  m_transferFailStart;
    uint32_t  m_transferFailStop;
};

#endif /* SPI_DMA_H_ */
