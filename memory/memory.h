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
    u8 WRAM[0x1000];
}Memory;

static inline u8 *get_address(Memory *p_mem, const u16 addr){
    if (addr > 0x0000 && addr < 0x8000){
        // Cartridge Rom
        printf("READING DATA FROM CARTRIDGE ROM AT: %.4xH\n",addr);
        return &p_mem->p_cartidge->rom[addr];
    }

    else if (addr >= 0xC000 && addr <= 0xDFFF){
        // WRAM 
        printf("READING DATA FROM RAM AT: %x\n",addr);
        return &p_mem -> WRAM[addr - 0xC000];
    }


    else{
        printf("Reading Unimplemented memory location %x\n",addr);
        exit(1);
    }

}

static inline u8  memory_read_8(Memory *p_mem, const u16 addr){
    return *get_address(p_mem,addr);
}



static inline void memory_write(Memory *p_mem, const u16 addr, const u8 data){
        if (addr > 0x0000 && addr < 0x8000){
        // Cartridge Rom
        printf("WRITING DATA TO CARTRIDGE ROM AT: %x and Value: %x\n",addr,data);
        p_mem->p_cartidge->rom[addr] = data;
    }

    else if (addr >= 0xC000 && addr <= 0xDFFF){
        // WRAM 
        printf("WRITING DATA TO RAM AT: %x and Value: %x\n",addr,data);
        p_mem -> WRAM[addr - 0xC000] = data;
    }

    else{
        printf("Writing Unimplemented memory location %x\n",addr);
        exit(1);
    }
}


