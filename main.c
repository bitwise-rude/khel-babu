#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>


#include "platform/platform.h"
#include "processor/cpu.h"
#include "memory/memory.h"
#include "interrupts/interrupts.h"

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

	
	for (int i = 0; i<=ITERATION; i++){
		int cycles_taken = step_cpu(&cpu);
		
		handle_interrupt(&im);
	}

	free(cartridge.rom);
}

