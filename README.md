# sp_arena

A robust lightweight memory arena allocator library written in C. It provides efficient memory management through arena allocation, making it ideal for applications that need to handle many small allocations with minimal overhead and fragmentation.

## Features

- **Arena-based Memory Management**: Allocates memory in large blocks and subdivides it as needed
- **Multiple Allocation Methods**: Standard, aligned, and zero-initialized allocation
- **Temporary Arenas**: Create temporary checkpoints for scoped memory management
- **Customizable Configuration**: Configure block size, alignment, and custom allocators
- **Fixed-Size Option**: Constrain arenas to a fixed size when needed
- **Thread Safety Option**: Optional thread safety support

## Basic Usage

```c
#include "sp_arena.h"
#include <stdio.h>

int main() {
    // Create an arena with default configuration
    sp_arena *arena = sp_arena_create();
    
    // Allocate memory from the arena
    int *numbers = sp_arena_alloc(arena, 10 * sizeof(int));
    
    // Use the memory...
    for (int i = 0; i < 10; i++) {
        numbers[i] = i;
    }

    // Create a string in the arena
    char *greeting = sp_arena_strdup(arena, "Hello, Arena!");
    printf("%s\n", greeting);
    
    // Clean up all memory with a single call
    sp_arena_destroy(arena);
    
    return 0;
}
```

## Advanced Usage

### Custom Configuration

```c
sp_arena_config config = {
    .block_size = 4096,         // 4KB blocks
    .alignment = 8,             // 8-byte alignment
    .fixed_size = false,        // Allow expanding
    .allocator = custom_malloc, // Custom allocator function
    .deallocator = custom_free  // Custom deallocator function
};

sp_arena *arena = sp_arena_create_with_config(config);
```

### Temporary Arenas

```c
// Begin a temporary arena scope
sp_arena_temp temp = sp_arena_temp_begin(arena);

// Allocate temporary memory
char *temp_data = sp_arena_alloc(arena, 1000);

// ... use the temporary memory ...

// End the temporary scope, freeing the memory
sp_arena_temp_end(temp);
```

**OR** with a convenient macro 

```c
// Create a temporary memory scope 
sp_arena_temp_scope(arena) {
    // All the memory allocations done within this scope will be temporary 
    char *temp_data = sp_arena_alloc(arena, 1000);
}

```

### Aligned Allocation

```c
// Allocate memory with 16-byte alignment
void *aligned_mem = sp_arena_alloc_aligned(arena, 100, 16);
```

## API Reference

### Creation and Destruction

- `sp_arena *sp_arena_create(void)` - Create an arena with default configuration
- `sp_arena *sp_arena_create_with_config(sp_arena_config config)` - Create an arena with custom configuration
- `void sp_arena_destroy(sp_arena *arena)` - Destroy an arena and free all memory

### Memory Allocation

- `void *sp_arena_alloc(sp_arena *arena, size_t size)` - Allocate memory from the arena
- `void *sp_arena_alloc_aligned(sp_arena *arena, size_t size, size_t alignment)` - Allocate aligned memory
- `void *sp_arena_calloc(sp_arena *arena, size_t size)` - Allocate zero-initialized memory
- `void *sp_arena_resize(sp_arena *arena, void *old_ptr, size_t old_size, size_t new_size)` - Resize an allocation
- `char *sp_arena_strdup(sp_arena *arena, const char *str)` - Duplicate a string into the arena

### Temporary Arenas

- `sp_arena_temp sp_arena_temp_begin(sp_arena *arena)` - Begin a temporary arena scope
- `void sp_arena_temp_end(sp_arena_temp temp)` - End a temporary arena scope

### Maintenance

- `void sp_arena_clear(sp_arena *arena)` - Clear arena, keeping memory allocated for reuse
- `sp_arena_err_t sp_arena_get_last_error(const sp_arena *arena)` - Get the last error
- `const char *sp_arena_error_string(sp_arena_err_t error)` - Get error message string

### Statistics

- `size_t sp_arena_total_allocated(const sp_arena *arena)` - Get total memory allocated
- `size_t sp_arena_total_used(const sp_arena *arena)` - Get total memory used
- `float sp_arena_utilization(const sp_arena *arena)` - Get memory utilization ratio

## Error Handling

```c
// Check for errors after allocation
void *ptr = sp_arena_alloc(arena, 1000);
if (!ptr) {
    sp_arena_err_t err = sp_arena_get_last_error(arena);
    printf("Error: %s\n", sp_arena_error_string(err));
}
```

## Thread Safety

To enable thread safety, define `SP_ARENA_THREAD_SAFE` before including the header:

```c
#define SP_ARENA_THREAD_SAFE 1
#include "sp_arena.h"
```

> [!NOTE]  
> This requires pthread support on your platform.

## License

This project is licensed under the MIT License - see the [LICENSE.md](./LICENSE) file for details

## Acknowledgements
    - Inspiration taken from Ryan Fluery`s Article: [untangling-lifetimes-the-arena-allocator](https://www.rfleury.com/p/untangling-lifetimes-the-arena-allocator).