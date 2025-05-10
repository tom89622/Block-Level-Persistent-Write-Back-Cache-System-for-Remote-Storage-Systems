#include <argp.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include "min_heap.h"
#include "buse.h"
#include "readCache.h"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

#define UNUSED(x) (void)(x)
int BLOCK_SIZE;
uint64_t server_disk;
// uint64_t local_disk_size= 5 * 1024 * 1024 * 1024 * 8; // 5GB // 10 * 4096;
uint64_t local_disk_size= 10 * 4096;
u_int64_t Hit_counter = 0;//record the hit times of cache
// Create the heap
MinHeap* h;

bool verbose = false;

void reBuildCache() {
    printf("---- Start to re-build our cache ----\n\n");
    traverse_directory("./0", 0, h, BLOCK_SIZE);
    // print_heap(h);
    printf("---- End of re-build our cache ----\n\n");

}

char* convertToPath(uint64_t number) {
    char str[13];
    sprintf(str, "%012lu", number);

    int len = strlen(str);
    char *path = (char*)malloc(3 * len + 3);
    
    if (!path) return NULL;

    char *ptr = path;
    *ptr++ = '.';
    *ptr++ = '/';

    for (int i = 0; i < len - 1; i++) {
        *ptr++ = str[i];
        *ptr++ = '/';

        *ptr = '\0';  
        if (access(path, F_OK) != 0) {
            mkdir(path, 0755); 
        }
    }
    strcpy(ptr, str);
    return path;
}

void* thread_consumer(void* arg) {
    while(1){
        for (int i = 0; i < h->size; i++) {
            // sleep(0.1);
            pthread_mutex_lock(&mutex);
            HeapNode curNode = h->arr[i];
            // printf("Lock -- line 68\n");
            if (curNode.dirty_flag == 1) {
                uint64_t block_number = curNode.value;
                pthread_mutex_unlock(&mutex);
                // printf("Unlock -- line 70\n");
                //get path of the block
                char *path=convertToPath(block_number);
                FILE* block_file = fopen(path, "r+b");
                if (block_file == NULL) {
                    printf("Error: Could not open file %s\n", path);
                    free(path);
                    continue;
                } // if
                else {
                    printf("Open file -- %d --\n", block_number);
                } // else

                
                // Read entire file including dirty bit
                char *temp_buffer = malloc(BLOCK_SIZE + 1);
                if (fread(temp_buffer, 1, BLOCK_SIZE + 1, block_file) < BLOCK_SIZE) {
                    printf("Error: Could not read entire file %s\n", path);
                    free(temp_buffer);
                    free(path);
                    fclose(block_file);
                    continue;
                }

                printf("size of block_file: %d\n", ftell(block_file));

                pthread_mutex_lock(&mutex);
                // printf("Lock -- line 113\n");
                for (int j = 0; j < h->size; j++) {
                    if (h->arr[j].value == curNode.value) {
                        h->arr[j].dirty_flag = 0;
                        break;
                    } // if
                } // for
                
                printf("----block_number %d Writing to server ---- \n", block_number);
                // Write data portion to server
                // sleep(5);
                pwrite(server_disk, temp_buffer, BLOCK_SIZE, block_number * BLOCK_SIZE);
                printf("----done: block_number %d Write to server ---- \n", block_number);
                
                // Update dirty bit in memory and file
                curNode.dirty_flag = 0;
                temp_buffer[BLOCK_SIZE] = (char)0;  // Update dirty bit to clean
                
                // Seek back to beginning and write entire buffer
                fseek(block_file, 0, SEEK_SET);
                fwrite(temp_buffer, 1, BLOCK_SIZE + 1, block_file);
                
                free(temp_buffer);
                free(path);
                fclose(block_file);

                pthread_cond_broadcast(&cond);  // Notify waiting threads
                // printf("Unlock -- line 123\n");
            } // if

            // aggrrasive check the min block
            if (i != 0 && h->arr[0].dirty_flag == 1) {
                i = -1;
            } // if
            pthread_mutex_unlock(&mutex);
            // printf("Unlock -- line 127\n");
        } // for
    } // while
    return NULL;
}

int write_to_cache_LRU(const void *buffer, u_int64_t offset, void *userdata, bool isWrite){
    uint64_t block_number = offset / BLOCK_SIZE;
    bool isFound = false;
    printf("----block_number %d Writing into cache ---- \n",block_number);
    // if (block_file != NULL) {
    pthread_mutex_lock(&mutex);
    if (find_value(h, block_number) >= 0) { // find the block in the cache
        printf("update block----\n");
        //remove old date from heap
        printf("block num: %i\n", block_number);
        HeapNode removedNode = remove_value(h,block_number);
        isFound = true;
        Hit_counter++;
        //printf("Hit_counter: %d, block_number: %d\n", Hit_counter,block_number);
    } // if
    else if (h->size >= h->capacity) {      
        // delete file according to heap
        while (h->size > 0 && h->arr[0].dirty_flag == 1) {
            printf("Waiting for block %lu to be clean...\n", h->arr[0].value);
            pthread_cond_wait(&cond, &mutex);  // Releases mutex, re-acquires it when signaled
        } // while

        if (h->arr[0].key >= 0) {
            // Get the block number before modifying the heap
            uint64_t min_value = h->arr[0].value; 
            
            // Update the heap
            extract_min(h);
            
            // Now remove the file we identified earlier
            char *path = convertToPath(min_value);
            remove(path);
            free(path);
        } // if
        else {
            printf("heap is empty\n");
        } // else
    } // else if
    

    // update heap with current time
    // time_t t;
    // time(&t);
    struct timeval t;
    gettimeofday(&t, NULL);
    insert(h, block_number, t.tv_sec + t.tv_usec / 1000000.0);
    // pthread_mutex_unlock(&mutex);
    if (isWrite != true && isFound == true) {
        // Do nothing in cache
        pthread_mutex_unlock(&mutex);
        return 0;
    } // if

    //get path of the block
    char *path=convertToPath(block_number);
    FILE* block_file = fopen(path, "r+b");
    if (block_file == NULL) {
        block_file = fopen(path, "w+b");
    } // if
    // Allocate a buffer with one extra byte for the dirty bit
    char *temp_buffer = malloc(BLOCK_SIZE + 1);
    memcpy(temp_buffer, buffer, BLOCK_SIZE);
    // pthread_mutex_lock(&mutex);
    int update_index=find_value(h, block_number);
    if (isWrite == true && update_index >= 0) {
        // If doing write, we udpate dirty bit
        h->arr[update_index].dirty_flag = 1;
        temp_buffer[BLOCK_SIZE] = (char)1;  // Explicit cast to ensure consistent binary value
    } // if
    else {
        // New data which read from server
        temp_buffer[BLOCK_SIZE] = (char)0;  // Explicit cast to ensure consistent binary value
    } // else 
    
    // Go back to start of file to overwrite
    fseek(block_file, 0, SEEK_SET);
    fwrite(temp_buffer,1,BLOCK_SIZE+1,block_file);
    printf("size of block_file: %d\n", ftell(block_file));
    pthread_mutex_unlock(&mutex);
    free(temp_buffer);
    fclose(block_file);
    return 0;
}

int cache_read(const void *buf, u_int32_t len, u_int64_t offset, void *userdata) {
    UNUSED(userdata);
    if (verbose)
        fprintf(stderr, "R - %lu, %u\n", offset, len);

    char *buffer = (char*)buf;
    //read in block size
    while (len > 0) {
        uint64_t block_number = offset / BLOCK_SIZE;
        //get path of the block
        char *path=convertToPath(block_number);
        FILE* block_file = fopen(path, "r");
        // if (block_file != NULL) {
        if (find_value(h, block_number) >= 0) { // find the block in the cache
            printf("----block_number %d Reading from cache ---- \n",block_number);
            
            fread(buffer,1,BLOCK_SIZE,block_file);
            
            //printf("---- increase ---- \n");
            // time_t t;
            // time(&t);  
            struct timeval t;
            gettimeofday(&t, NULL);
            increase_key(h,block_number,t.tv_sec + t.tv_usec / 1000000.0);
            // print_heap(h);
            printf("----done: block_number %d Read from cache ---- \n",block_number);
            fclose(block_file);
            Hit_counter++;
            //printf("Hit_counter: %d, block_number: %d\n", Hit_counter,block_number);
        } // if
        else {
            printf("----block_number %d Reading from server ---- \n",block_number);
            pread(server_disk, buffer, BLOCK_SIZE, offset);

            //write this block size of data into cache and update heap
            write_to_cache_LRU(buffer,offset,userdata, false);
            // print_heap(h);
            printf("----done: block_number %d Read from server ---- \n",block_number);
        } // else 
        
        buffer += BLOCK_SIZE;
        offset += BLOCK_SIZE;
        len -= BLOCK_SIZE;
    }

    return 0;
 }

int cache_write(const void *buf, u_int32_t len, u_int64_t offset, void *userdata) {
    UNUSED(userdata);
    if (verbose)
        fprintf(stderr, "W - %lu, %u\n", offset, len);
    
    char *buffer = (char*)buf;
    while (len > 0) {
        // write to cache (local)
        write_to_cache_LRU(buffer,offset,userdata, true);
        // write to disks (server)
        // pwrite(server_disk, buffer, BLOCK_SIZE, offset);

        buffer += BLOCK_SIZE;
        offset += BLOCK_SIZE;
        len -= BLOCK_SIZE;
    }
    
    return 0;
 }

static struct argp_option options[] = {
    {"verbose", 'v', 0, 0, "Produce verbose output", 0},
    {0},
};
struct arguments {
    uint32_t block_size;
    char* raid_device; // nbd
    char* server_device; // real block device
    int verbose;
};

/* Parse a single option. */
static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    struct arguments *arguments = state->input;
    char * endptr;

    switch (key) {

        case 'v':
            arguments->verbose = 1;
            break;

        case ARGP_KEY_ARG:
            switch (state->arg_num) {

                case 0:
                    arguments->block_size = strtoul(arg, &endptr, 10);
                    if (*endptr != '\0') {
                        /* failed to parse integer */
                        errx(EXIT_FAILURE, "SIZE must be an integer");
                    }
                    break;

                case 1:
                    arguments->raid_device = arg;
                    break;

                case 2:
                    arguments->server_device = arg;
                    break;
                default:
                    /* Too many arguments. */
                    return ARGP_ERR_UNKNOWN;
            }
            break;

        case ARGP_KEY_END:
            if (state->arg_num < 2) {
                warnx("not enough arguments");
                argp_usage(state);
            }
            break;

        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = {
    .options = options,
    .parser = parse_opt,
    .args_doc = "BLOCKSIZE LocalCache and RomoteServer",
    .doc = "BUSE implementation of local disk as cache for remote server.\n"
};

int main(int argc, char *argv[]) {
    struct arguments arguments = {
        .verbose = 0,
    };
    argp_parse(&argp, argc, argv, 0, 0, &arguments);
    struct buse_operations bop = {
        .read = cache_read,
        .write = cache_write
    };
    verbose = arguments.verbose;
    BLOCK_SIZE = arguments.block_size;
    //initial heap
    h=init_minheap(local_disk_size/BLOCK_SIZE);
    reBuildCache();

    pthread_t tid;
    pthread_create(&tid, NULL, thread_consumer, NULL);
    // pthread_detach(tid);

    char* dev_path = arguments.server_device;

    server_disk = open(dev_path,O_RDWR);
    if (server_disk < 0) {
        perror(dev_path);
        exit(1);
    } // if

    uint64_t size = lseek(server_disk,0,SEEK_END); // used to find device size by seeking to end
    fprintf(stderr, "Got device '%s', size %ld bytes.\n", dev_path, size);
    bop.size = size; // tell BUSE how big our block device is
    fprintf(stderr, "RAID device resulting size: %ld.\n", bop.size);
    int ret = buse_main(arguments.raid_device, &bop, NULL);
    printf("Hit_counter: %d\n", Hit_counter);
    return ret;
}
