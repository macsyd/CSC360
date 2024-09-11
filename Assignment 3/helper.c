#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "helper.h"

/* Returns physical sector number for logical sector */
int physical_sector(int logical_sector) {
    return 33 + logical_sector - 2;
}

/* Returns the next logical cluster, i.e. value of FAT entry */
uint16_t getNextSector(uint16_t cur_cluster, char * fat_ptr) {
    uint16_t value;

    if(cur_cluster%2 == 0) {
        value = ((fat_ptr[1+(3*cur_cluster)/2] << 8) | fat_ptr[(3*cur_cluster)/2]) & 0x0111;
    } else {
        value = (((fat_ptr[1+(3*cur_cluster)/2] << 8) | fat_ptr[(3*cur_cluster)/2]) & 0x1110) >> 4;
    }

    return value;
}

uint32_t getFileSize(char * dir) {
    return (uint32_t)((unsigned char)dir[31] << 24) | ((unsigned char)dir[30] << 16) | ((unsigned char)dir[29] << 8) | (unsigned char)dir[28];
}