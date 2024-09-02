#ifndef MAXLLOC_H
#define MAXLLOC_H

#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

// memory protection and mapping flags for mmap
#define MAXLLOC_MEMORY_PROTECTION (PROT_READ | PROT_WRITE)
#define MAXLLOC_MEMORY_FLAGS (MAP_PRIVATE | MAP_ANONYMOUS)

// header for allocated memory blocks
typedef union _MaxllocHeader
{
    uintptr_t alignment;   // alignment requirement
    size_t block_size;     // size of the allocated memory block
} MaxllocHeader;

/**
 * rounds up the requested size to the nearest page size
 * adds space for metadata (MaxllocHeader) and aligns to the system page size
 */
static size_t _maxlloc_round_up_to_page_size(size_t size) 
{
    size_t page_size = getpagesize() - 1;
    size += sizeof(MaxllocHeader);
    size += page_size;
    size &= ~page_size; // align size to the next page boundary
    return size;
}

/**
 * allocates a block of memory of the specified size
 * uses mmap to allocate memory
 */
void *maxlloc(size_t size) 
{
    if (size == 0) 
    {
        return NULL; // invalid size request
    }

    size = _maxlloc_round_up_to_page_size(size); // adjust size to page boundaries
    MaxllocHeader *header = mmap(NULL, size, MAXLLOC_MEMORY_PROTECTION, MAXLLOC_MEMORY_FLAGS, -1, 0); // allocate memory

    if (header == MAP_FAILED) 
    {
        return NULL; // allocation failed
    }

    header->block_size = size; // store allocated size in metadata
    return (void *)(header + 1); // return pointer after metadata
}

/**
 * frees the previously allocated memory block
 * uses munmap to securely release memory back to the os
 */
void free(void *ptr) 
{
    if (ptr == NULL) 
    {
        return; // invalid pointer
    }

    MaxllocHeader *header = (MaxllocHeader *)ptr - 1; // retrieve metadata
    munmap(header, header->block_size); // unmap the memory
}

/**
 * allocates a zero-initialized memory block
 * equivalent to calloc using secure maxlloc
 */
void *calloc(size_t count, size_t size) 
{
    size_t total_size = count * size;
    void *ptr = maxlloc(total_size);
    if (ptr) 
    {
        memset(ptr, 0, total_size); // zero-initialize allocated memory
    }
    return ptr;
}

/**
 * reallocates a previously allocated memory block to a new size
 * uses mmap to allocate new memory if necessary, and copies old data
 */
void *realloc(void *ptr, size_t size) 
{
    if (size == 0) 
    { 
        // equivalent to free when size is 0
        free(ptr);
        return NULL;
    }

    if (ptr == NULL) 
    {
        return maxlloc(size); // equivalent to malloc when ptr is NULL
    }

    MaxllocHeader *old_header = (MaxllocHeader *)ptr - 1;
    size_t old_size = old_header->block_size;
    size_t new_size = _maxlloc_round_up_to_page_size(size);

    if (new_size <= old_size) 
    {
        return ptr; // no need to reallocate if size is smaller or equal
    }

    // allocate new memory block
    MaxllocHeader *new_header = mmap(NULL, new_size, MAXLLOC_MEMORY_PROTECTION, MAXLLOC_MEMORY_FLAGS, -1, 0);
    if (new_header == MAP_FAILED) 
    {
        return NULL; // allocation failed
    }

    // copy old data to the new block and free the old block
    memcpy(new_header + 1, ptr, old_size - sizeof(MaxllocHeader));
    new_header->block_size = new_size;
    free(ptr);

    return (void *)(new_header + 1);
}

#endif // MAXLLOC_H
