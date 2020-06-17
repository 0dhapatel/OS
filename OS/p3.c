#include "my_vm.h"
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
        printf("ERROR: Defined memory size is greater than max memory size supported.\nSetting to max memory size of 4GB.\n");
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

/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
pte_t * Translate(pde_t *pgdir, void *va) {
    unsigned long va_long = (long)va;
    unsigned long offset_va = (va_long<<(dir_bits+table_bits)) >> (dir_bits+table_bits);
     //check if virtual address is taken
    unsigned long va_long_un = va_long>>(offset_bits);
    unsigned long tlb_index = va_long_un % (TLB_SIZE);
    if(!in_virt){
        if(tlb_store.tlb_arr[tlb_index].virt_add == va_long_un && tlb_store.tlb_arr[tlb_index].is_ok == true){
            return (pte_t*)(tlb_store.tlb_arr[tlb_index].phys_add + offset_va);
        }
    }

    unsigned long index = floor((double)va_long_un / 8);
    unsigned long offset = va_long_un % 8;
    unsigned char * byte = ((unsigned char*)dir_entry + index);
    if(!(*byte & (1 << offset))){
        printf("\nNo such address exists.\n");
        return NULL;
    }

    //save pd index
    unsigned long pd_ind = va_long>>(offset_bits+table_bits);
    //save pt index
    unsigned long pt_ind = va_long<<(dir_bits);
    pt_ind = pt_ind>>(dir_bits + offset_bits);
    //save offset
    unsigned long off_ind = va_long<<(dir_bits + table_bits);
    off_ind = off_ind>>(dir_bits + table_bits);

    //get pd entry w/ pd index, check if valid, proceed
    unsigned long index_pd = floor((double)pd_ind / 8);
    unsigned long offset_pd = pd_ind % 8;
    unsigned char * byte_pd = ((char*)pgs_m + index_pd);
    if(!(*byte_pd & (1 << offset_pd))){
            printf("\nInvalid Page Directory Index.\n");
            return NULL;
    }
    pde_t *pd_entry = pgdir + pd_ind;
    //get pt entry w/ pt index, check if valid, return address stored there
    pte_t *pt_entry = ((pte_t*)(phys + (((long)*(pd_entry) >> offset_bits)*PGSIZE))) + pt_ind;
    if(!in_virt){
        tlb_store.tlb_arr[tlb_index].virt_add = va_long_un;
        tlb_store.tlb_arr[tlb_index].phys_add = (unsigned long)(phys + ((*pt_entry >> offset_bits)*PGSIZE));
        tlb_store.tlb_arr[tlb_index].is_ok = true;
        return(pte_t*)((pte_t)(phys + ((*pt_entry >> offset_bits)*PGSIZE))+offset_va);
    }
    return (pte_t*)(*pt_entry + offset_va);
}

/*
The function takes a page directory address, virtual address, physical address
as an argument, and sets a page table entry. This function will walk the page
directory to see if there is an existing mapping for a virtual address. If the
virtual address is not present, then a new entry will be added
*/
int
PageMap(pde_t *pgdir, void *va, void *pa)
{
    unsigned long va_long = (long)va;
    unsigned long pa_long = (long)pa;
    //check if virtual address is taken
    unsigned long va_long_un = va_long>>(offset_bits);
    unsigned long index = floor((double)va_long_un / 8);
    unsigned long offset = va_long_un % 8;
    unsigned char * byte = ((unsigned char*)dir_entry + index);
    if(*byte & (1 << offset)){
        printf("\nvirtual address is taken\n");
        return -1;
    }

    
    //check if physical address is taken
    unsigned long pa_long_un = pa_long>>(offset_bits);
    unsigned long index_p = floor((double)pa_long_un / 8);
    unsigned long offset_p = pa_long_un % 8;
    unsigned char * byte_p = ((char*)table_entry + index_p);
    if(*byte_p & (1 << offset_p)){
        printf("physical address is taken\n");
        return -1;
    }

    /*HINT: Similar to Translate(), find the page directory (1st level)
    and page table (2nd-level) indices. If no mapping exists, set the
    virtual to physical mapping */

    //save pd index
    unsigned long pd_ind = va_long>>(offset_bits+table_bits);
    //save pt index
    unsigned long pt_ind = va_long<<(dir_bits);
    pt_ind = pt_ind>>(dir_bits + offset_bits);
    //save offset
    unsigned long off_ind = va_long<<(dir_bits + table_bits);
    off_ind = off_ind>>(dir_bits + table_bits);
    //check if pd_ind is initialized
    unsigned long index_pd = floor((double)pd_ind / 8);
    unsigned long offset_pd = pd_ind % 8;
    unsigned char * byte_pd = ((char*)pgs_m + index_pd);
    *byte_p = *byte_p | (1 << offset_p);
    if(!(*byte_pd & (1 << offset_pd))){
        //insert next available pfn(s) into pg_directory
        *(pgdir + pd_ind) = (pde_t)get_next_avail_phy_mult(pgs_pt);
        //mark bitmap
        *byte_pd = *byte_pd | (1 << offset_pd);
    }
    //get pd entry w/ pd index, proceed
    pde_t *pd_entry = pgdir + pd_ind;
    //get pt entry w/ pt index
    pte_t *pt_entry = ((pte_t*)(phys + (((long)*(pd_entry) >> offset_bits)*PGSIZE))) + pt_ind;
    //load pa into pt entry
    *pt_entry = (pte_t)pa;
    *byte = *byte | (1 << offset);
    return 0;
}


/*Function that gets the next available page
*/
void *get_next_avail(int num_pages) {
    //for loop going through bitmap, bytewise
    unsigned long i;
    unsigned long j;
    unsigned char * byte;
    unsigned char bmask;
    unsigned long offset = NULL;
    unsigned long index = NULL;
    unsigned long count = NULL;
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
    printf("No available address space.\n");
    return NULL;
}

void *get_next_avail_phy(){
    //scan phys bitmap, return first free physical address
    unsigned long i;
    unsigned long j;
    unsigned char * byte;
    unsigned char bmask;
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
    printf("No available physical addresses.\n");
    return NULL;

}

/*Function that gets the next available page
*/
void *get_next_avail_phy_mult(int num_pages) {
    //for loop going through bitmap, bytewise
    unsigned long i;
    unsigned long j;
    unsigned char * byte;
    unsigned char bmask;
    unsigned long offset = NULL;
    unsigned long index = NULL;
    unsigned long count = NULL;
    bool first = false;
    for(i = 0; i < mmap_in; i++){
        byte = ((unsigned char*)table_entry + i);
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
                *byte = *byte | (1 << j);
                //keep going through, if count = num_pages, 
                count++;
                if(count == num_pages){
                    //mark bmask, return address of original offset
                    return (void*)(((index*8)+offset) << offset_bits);
                }
            } else {
                //if a 1 is encountered, reset count and offsets
                //we need CONTIGUOUS pages
                count = 0;
                offset = (unsigned long)NULL;
                index = (unsigned long)NULL;
            }
            //if byte ends, keep count, offset & index and keep counting
        }
    }
    printf("No available address space.\n");
    return NULL;
}


/* Function responsible for allocating pages
and used by the benchmark
*/
void *m_alloc(unsigned int num_bytes) {
    pthread_mutex_lock(&lock);
    if(!in_mem){
        SetPhysicalMem();
    }

    unsigned int num_pages = (int)ceil((double)num_bytes / (PGSIZE));
    void *va = get_next_avail(num_pages);
    unsigned long va_long = (long)va;
    unsigned long va_long_un = va_long>>(offset_bits);
    unsigned long index = floor((double)va_long_un / 8);
    unsigned long offset = va_long_un % 8;
    unsigned long count = num_pages;
    unsigned char * byte;

    void *va2 = (void*)(((long)va) + (long)num_bytes);

    unsigned long va_long2 = (long)va2;
    unsigned long va_long_un2 = va_long2>>(offset_bits);
    unsigned long index2 = floor((double)va_long_un2 / 8);
    unsigned long offset2 = va_long_un2 % 8;
    while(index <= index2){
        if(index == index2 && offset > offset2){
            index = mmap_in;
            offset = 0;
            break;
        }
        byte = ((unsigned char*)dir_entry + index);
        if(*byte & (1 << offset)){
            printf("Range selected by get_next_avail is not free.\n");
            pthread_mutex_unlock(&lock);
            return NULL;
        }
        PageMap(pg_dir, (void*)(((index*8)+offset) << offset_bits), get_next_avail_phy());
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
        printf("Range selected by get_next is invalid, vir_bitmap is now also invalid.\n");
        pthread_mutex_unlock(&lock);
        return NULL;
    }
    pthread_mutex_unlock(&lock);
    return va;
}

/* Responsible for releasing one or more memory pages using virtual address (va)
*/
void a_free(void *va, int size) {
    pthread_mutex_lock(&lock);
    //Free the page table entries starting from this virtual address (va)
    //Also mark the pages free in the bitmap
    //Only free if the memory from "va" to va+size is valid
    unsigned int num_pages = (int)ceil((double)size / (double)(PGSIZE));
    unsigned long va_long = (long)va;
    unsigned long va_long_un = va_long>>(offset_bits);
    unsigned long tlb_index = va_long_un % (TLB_SIZE);
    unsigned long index = floor((double)va_long_un / 8);
    unsigned long offset = va_long_un % 8;
    unsigned long count = num_pages;
    unsigned char * byte;
    unsigned char bmask;

    void *va2 = (void*)(((long)va) + (long)size);

    unsigned long va_long2 = (long)va2;
    unsigned long va_long_un2 = va_long2>>(offset_bits);
    unsigned long index2 = floor((double)va_long_un2 / 8);
    unsigned long offset2 = va_long_un2 % 8;
    unsigned long pa_long;
    unsigned long pa_long_un;
    unsigned long index_p;
    unsigned long offset_p;
    unsigned char * byte_p;
    unsigned char bmask_p;

    unsigned long i;
    unsigned long j;
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
                printf("This address is not allocated. %d %d %d\n", count, i, j);
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
            if(tlb_store.tlb_arr[tlb_index].virt_add == va_long_un && tlb_store.tlb_arr[tlb_index].is_ok == true){
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


/* The function copies data pointed by "val" to physical
 * memory pages using virtual address (va)
*/
void PutVal(void *va, void *val, int size) {
    pthread_mutex_lock(&lock);
    void *va2 = (void*)(((long)va) + (long)size);

    unsigned long va_long = (long)va;
    unsigned long va_long_un = va_long>>(offset_bits);
    unsigned long index = floor((double)va_long_un / 8);
    unsigned long offset = va_long_un % 8;

    unsigned long va_long2 = (long)va2;
    unsigned long va_long_un2 = va_long2>>(offset_bits);
    unsigned long index2 = floor((double)va_long_un2 / 8);
    unsigned long offset2 = (va_long_un2 % 8);
    unsigned long i;
    unsigned long j;
    unsigned char *byte;
    unsigned char bmask;

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
                printf("Put value fail: invalid Index: %d %d+Offset: %d %d\n", i, index2, j, offset2);
                pthread_mutex_unlock(&lock);
                return;
            }
        }
    }

    void *pa;
    void *va_inc = va;
    for(i=0; i < size; i++){
        va_inc = va + i;
        pa = Translate(pg_dir, va_inc);
        *(char*)pa = *(char*)val;
        val = (char*)val + 1;
    }
    pthread_mutex_unlock(&lock);
}


/*Given a virtual address, this function copies the contents of the page to val*/
void GetVal(void *va, void *val, int size) {
    pthread_mutex_lock(&lock);
    void *va2 = (void*)(((long)va) + (long)size);

    unsigned long va_long = (long)va;
    unsigned long va_long_un = va_long>>(offset_bits);
    unsigned long index = floor((double)va_long_un / 8);
    unsigned long offset = va_long_un % 8;

     unsigned long va_long2 = (long)va2;
    unsigned long va_long_un2 = va_long2>>(offset_bits);
    unsigned long index2 = floor((double)va_long_un2 / 8);
    unsigned long offset2 = (va_long_un2 % 8);

    unsigned long i;
    unsigned long j;
    unsigned char *byte;
    unsigned char bmask;

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
                printf("Get value fail: invalid address+range\n");
                pthread_mutex_unlock(&lock);
                return;
            }
        }
    }

    void *pa;
    void *va_inc = va;
    for(i=0; i < size; i++){
        va_inc = va + i;
        pa = Translate(pg_dir, va_inc);
        *(char*)val = *(char*)pa;
        val = (char*)val + 1;
    }
    pthread_mutex_unlock(&lock);
}



/*
This function receives two matrices mat1 and mat2 as an argument with size
argument representing the number of rows and columns. After performing matrix
multiplication, copy the result to answer.
*/
void mat_mult(void *mat1, void *mat2, int size, void *answer) {
    int *mat1_val = (int*)malloc(sizeof(int));
    int *mat2_val = (int*)malloc(sizeof(int));
    int *mat_sum = (int*)malloc(sizeof(int));
    int i;
    int j;
    int k;
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
