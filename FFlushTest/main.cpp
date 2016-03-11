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
/* Exercise my FATFileSystem::fflush() code. */
#include <mbed.h>
#include <SDFileSystem.h>

static SDFileSystem g_sd(p5, p6, p7, p8, "sd");

int main()
{
    const char data1[] = "Test data1";
    const char data2[] = "Test data2";

    FILE* pFile1 = fopen("/sd/test1.txt", "w");
    FILE* pFile2 = fopen("/sd/test2.txt", "w");
    fwrite(data1, 1, sizeof(data1)-1, pFile1);
    fwrite(data2, 1, sizeof(data2)-1, pFile2);

    fflush(NULL);
    g_sd.sync();

    fclose(pFile2);
    g_sd.sync();
    fclose(pFile1);
    g_sd.sync();


    // Do the same operations again but close the files in the opposite order.
    pFile1 = fopen("/sd/test1.txt", "w");
    pFile2 = fopen("/sd/test2.txt", "w");
    fwrite(data1, 1, sizeof(data1)-1, pFile1);
    fwrite(data2, 1, sizeof(data2)-1, pFile2);

    fflush(NULL);
    g_sd.sync();

    fclose(pFile1);
    g_sd.sync();
    fclose(pFile2);
    g_sd.sync();
}
