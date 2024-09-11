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

#define CLUSTER_SIZE 512
#define DIR_ENTRIES 16
#define ROOT_SECTORS 14
#define NON_DATA_SECTORS 33

void putFile(char * file, char * new_file_name);


int main(int argc, char* argv[]) {
    if(argc != 3) {
        fprintf(stderr, "Error: Please run with two input files.\n");
        exit(-1);
    }

    // Open file and map to memory
    int fd;
	struct stat sb;

	fd = open(argv[1], O_RDWR);
	fstat(fd, &sb);
	// printf("Size: %lu\n\n", (uint64_t)sb.st_size);

    // file_pointer points to the starting pos of mapped memory
	char * file_pointer = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); 
	if (file_pointer == MAP_FAILED) {
		printf("Error: failed to map memory\n");
		exit(1);
	}

    // Find and print disk info
    putFile(file_pointer, argv[2]);

    // Unmap memory and close file
    munmap(file_pointer, sb.st_size);
    close(fd);

    return 0;
}

void putFile(char * file, char * new_file_name) {
    char * boot_ptr = file;

    short tot_sectors = (boot_ptr[20] << 8) | boot_ptr[19];
    uint16_t fat_sectors = (boot_ptr[23] << 8) | boot_ptr[22];
    uint8_t num_fats = boot_ptr[16];

    int fat_offset = CLUSTER_SIZE;
    char * fat_ptr = file + fat_offset + 2;
    int num_fat_entries = tot_sectors - NON_DATA_SECTORS - 2;

    int root_offset = CLUSTER_SIZE + fat_sectors*CLUSTER_SIZE*num_fats;
    char * root_ptr = file + root_offset;

    char * token = strtok(new_file_name, "/");

    int cur_dir = 0;
    int found = 0;
    char * cur;
    char * dir_ptr = root_ptr;
    int num_entries;
    while(token != NULL) {
        if(strchr(token, '.') == NULL) {
            //token is subdir
            if(dir_ptr == root_ptr) {
                num_entries = ROOT_SECTORS*DIR_ENTRIES;
            } else {
                num_entries = DIR_ENTRIES;
            }
            while(cur_dir < num_entries) {
                cur = dir_ptr + cur_dir*32;

                if(strncmp(token, cur, strlen(token)) == 0) {
                    // found subdir
                    found = 1;
                    uint16_t first_cluster = (cur[27] << 8) | cur[26];
                    dir_ptr = file + physical_sector(first_cluster)*CLUSTER_SIZE;
                    cur_dir = 0;
                    break;
                } 
                cur_dir++;
                continue;
            }
            if(!found) {
                printf("Directory not found,\n");
                return;
            }
            found = 0;
        } else {
            // token is file
            break;
        }
        token = strtok(NULL, "/");
    }
    // token is file
    

    int f;
    f = open(new_file_name, O_RDWR);
    if(f == -1) {
        printf("File not found.\n");
        return;
    }
    struct stat sb;
    fstat(f, &sb);

    int free_count = 0;
    int n = 0;
    while(n < num_fat_entries) {
        if((((fat_ptr[1+(3*n)/2] << 8) | fat_ptr[(3*n)/2]) & 0x0111) == 0x0000) {
            free_count++;
        }

        n++;
        if((((fat_ptr[1+(3*n)/2] << 8) | fat_ptr[(3*n)/2]) & 0x1110) == 0x0000) {
            free_count++;
        }

        n++;
    }
    if(sb.st_size > free_count*CLUSTER_SIZE) {
        printf("Not enough free space in the disk image.\n");
        return;
    }

    cur_dir = 0;
    // int found = 0;
    // char * cur;
    while(cur_dir < num_entries) {
        cur = dir_ptr + cur_dir*32;

        if(cur[0] == 0xE5 || cur[0] == 0x00) {
            //entry free
            break;
        }
        
    }
    // update entry with new file name and filesize
    char * new_token = strtok(token, ".");
    strncpy(cur, new_token, 8);
    new_token = strtok(NULL, ".");
    strncpy(&cur[8], new_token, 3);

    uint32_t filesize = (uint32_t)sb.st_size;
    cur[28] = (filesize & 0x00000011);
    cur[29] = (filesize & 0x00001100) >> 8;
    cur[30] = (filesize & 0x00110000) >> 16;
    cur[31] = (filesize & 0x11000000) >> 24;

    // find empty fat entry
    n = 0;
    while(n < num_fat_entries) {
        if((((fat_ptr[1+(3*n)/2] << 8) | fat_ptr[(3*n)/2]) & 0x0111) == 0x0000) {
            break;
        }

        n++;
        if((((fat_ptr[1+(3*n)/2] << 8) | fat_ptr[(3*n)/2]) & 0x1110) == 0x0000) {
            break;
        }

        n++;
    }
    // update directory entry to point to fat slot
    uint16_t small_n = (uint16_t)n;
    cur[27] = small_n >> 8;
    cur[26] = small_n & 0x0011;
    if(n%2 == 0) {
        fat_ptr[1+(3*n)/2] |= 1 << 3;
        fat_ptr[1+(3*n)/2] |= 1 << 2;
        fat_ptr[1+(3*n)/2] |= 1 << 1;
        fat_ptr[1+(3*n)/2] |= 1 << 0;
        fat_ptr[(3*n)/2] = 0xFF;
    } else {
        fat_ptr[1+(3*n)/2] = 0xFF;
        fat_ptr[(3*n)/2] |= 1 << 7;
        fat_ptr[(3*n)/2] |= 1 << 6;
        fat_ptr[(3*n)/2] |= 1 << 5;
        fat_ptr[(3*n)/2] |= 1 << 4;
    }


    close(f);
}