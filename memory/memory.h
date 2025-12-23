/* 
    Memory Contents
*/

#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "../platform/platform.h"

typedef uint8_t u8;
typedef uint16_t  u16;

typedef struct
{
    Cartridge *p_cartidge;
}Memory;

static inline u8  memory_read_8(Memory *p_mem, u16 addr){
    printf("Reading from %x", addr);
}

static inline void memory_write(Memory *p_mem, u16 addr, u8 data){
    printf("Writing to %4x with value %2x", addr, data);
}


