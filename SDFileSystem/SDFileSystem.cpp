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
/*
    SD Card based file system using DMA based SPI.
    No MMC support as most modern embedded projects use a uSD slot which won't fit MMC media anyway.

    This code is derived from multiple sources:
    SD Specifications Part 1 Physical Layer Simplified Specification Version 4.10
      (https://www.sdcard.org/downloads/pls/pdf/part1_410.pdf)
    Neil Thiessen's update SDFileSystem driver for mbed.
      (https://developer.mbed.org/users/neilt6/code/SDFileSystem/file/d10a519c0910/SDFileSystem.cpp)
    The lpc176x sample from ChaN's, the creator of the FatFS driver, ffsample.zip archive of samples.
      (http://elm-chan.org/fsw/ff/ffsample.zip)
*/
#include <assert.h>
#include <diskio.h>
#include "SDFileSystem.h"
#include "SDCRC.h"


// Possible states for SD Chip Select signal.
#define HIGH 1
#define LOW  0

// SD Commands used by this code.
#define CMD0    0   // GO_IDLE_STATE - Resets the SD Memory Card.
#define CMD8    8   // SEND_IF_COND - Sends SD Memory Card interface condition that includes host supply voltage
                    //                information and asks the accessed card whether card can operate in supplied
                    //                voltage range.
#define CMD9    9   // SEND_CSD - Asks the selected card to send its card-specific data (CSD).
#define CMD10   10  // SEND_CID - Asks the selected card to send its card identification (CID).
#define CMD12   12  // STOP_TRANSMISSION - Forces the card to stop transmission in Multiple Block Read Operation.
#define CMD13   13  // SEND_STATUS - Asks the selected card to send its status register.
#define CMD16   16  // SET_BLOCKLEN - In case of SDSC Card, block length is set by this command. In case of SDHC and
                    //                SDXC Cards, block length of the memory access commands is fixed to 512 bytes.
#define CMD17   17  // READ_SINGLE_BLOCK - Reads one block.  Argument is block number (or byte address for SDSC).
#define CMD18   18  // READ_MULTIPLE_BLOCK - Continuously transfers data blocks from card to host until interrupted by
                    //                       a STOP_TRANSMISSION command.  Argument is first block number (or byte
                    //                       address for SDSC).
#define CMD24   24  // WRITE_BLOCK - Writes a block of the size selected by the SET_BLOCKLEN command.  Argument is
                    //               block number (or byte address for SDSC).
#define CMD25   25  // WRITE_MULTIPLE_BLOCK - Continuously writes blocks of data until 'Stop Tran' token is sent
                    //                        (instead 'Start Block'). Argument is block number (or byte address for
                    //                        SDSC).
#define CMD55   55  // APP_CMD - Defines to the card that the next command is an application specific command rather
                    //           than a standard command.
#define CMD58   58  // READ_OCR - Reads the OCR register of a card. CCS bit is assigned to OCR[30].
#define CMD59   59  // CRC_ON_OFF - Turns the CRC option on/off. A 1 in the lsb of argument will enable it.

// ACMD are application SD commands that are preceeded by a CMD55.
#define ACMD_BIT    (1 << 7)        // If high bit is set in command code then it is actually an ACMD.
#define ACMD22      (ACMD_BIT | 22) // SEND_NUM_WR_BLOCKS - Send the numbers of the well written (without errors)
                                    //                      blocks. Responds with 32-bit+CRC data block.
#define ACMD23      (ACMD_BIT | 23) // SET_WR_BLK_ERASE_COUNT - Set the number of write blocks to be pre-erased before
                                    //                          writing (to be used for faster Multiple Block WR
                                    //                          command). "1"=default (one wr block)
#define ACMD41      (ACMD_BIT | 41) // SD_SEND_OP_COND - Sends host capacity support information and activates the
                                    //                   card's initialization process.


// This bit is clear in the first byte of a command packet as start bit.
#define CMD_START_BIT           (1 << 7)
// This bit is set in the first byte of command packet to indicate that it is coming from host.
#define CMD_TRANSMISSION_BIT    (1 << 6)
// This bit is set in the last byte of the command packet as stop bit.
#define CMD_STOP_BIT            (1 << 0)

// Bits for CMD8 - SEND_IF_COND
#define CMD8_CHECK_OFFSET       0
#define CMD8_CHECK_PATTERN      (0xAD << CMD8_CHECK_OFFSET)
#define CMD8_VHS_OFFSET         8
#define CMD8_VHS_2_7__3_6V      (1 << CMD8_VHS_OFFSET)  // 2.7 - 3.6V

// Bits for CMD59 - CRC_ON_OFF
#define CMD59_CRC_OPTION_BIT    (1 << 0)

// Bits for ACMD41 - SD_SEND_OP_COND
#define ACMD41_HCS_BIT          (1 << 30)

// Command response bits.
#define R1_IDLE             (1 << 0)
#define R1_ERRORS_MASK      (0x3F << 1)
#define R1_ILLEGAL_COMMAND  (1 << 2)
#define R1_CRC_ERROR        (1 << 3)
#define R1_START_BIT        (1 << 7)

#define R7_VHS_CHECK_MASK   0xFFFF

// Block token codes.
#define BLOCK_START             0xFE
#define MULTIPLE_BLOCK_START    0xFC
#define MULTIPLE_BLOCK_STOP     0xFD

// Data Response Token bits.
#define DATA_RESPONSE_MASK          0x1F
#define DATA_RESPONSE_DATA_ACCEPTED ((2 << 1) | 1)
#define DATA_RESPONSE_CRC_ERROR     ((5 << 1) | 1)
#define DATA_RESPONSE_WRITE_ERROR   ((6 << 1) | 1)
#define DATA_RESPONSE_UNKNOWN_ERROR 0x1E

// OCR (Operation Condtions Register) bit fields.
#define OCR_3_2__3_3V   (1 << 20)
#define OCR_CCS         (1 << 30)


SDFileSystem::SDFileSystem(PinName mosi, PinName miso, PinName sclk, PinName cs, const char* name)
    : FATFileSystem(name), m_spi(mosi, miso, sclk, cs, HIGH)
{
    static const int polarity0phase0 = 0;

    m_status = STA_NOINIT;
    m_blockToAddressShift = 0;
    m_timer.start();
    m_timerOuter.start();

    // Initialize Diagnostic Fields.
    m_selectFirstExchangeRequiredCount = 0;
    m_maximumWaitWhileBusyTime = 0;
    m_maximumWaitForR1ResponseLoopCount = 0;
    m_maximumCRCRetryCount = 0;
    m_maximumACMD41LoopTime = 0;
    m_maximumReceiveDataBlockWaitTime = 0;
    m_maximumReadRetryCount = 0;
    m_cmd12PaddingByteRequiredCount = 0;
    m_maximumWriteRetryCount = 0;

    m_spi.format(8, polarity0phase0);
}

int SDFileSystem::disk_initialize()
{
    // Follow the flow-chart from section "7.2.1 Mode Selection and Initialization"
    // of the "SD Specifications Part 1 Physical Layer Simplified Specification Version 4.10"
    bool isSDv2 = false;

    // 4.2.1 Card Reset - Initializes to accept 400kHz clock rate in idle state.
    m_spi.frequency(400000);

    // 6.4.1.1 Power Up Time of Card - Send 8 * 10 >= 74 clocks during power-up.
    m_spi.setChipSelect(HIGH);
    for (int i = 0 ; i < 8 ; i++)
    {
        m_spi.send(0xFF);
    }

    // 7.2.1 Mode Selection and Initialization - Issuing CMD0 will reset all types of SD cards into idle state.
    // All SPI commands are sent with chip select pulled low. When this is done for the first command (CMD0) then the
    // card will switch into SPI mode.
    uint8_t r1Response = cmd(CMD0);
    if (r1Response != R1_IDLE)
    {
        m_log.log("disk_initialize() - CMD0 returned 0x%02X. Is card inserted?\n", r1Response);
        return m_status;
    }

    // 7.2.2 Bus Transfer Protection - CRC is disabled by default in SPI mode.  Send CMD59 to enable it.
    r1Response = cmd(CMD59, CMD59_CRC_OPTION_BIT);
    if (r1Response != R1_IDLE)
    {
        m_log.log("disk_initialize() - CMD59 returned 0x%02X\n", r1Response);
        return m_status;
    }

    // 4.3.13 Send Interface Condition Command (CMD8) - Host signals that it can supply 2.7 - 3.6V to the card. SDv2
    // cards will echo back command argument if it supports that voltage range. A SDv1 card will return illegal command
    // R1 error response.
    uint32_t r7Response = 0xFFFFFFFF;
    r1Response = cmd(CMD8, CMD8_VHS_2_7__3_6V | CMD8_CHECK_PATTERN, &r7Response);
    if (r1Response == R1_IDLE)
    {
        // This is a SDv2 card.
        isSDv2 = true;

        // 4.3.13 Send Interface Condition Command (CMD8)
        // 7.3.2.6 Format R7 - Should echo back the VHS and check pattern.
        if ((r7Response & R7_VHS_CHECK_MASK) != (CMD8_VHS_2_7__3_6V | CMD8_CHECK_PATTERN))
        {
            // SDv2 card doesn't support the indicated voltage range so error out.
            m_log.log("disk_initialize() - CMD8 returned 0x%08X (expected 0x%08X)\n",
                      r7Response, CMD8_VHS_2_7__3_6V | CMD8_CHECK_PATTERN);
            return m_status;
        }
    }
    else if (r1Response & R1_ILLEGAL_COMMAND)
    {
        // This is a SDv1 card.
        isSDv2 = false;
    }
    else
    {
        m_log.log("disk_initialize() - CMD8 returned 0x%02X\n", r1Response);
        return m_status;
    }

    // 5.1 OCR register - Shows what the bits in the OCR returned by CMD58 mean.
    uint32_t ocr = 0xFFFFFFFF;
    r1Response = cmd(CMD58, 0, &ocr);
    if (r1Response != R1_IDLE)
    {
        m_log.log("disk_initialize() - CMD58 returned 0x%02X during voltage check\n", r1Response);
        return m_status;
    }
    // Make sure that the card supports 3.3V.
    if (0 == (ocr & OCR_3_2__3_3V))
    {
        m_log.log("disk_initialize() - CMD58 3.3V not supported. OCR=0x%08X\n", ocr);
        return m_status;
    }

    // Issue ACMD41 to start the initialization process.  Keep issuing this command until the card leaves the
    // idle state.  For SDv2, set the HCS bit to indicate that this host supports high capacity cards.
    // Keep attempting for up to a maximum of 1 second.
    uint32_t elapsedTime = 0;
    m_timerOuter.reset();
    do
    {
        r1Response = cmd(ACMD41, isSDv2 ? ACMD41_HCS_BIT : 0);
        elapsedTime = m_timerOuter.read_ms();
    } while (r1Response == R1_IDLE && elapsedTime < 1000);
    // Record longest time it has taken to leave idle state.
    if (elapsedTime > m_maximumACMD41LoopTime)
    {
        m_maximumACMD41LoopTime = elapsedTime;
    }
    // Check for errors.
    if (r1Response == R1_IDLE)
    {
        m_log.log("disk_initialize() - ACMD41 timed out attempting to leave idle state\n");
        return m_status;
    }
    else if (r1Response & R1_ERRORS_MASK)
    {
        m_log.log("disk_initialize() - ACMD41 returned 0x%02X\n", r1Response);
        return m_status;
    }

    // Detect the capacity of the card (high capacity or not).
    if (isSDv2)
    {
        // SDv2 cards can be either standard or high capacity.
        // Call CMD58 again to get the OCR with its CCS (Card Capacity Status) bit which indicates if the card is
        // high capacity or not.
        r1Response = cmd(CMD58, 0, &ocr);
        if (r1Response & R1_ERRORS_MASK)
        {
            m_log.log("disk_initialize() - CMD58 returned 0x%02X during capacity check\n", r1Response);
            return m_status;
        }

        if (ocr & OCR_CCS)
        {
            // SDHC/SDXC cards use block addresses for read/write commands so use a conversion factor of 1 (1 << 0).
            m_blockToAddressShift = 0;
        }
        else
        {
            // SDSC cards use byte addresses for read/write commands so use a conversion factor of 512 (1 << 9).
            m_blockToAddressShift = 9;
        }
    }
    else
    {
        // This is a SDv1 card which only supports SDSC.
        // SDSC cards use byte addresses for read/write commands so use a conversion factor of 512 (1 << 9).
        m_blockToAddressShift = 9;
    }

    //7.2.3 Data Read - Documents how SDSC needs a CMD16 to be sent to setup for 512byte / block operations.
    if (m_blockToAddressShift == 9)
    {
        r1Response = cmd(CMD16, 512);
        if (r1Response & R1_ERRORS_MASK)
        {
            m_log.log("disk_initialize() - CMD16 returned 0x%02X\n", r1Response);
            return m_status;
        }
    }

    // 2. System Features - Default speed mode (slowest) is 25MHz.
    m_spi.frequency(25000000);

    // Mark that the driver is now initialized.
    m_status &= ~STA_NOINIT;

    return m_status;
}

int SDFileSystem::disk_status()
{
    return m_status;
}

int SDFileSystem::disk_read(uint8_t* pBuffer, uint32_t blockNumber, uint32_t count)
{
    // Save for the purpose of error logging original parameter values.
    uint8_t* pOrigBuffer = pBuffer;
    uint32_t origBlockNumber = blockNumber;
    uint32_t origCount = count;

    if (m_status & STA_NOINIT)
    {
        m_log.log("disk_read(%X,%d,%d) - Attempt to read uninitialized drive\n", pOrigBuffer, origBlockNumber, origCount);
        return RES_NOTRDY;
    }
    if (!count)
    {
        m_log.log("disk_read(%X,%d,%d) - Attempt to read 0 blocks\n", pOrigBuffer, origBlockNumber, origCount);
        return RES_PARERR;
    }

    // 7.2.3 Data Read - Gives an overview of the single/multi block read process for SPI mode.
    if (count == 1)
    {
        // 7.3.1.3 Detailed Command Description - Refer to note 10 for read/write commands.
        // SDSC will require converting block number to byte address and high capacity disks use block number as address.
        uint32_t blockAddress = blockNumber << m_blockToAddressShift;

        // Issue CMD17 to start block read and then process transmitted data block.
        int response = sendCommandAndReceiveDataBlock(CMD17, blockAddress, pBuffer, 512);
        if (response != RES_OK)
        {
            m_log.log("disk_read(%X,%d,%d) - Read failed\n", pOrigBuffer, origBlockNumber, origCount);
        }
        return response;
    }

    for (uint32_t retry = 1 ; retry <= 3 ; retry++)
    {
        // 7.3.1.3 Detailed Command Description - Refer to note 10 for read/write commands.
        // SDSC will require converting block number to byte address and high capacity disks use block number as address.
        uint32_t blockAddress = blockNumber << m_blockToAddressShift;

        if (!select())
        {
            // Log error error and return immediately.  No need to deselect() again when select() failed.
            m_log.log("disk_read(%X,%d,%d) - Select timed out\n", pOrigBuffer, origBlockNumber, origCount);
            return RES_ERROR;
        }

        // CMD18 is used to start the multi-block read.
        uint8_t r1Response = sendCommandAndGetResponse(CMD18, blockAddress);
        if (r1Response != 0)
        {
            m_log.log("disk_read(%X,%d,%d) - CMD18 returned 0x%02X\n",
                      pOrigBuffer, origBlockNumber, origCount, r1Response);
            deselect();
            return RES_ERROR;
        }

        while (count)
        {
            if (!receiveDataBlock(pBuffer, 512))
            {
                m_log.log("disk_read(%X,%d,%d) - receiveDataBlock failed. block=%d\n",
                          pOrigBuffer, origBlockNumber, origCount, blockNumber);
                // Record maximum number of read retries.
                if (retry > m_maximumReadRetryCount)
                {
                    m_maximumReadRetryCount = retry;
                }
                // Break out of this inner loop and allow outer loop to retry.
                break;
            }

            // Reset retry counter when any read goes through successfully since we only want to fail
            // when the retry counter is exceeded for a single block.
            retry = 1;
            // Advance to next block.
            pBuffer += 512;
            blockNumber++;
            count--;
        }

        // CMD12 is sent to stop the multi-block read and then deselect() at end of multi-block read, error or not.
        r1Response = sendCommandAndGetResponse(CMD12);
        deselect();
        if (r1Response != 0)
        {
            m_log.log("disk_read(%X,%d,%d) - CMD12 returned 0x%02X\n",
                      pOrigBuffer, origBlockNumber, origCount, r1Response);
            return RES_ERROR;
        }

        if (count == 0)
        {
            // Have successfully completed the read.
            return RES_OK;
        }
    }

    // Get here if we have run out of retries so return an error back to the caller.
    return RES_ERROR;
}

int SDFileSystem::disk_write(const uint8_t* pBuffer, uint32_t blockNumber, uint32_t count)
{
    // Save for the purpose of error logging original parameter values.
    uint32_t origCount = count;
    uint32_t origBlockNumber = blockNumber;
    const uint8_t* pOrigBuffer = pBuffer;

    if (m_status & STA_NOINIT)
    {
        m_log.log("disk_write(%X,%d,%d) - Attempt to write uninitialized drive\n", pOrigBuffer, origBlockNumber, origCount);
        return RES_NOTRDY;
    }
    if (!count)
    {
        m_log.log("disk_write(%X,%d,%d) - Attempt to write 0 blocks\n", pOrigBuffer, origBlockNumber, origCount);
        return RES_PARERR;
    }

    // 7.2.4 Data Write - Gives an overview of the single/multi block write process for SPI mode.
    for (uint32_t retry = 1 ; retry <= 3 ; retry++)
    {
        // 7.3.1.3 Detailed Command Description - Refer to note 10 for read/write commands.
        // SDSC will require converting block number to byte address and high capacity disks use block number as address.
        uint32_t blockAddress = blockNumber << m_blockToAddressShift;
        uint8_t  r1Response = 0xFF;
        if (origCount == 1)
        {
            if (!select())
            {
                m_log.log("disk_write(%X,%d,%d) - Select timed out\n", pOrigBuffer, origBlockNumber, origCount);
                return RES_ERROR;
            }

            // CMD24 is used to start single block write.
            uint8_t r1Response = sendCommandAndGetResponse(CMD24, blockAddress);
            if (r1Response != 0)
            {
                m_log.log("disk_write(%X,%d,%d) - CMD24 returned 0x%02X\n", pOrigBuffer, origBlockNumber, origCount, r1Response);
                deselect();
                return RES_ERROR;
            }

            uint8_t dataResponse = transmitDataBlock(BLOCK_START, pBuffer, 512);
            if (dataResponse != DATA_RESPONSE_DATA_ACCEPTED)
            {
                m_log.log("disk_write(%X,%d,%d) - transmitDataBlock failed\n", pOrigBuffer, origBlockNumber, origCount);
                // Record if this was the maximum number of write attempts we have made for a single block.
                if (retry > m_maximumWriteRetryCount)
                {
                    m_maximumWriteRetryCount = retry;
                }
                // Encountered error while transmitting data block so try again.
                deselect();
                continue;
            }
        }
        else
        {
            // 4.3.4 Data Write - ACMD23 can be used before CMD25 to indicate how many blocks should be pre-erased to
            //                    improve multi-block write performance.
            cmd(ACMD23, count & 0x07FFFF);

            if (!select())
            {
                m_log.log("disk_write(%X,%d,%d) - Select timed out\n", pOrigBuffer, origBlockNumber, origCount);
                return RES_ERROR;
            }

            // CMD25 is used to start multi block write.
            r1Response = sendCommandAndGetResponse(CMD25, blockAddress);
            if (r1Response != 0)
            {
                m_log.log("disk_write(%X,%d,%d) - CMD25 returned 0x%02X\n", pOrigBuffer, origBlockNumber, origCount, r1Response);
                deselect();
                return RES_ERROR;
            }

            // Loop through and send each block to the card.
            const uint8_t* pStartBuffer = pBuffer;
            uint32_t startBlockNumber = blockNumber;
            uint32_t startCount = count;
            while (count)
            {
                uint8_t dataResponse = transmitDataBlock(MULTIPLE_BLOCK_START, pBuffer, 512);
                if (dataResponse != DATA_RESPONSE_DATA_ACCEPTED)
                {
                    m_log.log("disk_write(%X,%d,%d) - transmitDataBlock failed. block=%d\n",
                               pOrigBuffer, origBlockNumber, origCount, blockNumber);

                    // Record if this was the maximum number of write attempts we have made for a single block.
                    if (retry > m_maximumWriteRetryCount)
                    {
                        m_maximumWriteRetryCount = retry;
                    }

                    // 7.3.3.1 Data Response Token - Send CMD12 to stop write when an error data response token is
                    //                               returned.
                    deselect();
                    cmd(12);

                    // 7.3.3.1 Data Response Token - Send ACMD22 on write error to determine number of
                    //                               successful writes.
                    if (dataResponse == DATA_RESPONSE_WRITE_ERROR)
                    {
                        // Determine number of blocks that were successfully written.
                        uint8_t data[4];
                        int result = sendCommandAndReceiveDataBlock(ACMD22, 0, data, sizeof(data));
                        if (result != RES_OK)
                        {
                            m_log.log("disk_write(%X,%d,%d) - Failed to retrieve written block count.\n",
                                       pOrigBuffer, origBlockNumber, origCount);
                            return result;
                        }

                        // Copy big-endian 32-bit value into machine appropriate 32-bit format.
                        uint32_t blocksWritten = ((uint32_t)data[0] << 24) |
                                                 ((uint32_t)data[1] << 16) |
                                                 ((uint32_t)data[2] << 8) |
                                                  (uint32_t)data[3];

                        // If the returned count is too large then default to no blocks being written successfully.
                        if (blocksWritten > startCount)
                        {
                            blocksWritten = 0;
                        }

                        // Rewind to first block that needs to be retried.
                        pBuffer = pStartBuffer + 512 * blocksWritten;
                        blockNumber = startBlockNumber + blocksWritten;
                        count = startCount - blocksWritten;
                    }

                    // Break out of this inner loop so that we can retry from the outer loop.
                    break;
                }

                // Reset retry counter when any write goes through successfully since we only want to fail
                // when the retry counter is exceeded for a single block.
                retry = 1;

                // Advance to next block.
                pBuffer += 512;
                blockNumber++;
                count--;
            }

            if (count == 0)
            {
                // Send stop transmission token.
                transmitDataBlock(MULTIPLE_BLOCK_STOP, NULL, 0);
            }
            else
            {
                // Still have blocks that need to be sent so retry.
                continue;
            }
        }

        // 7.2.4 Data Write - Validate write by issuing CMD13 to get current card status.
        uint32_t cardStatus = 0;
        deselect();
        r1Response = cmd(CMD13, 0, &cardStatus);
        if (r1Response != 0)
        {
            m_log.log("disk_write(%X,%d,%d) - CMD13 failed. r1Response=0x%02X\n",
                      pOrigBuffer, origBlockNumber, origCount, r1Response);
            return RES_ERROR;
        }
        if (cardStatus != 0)
        {
            m_log.log("disk_write(%X,%d,%d) - CMD13 failed. Status=0x%02X\n",
                      pOrigBuffer, origBlockNumber, origCount, cardStatus);
            return RES_ERROR;
        }

        // Write was successful.
        return RES_OK;
    }

    return RES_ERROR;
}

int SDFileSystem::disk_sync()
{
    // Calling select() will assert chip select low and wait for any outstanding writes to leave busy state before
    // returning or timing out.
    if (!select())
    {
        m_log.log("disk_sync() - Failed waiting for not busy\n");
        return RES_ERROR;
    }
    deselect();
    return RES_OK;
}

uint32_t SDFileSystem::disk_sectors()
{
    if (m_status & STA_NOINIT)
    {
        m_log.log("disk_sectors() - Attempt to query uninitialized drive\n");
        return 0;
    }

    // 5.3.1 CSD_STRUCTURE - How to parse the CSD register structure to obtain block count.
    uint8_t csd[16];
    int response = getCSD(csd, sizeof(csd));
    if (response != RES_OK)
    {
        m_log.log("disk_sectors() - Failed to read CSD\n");
        return 0;
    }

    // Calculate the sector count from the bits in the CSD register.
    uint32_t CSD_STRUCTURE = extractBits(csd, sizeof(csd), 126, 127);
    if (CSD_STRUCTURE == 0)
    {
        // 5.3.2 CSD Register (CSD Version 1.0)
        uint32_t READ_BL_LEN = extractBits(csd, sizeof(csd), 80, 83);
        uint32_t C_SIZE = extractBits(csd, sizeof(csd), 62, 73);
        uint32_t C_SIZE_MULT = extractBits(csd, sizeof(csd), 47, 49);
        return (C_SIZE + 1)  << ((C_SIZE_MULT + 2 + READ_BL_LEN) - 9); // -9 at end accounts for 2^9 = 512 bytes/block.
    }
    else
    {
        // 5.3.3 CSD Register (CSD Version 2.0)
        uint32_t C_SIZE = extractBits(csd, sizeof(csd), 48, 69);
        return (C_SIZE + 1) << 10;
    }
}

int SDFileSystem::getCID(uint8_t* pCID, size_t cidSize)
{
    // CID register is 16 bytes in length.
    assert ( cidSize == 16 );

    // CMD10 is used to fetch the CID register.
    int response = sendCommandAndReceiveDataBlock(CMD10, 0, pCID, 16);
    if (response != RES_OK)
    {
        m_log.log("getCID(%X,%d) - Register read failed\n", pCID, cidSize);
    }
    return response;
}

int SDFileSystem::getCSD(uint8_t* pCSD, size_t csdSize)
{
    // CSD register is 16 bytes in length.
    assert ( csdSize == 16 );

    // CMD9 is used to fetch the CSD register.
    int response = sendCommandAndReceiveDataBlock(CMD9, 0, pCSD, 16);
    if (response != RES_OK)
    {
        m_log.log("getCSD(%X,%d) - Register read failed\n", pCSD, csdSize);
    }
    return response;
}

int SDFileSystem::getOCR(uint32_t* pOCR)
{
    uint8_t r1Response = cmd(CMD58, 0, pOCR);
    if (r1Response & R1_ERRORS_MASK)
    {
        m_log.log("getOCR(%X) - Register read failed. Response=0x%02X\n", pOCR, r1Response);
        return RES_ERROR;
    }
    return RES_OK;
}

uint32_t SDFileSystem::extractBits(const uint8_t* p, size_t size, uint32_t lowBit, uint32_t highBit)
{
    uint32_t bitCount = highBit - lowBit + 1;
    int      lowByte = (size-1) - (lowBit >> 3);
    int      highByte = (size-1) - (highBit >> 3);
    uint32_t val = 0;

    assert ( bitCount <= 32 );
    assert ( lowByte >= 0 );
    assert ( highByte >= 0 );

    uint32_t bitsLeft = bitCount;
    uint32_t bitSrcOffset = lowBit & 7;
    uint32_t bitDestOffset = 0;
    for (int i = lowByte ; i >= highByte ; i--)
    {
        uint32_t bitsFromByte = 8 - bitSrcOffset;
        if (bitsFromByte > bitsLeft)
        {
            bitsFromByte = bitsLeft;
        }
        uint32_t byteMask = (1 << bitsLeft) - 1;

        val |= ((p[i] >> bitSrcOffset) & byteMask) << bitDestOffset;

        bitSrcOffset = 0;
        bitDestOffset += bitsFromByte;
        bitsLeft -= bitsFromByte;
    }
    assert ( bitsLeft == 0 );

    return val;
}




uint8_t SDFileSystem::cmd(uint8_t cmd, uint32_t argument /* = 0 */, uint32_t* pResponse /* = NULL */)
{
    // 7.2 SPI Bus Protocol - Need to assert chip select low before writing the command out over SPI.
    if (!select())
    {
        m_log.log("cmd(%s,%X,%X) - Select timed out\n", cmdToString(cmd), argument, pResponse);
        return 0xFF;
    }

    uint8_t response = sendCommandAndGetResponse(cmd, argument, pResponse);

    // Need to de-assert the chip select to high now that the command is completed.
    deselect();

    return response;
}

const char* SDFileSystem::cmdToString(uint8_t cmd)
{
    static char cmdString[7];

    if (cmd & ACMD_BIT)
    {
        snprintf(cmdString, sizeof(cmdString), "ACMD%d", cmd & ~ACMD_BIT);
    }
    else
    {
        snprintf(cmdString, sizeof(cmdString), "CMD%d", cmd);
    }

    return cmdString;
}

bool SDFileSystem::select()
{
    // 7.2 SPI Bus Protocol - Prepare to start sending next command to SD card.
    // Assert chip select low before starting to send any command.
    m_spi.setChipSelect(LOW);

    // Send 0xFF to prime card for next command.
    // I want to know if this exchange is necessary or if it could have just gone straight to waitWhileBusy().
    // The only reason it would be needed is if it would read 0xFF (which doesn't require wait) but the next read
    // returns !0xFF (which does require wait).
    uint8_t response = m_spi.exchange(0xFF);
    if (response == 0xFF && m_spi.exchange(0xFF) != 0xFF)
    {
        m_selectFirstExchangeRequiredCount++;
    }

    // Wait for card to exit busy state.
    if (!waitWhileBusy(500))
    {
        // Card never left busy state after 500 msecs.
        m_log.log("select() - 500 msec time out\n");
        deselect();
        return false;
    }

    return true;
}

bool SDFileSystem::waitWhileBusy(uint32_t msecTimeout)
{
    // 7.2.4 Data Write - Card will keep MISO asserted low while it is busy. Will receive 0xFF once it is no longer
    //                    in busy state.
    uint32_t elapsedTime = 0;
    uint8_t  response;
    m_timer.reset();
    do
    {
        response = m_spi.exchange(0xFF);
        elapsedTime = m_timer.read_ms();
    } while (response != 0xFF && elapsedTime < msecTimeout);

    // Record the maximum wait time.
    if (elapsedTime >  m_maximumWaitWhileBusyTime)
    {
        m_maximumWaitWhileBusyTime = elapsedTime;
    }

    // Response won't be 0xFF if we timed out.
    if (response != 0xFF)
    {
        m_log.log("waitWhileBusy(%u) - Time out. Response=0x%02X\n", msecTimeout, response);
        return false;
    }

    return true;
}

void SDFileSystem::deselect()
{
    // 7.2 SPI Bus Protocol - De-assert chip select at end of command.
    m_spi.setChipSelect(HIGH);

    // 4.4 Clock Control - Send 8 additional bit clocks after completing a transaction.
    m_spi.send(0xFF);
}

uint8_t SDFileSystem::sendCommandAndGetResponse(uint8_t cmd, uint32_t argument /* = 0 */, uint32_t* pResponse /* = NULL */)
{
    uint8_t  r1Response = 0xFF;
    uint8_t  origCmd = cmd;
    uint32_t retry;

    // Handle relooping on CRC error.
    for (retry = 1 ; retry <= 4 ; retry++)
    {
        // If this is an ACMD then it needs to be preceded with a CMD55 command.
        if (cmd & ACMD_BIT)
        {
            r1Response = sendCommandAndGetResponse(CMD55);
            if (r1Response & R1_ERRORS_MASK)
            {
                m_log.log("sendCommandAndGetResponse(%s,%X,%X) - CMD55 prefix returned 0x%02X\n",
                          cmdToString(origCmd), argument, pResponse, r1Response);
                return r1Response;
            }

            // Cycle the chip select signal between commands.
            deselect();
            if (!select())
            {
                m_log.log("sendCommandAndGetResponse(%s,%X,%X) - CMD55 prefix select timed out\n",
                          cmdToString(origCmd), argument, pResponse);
                return 0xFF;
            }

            // Continue and send the ACMD command index.
            cmd &= ~ACMD_BIT;
        }

        // 7.3.1.1 Command Format - Build up the 48-bit command token based on function arguments.
        // NOTE: Always using CRC.
        uint8_t packet[6];
        packet[0] = CMD_TRANSMISSION_BIT | (cmd & 0x3F);
        packet[1] = argument >> 24;
        packet[2] = argument >> 16;
        packet[3] = argument >> 8;
        packet[4] = argument;
        packet[5] = (SDCRC::crc7(packet, 5) << 1) | CMD_STOP_BIT;

        // Write this 6-byte packet to the SPI bus.
        for (size_t i = 0 ; i < sizeof(packet) ; i++)
        {
            m_spi.send(packet[i]);
        }

        // Discard extra byte after CMD12.
        // Is this really required?  Would probably be required if this padding byte had start bit cleared.
        if (cmd == 12)
        {
            r1Response = m_spi.exchange(0xFF);
            if (0 == (r1Response & R1_START_BIT) && (r1Response & R1_ERRORS_MASK))
            {
                m_cmd12PaddingByteRequiredCount++;
            }
        }

        // 7.3.2.1 Format R1 - The R1 response should have the high (start) bit clear.  Loop until such a response is
        //                     encountered.
        uint32_t maxIterations = 10;
        do
        {
            r1Response = m_spi.exchange(0xFF);
        } while ((r1Response & R1_START_BIT) && --maxIterations);

        // Record the maximum number of iterations we wait to see if we even need to do this.
        uint32_t iterations = 10 - maxIterations;
        if (iterations > m_maximumWaitForR1ResponseLoopCount)
        {
            m_maximumWaitForR1ResponseLoopCount = iterations;
        }

        // Check for errors.
        if (r1Response & R1_START_BIT)
        {
            m_log.log("sendCommandAndGetResponse(%s,%X,%X) - Timed out waiting for valid R1 response. r1Response=0x%02X\n",
                      cmdToString(origCmd), argument, pResponse, r1Response);
            return 0xFF;
        }
        else if (r1Response & R1_CRC_ERROR)
        {
            // Record the maximum number of CRC iterations we have tried.
            if (retry > m_maximumCRCRetryCount)
            {
                m_maximumCRCRetryCount = retry;
            }
            // Retry
            continue;
        }
        else if (r1Response & R1_ERRORS_MASK)
        {
            // Don't log this error since higher level code might handle it and if not it will log the error code.
            return r1Response;
        }

        if (cmd == CMD8 || cmd == CMD58)
        {
            // These commands return a longer R7/R3 response.
            assert ( pResponse );
            uint32_t response = m_spi.exchange(0xFF) << 24;
            response |= m_spi.exchange(0xFF) << 16;
            response |= m_spi.exchange(0xFF) << 8;
            response |= m_spi.exchange(0xFF);
            *pResponse = response;
        }
        else if (cmd == CMD13)
        {
            // This command returns an extra byte as the R2 response.
            assert ( pResponse );
            *pResponse = m_spi.exchange(0xFF);
        }

        return r1Response;
    }

    // Get here if failed CRC multiple times.
    m_log.log("sendCommandAndGetResponse(%s,%X,%X) - Failed CRC check %d times\n",
              cmdToString(origCmd), argument, pResponse, retry - 1);
    return r1Response;
}

int SDFileSystem::sendCommandAndReceiveDataBlock(uint8_t cmd, uint32_t cmdArgument, uint8_t* pBuffer, size_t bufferSize)
{
    // 7.2.3 Data Read - Gives an overview of the single block read process for SPI mode.
    // Assume the read operation has failed until we get all the way through the process successfully.
    int retVal = RES_ERROR;

    for (uint32_t retry = 1 ; retry <= 3 ; retry++)
    {
        if (!select())
        {
            // Log error error and return immediately.  No need to deselect() again when select() failed.
            m_log.log("sendCommandAndReceiveDataBlock(%s,%X,%X,%d) - Select timed out\n",
                      cmdToString(cmd), cmdArgument, pBuffer, bufferSize);
            return RES_ERROR;
        }

        // Send the requested read command to the card to start the block transmission process.
        uint8_t r1Response = sendCommandAndGetResponse(cmd, cmdArgument);
        if (r1Response != 0)
        {
            m_log.log("sendCommandAndReceiveDataBlock(%s,%X,%X,%d) - %s returned 0x%02X\n",
                       cmdToString(cmd), cmdArgument, pBuffer, bufferSize, cmdToString(cmd), r1Response);
            break;
        }
        if (!receiveDataBlock(pBuffer, bufferSize))
        {
            m_log.log("sendCommandAndReceiveDataBlock(%s,%X,%X,%d) - receiveDataBlock failed\n",
                      cmdToString(cmd), cmdArgument, pBuffer, bufferSize);
            // Record maximum number of read retries.
            if (retry > m_maximumReadRetryCount)
            {
                m_maximumReadRetryCount = retry;
            }
            // Try again.
            deselect();
            continue;
        }

        // If we get here then the read was successful.
        retVal = RES_OK;
        break;
    }
    deselect();

    return retVal;
}

bool SDFileSystem::receiveDataBlock(uint8_t* pBuffer, size_t bufferSize)
{
    // 4.3.3 Data Read - Keeps the DAT bus lines pulled high when not transmitting data.
    // 4.6.2.1 Read - 100ms as the minimum read timeout.
    // Wait up to 500msec until something other than 0xFF is encountered.
    uint32_t elapsedTime = 0;
    uint8_t byte;
    m_timer.reset();
    do
    {
        byte = m_spi.exchange(0xFF);
        elapsedTime = m_timer.read_ms();
    } while (byte == 0xFF && elapsedTime < 500);

    // Record maximum amount of wait time.
    if (elapsedTime > m_maximumReceiveDataBlockWaitTime)
    {
        m_maximumReceiveDataBlockWaitTime = elapsedTime;
    }

    // Check for timeout waiting for non-0xFF byte read.
    if (byte == 0xFF)
    {
        m_log.log("receiveDataBlock(%X,%d) - Time out after 500ms\n", pBuffer, bufferSize);
        return false;
    }

    // 7.3.3.2 Start Block Tokens and Stop Tran Token
    // 0xFE is the start block for single/multiple reads.
    if (byte != BLOCK_START)
    {
        m_log.log("receiveDataBlock(%X,%d) - Expected 0xFE start block token. Response=0x%02X\n",
                  pBuffer, bufferSize, byte);
        return false;
    }

    // Read block bytes into provided buffer.
    uint32_t byteToWrite = 0xFF;
    m_spi.transfer(&byteToWrite, 1, pBuffer, bufferSize);

    // Read and check 16-bit CRC
    uint16_t crcExpected = m_spi.exchange(0xFF) << 8;
    crcExpected |= m_spi.exchange(0xFF);
    uint16_t crcActual = SDCRC::crc16(pBuffer, bufferSize);
    if (crcActual != crcExpected)
    {
        m_log.log("receiveDataBlock(%X,%d) - Invalid CRC. Expected=0x%04X Actual=0x%04X\n",
                  pBuffer, bufferSize, crcExpected, crcActual);
        return false;
    }

    return true;
}

uint8_t SDFileSystem::transmitDataBlock(uint8_t blockToken, const uint8_t* pBuffer, size_t bufferSize)
{
    // 7.2.4 Data Write - Overview of write process. If there was a previous data block write then we must wait for
    //                    the chip to no longer be busy.
    if (!waitWhileBusy(500))
    {
        m_log.log("transmitDataBlock(%X,%X,%d) - Time out after 500ms\n", blockToken, pBuffer, bufferSize);
        return DATA_RESPONSE_UNKNOWN_ERROR;
    }

    // 7.3.3.2 Start Block Tokens and Stop Tran Token - Token to prefix to data buffer.
    m_spi.send(blockToken);

    if (blockToken == MULTIPLE_BLOCK_STOP)
    {
        // 7.2.4 Data Write - When sending stop transmission token, just need to wait while busy.
        // There is no buffer to send.
        assert ( !pBuffer );
        return DATA_RESPONSE_DATA_ACCEPTED;
    }

    // Write block bytes from provided buffer.
    m_spi.transfer(pBuffer, bufferSize, NULL, 0);

    // Send 16-bit CRC.
    uint16_t crc = SDCRC::crc16(pBuffer, bufferSize);
    m_spi.send(crc >> 8);
    m_spi.send(crc);

    // 7.3.3.1 Data Response Token - Should return 0x05 in lower five bits if data block was received by card
    //                               successfully.
    uint8_t dataResponse = m_spi.exchange(0xFF);
    if ((dataResponse & DATA_RESPONSE_MASK) != DATA_RESPONSE_DATA_ACCEPTED)
    {
        m_log.log("transmitDataBlock(%X,%X,%d) - Data Response=0x%02X\n", blockToken, pBuffer, bufferSize, dataResponse);
    }
    return dataResponse & DATA_RESPONSE_MASK;
}
