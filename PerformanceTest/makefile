# Copyright 2016 Adam Green (http://mbed.org/users/AdamGreen/)
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
PROJECT         := PerformanceTest
DEVICES         := LPC1768
GCC4MBED_DIR    := ../gcc4mbed
NEWLIB_NANO     := 0
NO_FLOAT_SCANF  := 0
NO_FLOAT_PRINTF := 0
USER_LIBS       := ../FATFileSystem ../SDFileSystem ../SPIDma ../CircularLog

# The following line can be uncommented to place the binary in a location where
# it can be used with the DFU bootloader on the Smoothie1 board.
LPC1768_LSCRIPT := SmoothieLPC1768.ld

include $(GCC4MBED_DIR)/build/gcc4mbed.mk
