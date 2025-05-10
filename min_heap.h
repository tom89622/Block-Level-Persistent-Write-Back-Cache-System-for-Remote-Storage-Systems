// min_heap.h
#ifndef MIN_HEAP_H
#define MIN_HEAP_H

#include <pthread.h>  // Add pthread support

// Structure definitions
typedef struct HeapNode {
    long value;    // The actual data stored // block number
    __time_t key;    // Key used for heap ordering // time
    int dirty_flag; // Flag to indicate if the node is dirty
} HeapNode;

typedef struct MinHeap {
    HeapNode* arr;  // Array to store heap nodes
    int size;       // Current size of heap
    int capacity;   // Maximum capacity of heap
    int* map;       // Maps value to its position in heap
    pthread_mutex_t lock;  // Mutex for thread safety
} MinHeap;

// Function declarations

// Helper functions for finding parent and children indices
int parent(int i);
int left_child(int i);
int right_child(int i);

// Initialize a new min heap
MinHeap* init_minheap(int capacity);

// Swap two nodes in the heap and update the map
void swap(MinHeap* heap, int i, int j);

// Get the minimum key node without removing it
HeapNode get_min(MinHeap* heap);

// Insert a new node into the heap
void insert(MinHeap* heap, int value, float key);

// Heapify the subtree rooted at index i
void heapify(MinHeap* heap, int i);

// Extract the minimum key node from the heap
HeapNode extract_min(MinHeap* heap);

// Find the index of a specific value in the heap
int find_value(MinHeap* heap, int value);

// Remove a specific value from the heap
HeapNode remove_value(MinHeap* heap, int value);

// Increase the key of a node with given value
void increase_key(MinHeap* heap, int value, float new_key);

// Print the heap
void print_heap(MinHeap* heap);

// Free the heap
void free_minheap(MinHeap* heap);

#endif // MIN_HEAP_H

