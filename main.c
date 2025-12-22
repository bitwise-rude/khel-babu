#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>


#include "platform/platform.h"

int main(){
	
	u8 *p_cartidge = load_cartidge();
	
	if (p_cartidge  == NULL){
		perror("FILE LOADING ERROR");
		exit(1);
	}

	
}

