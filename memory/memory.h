/* 
    Memory Contents
*/

#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <error.h>
#include "../platform/platform.h"

typedef uint8_t u8;
typedef uint16_t  u16;

typedef struct
{
    Cartridge *p_cartidge;
}Memory;

static inline u8  memory_read_8(Memory *p_mem, const u16 addr){
    if (addr > 0x0000 && addr < 0x8000){
        // Cartridge Rom
        printf("READING DATA FROM CARTRIDGE ROM AT: %x\n",addr);
        return p_mem->p_cartidge->rom[addr];
    }

    else{
        printf("Reading Unimplemented memory location %x\n",addr);
        exit(1);
    }

}

static inline void memory_write(Memory *p_mem, const u16 addr, const u8 data){
        if (addr > 0x0000 && addr < 0x8000){
        // Cartridge Rom
        printf("WRITING DATA TO CARTRIDGE ROM AT: %x\n and Value: %x",addr,data);
        p_mem->p_cartidge->rom[addr];
    }

    else{
        printf("Writing Unimplemented memory location %x\n",addr);
        exit(1);
    }
}


