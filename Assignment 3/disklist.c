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
#define timeOffset 14 //offset of creation time in directory entry
#define dateOffset 16 //offset of creation date in directory entry

void print_date_time(char * directory_entry_startPos);
void listDirectories(char * file);
void getAllSectors(uint16_t first_logical_cluster, char * fat_ptr, char * file);
void listSubDirectories(char * dir_ptr, char * fat_ptr, char* file);


void print_date_time(char * directory_entry_startPos){
	
	int time, date;
	int hours, minutes, day, month, year;
	
	time = *(unsigned short *)(directory_entry_startPos + timeOffset);
	date = *(unsigned short *)(directory_entry_startPos + dateOffset);
	
	//the year is stored as a value since 1980
	//the year is stored in the high seven bits
	year = ((date & 0xFE00) >> 9) + 1980;
	//the month is stored in the middle four bits
	month = (date & 0x1E0) >> 5;
	//the day is stored in the low five bits
	day = (date & 0x1F);
	
	printf("%d-%02d-%02d ", year, month, day);
	//the hours are stored in the high five bits
	hours = (time & 0xF800) >> 11;
	//the minutes are stored in the middle 6 bits
	minutes = (time & 0x7E0) >> 5;
	
	printf("%02d:%02d\n", hours, minutes);
	
	return ;	
}


int main(int argc, char* argv[]) {
    if(argc != 2) {
        fprintf(stderr, "Error: Please run with one input file.\n");
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
    listDirectories(file_pointer);

    // Unmap memory and close file
    munmap(file_pointer, sb.st_size);
    close(fd);

    return 0;
}

void listDirectories(char * file) {
    char * boot_ptr = file;

    // short tot_sectors = (boot_ptr[20] << 8) | boot_ptr[19];
    uint16_t fat_sectors = (boot_ptr[23] << 8) | boot_ptr[22];
    uint8_t num_fats = boot_ptr[16];
    // uint16_t num_root_entries = (boot_ptr[18] << 8) | boot_ptr[17];
    // printf("num root entries: %hu\n", num_root_entries);

    int fat_offset = CLUSTER_SIZE;
    char * fat_ptr = file + fat_offset + 2;
    // int num_fat_entries = tot_sectors - NON_DATA_SECTORS - 2;

    int root_offset = CLUSTER_SIZE + fat_sectors*CLUSTER_SIZE*num_fats;
    char * root_ptr = file + root_offset;

    printf("Root\n");
    printf("==================\n");
    int cur_dir = 0;
    while(cur_dir < ROOT_SECTORS*DIR_ENTRIES) {
        char * cur = root_ptr + cur_dir*32;

        // attribute = 0x0F
        if(cur[11] == 0x0F) {
            cur_dir++;
            continue;
        }
        // first logical cluster is 1 or 0
        uint16_t first_logical_cluster = (cur[27] << 8) | cur[26];
        if(first_logical_cluster == 0x0001 || first_logical_cluster == 0x0000) {
            cur_dir++;
            continue;
        }
        // volume label bit set
        if((cur[11] & 0x08) != 0) {
            cur_dir++;
            continue;
        }
        // directory free
        if(cur[0] == 0xE5 || cur[0] == '.') {
            cur_dir++;
            continue;
        }
        if(cur[0] == 0x00) {
            break;
        }

        // subdirectory
        if((cur[11] & 0x10) != 0) {
            // print sub directory
            uint32_t dirsize = getFileSize(cur);
            char dirname[20];
            strncpy(dirname, &cur[0], 8);
            strncpy(&dirname[8], &cur[8], 3);
            dirname[11] = '\0';
            printf("D %10.d %s ", dirsize, dirname);
            print_date_time(cur);
            printf("==================\n");
            getAllSectors(first_logical_cluster, fat_ptr, file);
            printf("\n");
            cur_dir++;
            continue;
        }

        // File!
        uint32_t filesize = getFileSize(cur);
        char filename[20];
        strncpy(filename, &cur[0], 8);
        strncpy(&filename[8], &cur[8], 3);
        filename[11] = '\0';
        printf("F %10u %s ", filesize, filename);
        print_date_time(cur);
        cur_dir++;
    }
}

/* This function counts files in a directory */
void listSubDirectories(char * dir_ptr, char * fat_ptr, char* file) {
    int cur_dir = 0;
    while(cur_dir < DIR_ENTRIES) {
        char * cur = dir_ptr + cur_dir*32;

        // attribute = 0x0F
        if(cur[11] == 0x0F) {
            cur_dir++;
            continue;
        }
        // first logical cluster is 1 or 0
        uint16_t first_logical_cluster = (cur[27] << 8) | cur[26];
        if(first_logical_cluster == 1 || first_logical_cluster == 0) {
            cur_dir++;
            continue;
        }
        // volume label bit set
        if((cur[11] & 0x08) != 0) {
            cur_dir++;
            continue;
        }
        // directory free
        if(cur[0] == 0xE5 || cur[0] == '.') {
            cur_dir++;
            continue;
        }
        if(cur[0] == 0x00) {
            break;
        }

        // subdirectory
        if((cur[11] & 0x10) != 0) {
            // print sub directory
            uint32_t dirsize = getFileSize(cur);
            char dirname[20];
            strncpy(dirname, &cur[0], 8);
            strncpy(&dirname[8], &cur[8], 3);
            dirname[11] = '\0';
            printf("D %10.d %s ", dirsize, dirname);
            print_date_time(cur);
            printf("==================\n");
            getAllSectors(first_logical_cluster, fat_ptr, file);
            printf("\n");
            cur_dir++;
            continue;
        }

        // File!
        uint32_t filesize = getFileSize(cur);
        char filename[20];
        strncpy(filename, &cur[0], 8);
        strncpy(&filename[8], &cur[8], 3);
        filename[11] = '\0';
        printf("F %10u %s ", filesize, filename);
        print_date_time(cur);
        cur_dir++;
    }

    return;
}


void getAllSectors(uint16_t first_logical_cluster, char * fat_ptr, char * file) {
    uint16_t cur_cluster = first_logical_cluster;

    while(!((getNextSector(cur_cluster, fat_ptr) >= 0xFF8 && getNextSector(cur_cluster, fat_ptr) <= 0xFFF) || getNextSector(cur_cluster, fat_ptr) == 0 || getNextSector(cur_cluster, fat_ptr) == 1)) {
        int phys_sector = physical_sector(cur_cluster);
        listSubDirectories(file + phys_sector*CLUSTER_SIZE, fat_ptr, file);

        cur_cluster = getNextSector(cur_cluster, fat_ptr);
    }
    int phys_sector = physical_sector(cur_cluster);
    listSubDirectories(file + phys_sector*CLUSTER_SIZE, fat_ptr, file);

    return;
}