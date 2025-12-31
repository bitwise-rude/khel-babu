/* _____ Platform Specific Module -------
 *
 * 	For Windows and Linux (Desktop based OS)
 *      (Tested in linux - debian)
 */

#include "platform.h"
#include <stdio.h>
#include <errno.h>
#include <SDL2/SDL.h>
#include <stdbool.h>

#define FILE_TO_LOAD "test_roms/2.gb"

#define GB_WIDTH  160
#define GB_HEIGHT  144

#define SCALE 4

void make_screen(){
	SDL_Window *window = NULL;
	SDL_Surface *screenSurface = NULL;

	if(SDL_Init(SDL_INIT_VIDEO)){
		printf("Couldn't Initialize SDL\n");
		exit(1);
	}

	window = SDL_CreateWindow ("Khel-Babu",
							SDL_WINDOWPOS_UNDEFINED,
							SDL_WINDOWPOS_UNDEFINED,
							GB_WIDTH * SCALE, GB_HEIGHT * SCALE,
							SDL_WINDOW_SHOWN);
	if(window == NULL){
		printf("Error Creating window");
		exit(1);
	}
	screenSurface = SDL_GetWindowSurface( window );
	SDL_FillRect( screenSurface, NULL, SDL_MapRGB( screenSurface->format, 0xFF, 0xFF, 0xFF ) );
	SDL_UpdateWindowSurface( window );
	SDL_Event e; bool quit = false; while( quit == false ){ while( SDL_PollEvent( &e ) ){ if( e.type == SDL_QUIT ) quit = true; } }
	SDL_DestroyWindow( window );
	SDL_Quit();
}
/* Uses the OS to read a rom (.bin) file and return the contents */
Cartridge load_cartridge() {
	FILE *fp = fopen(FILE_TO_LOAD, "rb");
	Cartridge cartridge = {.rom = NULL, .length = 0};

	if (fp == NULL) {
		perror("Error in opening the file");
		return cartridge;
	}

	if (fseek(fp, 0L, SEEK_END) != 0){
		perror("Error seeking to the end of the file ");
		fclose(fp);
		return cartridge;
	}

	long size = ftell(fp);

	if (size == -1){
		perror("Error getting file size");
		fclose(fp);
		return cartridge;
	}


	if (fseek(fp, 0L,  SEEK_SET) != 0) {
		perror("Error seeking to the end");
		fclose(fp);
		return cartridge;
	}

	u8 *pcartidge = (u8 *) malloc(size);
	if(pcartidge == NULL){
		perror("Error allocating memory");
		fclose(fp);
		return cartridge;
	}

	size_t elements_read = fread(pcartidge, 1, size, fp);
	fclose(fp);

	if((long) elements_read != size){
		perror("Some error occured while reading the file");
		free(pcartidge);
		return cartridge;
	}

	return (Cartridge) {.rom = pcartidge, .length=elements_read};
		 
}

