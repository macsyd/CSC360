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

int physical_sector(int logical_sector);
uint16_t getNextSector(uint16_t cur_cluster, char * fat_ptr);
uint32_t getFileSize(char * dir);