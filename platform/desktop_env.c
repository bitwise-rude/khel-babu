/* _____ Platform Specific Module -------
 *
 * 	For Windows and Linux (Desktop based OS)
 *      (Tested in linux - debian)
 */
#include "platform.h"
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#define FILE_TO_LOAD "test_roms/timing.gb"

#define SCALE 4

// testing remove this TODO
static const uint8_t dmg_palette[4][3] = {
    {255, 255, 255}, 
    {170, 170, 170},
    { 85,  85,  85}, 
    {  0,   0,   0} 
};


struct DrawingContext{
    SDL_Window* window;
    SDL_Renderer* renderer;
} ;



void screen_event_loop(struct DrawingContext *context) {
    SDL_Event e;
    
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) {
			printf("Escape pressed. Signalling quit.\n");
            cleanup_screen(context);
			exit(1);
        }

        if (e.type == SDL_KEYDOWN) {
            if (e.key.keysym.sym == SDLK_ESCAPE) {
                printf("Escape pressed. Signalling quit.\n");
                cleanup_screen(context);
				exit(1);
            }
        }
        
    }
    }

void cleanup_screen(struct DrawingContext *context) {
    if (context->renderer != NULL) {
        SDL_DestroyRenderer(context->renderer);
    }
    if (context->window != NULL) {
        SDL_DestroyWindow(context->window);
    }
    SDL_Quit();
}

struct DrawingContext *make_screen(){
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("[ERROR] SDL could not initialize! SDL Error: %s\n", SDL_GetError());
        exit(1);
    }
	struct DrawingContext *context = (struct DrawingContext *) malloc(sizeof(struct DrawingContext));

	context->window = SDL_CreateWindow(
        "Khel-Babu",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        SCREEN_WIDTH*SCALE,
        SCREEN_HEIGHT*SCALE,
        SDL_WINDOW_SHOWN
    );

	if (context->window == NULL) {
        printf("[ERROR] Window could not be created! SDL Error: %s\n", SDL_GetError());
        SDL_Quit(); 
        exit(1);
    }

	context->renderer = SDL_CreateRenderer(
        context->window, 
		-1,
        SDL_RENDERER_ACCELERATED
    );

	if (context->renderer == NULL) {
        printf("[ERROR] Renderer could not be created! SDL Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(context->window);
        SDL_Quit();
        context->window = NULL;
		exit(1);
    };

	return context;
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

void present_framebuffer( struct DrawingContext *ctx, u8 framebuffer[144][160]
){
    SDL_Renderer *r = ctx->renderer;

    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);

    for (int y = 0; y < 144; y++) {
        for (int x = 0; x < 160; x++) {

            u8 color = framebuffer[y][x] & 3;
            const u8 *rgb = dmg_palette[color];

            SDL_SetRenderDrawColor(r, rgb[0], rgb[1], rgb[2], 255);

            SDL_Rect pixel = {
                x * SCALE,
                y * SCALE,
                SCALE,
                SCALE
            };

            SDL_RenderFillRect(r, &pixel);
        }
    }

    SDL_RenderPresent(r);
}
