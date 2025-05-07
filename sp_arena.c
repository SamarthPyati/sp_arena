#include "sp_arena.h"
#include <assert.h>
#include <string.h>
#include <stdint.h>

const sp_arena_config SP_ARENA_DEFAULT_CONFIG = {
    .block_size = SP_ARENA_DEFAULT_BLOCK_SIZE, 
    .alignment = SP_ARENA_DEFAULT_ALIGNMENT, 
    .fixed_size = false, 
    .allocator = SP_ARENA_DEFAULT_ALLOCATOR, 
    .deallocator = SP_ARENA_DEFAULT_DEALLOCATOR
};


static inline bool is_power_of_two(size_t n) {
    return (n != 0) && ((n & (n - 1))) == 0; 
}

/* Helper functions for alignment */
static inline size_t align_forward(size_t ptr, size_t align) {
    assert(is_power_of_two(align) && "Alignment must be a power of 2");
    
    size_t mask = align - 1;
    return (ptr + mask) & ~mask;
}

static inline void *align_forward_ptr(void *ptr, size_t align) {
    uintptr_t _ptr = (uintptr_t)(ptr);
    uintptr_t aligned = align_forward(_ptr, align);
    return (void *)aligned;
}

/* Create a new block for an arena */
static sp_arena_block* sp_arena_create_block(sp_arena *arena, size_t min_size) {
    size_t block_size = arena->config.block_size;

    // If requested size is larger than the default block_size, 
    // allocate a block big enough to fit it 
    if (min_size > block_size) {
        block_size = min_size;
        // Align to multiples of page size 
        block_size = align_forward(block_size, 4096);
    }

    void *memory = arena->config.allocator(block_size);
    if (!memory) {
        arena->last_err = SP_ARENA_ERR_OUT_OF_MEMORY;
        return NULL;
    }

    sp_arena_block *block = arena->config.allocator(sizeof(*block));
    if (!block) {
        arena->config.deallocator(memory);
        arena->last_err = SP_ARENA_ERR_OUT_OF_MEMORY;
        return NULL;
    }

    block->memory = memory;
    block->next = NULL;
    block->size = block_size;
    block->used = 0;

    arena->total_allocated += block_size;

    return block;
}

/* Initialize an arena with the default configuration */
sp_arena* sp_arena_create(void) {
    return sp_arena_create_with_config(SP_ARENA_DEFAULT_CONFIG);
}

/* Initialize an arena with a custom configuration */
sp_arena* sp_arena_create_with_config(sp_arena_config config) {
    sp_arena* arena = (sp_arena *)malloc(sizeof(*arena));
    if (!arena) return NULL;

    // Validating config 
    if (config.alignment == 0 || !is_power_of_two(config.alignment)) {
        free(arena);
        return NULL;
    }

    if (config.block_size == 0) {
        free(arena);
        return NULL;
    }

    // Custom allocator must come with custom deallocator and vice versa 
    if ((config.allocator != NULL && config.deallocator == NULL) ||
        (config.allocator == NULL && config.deallocator != NULL)) {
        free(arena);
        return NULL;
    }

    // Use default allocator/deallocator if none provided 
    if (config.allocator == NULL) {
        config.allocator = malloc;
        config.deallocator = free;
    }

    memset(arena, 0, sizeof(*arena));
    arena->config = config;

#if SP_ARENA_THREAD_SAFE 
    if (pthread_mutex_init(&arena->mutex, NULL) != 0) {
        free(arena);
        return NULL;
    }
#endif

    // Create first block 
    sp_arena_block *block = sp_arena_create_block(arena, 0);
    if (!block) {
        free(arena);
        arena->last_err = SP_ARENA_ERR_OUT_OF_MEMORY;
        return NULL;
    }

    arena->first = block;
    arena->current = block;
    return arena;
}

static void* sp_arena_alloc_internal(sp_arena* arena, size_t size, size_t alignment) {
    if (!arena || !arena->current || size == 0) {
        if (arena) {
            arena->last_err = size == 0 ? SP_ARENA_ERR_INVALID_SIZE : SP_ARENA_ERR_INVALID_ARENA;
        }
        return NULL;
    }

    if (!is_power_of_two(alignment)) {
        arena->last_err = SP_ARENA_ERR_INVALID_ALIGNMENT;
        return NULL;
    }

#if SP_ARENA_THREAD_SAFE 
    pthread_mutex_lock(&arena->mutex);
#endif 

    sp_arena_block *block = arena->current;

    // Align current used position 
    size_t aligned_used = align_forward(block->used, alignment);  // 3, 8 -> 8  
    size_t padding = aligned_used - block->used;                  // p -> 8 - 3 = 5
    size_t req_size = size + padding;                             // 2 + 5 = 7 
                                                                  // 8 + 2 = 10 < 64 
    // Check if there's enough space in current block
    if (aligned_used + size <= block->size) {
        // Have space in current block
        void* result = (char*)block->memory + aligned_used;
        block->used = aligned_used + size;
        arena->total_used += req_size;
        
    #if SP_ARENA_THREAD_SAFE
        pthread_mutex_unlock(&arena->mutex);
    #endif
        return result;
    }
    
    // Not enough space in current block
    if (arena->config.fixed_size) {
        // Fixed sized arena cannot create more blocks
        arena->last_err = SP_ARENA_ERR_OUT_OF_MEMORY;
    #if SP_ARENA_THREAD_SAFE 
        pthread_mutex_unlock(&arena->mutex);
    #endif 
        return NULL;
    }
    
    // Look for an existing next block with enough space if not fixed arena 
    sp_arena_block* next_block = block->next;
    while (next_block != NULL) {
        aligned_used = align_forward(next_block->used, alignment);
        padding = aligned_used - next_block->used;
        
        if (aligned_used + size <= next_block->size) {
            // Found a suitable existing block
            arena->current = next_block;
            void* result = (char*)next_block->memory + aligned_used;
            next_block->used = aligned_used + size;
            arena->total_used += size + padding;
            
        #if SP_ARENA_THREAD_SAFE
            pthread_mutex_unlock(&arena->mutex);
        #endif
            return result;
        }
        
        // Reset this block since we're skipping past it
        next_block->used = 0;
        next_block = next_block->next;
    }
    
    // If not existing block found then need to create a new block
    sp_arena_block* new_block = sp_arena_create_block(arena, size);
    if (!new_block) {
        arena->last_err = SP_ARENA_ERR_OUT_OF_MEMORY;
    #if SP_ARENA_THREAD_SAFE
        pthread_mutex_unlock(&arena->mutex);
    #endif
        return NULL;
    }
    
    // Link the new block
    new_block->next = block->next;
    block->next = new_block;
    arena->current = new_block;
    
    // Allocate from the new block
    void* result = new_block->memory; // No alignment needed for fresh block
    new_block->used = size;
    arena->total_used += size;
    
#if SP_ARENA_THREAD_SAFE
    pthread_mutex_unlock(&arena->mutex);
#endif

    return result;
}

/* Allocate memory from an arena */
void *sp_arena_alloc(sp_arena *arena, size_t size) {
    return sp_arena_alloc_internal(arena, size, arena->config.alignment);
}   

/* Allocate aligned memory from an arena */
void *sp_arena_alloc_aligned(sp_arena *arena, size_t size, size_t alignment) {
    return sp_arena_alloc_internal(arena, size, alignment);
}

/* Allocate zero-initialized memory from an arena */
void *sp_arena_calloc(sp_arena *arena, size_t size) {
    void *result = sp_arena_alloc(arena, size);
    if (result) memset(result, 0, size);
    return result;
}

/* Resize an allocation from an arena */
void *sp_arena_resize(sp_arena *arena, void *old_ptr, size_t old_size, size_t new_size) {
    if (!arena || !old_ptr || old_size == 0 || new_size == 0) {
        if (arena) arena->last_err = SP_ARENA_ERR_INVALID_SIZE;
        return NULL;
    }
    
#if SP_ARENA_THREAD_SAFE
    pthread_mutex_lock(&arena->mutex);
#endif

    sp_arena_block *block = arena->current;
    if (!block) {
        arena->last_err = SP_ARENA_ERR_INVALID_ARENA;
#if SP_ARENA_THREAD_SAFE
        pthread_mutex_unlock(&arena->mutex);
#endif
        return NULL;
    }

    // Check if old_ptr is at the end of the current block
    char *block_end = (char*)block->memory + block->used - old_size;
    if ((char*)old_ptr != block_end) {
        // Not the last allocation, need to allocate new memory
        void *new_ptr = sp_arena_alloc(arena, new_size);
        if (new_ptr) {
            // Copy old data to new location
            size_t copy_size = old_size < new_size ? old_size : new_size;
            memcpy(new_ptr, old_ptr, copy_size);
        }
        
#if SP_ARENA_THREAD_SAFE
        pthread_mutex_unlock(&arena->mutex);
#endif
        return new_ptr;
    }
    
    // We can resize in place
    if (new_size > old_size) {
        size_t additional = new_size - old_size;
        if (block->used + additional > block->size) {
            // Not enough space, allocate new memory
            if (arena->config.fixed_size) {
                arena->last_err = SP_ARENA_ERR_OUT_OF_MEMORY;
#if SP_ARENA_THREAD_SAFE
                pthread_mutex_unlock(&arena->mutex);
#endif
                return NULL;
            }
            
            void *new_ptr = sp_arena_alloc(arena, new_size);
            if (new_ptr) {
                // Copy old data
                memcpy(new_ptr, old_ptr, old_size);
                // Adjust the old block's used size
                block->used -= old_size;
                arena->total_used -= old_size;
            }
            
#if SP_ARENA_THREAD_SAFE
            pthread_mutex_unlock(&arena->mutex);
#endif
            return new_ptr;
        }
    }
    
    // Adjust block size
    block->used = block->used - old_size + new_size;
    arena->total_used = arena->total_used - old_size + new_size;
    
#if SP_ARENA_THREAD_SAFE
    pthread_mutex_unlock(&arena->mutex);
#endif
    return old_ptr;
}

/* Allocate and copy a string into an arena */
char *sp_arena_strdup(sp_arena *arena, const char *str) {
    if (!str) return NULL;

    size_t len = strlen(str) + 1;
    char *dup = sp_arena_alloc(arena, len);
    if (dup) memcpy(dup, str, len);
    return dup;
}

/* Create a temporary checkpoint for the arena */ 
sp_arena_temp sp_arena_temp_begin(sp_arena *arena) {
    sp_arena_temp temp;
    temp.arena = arena;

    if (!arena || !arena->current) {
        temp.block = NULL;
        temp.total_used = 0;
        temp.used = 0;
        return temp;
    }
    
#if SP_ARENA_THREAD_SAFE
    pthread_mutex_lock(&arena->mutex);
#endif

    temp.block = arena->current;
    temp.used = arena->current->used;
    temp.total_used = arena->total_used;
    
#if SP_ARENA_THREAD_SAFE
    pthread_mutex_unlock(&arena->mutex);
#endif

    return temp;
}

void sp_arena_temp_end(sp_arena_temp temp) {
    sp_arena *arena = temp.arena;
    if (!arena || !temp.block) return;
#if SP_ARENA_THREAD_SAFE
    pthread_mutex_lock(&arena->mutex);
#endif

    // Restore the arena to the state at the temporary checkpoint 
    arena->current = temp.block;
    
    // Reset the used amount of all blocks after the checkpoint block 
    sp_arena_block* block = temp.block;
    block->used = temp.used;
    
    arena->total_used = temp.total_used;
    
#if SP_ARENA_THREAD_SAFE
    pthread_mutex_unlock(&arena->mutex);
#endif
}

/* Clear arena, keeping its memory for reuse */ 
void sp_arena_clear(sp_arena *arena) {
    if (!arena) return;
#if SP_ARENA_THREAD_SAFE
    pthread_mutex_lock(&arena->mutex);
#endif

    sp_arena_block *block = arena->first;
    while (block) {
        block->used = 0;
        block = block->next;
    }

    arena->current = arena->first;
    arena->total_used = 0;

#if SP_ARENA_THREAD_SAFE
    pthread_mutex_unlock(&arena->mutex);
#endif
}

void sp_arena_destroy(sp_arena *arena) {
    if (!arena) return;
#if SP_ARENA_THREAD_SAFE
    pthread_mutex_lock(&arena->mutex);
#endif

    sp_arena_block *block = arena->first;
    while (block) {
        sp_arena_block *next = block->next;
        arena->config.deallocator(block->memory);
        arena->config.deallocator(block);
        block = next;
    }

    arena->first = NULL;
    arena->current = NULL;
    arena->total_allocated = 0;
    arena->total_used = 0;

#if SP_ARENA_THREAD_SAFE
    pthread_mutex_unlock(&arena->mutex);
    pthread_mutex_destroy(&arena->mutex);
#endif

    free(arena);
}

sp_arena_err_t sp_arena_get_last_error(const sp_arena *arena) {
    if (!arena) {
        return SP_ARENA_ERR_INVALID_ARENA;
    } 
    return arena->last_err;
}

const char *sp_arena_error_string(sp_arena_err_t error) {
    switch (error)
    {
    case SP_ARENA_ERR_NONE:
        return "No error";
    case SP_ARENA_ERR_ARENA_NOT_ALLOCATED:
        return "Failed to allocate arena";
    case SP_ARENA_ERR_OUT_OF_MEMORY: 
        return "Out of Memory";
    case SP_ARENA_ERR_INVALID_ALIGNMENT: 
        return "Invalid Alignment";
    case SP_ARENA_ERR_INVALID_SIZE: 
        return "Invalid size";
    case SP_ARENA_ERR_INVALID_ARENA: 
        return "Invalid arena";
    case SP_ARENA_ERR_ALLOCATION_TOO_LARGE: 
        return "Allocation too large"; 
    default:        
        return "Unknown error";
    }
}

/* Get the total amount of memory allocated to the arena */
size_t sp_arena_total_allocated(const sp_arena *arena) {
    if (!arena) {
        return 0;
    }
    return arena->total_allocated;
}

/* Get the total amount of memory used in the arena */
size_t sp_arena_total_used(const sp_arena *arena) {
    if (!arena) {
        return 0;
    }
    return arena->total_used;
}

/* Get the utilization ratio of the arena (used/allocated)*/
float sp_arena_utilization(const sp_arena *arena) {
    if (!arena || arena->total_allocated == 0) {
        return 0.0f;
    }
    return (float)arena->total_used / (float)arena->total_allocated;
    }

/* Get the report of memory utilisation by arena. */ 
void sp_arena_usage_report(const sp_arena *arena) {
    printf("Total allocated: %zu bytes\n", sp_arena_total_allocated(arena));
    printf("Total used: %zu bytes\n", sp_arena_total_used(arena));
    printf("Utilization: %.2f%%\n", sp_arena_utilization(arena) * 100.0f);
}