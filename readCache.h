#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include "min_heap.h"

// Function to recursively traverse directories using DFS
void traverse_directory(const char *dir_path, int depth, MinHeap *heap, int BLOCK_SIZE);
