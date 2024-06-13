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

// Define the MemoryConfig structure
typedef struct {
    int memory_fd;
    uint32_t* axi_mmap;
    int chunk_length;
} MemoryConfig;

// Function to configure memory mapping
MemoryConfig configureMemory(int MEMORY_OFFSET, int SIZE) {
    MemoryConfig config;

    // Open memory file
    config.memory_fd = open("/dev/mem", O_RDWR);
    if(config.memory_fd < 0) {
        perror("Error opening /dev/mem");
        exit(EXIT_FAILURE);
    }

    // Set chunk length to the size of the mapped region
    config.chunk_length = SIZE / sizeof(uint32_t);

    // Map memory
    config.axi_mmap = mmap(NULL, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, config.memory_fd, MEMORY_OFFSET);
    if(config.axi_mmap == MAP_FAILED) {
        perror("Error mapping memory");
        exit(EXIT_FAILURE);
    }

    return config;
}

// Function to read data from mapped memory
typedef struct {
    uint32_t* data;
} Array;

Array readData(MemoryConfig* config, int start_offset, int stop_offset) {
    Array axi_array;
    int start = start_offset / sizeof(uint32_t);
    int stop = stop_offset / sizeof(uint32_t);
    int chunk_length = stop - start;

    // Allocate memory for array
    axi_array.data = (uint32_t*)malloc(chunk_length * sizeof(uint32_t));
    if(axi_array.data == NULL) {
        perror("Error allocating memory for array");
        exit(EXIT_FAILURE);
    }

    // Copy data from mmap to array
    for(int i = 0; i < chunk_length; i++) {
        axi_array.data[i] = config->axi_mmap[start + i];
    }

    return axi_array;
}

// Function to free the allocated data
void freeData(uint32_t* data) {
    free(data);
}

// Function to clean up memory mapping
void cleanupMemory(MemoryConfig* config) {
    // Unmap memory
    if(munmap(config->axi_mmap, config->chunk_length * sizeof(uint32_t)) < 0) {
        perror("Error unmapping memory");
        exit(EXIT_FAILURE);
    }

    // Close memory file
    close(config->memory_fd);
}
