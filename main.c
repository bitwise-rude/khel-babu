#include <stdio.h>
#include "platform/platform.h"
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

int main(){
	
	u8 *prom = load_cartidge();
	
	if (prom == NULL){
		perror("FILE LOADING ERROR");
		exit(1);
	}

	
}

