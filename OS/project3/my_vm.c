#include "my_vm.h"

bool in_mem = false;
bool in_virt = false;
int pgs_pt = 0;
int pgs_bits = 0;
int offset_bits=0;
int dir_bits= 0;
int table_bits=0;
unsigned long mmap_in;
void* phys=NULL;
void* pgs_m=NULL;
void*  dir_entry=NULL;
void* table_entry=NULL;
pde_t* pg_dir;

/*
Function responsible for allocating and setting your physical memory 
*/
void SetPhysicalMem() {
    if(in_mem){
        return;
    }
    in_mem = true;
    //initialize vars
    offset_bits = (int)ceil((double)log(PGSIZE)/log(2));
    table_bits = (int)floor((double)(32 - offset_bits) / 2);
    dir_bits = 32 - table_bits - offset_bits;
    pgs_bits = (int)floor((double)(MEMSIZE)/(PGSIZE));
    pgs_pt = (int)ceil((double)(pow(2, table_bits)*4) / PGSIZE);
    mmap_in = ceil((double)pgs_bits / 8);
    while(!pg_dir){
        pg_dir = (void*)malloc(ceil((double)(pow(2,dir_bits)*sizeof(pde_t))/8));
    }
    memset(pg_dir, 0, ceil((double)(pow(2,dir_bits)*sizeof(pde_t))/8));
    
    //Allocate physical memory using mmap or malloc; this is the total size of your memory you are simulating
    if(MEMSIZE > MAX_MEMSIZE){
        printf("ERROR: Defined memory size is greater than max memory size\n");
       	while(!phys){
		phys = (void*)malloc(ceil((double)(MAX_MEMSIZE)/8));
	}
    } else{
    	while(!phys){
        	phys = (void*)malloc(ceil((double)(MEMSIZE)/8));
    	}
    }
    
    //virtual and physical bitmaps and initialize them
    while(!table_entry){
        table_entry = (void*)malloc(ceil((double)pgs_bits/8));
    }
    memset(table_entry, 0, ceil((double)pgs_bits/8));
    while(!dir_entry){
        dir_entry = (void*)malloc(ceil((double)pgs_bits/8));
    }
    memset(dir_entry, 0, ceil((double)pgs_bits/8));
    //initialize page directory bitmap
    while(!pgs_m){
        pgs_m = (void*)malloc(ceil((double)pow(2,dir_bits)/8));
    }
    memset(pgs_m, 0, ceil((double)pow(2,dir_bits)/8));

}

bool check_in_tlb(void *va){
	unsigned long va_l = (long)va;
    unsigned long offset_va = (va_l<<(dir_bits+table_bits)) >> (dir_bits+table_bits);
     //check if virtual address is taken
    unsigned long va_l_un = va_l>>(offset_bits);
    unsigned long tlb_index = va_l_un % (TLB_SIZE);
    if(!in_virt){
        if(tlb_store.tlb_arr[tlb_index].virt_add == va_l_un && tlb_store.tlb_arr[tlb_index].is_ok == true){
            return true;
        }
    }
	return false;
}

pde_t *pgdir;
void put_in_tlb(void *va, void *pa){
	unsigned long va_l = (long)va;
    unsigned long offset_va = (va_l<<(dir_bits+table_bits)) >> (dir_bits+table_bits);
     //check if virtual address is taken
    unsigned long va_l_un = va_l>>(offset_bits);
    unsigned long tlb_index = va_l_un % (TLB_SIZE);
    unsigned long index = floor((double)va_l_un / 8);
    unsigned long offset = va_l_un % 8;
    unsigned char * byte = ((unsigned char*)dir_entry + index);
    if(!(*byte & (1 << offset))){
        printf("Address does not exist\n");
    }
	//save pd index, pt index, and offset
    unsigned long pd_in = va_l>>(offset_bits+table_bits);
    unsigned long pt_in = va_l<<(dir_bits);
    pt_in = pt_in>>(dir_bits + offset_bits);
    unsigned long off_ind = va_l<<(dir_bits + table_bits);
    off_ind = off_ind>>(dir_bits + table_bits);

    //get pd entry with pd index, check if valid, proceed
    unsigned long index_pd = floor((double)pd_in / 8);
    unsigned long offset_pd = pd_in % 8;
    unsigned char * byte_pd = ((char*)pgs_m + index_pd);
    if(!(*byte_pd & (1 << offset_pd))){
            printf("Invalid Page Directory\n");
    }
    pde_t *pd_entry = pgdir + pd_in;
    //get pt entry with pt index, check if valid, return address stored there
    pte_t *pt_entry = ((pte_t*)(pa + (((long)*(pd_entry) >> offset_bits)*PGSIZE))) + pt_in;
    if(!in_virt){
        tlb_store.tlb_arr[tlb_index].virt_add = va_l_un;
        tlb_store.tlb_arr[tlb_index].phys_add = (unsigned long)(pa + ((*pt_entry >> offset_bits)*PGSIZE));
        tlb_store.tlb_arr[tlb_index].is_ok = true;
    }
}

/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
pte_t * Translate(pde_t *pgdir, void *va) {
    unsigned long va_l = (long)va;
    unsigned long offset_va = (va_l<<(dir_bits+table_bits)) >> (dir_bits+table_bits);
     //check if virtual address is taken
    unsigned long va_l_un = va_l>>(offset_bits);
    unsigned long tlb_index = va_l_un % (TLB_SIZE);
    if(!in_virt){
        if(tlb_store.tlb_arr[tlb_index].virt_add == va_l_un && tlb_store.tlb_arr[tlb_index].is_ok == true){
            return (pte_t*)(tlb_store.tlb_arr[tlb_index].phys_add + offset_va);
        }
    }
    unsigned long index = floor((double)va_l_un / 8);
    unsigned long offset = va_l_un % 8;
    unsigned char * byte = ((unsigned char*)dir_entry + index);
    if(!(*byte & (1 << offset))){
        printf("Address does not exist\n");
        return NULL;
    }
	//save pd index, pt index, and offset
    unsigned long pd_in = va_l>>(offset_bits+table_bits);
    unsigned long pt_in = va_l<<(dir_bits);
    pt_in = pt_in>>(dir_bits + offset_bits);
    unsigned long off_ind = va_l<<(dir_bits + table_bits);
    off_ind = off_ind>>(dir_bits + table_bits);

    //get pd entry with pd index, check if valid, proceed
    unsigned long index_pd = floor((double)pd_in / 8);
    unsigned long offset_pd = pd_in % 8;
    unsigned char * byte_pd = ((char*)pgs_m + index_pd);
    if(!(*byte_pd & (1 << offset_pd))){
            printf("Invalid Page Directory\n");
            return NULL;
    }
    pde_t *pd_entry = pgdir + pd_in;
    //get pt entry with pt index, check if valid, return address stored there
    pte_t *pt_entry = ((pte_t*)(phys + (((long)*(pd_entry) >> offset_bits)*PGSIZE))) + pt_in;
    if(!in_virt){
        tlb_store.tlb_arr[tlb_index].virt_add = va_l_un;
        tlb_store.tlb_arr[tlb_index].phys_add = (unsigned long)(phys + ((*pt_entry >> offset_bits)*PGSIZE));
        tlb_store.tlb_arr[tlb_index].is_ok = true;
        return(pte_t*)((pte_t)(phys + ((*pt_entry >> offset_bits)*PGSIZE))+offset_va);
    }
    return (pte_t*)(*pt_entry + offset_va);
}

/*Function that gets the next available page */
void *get_pgs(int num_pages) {
    //for loop going through bitmap, bytewise
    unsigned long i, j;
    unsigned char * byte;
    unsigned char bmask;
    unsigned long offset = 0, index = 0, count = 0;
    bool first = false;
    for(i = 0; i < mmap_in; i++){
        byte = ((unsigned char*)table_entry + i);
        //for loop going through byte with 8 masks to count zeroes
        for(j = 0; j < 8; j++){
            bmask = (1 << j);
            if(((bmask ^ *byte) >> j) == 1){
                //on first zero, save offset and index
                if((!offset) && (!index) && !first){
                    index = i;
                    offset = j;
                    if(index == 0 && offset == 0){
                        first = true;
                    }
                }
                *byte = *byte | (1 << j);
                //keep going through, if count = num_pages, 
                count++;
                if(count == num_pages){
                    //mark bmask, return address of original offset
                    return (void*)(((index*8)+offset) << offset_bits);
                }
            } else {
                //if a 1 is encountered, reset count and offsets; we need CONTIGUOUS pages
                count = 0;
                offset = (unsigned long)NULL;
                index = (unsigned long)NULL;
            }
            //if byte ends, keep count, offset and index and keep counting
        }
    }
    printf("address space not available\n");
    return NULL;
}

/* The function takes a page directory address, virtual address, physical address
as an argument, and sets a page table entry. This function will walk the page
directory to see if there is an existing mapping for a virtual address. If the
virtual address is not present, then a new entry will be added */
int PageMap(pde_t *pgdir, void *va, void *pa){
    unsigned long va_l = (long)va;
    unsigned long pa_long = (long)pa;
    //check if va is taken
    unsigned long va_l_un = va_l>>(offset_bits);
    unsigned long index = floor((double)va_l_un / 8);
    unsigned long offset = va_l_un % 8;
    unsigned char * byte = ((unsigned char*)dir_entry + index);
    if(*byte & (1 << offset)){
        printf("Something in Virtual address\n");
        return -1;
    }
    //check if pa is taken
    unsigned long pa_long_un = pa_long>>(offset_bits);
    unsigned long index_p = floor((double)pa_long_un / 8);
    unsigned long offset_p = pa_long_un % 8;
    unsigned char * byte_p = ((char*)table_entry + index_p);
    if(*byte_p & (1 << offset_p)){
        printf("something in physical address\n");
        return -1;
    }
    //save pd index, pt index, and offset
    unsigned long pd_in = va_l>>(offset_bits+table_bits);
    unsigned long pt_in = va_l<<(dir_bits);
    pt_in = pt_in>>(dir_bits + offset_bits);
    unsigned long off_ind = va_l<<(dir_bits + table_bits);
    off_ind = off_ind>>(dir_bits + table_bits);
    //check if pd index is initialized
    unsigned long index_pd = floor((double)pd_in / 8);
    unsigned long offset_pd = pd_in % 8;
    unsigned char * byte_pd = ((char*)pgs_m + index_pd);
    *byte_p = *byte_p | (1 << offset_p);
    if(!(*byte_pd & (1 << offset_pd))){
        //insert next available pfn(s) into pg_directory
        *(pgdir + pd_in) = (pde_t)get_pgs(pgs_pt);
        //mark bitmap
        *byte_pd = *byte_pd | (1 << offset_pd);
    }
    //get pd entry w/ pd index, proceed
    pde_t *pd_entry = pgdir + pd_in;
    //get pt entry w/ pt index
    pte_t *pt_entry = ((pte_t*)(phys + (((long)*(pd_entry) >> offset_bits)*PGSIZE))) + pt_in;
    //load pa into pt entry
    *pt_entry = (pte_t)pa;
    *byte = *byte | (1 << offset);
    return 0;
}

void *v_a;
unsigned long va_l, va_l_un, index, offset, va_l2, va_l_un2, index2, offset2, i, j;
unsigned char *byte;
unsigned char bmask;

void helpper(void*va, int size){
	void *v_a = (void*)(((long)va) + (long)size);
    unsigned long va_l = (long)va;
    unsigned long va_l_un = va_l>>(offset_bits);
    unsigned long index = floor((double)va_l_un / 8);
    unsigned long offset = va_l_un % 8;
    unsigned long va_l2 = (long)v_a;
    unsigned long va_l_un2 = va_l2>>(offset_bits);
    unsigned long index2 = floor((double)va_l_un2 / 8);
    unsigned long offset2 = (va_l_un2 % 8);
}

/*Function that gets the next available page */
void *get_next_avail(int num_pages) {
    //for loop going through bitmap, bytewise
    unsigned long offset = NULL, index = NULL, count = NULL;
    bool first = false;
    for(i = 0; i < mmap_in; i++){
        byte = ((unsigned char*)dir_entry + i);
        //for loop going through byte with 8 masks to count zeroes
        for(j = 0; j < 8; j++){
            bmask = (1 << j);
            if(((bmask ^ *byte) >> j) == 1){
                //on first zero, save offset & index
                if((!offset) && (!index) && !first){
                    index = i;
                    offset = j;
                    if(index == 0 && offset == 0){
                        first = true;
                    }
                }
                //keep going through, if count = num_pages, 
                count++;
                if(count == num_pages){
                    //mark bmask, return address of original offset
                    return (void*)(((index*8)+offset) << offset_bits);
                }
            } else {
                //if a 1 is encountered, reset count and offsets
                //we need CONTIGUOUS pages
                //printf("No\n");
                count = 0;
                offset = (unsigned long)NULL;
                index = (unsigned long)NULL;
            }
            //if byte ends, keep count, offset & index and keep counting
        }
    }
    printf("address space not available\n");
    return NULL;
}

void *get_phy(){
    //scan phys bitmap, return first free physical address
    //loop through bitmap, byte by byte, look for one less than max value of byte
    //this indicates that there is at least one zero in it
    for(i = 0; i < mmap_in; i++){
        byte = (unsigned char*)table_entry + i;
        //for loop going through byte with 8 masks to count zeroes
        for(j = 0; j < 8; j++){
            bmask = (1 << j);
            if(((bmask ^ *byte) >> j) == 1){
                //on first zero, return physical address w/ offset & index
                return (void*)((((i*8)+j)) << offset_bits);
            }
        }
    }
    printf("address space not available\n");
    return NULL;

}

/* Function responsible for allocating pages
and used by the benchmark */
void *m_alloc(unsigned int num_bytes) {
    pthread_mutex_lock(&lock);
    if(!in_mem){
        SetPhysicalMem();
    }

    unsigned int num_pages = (int)ceil((double)num_bytes / (PGSIZE));
    void *va = get_next_avail(num_pages);
    va_l = (long)va;
    va_l_un = va_l>>(offset_bits);
    index = floor((double)va_l_un / 8);
    offset = va_l_un % 8;
    unsigned long count = num_pages;
    v_a = (void*)(((long)va) + (long)num_bytes);
    va_l2 = (long)v_a;
    va_l_un2 = va_l2>>(offset_bits);
    index2 = floor((double)va_l_un2 / 8);
    offset2 = va_l_un2 % 8;
    while(index <= index2){
        if(index == index2 && offset > offset2){
            index = mmap_in;
            offset = 0;
            break;
        }
        byte = ((unsigned char*)dir_entry + index);
        if(*byte & (1 << offset)){
            printf("Range not free.\n");
            pthread_mutex_unlock(&lock);
            return NULL;
        }
        PageMap(pg_dir, (void*)(((index*8)+offset) << offset_bits), get_phy());
        count--;
        if(count == 0){
            break;
        }
        offset++;
        if(offset == 8){
            offset = 0;
            index++;
        }
    }

    if(count != 0){
        printf("Range invalid.\n");
        pthread_mutex_unlock(&lock);
        return NULL;
    }
    pthread_mutex_unlock(&lock);
    return va;
}

/* Responsible for releasing one or more memory pages using virtual address (va) */
void a_free(void *va, int size) {
    pthread_mutex_lock(&lock);
    //Free the page table entries starting from this virtual address (va)
    //Also mark the pages free in the bitmap
    //Only free if the memory from "va" to va+size is valid
    unsigned int num_pages = (int)ceil((double)size / (double)(PGSIZE));
    va_l = (long)va;
    va_l_un = va_l>>(offset_bits);
    unsigned long tlb_index = va_l_un % (TLB_SIZE);
    index = floor((double)va_l_un / 8);
    offset = va_l_un % 8;
    unsigned long count = num_pages;
    v_a = (void*)(((long)va) + (long)size);
    va_l2 = (long)v_a;
    va_l_un2 = va_l2>>(offset_bits);
    index2 = floor((double)va_l_un2 / 8);
    offset2 = va_l_un2 % 8;
    unsigned long pa_long, pa_long_un, index_p, offset_p;
    unsigned char * byte_p;
    unsigned char bmask_p;
    
    for(i = index; i <= index2; i++){
        byte = (unsigned char*)dir_entry + i;
        //for loop going through byte with 8 masks to count zeroes
        for(j = (i == index) ? offset : 0; j < 8; j++){
            bmask = (1 << j);
            if(i >= index2 && j > offset2){
                i = mmap_in;
                j = 8;
                break;
            }
            if(((bmask & *byte) >> j) != 1){
                printf("Address not allocated. %d %d %d\n", count, i, j);
                pthread_mutex_unlock(&lock);
                return;
            }
            //mark physical bitmap
            in_virt = true;
            void *pa = Translate(pg_dir, (void*)(((i*8)+j) << offset_bits));
            in_virt = false;
            pa_long = (long)pa;
            pa_long_un = pa_long>>(offset_bits);
            index_p = floor((double)pa_long_un / 8);
            offset_p = pa_long_un % 8;
            byte_p = (unsigned char*)table_entry + index_p;
            bmask_p = (1 << offset_p);
            *byte_p = *byte_p ^ (bmask_p);
            //mark virtual bitmap
            *byte = *byte ^ (bmask);
            //clear TLB entry if applicable
            if(tlb_store.tlb_arr[tlb_index].virt_add == va_l_un && tlb_store.tlb_arr[tlb_index].is_ok == true){
                tlb_store.tlb_arr[tlb_index].virt_add = (unsigned long)NULL;
                tlb_store.tlb_arr[tlb_index].phys_add = (unsigned long)NULL;
                tlb_store.tlb_arr[tlb_index].is_ok = false;
            }
            count--;
            if(count == 0){
                pthread_mutex_unlock(&lock);
                return;
            }
        }
    }
}



/* The function copies data pointed by "val" to physical memory pages using virtual address (va) */
void PutVal(void *va, void *val, int size) {
    pthread_mutex_lock(&lock);
    
    helpper(va,size);
    
    //check validity of virtual address range
    for(i = index; i <= index2; i++){
        byte = ((unsigned char*)dir_entry + i);
        //for loop going through byte with 8 masks to count zeroes
        for(j = (i == index) ? offset : 0; j < 8; j++){
            bmask = (1 << j);
            if(i >= index2 && j >= offset2){
                i = mmap_in;
                j = 8;
                break;
            }
            if(((bmask & *byte) >> j) != 1){
                printf("Invalid Index: %d %d+ Offset: %d %d in PutVal\n", i, index2, j, offset2);
                pthread_mutex_unlock(&lock);
                return;
            }
        }
    }

    void *pa_inc;
    void *va_inc = va;
    for(i=0; i < size; i++){
        va_inc = va + i;
        pa_inc = Translate(pg_dir, va_inc);
        *(char*)pa_inc = *(char*)val;
        val = (char*)val + 1;
    }
    
    pthread_mutex_unlock(&lock);
}


/*Given a virtual address, this function copies the contents of the page to val*/
void GetVal(void *va, void *val, int size) {
    pthread_mutex_lock(&lock);
    
    helpper(va,size);

    //check validity of virtual address range
    for(i = index; i <= index2; i++){
        byte = ((unsigned char*)dir_entry + i);
        //for loop going through byte with 8 masks to count zeroes
        for(j = (i == index) ? offset : 0; j < 8; j++){
            bmask = (1 << j);
            if(i >= index2 && j >= offset2){
                i = mmap_in;
                j = 8;
                break;
            }
            if(((bmask & *byte) >> j) != 1){
                printf("Invalid address+range in GetVal\n");
                pthread_mutex_unlock(&lock);
                return;
            }
        }
    }

    void *pa_inc;
    void *va_inc = va;
    for(i=0; i < size; i++){
        va_inc = va + i;
        pa_inc = Translate(pg_dir, va_inc);
        *(char*)val = *(char*)pa_inc;
        val = (char*)val + 1;
    }
    pthread_mutex_unlock(&lock);
}


/* This function receives two matrices mat1 and mat2 as an argument with size
argument representing the number of rows and columns. After performing matrix
multiplication, copy the result to answer. */
void MatMult(void *mat1, void *mat2, int size, void *answer) {
    if(size<1){
    	printf("Invalid Size");
    	exit(0);
    }
    int *mat1_val = (int*)malloc(sizeof(int));
    int *mat2_val = (int*)malloc(sizeof(int));
    int *mat_sum = (int*)malloc(sizeof(int));
    int i,j,k;
    for(i = 0; i < size; i++){
        for(j = 0; j < size; j++){
            *mat_sum = 0;
            for(k = 0; k < size; k++){
                GetVal((int*)mat1 + ((i*size)+k), (void*)mat1_val, sizeof(int));
                GetVal((int*)mat2 + ((k*size)+j), (void*)mat2_val, sizeof(int));
                *mat_sum +=  (*mat1_val) * (*mat2_val);
            }
            PutVal((int*)answer + ((i*size)+j), (void*)mat_sum, sizeof(int));
        }
    }  
}
