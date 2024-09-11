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

void getFile(char * file, char * file_name);

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

    // Find and copy file
    getFile(file_pointer, argv[2]);

    // Unmap memory and close file
    munmap(file_pointer, sb.st_size);
    close(fd);

    return 0;
}

void getFile(char * file, char * file_name) {
    char * boot_ptr = file;

    uint16_t fat_sectors = (boot_ptr[23] << 8) | boot_ptr[22];
    uint8_t num_fats = boot_ptr[16];

    int fat_offset = CLUSTER_SIZE;
    char * fat_ptr = file + fat_offset + 2;
    // int num_fat_entries = tot_sectors - NON_DATA_SECTORS - 2;

    int root_offset = CLUSTER_SIZE + fat_sectors*CLUSTER_SIZE*num_fats;
    char * root_ptr = file + root_offset;

    char * token = strtok(file_name, ".");
    char name[9];
    strncpy(name, token, strlen(token));
    token = strtok(NULL, ".");
    if(token != NULL) {
        char extension[4];
        strncpy(extension, token, strlen(token));
    }

    int cur_dir = 0;
    int found = 0;
    char * cur;
    while(cur_dir < ROOT_SECTORS*DIR_ENTRIES) {
        cur = root_ptr + cur_dir*32;

        if(strncmp(name, cur, strlen(name)) != 0) {
            cur_dir++;
            continue;
        } 
        if(token != NULL) {
            if(strncmp(token, &cur[8], strlen(token)) != 0) {
                // wrong file
                cur_dir++;
                continue;
            } else {
                found = 1;
                break;
            }
        }
        
    }
    if(!found) {
        printf("File not found.\n");
        return;
    }

    // Get current directory
    // char * cwd_buffer[50];
    // if(getcwd(cwd_buffer, 50) == NULL) {
    //     fprintf(stderr, "Error: Could not find current working directory.\n");
    //     exit(-1);
    // }

    FILE *new_file;

    new_file = fopen(file_name, "w");

    char * data;
    uint32_t filesize = getFileSize(cur);
    data = (char *)malloc(filesize);
    if(data == NULL) {
        fprintf(stderr, "Error: malloc of size %d failed.\n", filesize);
        exit(-1);
    }

    uint16_t log_cluster = (cur[27] << 8) | cur[26];
    int phys_sector;
    char * ptr;
    int count = 0;
    while(!((getNextSector(log_cluster, fat_ptr) >= 0xFF8 && getNextSector(log_cluster, fat_ptr) <= 0xFFF) || getNextSector(log_cluster, fat_ptr) == 0 || getNextSector(log_cluster, fat_ptr) == 1)) {
        // get data
        phys_sector = physical_sector(log_cluster);
        ptr = file + phys_sector*CLUSTER_SIZE;
        strncpy(&data[count*CLUSTER_SIZE], ptr, CLUSTER_SIZE);

        log_cluster = getNextSector(log_cluster, fat_ptr);
        count++;
    }
    //get data
    phys_sector = physical_sector(log_cluster);
    ptr = file + phys_sector*CLUSTER_SIZE;
    strncpy(&data[count*CLUSTER_SIZE], ptr, CLUSTER_SIZE);

    fwrite(data, CLUSTER_SIZE, count, new_file);

    fclose(new_file);
    free(data);
}