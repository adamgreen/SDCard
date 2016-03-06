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
#include <SPIDma.h>

// Include C++ headers for test harness.
#include "CppUTest/TestHarness.h"


#define HIGH 1
#define LOW  0


TEST_GROUP(SPIDma)
{
    void setup()
    {
    }

    void teardown()
    {
    }
};


TEST(SPIDma, WriteNothing_VerifyNoBytesRecorded)
{
    SPIDma spi(1, 2, 3);

    STRCMP_EQUAL("", spi.getOutboundAsString());
}

TEST(SPIDma, WriteOneByte_VerifyItWasRecordedAsOutbound)
{
    SPIDma spi(1, 2, 3);

    spi.send(0xFF);
    STRCMP_EQUAL("FF", spi.getOutboundAsString());
}

TEST(SPIDma, WriteTwoBytes_VerifyAllAtOnce)
{
    SPIDma spi(1, 2, 3);

    spi.send(0x12);
    spi.send(0x34);
    STRCMP_EQUAL("1234", spi.getOutboundAsString());
}

TEST(SPIDma, WriteTwoBytes_VerifyOneAtATime)
{
    SPIDma spi(1, 2, 3);

    spi.send(0x56);
    spi.send(0x78);
    STRCMP_EQUAL("56", spi.getOutboundAsString(0, 1));
    STRCMP_EQUAL("78", spi.getOutboundAsString(1, 1));
}

TEST(SPIDma, GetOutboundAsStringWithIndexOutOfBounds_ShouldReturnEmptyString)
{
    SPIDma spi(1, 2, 3);

    spi.send(0x12);
    STRCMP_EQUAL("", spi.getOutboundAsString(1, 1));
}

TEST(SPIDma, GetOutboundAsStringWithNegativeIndex_ShouldReturnEmptyString)
{
    SPIDma spi(1, 2, 3);

    spi.send(0x12);
    STRCMP_EQUAL("", spi.getOutboundAsString(-1, 1));
}

TEST(SPIDma, ExchangeOneByte_VerifyWriteAndRead)
{
    SPIDma spi(1, 2, 3);

    spi.setInboundFromString("12");
    LONGS_EQUAL(0x12, spi.exchange(0x9A));
    STRCMP_EQUAL("9A", spi.getOutboundAsString());
}

TEST(SPIDma, ExchangeTwoBytes_VerifyWritesAndReads)
{
    SPIDma spi(1, 2, 3);

    spi.setInboundFromString("3456");
    LONGS_EQUAL(0x34, spi.exchange(0xBC));
    LONGS_EQUAL(0x56, spi.exchange(0xDE));
    STRCMP_EQUAL("BCDE", spi.getOutboundAsString());
}

TEST(SPIDma, CallSetInboundFromStringTwice_ShouldAppendBytes)
{
    SPIDma spi(1, 2, 3);

    spi.setInboundFromString("78");
    spi.setInboundFromString("9A");
    LONGS_EQUAL(0x78, spi.exchange(0xF0));
    LONGS_EQUAL(0x9A, spi.exchange(0x12));
    STRCMP_EQUAL("F012", spi.getOutboundAsString());
}

TEST(SPIDma, TransferOneByte_VerifyWriteAndRead)
{
    SPIDma spi(1, 2, 3);
    uint8_t writeBuffer[1] = { 0x34 };
    uint8_t readBuffer[1] = { 0xFF };

    spi.setInboundFromString("BC");
    spi.transfer(writeBuffer, sizeof(writeBuffer), readBuffer, sizeof(readBuffer));
    LONGS_EQUAL(0xBC, readBuffer[0]);
    STRCMP_EQUAL("34", spi.getOutboundAsString());
}

TEST(SPIDma, TransferTwoBytes_VerifyWritesAndReads)
{
    SPIDma spi(1, 2, 3);
    uint8_t writeBuffer[2] = { 0x56, 0x78 };
    uint8_t readBuffer[2] = { 0xFF, 0xFF };

    spi.setInboundFromString("DEF0");
    spi.transfer(writeBuffer, sizeof(writeBuffer), readBuffer, sizeof(readBuffer));
    LONGS_EQUAL(0xDE, readBuffer[0]);
    LONGS_EQUAL(0xF0, readBuffer[1]);
    STRCMP_EQUAL("5678", spi.getOutboundAsString());
}

TEST(SPIDma, TransferWithMultiByteWriteButNoRead_ShouldStillWriteSuccessfully)
{
    SPIDma spi(1, 2, 3);
    uint8_t writeBuffer[2] = { 0x9A, 0xBC };

    spi.transfer(writeBuffer, sizeof(writeBuffer), NULL, 0);
    STRCMP_EQUAL("9ABC", spi.getOutboundAsString());
}

TEST(SPIDma, TransferWithMultiByteWriteButOneByteReadBuffer_ShouldJustReturnLastByteRead)
{
    SPIDma spi(1, 2, 3);
    uint8_t writeBuffer[2] = { 0xDE, 0xF0 };
    uint8_t readBuffer[1] = { 0xFF };

    spi.setInboundFromString("1234");
    spi.transfer(writeBuffer, sizeof(writeBuffer), readBuffer, sizeof(readBuffer));
    LONGS_EQUAL(0x34, readBuffer[0]);
    STRCMP_EQUAL("DEF0", spi.getOutboundAsString());
}

TEST(SPIDma, TransferWithMultiByteReadButOneByteWriteBuffer_ShouldSendSameBytTwice)
{
    SPIDma spi(1, 2, 3);
    uint8_t writeBuffer = 0x12;
    uint8_t readBuffer[2] = { 0xFF, 0xFF };

    spi.setInboundFromString("5678");
    spi.transfer(&writeBuffer, sizeof(writeBuffer), readBuffer, sizeof(readBuffer));
    LONGS_EQUAL(0x56, readBuffer[0]);
    LONGS_EQUAL(0x78, readBuffer[1]);
    STRCMP_EQUAL("1212", spi.getOutboundAsString());
}

TEST(SPIDma, SetSpecificFrequency_VerifyThatItIsRecorded)
{
    SPIDma spi(1, 2, 3);

    spi.frequency(400000);

    LONGS_EQUAL(1, spi.getSettingsCount());
    SPIDma::Settings settings = spi.getSetting(0);
    LONGS_EQUAL(SPIDma::Frequency, settings.type);
    LONGS_EQUAL(400000, settings.frequency);
    LONGS_EQUAL(0, settings.bytesSentBefore);
}

TEST(SPIDma, SetDefaultFrequency_VerifyThatItIsRecordedAs1MHz)
{
    SPIDma spi(1, 2, 3);

    spi.frequency();

    LONGS_EQUAL(1, spi.getSettingsCount());
    SPIDma::Settings settings = spi.getSetting(0);
    LONGS_EQUAL(SPIDma::Frequency, settings.type);
    LONGS_EQUAL(1000000, settings.frequency);
    LONGS_EQUAL(0, settings.bytesSentBefore);
}

TEST(SPIDma, SetFrequencyAfterWritingOneByte_VerifyThatItIsRecordedWithCorrectOffset)
{
    SPIDma spi(1, 2, 3);

    spi.send(0xFF);
    spi.frequency(100000);

    LONGS_EQUAL(1, spi.getSettingsCount());
    SPIDma::Settings settings = spi.getSetting(0);
    LONGS_EQUAL(SPIDma::Frequency, settings.type);
    LONGS_EQUAL(100000, settings.frequency);
    LONGS_EQUAL(1, settings.bytesSentBefore);
}

TEST(SPIDma, SetFrequencyTwice_VerifyBothAreRecorded)
{
    SPIDma spi(1, 2, 3);

    spi.frequency(400000);
    spi.send(0xFF);
    spi.frequency(100000);

    LONGS_EQUAL(2, spi.getSettingsCount());

    SPIDma::Settings settings = spi.getSetting(0);
    LONGS_EQUAL(SPIDma::Frequency, settings.type);
    LONGS_EQUAL(400000, settings.frequency);
    LONGS_EQUAL(0, settings.bytesSentBefore);

    settings = spi.getSetting(1);
    LONGS_EQUAL(SPIDma::Frequency, settings.type);
    LONGS_EQUAL(100000, settings.frequency);
    LONGS_EQUAL(1, settings.bytesSentBefore);
}

TEST(SPIDma, SetFormat_VerifyItGetsRecorded)
{
    SPIDma spi(1, 2, 3);

    spi.format(8, 3);

    LONGS_EQUAL(1, spi.getSettingsCount());
    SPIDma::Settings settings = spi.getSetting(0);
    LONGS_EQUAL(SPIDma::Format, settings.type);
    LONGS_EQUAL(8, settings.bits);
    LONGS_EQUAL(3, settings.mode);
    LONGS_EQUAL(0, settings.bytesSentBefore);
}

TEST(SPIDma, SetFormatToDefaultMode_VerifyModeIsZero)
{
    SPIDma spi(1, 2, 3);

    spi.format(8);

    LONGS_EQUAL(1, spi.getSettingsCount());
    SPIDma::Settings settings = spi.getSetting(0);
    LONGS_EQUAL(SPIDma::Format, settings.type);
    LONGS_EQUAL(8, settings.bits);
    LONGS_EQUAL(0, settings.mode);
    LONGS_EQUAL(0, settings.bytesSentBefore);
}

TEST(SPIDma, SetFormatToMaximumBitsOf16_VerifyItGetsRecorded)
{
    SPIDma spi(1, 2, 3);

    spi.format(16, 2);

    LONGS_EQUAL(1, spi.getSettingsCount());
    SPIDma::Settings settings = spi.getSetting(0);
    LONGS_EQUAL(SPIDma::Format, settings.type);
    LONGS_EQUAL(16, settings.bits);
    LONGS_EQUAL(2, settings.mode);
    LONGS_EQUAL(0, settings.bytesSentBefore);
}

TEST(SPIDma, SetFormatTwice_VerifyTheyBothGetRecorded)
{
    SPIDma spi(1, 2, 3);

    spi.format(16, 2);
    spi.send(0xFF);
    spi.format(8);

    LONGS_EQUAL(2, spi.getSettingsCount());
    SPIDma::Settings settings = spi.getSetting(0);
    LONGS_EQUAL(SPIDma::Format, settings.type);
    LONGS_EQUAL(16, settings.bits);
    LONGS_EQUAL(2, settings.mode);
    LONGS_EQUAL(0, settings.bytesSentBefore);

    settings = spi.getSetting(1);
    LONGS_EQUAL(SPIDma::Format, settings.type);
    LONGS_EQUAL(8, settings.bits);
    LONGS_EQUAL(0, settings.mode);
    LONGS_EQUAL(1, settings.bytesSentBefore);
}

TEST(SPIDma, SetFormatThenFrequencyAndIncreaseFrequencyAfterSendingBytes_VerifyAllAreRecorded)
{
    SPIDma spi(1, 2, 3);

    spi.format(8, 0);
    spi.frequency(400000);
    spi.send(0xFF);
    spi.frequency(25000000);

    LONGS_EQUAL(3, spi.getSettingsCount());
    SPIDma::Settings settings = spi.getSetting(0);
    LONGS_EQUAL(SPIDma::Format, settings.type);
    LONGS_EQUAL(8, settings.bits);
    LONGS_EQUAL(0, settings.mode);
    LONGS_EQUAL(0, settings.bytesSentBefore);

    settings = spi.getSetting(1);
    LONGS_EQUAL(SPIDma::Frequency, settings.type);
    LONGS_EQUAL(400000, settings.frequency);
    LONGS_EQUAL(0, settings.bytesSentBefore);

    settings = spi.getSetting(2);
    LONGS_EQUAL(SPIDma::Frequency, settings.type);
    LONGS_EQUAL(25000000, settings.frequency);
    LONGS_EQUAL(1, settings.bytesSentBefore);
}

TEST(SPIDma, SetSelectHighInConstructor_VerifyItGetsRecorded)
{
    SPIDma spi(1, 2, 3, 4, HIGH);

    LONGS_EQUAL(1, spi.getSettingsCount());
    SPIDma::Settings settings = spi.getSetting(0);
    LONGS_EQUAL(SPIDma::ChipSelect, settings.type);
    LONGS_EQUAL(HIGH, settings.chipSelect);
    LONGS_EQUAL(0, settings.bytesSentBefore);
}

TEST(SPIDma, SetSelectLowInConstructor_VerifyItGetsRecorded)
{
    SPIDma spi(1, 2, 3, 4, LOW);

    LONGS_EQUAL(1, spi.getSettingsCount());
    SPIDma::Settings settings = spi.getSetting(0);
    LONGS_EQUAL(SPIDma::ChipSelect, settings.type);
    LONGS_EQUAL(LOW, settings.chipSelect);
    LONGS_EQUAL(0, settings.bytesSentBefore);
}

TEST(SPIDma, SetSelectLowAfterWritingAByte_VerifyItGetsRecorded)
{
    SPIDma spi(1, 2, 3, 4, HIGH);

    spi.send(0xFF);
    spi.setChipSelect(LOW);

    LONGS_EQUAL(2, spi.getSettingsCount());

    SPIDma::Settings settings = spi.getSetting(0);
    LONGS_EQUAL(SPIDma::ChipSelect, settings.type);
    LONGS_EQUAL(HIGH, settings.chipSelect);
    LONGS_EQUAL(0, settings.bytesSentBefore);

    settings = spi.getSetting(1);
    LONGS_EQUAL(SPIDma::ChipSelect, settings.type);
    LONGS_EQUAL(LOW, settings.chipSelect);
    LONGS_EQUAL(1, settings.bytesSentBefore);
}

TEST(SPIDma, IsInboundBufferEmpty)
{
    SPIDma spi(1, 2, 3, 4, HIGH);

    CHECK_TRUE(spi.isInboundBufferEmpty());
        // Place one item in buffer and should now be non-empty.
        spi.setInboundFromString("FF");
    CHECK_FALSE(spi.isInboundBufferEmpty());
        // Read the one item out of buffer and now should be empty again.
        spi.exchange(0xFF);
    CHECK_TRUE(spi.isInboundBufferEmpty());
}
