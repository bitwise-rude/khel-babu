/* 	platform.h 
 *  	
 *  	Contains platform independent code, every platform should implement or override these
 *  	methods and functions
 *  		
 */


#include <stdlib.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;

// load cratidge rom data and return the array to the cartidge
u8 *load_cartidge(void);	
