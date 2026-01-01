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

// #define DEBUG // print logs to console
#define LOG // Log to an output file "logging.txt"
#define ITERATION 999999999
#define LOG_BUFFER_SIZE 1000
#define SCREEN_WIDTH  160
#define SCREEN_HEIGHT  144


struct DrawingContext;

typedef struct {
    u8 *rom;
    size_t length;
} Cartridge;

// load cratidge rom data and return the array to the cartidge
Cartridge load_cartridge(void);	

// screen things
struct DrawingContext *make_screen();
void cleanup_screen(struct DrawingContext *context);
void screen_event_loop(struct DrawingContext *context) ;