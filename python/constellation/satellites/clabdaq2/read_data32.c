#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

/*
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module reads the redpitaya data registers
*/

typedef struct {
    uint32_t* data;
} Array;

Array readData(int start, int stop, int MEMORY_OFFSET) {
    int memory_fd;
    uint32_t* axi_mmap;
    Array axi_array;

    // Open memory file
    memory_fd = open("/dev/mem", O_RDWR);
    if(memory_fd < 0) {
        perror("Error opening /dev/mem");
        exit(EXIT_FAILURE);
    }

    // Compute chunk length
    int chunk_length = stop - start;

    // Adjust offsets for mmap
    off_t mmap_start = MEMORY_OFFSET;

    // Map memory
    axi_mmap = mmap(NULL, chunk_length * sizeof(uint32_t), PROT_READ | PROT_WRITE, MAP_SHARED, memory_fd, mmap_start);
    if(axi_mmap == MAP_FAILED) {
        perror("Error mapping memory");
        exit(EXIT_FAILURE);
    }

    // Allocate memory for array
    axi_array.data = (uint32_t*)malloc(chunk_length * sizeof(uint32_t));
    if(axi_array.data == NULL) {
        perror("Error allocating memory for array");
        exit(EXIT_FAILURE);
    }

    // Copy data from mmap to array
    for(int i = 0; i < chunk_length; i++) {
        axi_array.data[i] = axi_mmap[i];
    }

    // Unmap memory
    if(munmap(axi_mmap, chunk_length * sizeof(uint32_t)) < 0) {
        perror("Error unmapping memory");
        exit(EXIT_FAILURE);
    }

    // Close memory file
    close(memory_fd);

    return axi_array;
}

void freeData(uint32_t* data) {
    free(data);
}
