#include <unistd.h> // sbrk
#include <string.h> // memset and memcpy
#include <pthread.h>

// memory alignment arranges data in memory at addresses that
// are multiples of the data types size (2, 4, 8, 16, etc.)
typedef char ALIGN[16];


// store different data types in the same memory location
// size is determined by the largest data type in the union
// we use a union here to ensure entire struct is aligned to 16 bytes
// this is the case because the largest member is stub
union header {
	struct {
		size_t size;
		unsigned is_free;
		union header *next; // next memory block in singly linked list
	} s;
	ALIGN stub; // padding
};

// each memory block has a header that stores metadata
typedef union header header_t;

header_t *head, *tail;
pthread_mutex_t global_malloc_lock;

// traverse the linked list and check if there is a free block
// that is large enough to store the requested size
// return type is a pointer to the header of the free block or NULL
header_t *getFreeBlock(size_t size) {
	header_t *curr = head;
	while(curr) {
		if (curr->s.is_free && curr->s.size >= size) {
			return curr;
		}
		curr = curr->s.next;
	}
	return NULL;
}

// when the user calls free, they provide a pointer
// to the memory block they want to free without metadata
// since malloc returns a pointer to the memory block and 
// not the header, free must accept a pointer as well
// doesnt need to return a pointer because it just 
// needs to mark the block as free
void free(void *block) {
	header_t *header, *tmp;
	void *programbreak;

	if (!block) {
		return;
	}
	pthread_mutex_lock(&global_malloc_lock);

	// block is a void pointer so we need to cast it to a header pointer
	// to treat it as a header with metadata
	// -------------------------------------------------------------------
	// visual representation:
	// memory address | data
	// ---------------|------
	// 0x1000         | [header (size, is_free, next)]
	// 0x1010         | [user data]
	// -------------------------------------------------------------------
	// when the user calls malloc, they get a pointer to 0x1010
	// when the user calls free, they pass a pointer to 0x1010
	// header = (header_t*)0x1010 - 1 = 0x1000, which is the header
	header = (header_t*)block - 1;

	// sbrk(0) = current address of program break.
	// sbrk(x) = increment by x bytes
	// sbrk(-x) = release by x bytes
	// On failure, sbrk() returns (void*) -1
	programbreak = sbrk(0);

	// block points to the start of the user data block
	// header->s.size is the size of the user data block in bytes
	// we need to move the pointer from the start to the end 
	// and that is exactly header->s.size bytes away
	// so if header->s.size is 16, then casting to char* allows
	// the pointer to move forward by exactly 16 bytes
	if ((char*)block + header->s.size == programbreak) {

		// if the block is the only block in the linked list
		// we can release the block and set head and tail to NULL
		if (head == tail) {
			head = tail = NULL;
		} else {

			// if its not the only block, then update the tail
			// by traversing the linked list with a while loop
			// and check if the current block is the one right 
			// before the last block
			tmp = head;
			while (tmp) {
				if (tmp->s.next == tail) {
					tmp->s.next = NULL;
					tail = tmp;
				}
				tmp = tmp->s.next;
			}
		}

		// this shrinks the heap by the size of the users data block
		// and the header, releasing the entire block back to the OS
		sbrk(0 - sizeof(header_t) - header->s.size);
		pthread_mutex_unlock(&global_malloc_lock);
		return;
	}
	header->s.is_free = 1;
	pthread_mutex_unlock(&global_malloc_lock);
}

void *malloc(size_t size) {
	size_t total_size;
	void *block;
	header_t *header;
	
	if (!size) {
		return NULL;
	}
	
	pthread_mutex_lock(&global_malloc_lock);
	header = getFreeBlock(size);
	if (header) {
		header->s.is_free = 0;
		pthread_mutex_unlock(&global_malloc_lock);
		return (void*)(header + 1);
	}

	total_size = sizeof(header_t) + size;
	block = sbrk(total_size);
	if (block == (void*) -1) {
		pthread_mutex_unlock(&global_malloc_lock);
		return NULL;
	}

	header = block;
	header->s.size = size;
	header->s.is_free = 0;
	header->s.next = NULL;

	if (!head) {
		head = header;
	}
	
	if (tail) {
		tail->s.next = header;
	}	
	tail = header;

	pthread_mutex_unlock(&global_malloc_lock);
	return (void*)(header + 1);
}

void *calloc(size_t num, size_t nsize) {
	size_t size;
	void *block;
	if (!num || !nsize)
		return NULL;
	size = num * nsize;
	/* check mul overflow */
	if (nsize != size / num)
		return NULL;
	block = malloc(size);
	if (!block)
		return NULL;
	memset(block, 0, size);
	return block;
}

void *realloc(void *block, size_t size) {
	header_t *header;
	void *ret;
	if (!block || !size)
		return malloc(size);
	header = (header_t*)block - 1;
	if (header->s.size >= size)
		return block;
	ret = malloc(size);
	if (ret) {
		/* Relocate contents to the new bigger block */
		memcpy(ret, block, header->s.size);
		/* Free the old memory block */
		free(block);
	}
	return ret;
}
