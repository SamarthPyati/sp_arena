/*
 * @brief: spar.h - A robust dynamic arena allocator
 * 
 * @ref:  implementation is based on principles from below article:
 *        "Untangling Lifetimes: The Arena Allocator"
 * 
 * Features:
 * - Block-based allocation with dynamic growth
 * - Proper alignment handling
 * - Temporary arena scopes with rewinding
 * - Arena reuse through clearing
 * - Thread safety options
 * - Clear API for arena creation, allocation, and destruction
 * 
 * @author: Samarth Pyati
 * @date: 4-05-2025
 * @version 1.0
 * 
 * License: MIT
 */

#ifndef SP_ARENA_H_ 
#define SP_ARENA_H_ 

#include <stdio.h>
#include <stdbool.h>

/* Data sizes */
#define KB(n) (1024 *   ((uint32_t)(n)))
#define MB(n) (1024 * KB((uint32_t)(n)))
#define GB(n) (1024 * MB((uint32_t)(n)))
#define TB(n) (1024 * GB((uint32_t)(n)))


/* Configurations */
#ifndef SP_ARENA_DEFAULT_BLOCK_SIZE 
#define SP_ARENA_DEFAULT_BLOCK_SIZE (KB(64))
#endif

#ifndef SP_ARENA_DEFAULT_ALIGNMENT 
#define SP_ARENA_DEFAULT_ALIGNMENT (sizeof(void*))
#endif

#ifndef SP_ARENA_THREAD_SAFE 
#define SP_ARENA_THREAD_SAFE 1
#endif


#ifdef SP_ARENA_THREAD_SAFE
#include <pthread.h>
#endif

/* Common useful macros */
#define Stmt(s) do { s } while (0);
#define ArrayLen(a) (sizeof((a)) / sizeof(*(a)))

#define Unused(x) (void)(x);

/* Error Handling */
typedef enum {
    SP_ARENA_ERR_NONE = 0, 
    SP_ARENA_ERR_OUT_OF_MEMORY, 
    SP_ARENA_ERR_INVALID_ALIGNMENT, 
    SP_ARENA_ERR_INVALID_SIZE, 
    SP_ARENA_ERR_INVALID_ARENA, 
    SP_ARENA_ERR_ARENA_NOT_ALLOCATED, 
    SP_ARENA_ERR_ALLOCATION_TOO_LARGE
} sp_arena_err_t;

typedef struct sp_arena             sp_arena;
typedef struct sp_arena_block       sp_arena_block;
typedef struct sp_arena_config      sp_arena_config;
typedef struct sp_arena_temp        sp_arena_temp;

/* Arena block */
struct sp_arena_block {
    void *memory;                   /* Pointer to block's memory region */
    size_t size;                    /* Size of block */
    size_t used;                    /* Memory utilised */
    sp_arena_block *next;           /* Pointer to next block */
};

/* Arena config struct */
struct sp_arena_config {
    size_t block_size;              /* Size of each block */
    size_t alignment;               /* Default alignment for allocations */
    bool fixed_size;                /* If true, don't allocate additional blocks (arena with fixed size)*/
    void *(*allocator)(size_t);     /* Custom allocator */
    void (*deallocator)(void*);      /* Custom deallocator */
};

/* Main arena struct */
struct sp_arena
{   
    sp_arena_block *first;          /* First block in list */
    sp_arena_block *current;        /* Current block being allocated from */
    size_t total_allocated;         /* Total memory allocated to the arena */
    size_t total_used;              /* Total memory used */
    sp_arena_config config;         /* Config for arena */
    sp_arena_err_t last_err;        /* Last error for arena */

#ifdef SP_ARENA_THREAD_SAFE
    pthread_mutex_t mutex;          /* Mutex for thread safe */
#endif
};

/* Temp arena for rewinding */
struct sp_arena_temp {
    sp_arena *arena;                /* Arena this temporary scope is for */
    sp_arena_block *block;          /* Block at beginning of temporary scope */
    size_t used;                    /* Used amount at beginning of temporary scope */
    size_t total_used;              /* Total used amount at beginning of scope */
};

/* Default arena config */
extern const sp_arena_config SP_ARENA_DEFAULT_CONFIG;

/**
 * Initialize an arena with the default configuration.
 * 
 * @param arena Pointer to an arena structure to initialize
 * @return true on success, false on failure
 */
sp_arena* sp_arena_create(void);

/**
 * Initialize an arena with the custom configuration.
 * 
 * @param arena Pointer to an arena structure to initialize
 * @param config Custom configuration struct 
 * @return true on success, false on failure
 */
sp_arena* sp_arena_create_with_config(sp_arena_config config);

/**
 * Allocate memory from an arena.
 * 
 * @param arena Pointer to the arena to allocate from
 * @param size Size of the allocation in bytes
 * @return Pointer to the allocated memory, or NULL on failure
 */
void *sp_arena_alloc(sp_arena *arena, size_t size);

/**
 * Allocate memory from an arena with a specific alignment.
 * 
 * @param arena Pointer to the arena to allocate from
 * @param size Size of the allocation in bytes
 * @param alignment Alignment of the allocation (must be a power of 2)
 * @return Pointer to the allocated memory, or NULL on failure
 */
void *sp_arena_alloc_aligned(sp_arena *arena, size_t size, size_t alignment);

/**
 * Allocate and zero-initialize memory from an arena.
 * 
 * @param arena Pointer to the arena to allocate from
 * @param size Size of the allocation in bytes
 * @return Pointer to the allocated and zeroed memory, or NULL on failure
 */
void *sp_arena_calloc(sp_arena *arena, size_t size);

/**
 * Resize an allocation made from an arena. This only works if the allocation
 * is the most recent one made from the arena.
 * 
 * @param arena Pointer to the arena
 * @param old_ptr Pointer to the existing allocation
 * @param old_size Size of the existing allocation
 * @param new_size New size for the allocation
 * @return Pointer to the resized allocation, or NULL on failure
 */
void *sp_arena_resize(sp_arena *arena, void *old_ptr, size_t old_size, size_t new_size); 

/**
 * Allocate and copy a string into an arena.
 * 
 * @param arena Pointer to the arena
 * @param str String to duplicate
 * @return Pointer to the duplicated string, or NULL on failure
 */
char *sp_arena_strdup(sp_arena *arena, const char *str);

/**
 * Create a temporary checkpoint for the arena that can be rewound later.
 * 
 * @param arena Pointer to the arena
 * @return Temporary arena scope object
 */
sp_arena_temp sp_arena_temp_begin(sp_arena *arena);

/**
 * Rewind an arena to a previous state.
 * 
 * @param temp Temporary arena scope to rewind to
 */
void sp_arena_temp_end(sp_arena_temp temp);

/**
 * Clear an arena, keeping its memory for reuse.
 * 
 * @param arena Pointer to the arena to clear
 */
void sp_arena_clear(sp_arena *arena);

/**
 * Free all memory associated with an arena.
 * 
 * @param arena Pointer to the arena to destroy
 */
void sp_arena_destroy(sp_arena *arena);

/**
 * Get the last error that occurred in an arena.
 * 
 * @param arena Pointer to the arena
 * @return The last error code
 */
sp_arena_err_t sp_arena_get_last_error(const sp_arena *arena);

/**
 * Get a string description of an arena error code.
 * 
 * @param error Error code
 * @return String description of the error
 */
const char *sp_arena_error_string(sp_arena_err_t error);

/**
 * Get the total amount of memory allocated to the arena.
 * 
 * @param arena Pointer to the arena
 * @return Total size of all blocks in bytes
 */
size_t sp_arena_total_allocated(const sp_arena *arena);

/**
 * Get the total amount of memory used in the arena.
 * 
 * @param arena Pointer to the arena
 * @return Total used memory in bytes
 */
size_t sp_arena_total_used(const sp_arena *arena);

/**
 * Get the utilization ratio of the arena (used/allocated).
 * 
 * @param arena Pointer to the arena
 * @return Utilization ratio between 0.0 and 1.0
 */
float sp_arena_utilization(const sp_arena *arena);

/**
 * Helper macro to allocate an object of a specific type.
 */
#define sp_arena_alloc_type(arena, type) ((type*) sp_arena_alloc(arena, sizeof(type)))

/**
 * Helper macro to allocate an array of objects of a specific type.
 */
#define sp_arena_alloc_array(arena, type, count) ((type*) sp_arena_alloc(arena, sizeof(type) * (count)))


#endif  // SP_ARENA_H_ 
