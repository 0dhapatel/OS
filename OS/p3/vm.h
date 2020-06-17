#ifndef VM_H_INCLUDED
#define VM_H_INCLUDED
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

//Assume the address space is 32 bits, so the max memory size is 4GB
//Page size is 4KB

//Add any important includes here which you may need

#define PGSIZE 4096

// Maximum size of your memory
#define MAX_MEMSIZE 4*1024*1024*1024

#define MEMSIZE 1024*1024*1024

// Represents a page table entry
typedef unsigned long pte_t;

// Represents a page directory entry
typedef unsigned long pde_t;

#define TLB_SIZE 64

//Structure to represents TLB
struct tlb {
    struct tlb_entry {
        unsigned long va;
        unsigned long pa;
        bool valid;
    } entries[TLB_SIZE];
    //Assume your TLB is a direct mapped TLB of TBL_SIZE (entries)
    // You must also define wth TBL_SIZE in this file.
    //Assume each bucket to be 4 bytes
};
struct tlb tlb_store;


void set_physical_mem();
pte_t* translate(pde_t *pgdir, void *va);
int page_map(pde_t *pgdir, void *va, void* pa);
bool check_in_tlb(void *va);
void put_in_tlb(void *va, void *pa);
void *a_malloc(unsigned int num_bytes);
void a_free(void *va, int size);
void put_value(void *va, void *val, int size);
void get_value(void *va, void *val, int size);
void mat_mult(void *mat1, void *mat2, int size, void *answer);
void *get_next_avail_phy_mult(int num_pages);


void *mem_root = NULL;
void *phy_map = NULL;
void *vir_map = NULL;
void *pgdir_map = NULL;
pde_t *pgdir2 = NULL;
int numOffBits=0;
int numPtBits=0;
int numPdBits=0;
int numPages=0;
int numPagesPT=0;
unsigned long maxBMapIndex;

bool isInitMem = false;

pthread_mutex_t mutex;
//pthread_mutex_t mutex2;

bool needVirtual = false;

#endif
