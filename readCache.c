#include "readCache.h"
#include "min_heap.h"

// Function to recursively traverse directories using DFS
void traverse_directory(const char *dir_path, int depth, MinHeap* heap, int BLOCK_SIZE) {
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    char path[1024];
    
    // Open the directory
    if ((dir = opendir(dir_path)) == NULL) {
        perror("opendir");
        return;
    }
    
    // Read directory entries
    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".." to avoid infinite recursion
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        
        // Create full path
        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);
        
        // Get file status
        if (stat(path, &file_stat) < 0) {
            perror("stat");
            continue;
        }
        
        // Print indentation based on depth
        // for (int i = 0; i < depth; i++)
        //     printf("  ");
        
        // Print file/directory name
        if (S_ISDIR(file_stat.st_mode)) {
            // printf("[DIR] %s\n", entry->d_name);
            // Recursively process subdirectory (DFS)
            traverse_directory(path, depth + 1, heap, BLOCK_SIZE);
        } else {
            printf("%s\n", entry->d_name);
            
            time_t t;
            time(&t);
            uint64_t block_number = atoi(entry->d_name);
            FILE* block_file = fopen(path, "r");
            
            insert(heap, block_number, t); // Put the block num back to the heap

            int index = find_value(heap, block_number);
            if (index >= 0) {
                if (block_file != NULL) {
                    // Update the dirty bit from cache (local disk)
                    char dirty_bit;
                    if (fseek(block_file, BLOCK_SIZE, SEEK_SET) == 0) {  // Move to the position after data
                        if (fread(&dirty_bit, 1, 1, block_file) == 1) {
                            // Successfully read the dirty bit
                            printf("%i's dirty bit: raw value=%d\n", block_number, (int)(unsigned char)dirty_bit);
                            
                            // ANY non-zero value is treated as dirty
                            heap->arr[index].dirty_flag = (dirty_bit != 0) ? 1 : 0;
                        } // if 
                        else {
                            heap->arr[index].dirty_flag = 0; // be safe to relif consumer's burden
                            printf("Error: Cannot read dirty bit from cache.\n");
                        } // else 
                    } // if
                    else {
                        printf("Error: Cannot find dirty bit position for block.\n");
                    } // else
                } // if
                else {
                    printf("Error: No such file (%i) on the cache.\n", block_number);
                } // else 
            } // if
            else {
                printf("Error: Insert block into heap FAILURE since we cannot fine the number from heap.\n");
            } // else

            fclose(block_file);
        }
    }

    closedir(dir);
}


