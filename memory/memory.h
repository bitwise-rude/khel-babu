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
    u8 WRAM[0x2000];
    u8 IO[0x80];
    u8 HRAM[0x7F];
    u8 VRAM[0x2000];
}Memory;

static inline u8 *get_address(Memory *p_mem, const u16 addr){
    if (addr<= 0x7FFF){
        // Cartridge Rom
        #ifdef DEBUG
            printf(" FROM CARTRIDGE ROM AT: %.4xH\n]",addr);
        #endif
        return &p_mem->p_cartidge->rom[addr];
    }

    if (addr >= 0x8000 && addr <=0x9FFF){
        // VRAM
        return &p_mem -> VRAM[addr - 0x8000];
    }

    else if (addr >= 0xC000 && addr <= 0xDFFF){
        // WRAM 
        #ifdef DEBUG
            printf(" FROM WRAM AT: %x]\n",addr);
        #endif
        return &p_mem -> WRAM[addr - 0xC000];
    }

    else if (addr >= 0xFF80 && addr <= 0xFFFE){
        // high ram area
        return &p_mem -> HRAM [addr - 0xFF80];
    }

    else if (addr>= 0xff04 && addr <= 0xff07){
        // some timer shit 
        return &p_mem -> IO[addr - 0xFF00];
    }
    else if (addr== 0xff0f){
        // Interrupt Flag 
        return &p_mem -> IO[addr - 0xFF00];
    }
    else if (addr== 0xffff){ 
        // Interrupt Enable
        return &p_mem -> IO[addr - 0xFF00];
    }
    else if (addr== 0xff26){
        // audio shit 
        printf("Audio  IS NOT IMPLEMENTED YET\n");
        return &p_mem -> IO[addr - 0xFF00];
    }
    else if (addr== 0xff25){
        // audio sound panning shit 
        printf("Audio  IS NOT IMPLEMENTED YET\n");
        return &p_mem -> IO[addr - 0xFF00];
    }
    else if (addr== 0xff24){
        // audio master volume shit 
        printf("Audio  IS NOT IMPLEMENTED YET\n");
        return &p_mem -> IO[addr - 0xFF00];
    }

    else if (addr == 0xff40){
        // not implemented LCDC : LCD CONTROL
     
        return &p_mem -> IO[addr - 0xFF00];
    }
    else if (addr == 0xff41){
        // not implemented STAT : LCD CONTROL
     
        return &p_mem -> IO[addr - 0xFF00];
    }
    else if (addr == 0xff4b || addr == 0xff4a || addr == 0xff06 || addr == 0xff48 || addr == 0xff49 || addr == 0xff44){
        // idk what this is 
     
        return &p_mem -> IO[addr - 0xFF00];
    }

    else if (addr == 0xFF42 || addr == 0xFF43){
        // scrolling not implemented
        return &p_mem -> IO[addr - 0xFF00];
    }
    else if (addr == 0xFF47){
                // pallete not implemented
        return &p_mem -> IO[addr - 0xFF00];
    }
    else if (addr == 0xFF01 || addr == 0xFF02){
        // serial transfer not implemented
        return &p_mem -> IO [addr - 0xFF00];
    }

    else{
        printf("NOT IMPLEMENTED MEMORY LOCATION\n");
        return NULL;
    }

}

static inline u8  memory_read_8(Memory *p_mem, const u16 addr){
    #ifdef DEBUG
        printf("READING ");
    #endif

    if (addr == 0xFF44){
        #ifdef LOG
        return 0x90; // TODO: change this just for experiment
        #endif
    }
   
    u8 *add =  get_address(p_mem,addr);
    if (add == NULL){
        printf("Reading Unimplemented memory location %x\n",addr);
        exit(0);
    }
    return *add;
}



static inline void memory_write(Memory *p_mem, const u16 addr, const u8 data){
    if (addr == 0xFF04){
        //reset to 0
        *get_address(p_mem, addr) = 0;
        return;
    }
    u8 *add =  get_address(p_mem,addr);

    
    if (add == NULL){
        printf("Writing Unimplemented memory location %x and data %x\n",addr, data);
        exit(0);
    }
    #ifdef DEBUG
        printf("WRITING  AT: %x and Value: %x\n",addr,data);
    #endif
    *add = data;
   
}


