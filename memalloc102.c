#define _GNU_SOURCE
#include <unistd.h> // sbrk
#include <string.h> // memset and memcpy
#include <pthread.h>
#include <stdio.h>
#include <sys/mman.h> // mmap
#include <bits/mman-linux.h>

typedef char ALIGN[16];

union header {
    struct {
        size_t size;
        unsigned is_free;
        union header *next;
    } s;
    ALIGN stub; // padding
};

typedef union header header_t;

typedef struct boundary_tag {
    size_t size;
    unsigned is_free;
} boundary_tag_t;

#define NUM_FREE_LISTS 10
header_t *free_lists[NUM_FREE_LISTS];

header_t *head, *tail;
pthread_mutex_t global_malloc_lock;

int get_free_list_index(size_t size) {
    int index = 0;
    while (size > 1) {
        size >>= 1;
        index++;
    }
    return index < NUM_FREE_LISTS ? index : NUM_FREE_LISTS - 1;
}

void add_to_free_list(header_t *block) {
    int index = get_free_list_index(block->s.size);
    block->s.next = free_lists[index];
    free_lists[index] = block;
}

header_t *get_free_block(size_t size) {
    int index = get_free_list_index(size);
    for (int i = index; i < NUM_FREE_LISTS; i++) {
        header_t *curr = free_lists[i];
        while (curr) {
            if (curr->s.is_free && curr->s.size >= size) {
                return curr;
            }
            curr = curr->s.next;
        }
    }
    return NULL;
}

void coalesce() {
    header_t *curr = head;
    while (curr && curr->s.next) {
        if (curr->s.is_free && curr->s.next->s.is_free) {
            curr->s.size += curr->s.next->s.size + sizeof(header_t);
            curr->s.next = curr->s.next->s.next;
        } else {
            curr = curr->s.next;
        }
    }
}

void *malloc(size_t size) {
    size_t total_size;
    void *block;
    header_t *header;

    if (size == 0) {
        fprintf(stderr, "Error: Cannot allocate 0 bytes\n");
        return NULL;
    }

    pthread_mutex_lock(&global_malloc_lock);
    header = get_free_block(size);

    if (header) {
        header->s.is_free = 0;
        pthread_mutex_unlock(&global_malloc_lock);
        return (void*)(header + 1);
    }

    total_size = sizeof(header_t) + size + sizeof(boundary_tag_t);
    block = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (block == MAP_FAILED) {
        perror("mmap");
        pthread_mutex_unlock(&global_malloc_lock);
        return NULL;
    }

    header = block;
    header->s.size = size;
    header->s.is_free = 0;
    header->s.next = NULL;

    boundary_tag_t *footer = (boundary_tag_t*)((char*)block + total_size - sizeof(boundary_tag_t));
    footer->size = size;
    footer->is_free = 0;

    if (!head)
        head = header;
    if (tail)
        tail->s.next = header;
    tail = header;

    pthread_mutex_unlock(&global_malloc_lock);
    return (void*)(header + 1);
}

void free(void *block) {
    header_t *header, *tmp;
    void *programbreak;

    if (!block) {
        return;
    }
    pthread_mutex_lock(&global_malloc_lock);

    header = (header_t*)block - 1;

    programbreak = sbrk(0);

    if ((char*)block + header->s.size == programbreak) {
        if (head == tail) {
            head = tail = NULL;
        } else {
            tmp = head;
            while (tmp) {
                if (tmp->s.next == tail) {
                    tmp->s.next = NULL;
                    tail = tmp;
                }
                tmp = tmp->s.next;
            }
        }
        sbrk(0 - (header->s.size + sizeof(header_t)));
    } else {
        header->s.is_free = 1;
        add_to_free_list(header);
        coalesce();
    }

    pthread_mutex_unlock(&global_malloc_lock);
}