/**
 * @file example.c - Example usage of the arena allocator
 */

#include "../sp_arena.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
 
typedef struct {
    int id;
    char name[32];
    float value;
} TestItem;

void basic_usage_example() {
    printf("\n=== Basic Usage Example ===\n");
    
    sp_arena *arena = sp_arena_create();
    
    // Basic allocations 
    int* int_array = sp_arena_alloc_array(arena, int, 100);
    for (int i = 0; i < 100; i++) {
        int_array[i] = i * 10;
    }
    
    TestItem* item = sp_arena_alloc_type(arena, TestItem);
    item->id = 42;
    strcpy(item->name, "Test Item");
    item->value = 3.14f;
    
    // String duplication 
    char* message = sp_arena_strdup(arena, "Hello from arena allocator!");
    
    // Print values to show they're valid 
    printf("int_array[5] = %d\n", int_array[5]);
    printf("item: id=%d, name=%s, value=%f\n", item->id, item->name, item->value);
    printf("message: %s\n", message);
    
    // Array of structs 
    TestItem* items = sp_arena_alloc_array(arena, TestItem, 5);
    for (int i = 0; i < 5; i++) {
        items[i].id = i;
        snprintf(items[i].name, sizeof(items[i].name), "Item %d", i);
        items[i].value = i * 1.5f;
        
        printf("items[%d]: id=%d, name=%s, value=%f\n", 
               i, items[i].id, items[i].name, items[i].value);
    }

    sp_arena_usage_report(arena);
    // Cleanup 
    sp_arena_destroy(arena);
}

void temp_arena_example() {
    printf("\n=== Temporary Arena Example ===\n");
    
    sp_arena_config config = {
        .alignment = SP_ARENA_DEFAULT_ALIGNMENT, 
        .allocator = SP_ARENA_DEFAULT_ALLOCATOR, 
        .block_size = KB(1), 
        .deallocator = SP_ARENA_DEFAULT_DEALLOCATOR, 
        .fixed_size = false
    };

    sp_arena *arena = sp_arena_create_with_config(config);
    
    // Allocate some initial data 
    int* permanent_data = sp_arena_alloc_array(arena, int, 5);
    for (int i = 0; i < 5; i++) {
        permanent_data[i] = i;
    }
    
    printf("Initial data: ");
    for (int i = 0; i < 5; i++) {
        printf("%d ", permanent_data[i]);
    }
    printf("\n");
    
    // Begin a temporary scope 
    sp_arena_temp_scope(arena) {
        /* Allocate temporary data */
        int* temp_data = sp_arena_alloc_array(arena, int, 10);
        for (int i = 0; i < 10; i++) {
            temp_data[i] = 100 + i;
        }
        
        printf("After temp allocations - permanent: ");
        for (int i = 0; i < 5; i++) {
            printf("%d ", permanent_data[i]);
        }
        printf(", temp: ");
        for (int i = 0; i < 10; i++) {
            printf("%d ", temp_data[i]);
        }
        printf("\n");
    }
    /* temp_data is no longer valid (would cause undefined behavior if accessed) */
    
    /* The original data is still valid */
    printf("After temp_end - permanent: ");
    for (int i = 0; i < 5; i++) {
        printf("%d ", permanent_data[i]);
    }
    printf("\n");
    
    /* Allocate more data after rewinding */
    int* more_data = sp_arena_alloc_array(arena, int, 3);
    for (int i = 0; i < 3; i++) {
        more_data[i] = 200 + i;
    }
    
    printf("New allocations after temp_end: ");
    for (int i = 0; i < 3; i++) {
        printf("%d ", more_data[i]);
    }
    printf("\n");
    
    // Show memory usage 
    sp_arena_usage_report(arena);
    sp_arena_destroy(arena);
}

int main(void) {
    basic_usage_example();
    temp_arena_example();
    return 0;
}
