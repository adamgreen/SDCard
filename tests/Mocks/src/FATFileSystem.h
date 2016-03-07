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
// Place holder for mbed FATFileSystem class to be used when running unit tests on PC host.
#ifndef FAT_FILE_SYSTEM_H
#define FAT_FILE_SYSTEM_H

#include <stdint.h>
#include <Timer.h>

class FATFileSystem
{
public:
    FATFileSystem(const char* name)
    {
    }

    virtual int disk_initialize() = 0;
    virtual int disk_status() = 0;
    virtual int disk_read(uint8_t* buffer, uint32_t block_number, uint32_t count) = 0;
    virtual int disk_write(const uint8_t* buffer, uint32_t block_number, uint32_t count) = 0;
    virtual int disk_sync() = 0;
    virtual uint32_t disk_sectors() = 0;

protected:
};

#endif // FAT_FILE_SYSTEM_H
