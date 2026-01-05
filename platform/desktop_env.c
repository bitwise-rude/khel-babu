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

#define FILE_TO_LOAD "test_roms/tennis.gb"
#define SCALE 4

static const uint8_t dmg_palette[4][3] = {
    {155, 188, 15},  // Lightest
    {139, 172, 15},  // Light
    { 48,  98, 48},  // Dark
    { 15,  56, 15}   // Darkest
};

struct DrawingContext {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    SDL_Texture  *texture;  
    Jpad *jp;
};

void screen_event_loop(struct DrawingContext *context) {
    SDL_Event e;
    
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) {
            printf("Window closed. Signalling quit.\n");
            cleanup_screen(context);
            exit(0);
        }

        if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {

            bool pressed = (e.type == SDL_KEYDOWN);

            switch (e.key.keysym.sym) {
            case SDLK_UP:     context->jp->up     = pressed; break;
            case SDLK_DOWN:   context->jp->down   = pressed; break;
            case SDLK_LEFT:   context->jp->left   = pressed; break;
            case SDLK_RIGHT:  context->jp->right  = pressed; break;

            case SDLK_a:      context->jp->a      = pressed; break;
            case SDLK_b:      context->jp->b      = pressed; break;
            case SDLK_z: context->jp->select  = pressed; break;
            case SDLK_x: context->jp->start = pressed; break;
        }
        }
    }
}

void cleanup_screen(struct DrawingContext *context) {
    if (context->texture != NULL) {
        SDL_DestroyTexture(context->texture);
    }
    if (context->renderer != NULL) {
        SDL_DestroyRenderer(context->renderer);
    }
    if (context->window != NULL) {
        SDL_DestroyWindow(context->window);
    }
    SDL_Quit();
    free(context);
}

struct DrawingContext *make_screen(Jpad *jpp) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("[ERROR] SDL could not initialize! SDL Error: %s\n", SDL_GetError());
        exit(1);
    }
    
    struct DrawingContext *context = (struct DrawingContext *) malloc(sizeof(struct DrawingContext));
    context->texture = NULL;

    context->window = SDL_CreateWindow(
        "Khel-Babu",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        SCREEN_WIDTH * SCALE,
        SCREEN_HEIGHT * SCALE,
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
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (context->renderer == NULL) {
        printf("[ERROR] Renderer could not be created! SDL Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(context->window);
        SDL_Quit();
        exit(1);
    }

    context->texture = SDL_CreateTexture(
        context->renderer,
        SDL_PIXELFORMAT_RGB24,
        SDL_TEXTUREACCESS_STREAMING,
        SCREEN_WIDTH,
        SCREEN_HEIGHT
    );

    if (context->texture == NULL) {
        printf("[ERROR] Texture could not be created! SDL Error: %s\n", SDL_GetError());
        cleanup_screen(context);
        exit(1);
    }

    context->jp = jpp;

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

    if (fseek(fp, 0L, SEEK_END) != 0) {
        perror("Error seeking to the end of the file");
        fclose(fp);
        return cartridge;
    }

    long size = ftell(fp);

    if (size == -1) {
        perror("Error getting file size");
        fclose(fp);
        return cartridge;
    }

    if (fseek(fp, 0L, SEEK_SET) != 0) {
        perror("Error seeking to the beginning");
        fclose(fp);
        return cartridge;
    }

    u8 *pcartridge = (u8 *) malloc(size);
    if (pcartridge == NULL) {
        perror("Error allocating memory");
        fclose(fp);
        return cartridge;
    }

    size_t elements_read = fread(pcartridge, 1, size, fp);
    fclose(fp);

    if ((long) elements_read != size) {
        perror("Some error occurred while reading the file");
        free(pcartridge);
        return cartridge;
    }

    return (Cartridge) {.rom = pcartridge, .length = elements_read};
}

void present_framebuffer(struct DrawingContext *ctx, u8 framebuffer[144][160]) {
    static uint8_t pixels[SCREEN_HEIGHT * SCREEN_WIDTH * 3];

    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            u8 color = framebuffer[y][x] & 3;
            const u8 *rgb = dmg_palette[color];
            
            int idx = (y * SCREEN_WIDTH + x) * 3;
            pixels[idx + 0] = rgb[0];
            pixels[idx + 1] = rgb[1];
            pixels[idx + 2] = rgb[2];
        }
    }

    SDL_UpdateTexture(ctx->texture, NULL, pixels, SCREEN_WIDTH * 3);
    SDL_RenderClear(ctx->renderer);
    SDL_RenderCopy(ctx->renderer, ctx->texture, NULL, NULL);
    SDL_RenderPresent(ctx->renderer);
}