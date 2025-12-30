/* _____ Platform Specific Module -------
 *
 * 	For Windows and Linux (Desktop based OS)
 *      (Tested in linux - debian)
 */

#include "platform.h"
#include <stdio.h>
#include <errno.h>

#define FILE_TO_LOAD "test_roms/11.gb"

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
