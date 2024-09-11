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


// Directory entry struct
struct __attribute__((__packed__)) dir_entry
{
    char filename[8];
    char extension[3];
    uint8_t attributes;
    uint16_t reserved;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t ignore;
    uint16_t last_write_time;
    uint16_t last_write_date;
    uint16_t first_logical_cluster;
    uint32_t filesize;
};

int countFiles(char * dir_ptr, char * fat_ptr, char* file);
int getAllSectors(uint16_t first_logical_cluster, char * fat_ptr, char * file);
void getInfo(char * file);


/* This function counts files in a directory */
int countFiles(char * dir_ptr, char * fat_ptr, char* file) {
    int count = 0;
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
            // read sub directory files
            count += getAllSectors(first_logical_cluster, fat_ptr, file);
            cur_dir++;
            continue;
        }

        // File!
        count++;
        cur_dir++;
    }

    return count;
}


int getAllSectors(uint16_t first_logical_cluster, char * fat_ptr, char * file) {
    uint16_t cur_cluster = first_logical_cluster;
    int count = 0;

    while(!((getNextSector(cur_cluster, fat_ptr) >= 0xFF8 && getNextSector(cur_cluster, fat_ptr) <= 0xFFF) || getNextSector(cur_cluster, fat_ptr) == 0 || getNextSector(cur_cluster, fat_ptr) == 1)) {
        // if(cur_cluster == 0 || cur_cluster == 1) {
        //     printf("cur cluster is %d in loop\n", cur_cluster);
        //     return count;
        // }
        int phys_sector = physical_sector(cur_cluster);
        count += countFiles(file + phys_sector*CLUSTER_SIZE, fat_ptr, file);

        cur_cluster = getNextSector(cur_cluster, fat_ptr);
    }
    int phys_sector = physical_sector(cur_cluster);
    count += countFiles(file + phys_sector*CLUSTER_SIZE, fat_ptr, file);

    return count;
}


/* getInfo finds and prints all info about the disk */
void getInfo(char * file){
    char * boot_ptr = file;

    short tot_sectors = (boot_ptr[20] << 8) | boot_ptr[19];
    uint16_t fat_sectors = (boot_ptr[23] << 8) | boot_ptr[22];
    uint8_t num_fats = boot_ptr[16];
    // uint16_t num_root_entries = (boot_ptr[18] << 8) | boot_ptr[17];

    int fat_offset = CLUSTER_SIZE;
    char * fat_ptr = file + fat_offset + 2;
    int num_fat_entries = tot_sectors - NON_DATA_SECTORS - 2;

    int root_offset = CLUSTER_SIZE + fat_sectors*CLUSTER_SIZE*num_fats;
    uint8_t * root_ptr = (uint8_t *)file + root_offset;

    // Count all free space from FAT
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

    // Find disk label in root directory
    char disk_label[9];
    int cur_entry = 0;
    while(cur_entry < DIR_ENTRIES*ROOT_SECTORS) {
        char * cur = (char *)root_ptr + cur_entry*32;

        // attribute = 0x08
        if(cur[11] == 0x08) {
            strncpy(disk_label, &cur[0], 8);
            disk_label[8] = '\0';
            break;
        } else {
            cur_entry++;
        }
    }
    
    int num_files = 0;
    int cur_dir = 0;
    while(cur_dir < DIR_ENTRIES*ROOT_SECTORS) {
        uint8_t * cur = root_ptr + cur_dir*32;
        // printf("file: %p, root: %p, cur: %p\n", file, root_ptr, cur);

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
            // read sub directory files
            num_files += getAllSectors(first_logical_cluster, fat_ptr, file);
            cur_dir++;
            continue;
        }

        // File!
        num_files++;
        cur_dir++;
    }


    printf("OS Name: %s\n", &boot_ptr[3]);
    printf("Label of the disk: %s\n", disk_label);
    printf("Total size of the disk: %d bytes\n", tot_sectors*CLUSTER_SIZE);
    printf("Free size of the disk: %d bytes\n", free_count*CLUSTER_SIZE);

    printf("=============\n");
    printf("Number of files in the disk: %d\n", num_files);

    printf("=============\n");
    printf("Number of FAT copies: %d\n", num_fats);
    printf("Sectors per FAT: %d\n", fat_sectors);
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
    getInfo(file_pointer);

    // Unmap memory and close file
    munmap(file_pointer, sb.st_size);
    close(fd);

    return 0;
}