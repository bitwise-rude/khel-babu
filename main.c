#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>


#include "platform/platform.h"

// TODO: checksums 
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
	
	u8 *p_cartridge = load_cartridge();
	
	if (p_cartridge  == NULL){
		perror("FILE LOADING ERROR");
		exit(1);
	}

	verify_cartridge_header(p_cartridge);

	free(p_cartridge);
	
}

