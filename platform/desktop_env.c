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

#define FILE_TO_LOAD "test_roms/tetris.gb"

#define SCALE 4
#define SCALE 4
#define FPS 60
#define FRAME_TIME_MS (1000 / FPS)

// testing remove this TODO
static const uint8_t dmg_palette[4][3] = {
    {255, 255, 255}, 
    {170, 170, 170},
    { 85,  85,  85}, 
    {  0,   0,   0} 
};


struct DrawingContext {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    SDL_Texture  *texture;
};


void screen_event_loop(struct DrawingContext *ctx) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT ||
           (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) {
            cleanup_screen(ctx);
            exit(0);
        }
    }
}


void cleanup_screen(struct DrawingContext *ctx) {
    if (!ctx) return;

    if (ctx->texture)  SDL_DestroyTexture(ctx->texture);
    if (ctx->renderer) SDL_DestroyRenderer(ctx->renderer);
    if (ctx->window)   SDL_DestroyWindow(ctx->window);

    SDL_Quit();
    free(ctx);
}

struct DrawingContext *make_screen(void) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
        exit(1);
    }

    struct DrawingContext *ctx = calloc(1, sizeof(*ctx));

    ctx->window = SDL_CreateWindow(
        "Khel-Babu",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH * SCALE,
        SCREEN_HEIGHT * SCALE,
        SDL_WINDOW_SHOWN
    );

    ctx->renderer = SDL_CreateRenderer(
        ctx->window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    ctx->texture = SDL_CreateTexture(
        ctx->renderer,
        SDL_PIXELFORMAT_RGB24,
        SDL_TEXTUREACCESS_STREAMING,
        SCREEN_WIDTH,
        SCREEN_HEIGHT
    );

    if (!ctx->window || !ctx->renderer || !ctx->texture) {
        fprintf(stderr, "SDL creation failed: %s\n", SDL_GetError());
        cleanup_screen(ctx);
        exit(1);
    }

    return ctx;
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

void present_framebuffer(
    struct DrawingContext *ctx,
    u8 framebuffer[144][160]
) {
    static uint8_t pixels[144][160][3];

    /* Convert palette indices → RGB */
    for (int y = 0; y < 144; y++) {
        for (int x = 0; x < 160; x++) {
            u8 c = framebuffer[y][x] & 3;
            pixels[y][x][0] = dmg_palette[c][0];
            pixels[y][x][1] = dmg_palette[c][1];
            pixels[y][x][2] = dmg_palette[c][2];
        }
    }

    SDL_UpdateTexture(
        ctx->texture,
        NULL,
        pixels,
        SCREEN_WIDTH * 3
    );

    SDL_RenderClear(ctx->renderer);
    SDL_RenderCopy(ctx->renderer, ctx->texture, NULL, NULL);
    SDL_RenderPresent(ctx->renderer);

    /* Frame pacing */
    static Uint32 last = 0;
    Uint32 now = SDL_GetTicks();
    if (now - last < FRAME_TIME_MS)
        SDL_Delay(FRAME_TIME_MS - (now - last));
    last = SDL_GetTicks();
}
