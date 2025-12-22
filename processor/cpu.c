#include "cpu.h"

CPU init_cpu(void){
    return (CPU) {
        .PC.value = 0x100
    };
}