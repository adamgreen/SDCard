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
/* Implementation of simple interlockeded operations for ARMv7-M based devices. */

    .text
    .code 16
    .syntax unified

    .global interlockedIncrement
    .type interlockedIncrement, function
    /* uint32_t interlockedIncrement(uint32_t* pValue); */
interlockedIncrement:
    ldrex   r1, [r0, #0]
    adds    r1, r1, #1
    strex   r2, r1, [r0, #0]
    cmp     r2, #0
    bne     interlockedIncrement
    mov     r0, r1
    bx      lr


    .global interlockedDecrement
    .type interlockedDecrement, function
    /* uint32_t interlockedDecrement(uint32_t* pValue); */
interlockedDecrement:
    ldrex   r1, [r0, #0]
    subs    r1, r1, #1
    strex   r2, r1, [r0, #0]
    cmp     r2, #0
    bne     interlockedDecrement
    mov     r0, r1
    bx      lr


    .global interlockedAdd
    .type interlockedAdd, function
    /* uint32_t interlockedAdd(uint32_t* pVal1, uint32_t val2); */
interlockedAdd:
    ldrex   r2, [r0, #0]
    adds    r2, r2, r1
    strex   r3, r2, [r0, #0]
    cmp     r3, #0
    bne     interlockedAdd
    mov     r0, r2
    bx      lr


    .global interlockedSubtract
    .type interlockedSubtract, function
    /* uint32_t interlockedSubtract(uint32_t* pVal1, uint32_t val2); */
interlockedSubtract:
    ldrex   r2, [r0, #0]
    subs    r2, r2, r1
    strex   r3, r2, [r0, #0]
    cmp     r3, #0
    bne     interlockedSubtract
    mov     r0, r2
    bx      lr
