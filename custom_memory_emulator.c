#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <gtest/gtest.h>

#define MEMORY_POOL_SIZE (1 * 1024 * 1024)  // 1MB

// 메모리 풀
static uint8_t memory_pool[MEMORY_POOL_SIZE];
static size_t memory_pool_offset = 0;

// 메모리 엔트리 구조체
typedef struct MemoryEntry {
    void* ptr;
    size_t size;
    struct MemoryEntry* next;
} MemoryEntry;

// 메모리 테이블 (리스트의 헤드)
static MemoryEntry* memory_table = NULL;

// 전체 할당된 메모리
static size_t total_allocated = 0;

// 메모리 풀에서 메모리 할당
void* pool_malloc(size_t size) {
    if (memory_pool_offset + size > MEMORY_POOL_SIZE) {
        return NULL;  // 메모리 풀 부족
    }
    void* ptr = &memory_pool[memory_pool_offset];
    memory_pool_offset += size;
    return ptr;
}

// 메모리 테이블에 엔트리 추가
void add_memory_entry(void* ptr, size_t size) {
    MemoryEntry* entry = (MemoryEntry*)pool_malloc(sizeof(MemoryEntry));
    if (!entry) {
        fprintf(stderr, "Failed to allocate memory for entry\n");
        return;
    }
    entry->ptr = ptr;
    entry->size = size;
    entry->next = memory_table;
    memory_table = entry;
    total_allocated += size;
}

// 메모리 테이블에서 엔트리 제거
void remove_memory_entry(void* ptr) {
    MemoryEntry** current = &memory_table;
    while (*current) {
        if ((*current)->ptr == ptr) {
            MemoryEntry* to_remove = *current;
            *current = to_remove->next;
            total_allocated -= to_remove->size;
            // 메모리 엔트리 자체는 해제하지 않음 (메모리 풀에서 관리)
            return;
        }
        current = &(*current)->next;
    }
    fprintf(stderr, "Error: Attempt to free unallocated memory\n");
}

// 사용자 정의 malloc
void* custom_malloc(size_t size) {
    void* ptr = pool_malloc(size);
    if (ptr) {
        add_memory_entry(ptr, size);
    }
    return ptr;
}

// 사용자 정의 free
void custom_free(void* ptr) {
    if (ptr) {
        remove_memory_entry(ptr);
        // 실제로 메모리를 해제하지는 않음 (메모리 풀에서 관리)
    }
}

// 메모리 테이블 초기화
void init_memory_table() {
    memory_table = NULL;
    memory_pool_offset = 0;
    total_allocated = 0;
}

// 테스트 예시
TEST(MemoryLeakTest, NoLeaks) {
    init_memory_table();

    int* arr = (int*)custom_malloc(10 * sizeof(int));
    for (int i = 0; i < 10; i++) {
        arr[i] = i;
    }

    custom_free(arr);

    EXPECT_EQ(total_allocated, 0) << "Memory leak detected!";
}

TEST(MemoryLeakTest, WithLeak) {
    init_memory_table();

    int* arr = (int*)custom_malloc(10 * sizeof(int));
    // 의도적으로 메모리를 해제하지 않음

    EXPECT_NE(total_allocated, 0) << "Expected memory leak not detected!";
}
