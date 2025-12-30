/* 	platform.h 
 *  	
 *  	Contains platform independent code, every platform should implement or override these
 *  	methods and functions
 *  		
 */

#pragma once

#include <stdlib.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;

#define DEBUG // print logs to console
#define LOG // Log to an output file "logging.txt"

typedef struct {
    u8 *rom;
    size_t length;
} Cartridge;

// load cratidge rom data and return the array to the cartidge
Cartridge load_cartridge(void);	
