#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>


#include "platform/platform.h"
#include "processor/cpu.h"
#include "memory/memory.h"
#include "interrupts/interrupts.h"
#include "timer/timer.h"
#include "PPU/ppu.h"

// TODO: perform checksums and switchable banks
void verify_cartridge_header(const u8 *p_cartridge){
	// get the title of the cartidge
	char title[17] = {0};
	
	for (int i = 0; i< 16 ; i++){
		u8 c = p_cartridge[0x134 + i];
		
		if (c == 0x00) break;
		if (c < 32 || c > 126) break; 
		
		title[i] = (char) c;

	}

	printf("\nLoading cartidge with game title :%s\n",title);

}

int main(){
	Cartridge cartridge = load_cartridge();
	
	if (cartridge.rom  == NULL){
		perror("FILE LOADING ERROR");
		exit(1);
	}

	verify_cartridge_header(cartridge.rom);

	// make memory from cartidge
	Memory memory = (Memory) {.p_cartidge = &cartridge};

	CPU cpu = init_cpu(&memory);

	InterruptManager im = make_interrupt_manager(&cpu);

	Timer_Manager tm = make_timer(&cpu, &im);

	struct DrawingContext *dr_ctx=make_screen();;

	PPU ppu = {
		.p_mem = &memory,
		.mode = 2,
		.m_cycles = 0,
		.ly = 0,
		.ih = &im,
		.frame_buffer = {{0}},
		.draw_ctx = dr_ctx,
	};


	for (int i = 0; i<=ITERATION; i++){
		int cycles_taken = step_cpu(&cpu);
		step_ppu(&ppu,cycles_taken);
		handle_interrupt(&im);
		// timer_step(&tm,cycles_taken);
	}

	printf("CLEANING UP\n");
	free(cartridge.rom);
	cleanup_screen(dr_ctx);
}

