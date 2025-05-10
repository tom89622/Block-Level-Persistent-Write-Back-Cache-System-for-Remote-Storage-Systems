#include <stdio.h>
#include <stdlib.h>
#include "min_heap.h"
#include <pthread.h>

// Helper functions for finding parent and children indices
int parent(int i) {
    return (i - 1) / 2;
}

int left_child(int i) {
    return (2 * i + 1);
}

int right_child(int i) {
    return (2 * i + 2);
}

// Initialize a new min heap
MinHeap* init_minheap(int capacity) {
    MinHeap* heap = (MinHeap*)calloc(1, sizeof(MinHeap));
    heap->arr = (HeapNode*)calloc(capacity, sizeof(HeapNode));
    // heap->map = (int*)calloc(capacity, sizeof(int));
    heap->capacity = capacity;
    heap->size = 0;
    for (int i = 0; i < capacity; i++) {
        heap->arr[i].dirty_flag = 0; // Initialize dirty flag to 0
    } // for
    pthread_mutex_destroy(&(heap->lock));
    pthread_mutex_init(&(heap->lock), NULL);

    return heap;
}

// Swap two nodes in the heap and update the map
void swap(MinHeap* heap, int i, int j) {
    // Swap the nodes
    HeapNode temp = heap->arr[i];
    heap->arr[i] = heap->arr[j];
    heap->arr[j] = temp;
    
    // Update the map
    // heap->map[heap->arr[i].value] = i;
    // heap->map[heap->arr[j].value] = j;
}

// Get the minimum key node without removing it
HeapNode get_min(MinHeap* heap) {
    if (heap->size <= 0) {
        HeapNode empty = {-1, -1.0};
        return empty;
    }
    return heap->arr[0];
}

// Insert a new node into the heap
void insert(MinHeap* heap, int value, float key) {
    if (heap->size >= heap->capacity) {
        printf("Heap overflow! Cannot insert.\n");
        return;
    }
    
    // Add the new node at the end
    int curr = heap->size;
    heap->arr[curr].value = value;
    heap->arr[curr].key = key;
    // heap->map[value] = curr;
    heap->size++;
    
    // Bubble up to maintain min-heap property
    while (curr > 0 && heap->arr[parent(curr)].key > heap->arr[curr].key) {
        swap(heap, curr, parent(curr));
        curr = parent(curr);
    }
}

// Heapify the subtree rooted at index i
void heapify(MinHeap* heap, int i) {
    int smallest = i;
    int left = left_child(i);
    int right = right_child(i);
    
    if (left < heap->size && heap->arr[left].key < heap->arr[smallest].key)
        smallest = left;
    
    if (right < heap->size && heap->arr[right].key < heap->arr[smallest].key)
        smallest = right;
    
    if (smallest != i) {
        swap(heap, i, smallest);
        heapify(heap, smallest);
    }
}

// Extract the minimum key node from the heap
HeapNode extract_min(MinHeap* heap) {
    if (heap->size <= 0) {
        HeapNode empty = {-1, -1.0};
        return empty;
    }
    
    HeapNode min_node = heap->arr[0];
    
    // Replace root with last element
    heap->arr[0] = heap->arr[heap->size - 1];
    // heap->map[heap->arr[0].value] = 0;
    heap->size--;
    
    // Heapify the root
    heapify(heap, 0);
    
    return min_node;
}

// Find the index of a specific value in the heap
int find_value(MinHeap* heap, int value) {

    // Check if the value is within the valid range // we should do something like hash to make us 
    if (value < 0 || heap->size == 0) {
        printf("out of range in find value\n");
        return -1; // Value out of range
    }
    
    // // Get the index from the map
    // int index = heap->map[value];
    
    // // Verify that the index is valid and the value at that index matches
    // if (index >= 0 && index < heap->size && heap->arr[index].value == value) {
    //     return index;
    // }
    
    for (int i = 0; i < heap->size; i++) {
        if (heap->arr[i].value == value) {
            return i;
        } // if

    } // for

    //printf("not found in find value\n");
    // Value not found
    return -1;
}

// Remove a specific value from the heap
HeapNode remove_value(MinHeap* heap, int value) {
    // Find the index of the value to remove
    int index = find_value(heap, value);

    if (index<0 || heap->size <= 0) {
        HeapNode empty = {-1, -1.0};
        return empty;
    }
    
    // Store the node to return
    HeapNode removed_node = heap->arr[index];
    
    // Replace with the last element
    heap->arr[index] = heap->arr[heap->size - 1];
    // heap->map[heap->arr[index].value] = index;
    heap->size--;
    
    // If the element was already the last one, we're done
    if (index == heap->size)
        return removed_node;
    
    // Check if we need to bubble up or down
    if (index > 0 && heap->arr[index].key < heap->arr[parent(index)].key) {
        // Bubble up if the new element is smaller than its parent
        int curr = index;
        while (curr > 0 && heap->arr[parent(curr)].key > heap->arr[curr].key) {
            swap(heap, curr, parent(curr));
            curr = parent(curr);
        }
    } else {
        // Otherwise, heapify down
        heapify(heap, index);
    }


    printf("removed node: %i, %i (key, value)", removed_node.key, removed_node.value);
    printf("heap size after remove: %i\n", heap->size);

    return removed_node;
}

// Increase the key of a node with given value
void increase_key(MinHeap* heap, int value, float new_key) {
    // Find the index of the node with the given value
    int i = find_value(heap, value);
    
    if (i > heap->size || i < 0) {
        printf("Cannot find the key of value (%i) in index (%i)\n", value, i);
        return;
    } // if

    /*
    printf("i: %i, value: %i\n", i, value);
    printf("new: %i, old: %i\n", new_key, heap->arr[i].key);
    // Check if new key is larger than current key
    if (new_key <= heap->arr[i].key) {
        printf("New key is smaller than current key. Cannot increase.\n");
        return;
    }
    */
   
    // Update the key
    heap->arr[i].key = new_key;
    
    // Since this is a min-heap, increasing a key might violate the heap property
    // We need to push this node down (heapify)
    heapify(heap, i);
}

// Print the heap
void print_heap(MinHeap* heap) {
    printf("Min Heap (value, key):\n");
    for (int i = 0; i < heap->size; i++) {
        printf("(%d, %d) ", heap->arr[i].value, heap->arr[i].key);
    }
    printf("\n");
}

// Free the heap
void free_minheap(MinHeap* heap) {
    // Destroy the mutex
    pthread_mutex_destroy(&(heap->lock));
    
    free(heap->arr);
    // free(heap->map);
    free(heap);
}
