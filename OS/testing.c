#include "my_vm.h"
pthread_mutex_t lock;

void *unoffset (void *va){
  if(va<20){
    printf("Bad Address. Cannot process. Returning -1\n");
    return -1;
  }
  unsigned int new_va = (unsigned int)va - 32;
  return (void*)new_va;
}

int getOptimalVacantPages(int pagesToAllocate){//returns index of starting page
  // bitmap
  int i = 0;
  int counter = 0;
  int leastRegionFound = pgs_bits+1;
  int index = -1;

  for(i=0; i<pgs_bits; i++){
    if(( (bit_map[i/32] & (1 << (i%32) )) != 0 )==0){ // not set
      counter++;
    }
    else{
      if(counter == pagesToAllocate){
        index = i - counter;//FOUND OPTIMAL SPOT
        return index;
      }
      if(counter>=pagesToAllocate && counter<leastRegionFound){
        leastRegionFound = counter;
        index = i - counter;
      }
      counter = 0;
    }
  }
  //last place is best
  if(counter>=pagesToAllocate && counter<leastRegionFound){
    index = i - counter;
    printf("last place best\n");
  }
  printf("pages to allocate: %d, i is: %d counter is %d and least region found is: %d\n",pagesToAllocate,i, counter,leastRegionFound);
  printf("index to alocate pages %d\n",index);
  return index;
}

/*
Function responsible for allocating and setting your physical memory 
*/
void SetPhysicalMem() {

    //Allocate physical memory using mmap or malloc; this is the total size of
    //your memory you are simulating
    
    //HINT: Also calculate the number of physical and virtual pages and allocate
    //virtual and physical maps and initialize them
    unsigned long size=MEMSIZE;
    unsigned long max=MAX_MEMSIZE;
    if(size>max || size<0){
      size=MAX_MEMSIZE;
       printf("Allocating %u, setting Max = %u\n",size,max);
    }
    else{
    	
    }
    physical =(char*) mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANON,  -1, 0);
    if((int)physical==-1){
      perror("error: ");
      exit(-1);
    }
    
    //Calculate bits
    total_bits=(int)ceil(log2(size));
    pgs=(int)ceil((size)/(PGSIZE));
    offset_bits = (int)ceil(log2(PGSIZE));
    pgs_bits=(int)(total_bits-offset_bits);
    dir_bits = pgs_bits/2; //Floor division
    table_bits = pgs_bits - dir_bits;
    tlb_bits= (int)ceil(log2(TLB_SIZE));
    dir_entry=pow(2,dir_bits);
    table_entry=pow(2,table_bits);
    low_bit= (int) pow(2,offset_bits)-1;
    mid_bit= (int) (pow(2, table_bits)-1 ) << offset_bits;
    up_bit=(int) (pow(2, dir_bits)-1 ) << (offset_bits+table_bits);
    tlb_bitm=(int) (pow(2,tlb_bits)-1)<<(offset_bits);
    
    //initialize page directory to point to power of 2 numbits
    page_dir=(pde_t*) calloc(dir_entry,(sizeof(pte_t)));
    int map_entry=(pgs%32)==0?ceil(pgs/32): (pgs/32)+1;
    bit_map=(int*) calloc(map_entry,sizeof(int));
    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        printf("Error: failed to initialize mutex\n");
        return;
    }
    
    //initialize TLB
    tlb_store=(struct tlb*) malloc(sizeof(struct tlb));
    tlb_store->translate=(pte_t*) calloc(TLB_SIZE,sizeof(pte_t));
    tlb_store->vt=(int*) calloc(TLB_SIZE,sizeof(int));
    tlb_store->hit=0;
    tlb_store->miss=0;
    int i=0;
    for (i=0;i<TLB_SIZE;i++){
      tlb_store->vt[i]=-1;
    }
}



/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
pte_t * Translate(pde_t *pgdir, void *va) {
    //HINT: Get the Page directory index (1st level) Then get the
    //2nd-level-page table index using the virtual address.  Using the page
    //directory index and page table index get the physical address
    unsigned int page_offset = (((unsigned int)va)&low_bit);
    //get the index of the page table
    unsigned int page_table_index = ((((unsigned int)va)&mid_bit) >> offset_bits);
    //get index of page directory;
    unsigned int page_directory_index = ((((unsigned int)va)&up_bit) >> (offset_bits+table_bits));
      // printf("page offset: 0x%X\n",page_offset);
      // printf("page table index: 0x%X\n",page_table_index);
      // printf("page directory index: 0x%X\n",page_directory_index);
    pde_t *addrOfPageDirEntry = pgdir + page_directory_index;
    pthread_mutex_lock(&lock);
    if(*addrOfPageDirEntry==NULL){
      printf("directory unallocated\n");
      pthread_mutex_unlock(&lock);
      return NULL;
    }
    pte_t *addrOfPageTable = (pte_t*)   *addrOfPageDirEntry;
    pte_t *addrOfPageTableEntry = addrOfPageTable + page_table_index;
    if(*addrOfPageTableEntry==NULL){
      printf("table unallocated\n");
      pthread_mutex_unlock(&lock);
      return NULL;
    }

    pte_t *physicalPageAddr = (pte_t*)(*addrOfPageTableEntry + page_offset); //can do this because *addrOfPageTableEntry is an unsigned long
    pthread_mutex_unlock(&lock);
    return physicalPageAddr;

    //If translation not successfull
    //return NULL; 
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
    /*HINT: Similar to Translate(), find the page directory (1st level)
    and page table (2nd-level) indices. If no mapping exists, set the
    virtual to physical mapping */
	if(pgdir == NULL){
      printf("page dir not set\n");
      return -1;
    }
    va=unoffset(va);
    //extract the directory index from the address
    unsigned int directory_index=((((unsigned int)va)&up_bit) >> (offset_bits+table_bits));
    //if the table at the index has not been initialized
    if(pgdir[directory_index]==0){
      //create the page table;
      pgdir[directory_index]=(pde_t) calloc(table_entry,sizeof(pte_t));
    }
    //get the beginning of the inner page table
    pte_t* page_table=(pte_t*)pgdir[directory_index];
    //get the index of the inner page table
    unsigned int table_index=((((unsigned int)va)&mid_bit) >> offset_bits);
    //if this page table entry isn't mapped to anything yet, map it to the physical addr
    if(page_table[table_index]==0){
      page_table[table_index]=(pte_t)pa;
    }
    else{
      printf("%d cannot be mapped\n",table_index);
      printf("0x%X still here\n",page_table[table_index]);
    }
    return -1;
}


/*Function that gets the next available page
*/
void *get_next_avail(int num_pages) {
 
    //Use virtual address map to find the next free page
}


/* Function responsible for allocating pages
and used by the benchmark
*/
void *m_alloc(unsigned int num_bytes) {

    //HINT: If the physical memory is not yet initialized, then allocate and initialize.

   /* HINT: If the page directory is not initialized, then initialize the
   page directory. Next, using get_next_avail(), check if there are free pages. If
   free pages are available, set the maps and map a new page. Note, you will 
   have to mark which physical pages are used. */
   
   pthread_mutex_lock(&lock);
    //Check if page directory has been initialized, if not, call SetPhysicalMem()
    if(page_dir==NULL){
      SetPhysicalMem();
    }
    //Convert num_bytes to allocate into pgs to allocate
    unsigned int pg_size = PGSIZE;
    unsigned int pages_to_allocate=((int)(num_bytes%pg_size))==0 ? ceil(num_bytes/pg_size) : (num_bytes/pg_size)+1;
    //Get Shortest Continuous Memory Region
    int pageIndex=getOptimalVacantPages(pages_to_allocate);
    printf("Page index to malloc %d pages: %d\n",pages_to_allocate,pageIndex );

    if(pageIndex==-1){
      printf("No space to allocate\n");
      pthread_mutex_unlock(&lock);
      return NULL;
    }
    //Allocate pages found by setting their value to \u201c1\u201d in the map.
    int i=0;
    for(i=0;i<pages_to_allocate;i++){
      bit_map[((pageIndex+i)/32)] |= (1 << ((pageIndex+i)%32));
    }
    //Choose the virtual addr and map it to the physical pages.
    void* va=(void*) (virt_page_size*pageIndex + 32);

    //Physical Page to allocate
    void* pa=physical+PGSIZE*pageIndex;
    for(i=0;i<pages_to_allocate;i++){
      int status=PageMap(page_dir, va+virt_page_size*i, pa+PGSIZE*i);
      if(status==-1){
        printf("Error mapping page\n");
        pthread_mutex_unlock(&lock);
        return NULL;
      }
    }
    // return virtual addr of first page in this contiguous block to user.
    pthread_mutex_unlock(&lock);
    return va;
}

void removeFromTLB(void *va){

  unsigned int tlb_index = ((((unsigned int)va)&tlb_bitm) >> (offset_bits));
  unsigned int offset = (((unsigned int)va)&low_bit);
  if(tlb_store->vt[tlb_index] == (unsigned int)va-offset){// this page was in the TLB
    tlb_store->vt[tlb_index] = -1;//(unsigned int)NULL;
    tlb_store->translate[tlb_index] = (unsigned int)NULL;
  }
}

int page_unmap(pde_t *pgdir, void *va)
{
    //walk the page directory to see if the virtual address is present or not
    //if not store the entry
    //you might have to reserve some extra space for page table if its not already allocated
    if(pgdir == NULL){
      printf("page directory not set, returning -1\n");
      return -1;
    }

    //extract the directory index from the address
    unsigned int directory_index=((((unsigned int)va)&up_bit) >> (offset_bits+table_bits));
    //pthread_mutex_lock(&lock);

    //if the table at the index has not been initialized
    if(pgdir[directory_index]==0){
      //create the page table;
      printf("!!! Can't unmap an unallocated memory address, returning -1\n");
      // pthread_mutex_unlock(&lock);
      return -1;
    }
    //get the beginning of the inner page table
    pte_t* page_table=(pte_t*)pgdir[directory_index];
    //get the index of the inner page table
    unsigned int table_index=((((unsigned int)va)&mid_bit) >> offset_bits);
    //if this page table entry isn't mapped to anything yet, map it to the physical addr
    if(page_table[table_index]==(pte_t)NULL){
      printf("!!! Can't unmap an unallocated memory address, returning -1\n");
      // pthread_mutex_unlock(&lock);
      return -1;
    }
    page_table[table_index]=(pte_t)NULL;
    removeFromTLB(va);
    return 0;
}

/* Responsible for releasing one or more memory pages using virtual address (va)
*/
void a_free(void *va, unsigned int size) {

    //Free the page table entries starting from this virtual address (va)
    // Also mark the pages free in the map
    //Only free if the memory from "va" to va+size is valid
    if(size<=0){
      printf("Can't free memory: %d\n", size);
      return;
    }
    va = unoffset(va);
    if(va==-1){
      printf("Va error\n");
      return;
    }

    pte_t** pa=malloc( (size/PGSIZE+2)*sizeof(pte_t*)  );
    int counter=0;
    do{
      printf("checking translation: 0x%x\n",va+virt_page_size*counter);
      pa[counter]=Translate(page_dir, va+virt_page_size*counter);
      if(pa[counter]==NULL){
        printf("0x%X was not allocated\n",va+virt_page_size*counter);
        return;
      }
      counter+=1;
      size-=PGSIZE;
    }while(size>0);
    pthread_mutex_lock(&lock);
    int i=0;
    int page_nums_to_free[counter];
    for(i=0;i<counter;i++){
      int offset=(((unsigned int)(va+i*virt_page_size))&low_bit);
      page_nums_to_free[i]=((int)pa[i]-offset-(int)physical)/PGSIZE;
      if(( (bit_map[(page_nums_to_free[i])/32] & (1 << ((page_nums_to_free[i])%32) )) != 0 )==0){
        //extra cases
        printf("Page (%d) has a translated but not allocated\n",page_nums_to_free[i]);
        pthread_mutex_unlock(&lock);
        free(pa);
        return;
      }
    }
    free(pa);
    //We are guaranteed that there was a translation and that the pages were allocated
    //Unmap the translations and mark them as unallocated in the map
    for(i=0;i<counter;i++){
      int status=page_unmap(page_dir,va+i*virt_page_size);
      if(status==-1){
        printf("Tried to unmap unallocated page\n");
        pthread_mutex_unlock(&lock);
        return;
      }
      bit_map[((page_nums_to_free[i])/32)] &= ~(1 << ((page_nums_to_free[i])%32));

    }
    pthread_mutex_unlock(&lock);
}

int checkAllocated(void *va, int size){
  if(va<0){
    printf("!!! Bad Virtual address: 0x%X\n",va);

    return -1;
  }
  int counter=0;
  pte_t** pa=malloc( (size/PGSIZE+2)*sizeof(pte_t*)  );
  //count pages
  do{
    pa[counter]=Translate(page_dir, va+virt_page_size*counter);
    if(pa[counter]==NULL){
      printf("!!! Virtual address: 0x%X was not allocated.\n",va+virt_page_size*counter);
      free(pa);
      return -1;
    }
    counter+=1;
    size-=PGSIZE;
  }while(size>0);
  pthread_mutex_lock(&lock);
  int i=0;
  int pageNums[counter];
  for(i=0;i<counter;i++){
    int offset=(((unsigned int)(va+i*virt_page_size))&low_bit);
    //printf("difference from base%d\n",(int)pa[i]);
    pageNums[i]=((unsigned int)pa[i]-offset-(unsigned int)physical)/PGSIZE;
    //printf("testing page number: %d\n",pageNums[i]);
    if(( (bit_map[(pageNums[i])/32] & (1 << ((pageNums[i])%32) )) != 0 )==0){
      printf("!!! Page %d was not allocated.\n", pageNums[i]);
      pthread_mutex_unlock(&lock);
      free(pa);
      return-1;
    }
  }
  free(pa);
  pthread_mutex_unlock(&lock);
  return 0;
}

pte_t* searchTLB(pte_t* va){


  unsigned int tlb_index=((((unsigned int)va)&tlb_bitm) >> (offset_bits));
  unsigned int offset=(((unsigned int)va)&low_bit);
  if(tlb_store->vt[tlb_index]==(unsigned int)va-offset){
    //TLB HIT
    pte_t* physical_address=tlb_store->translate[tlb_index];
    tlb_store->hit+=1;

    //delete page_num, its just for debugging
    unsigned int page_num=((unsigned int)physical_address-(unsigned int)physical)/PGSIZE;

    return (char*)physical_address+offset;

  }
  else{
    //TLB MISS
    tlb_store->vt[tlb_index]=(unsigned int)va-offset;
    pte_t* physical_address=Translate(page_dir,(unsigned int)(va)-offset);
    tlb_store->translate[tlb_index]=physical_address;

    //delete page_num, its just for debugging
    unsigned int page_num=((unsigned int)physical_address-(unsigned int)physical)/PGSIZE;
    tlb_store->miss+=1;
    return (unsigned int)physical_address+offset;

  }
}


/* The function copies data pointed by "val" to physical
 * memory pages using virtual address (va)
*/
void PutVal(void *va, void *val, int size) {

    /* HINT: Using the virtual address and Translate(), find the physical page. Copy
       the contents of "val" to a physical page. NOTE: The "size" value can be larger
       than one page. Therefore, you may have to find multiple pages using Translate()
       function.*/
       va = unoffset(va);
    if(va==-1){
      printf("Va error\n");
      return;
    }
    char* val_ptr=(char*)val;
    // Translate VA to PA by checking TLB, if it misses, the searchTLB function calls Translate()
    //Check that all memory regions in the range are allocated
    int allocated=checkAllocated(va,size);
    if(allocated!=0){
      printf("Not valid memory addresses\n");
      return;
    }

    char* pa=(char*)searchTLB(va);
    if(pa==NULL){
      printf("Not valid memory address\n");
      return;
    }

    // For i: 0 to Size
    int i=0;
    pthread_mutex_lock(&lock);
    for(i=0;i<size;i++){
      *(pa+i)=*(val_ptr+i);
    }
    pthread_mutex_unlock(&lock);
}


/*Given a virtual address, this function copies the contents of the page to val*/
void GetVal(void *va, void *val, int size) {

    /* HINT: put the values pointed to by "va" inside the physical memory at given
    "val" address. Assume you can access "val" directly by derefencing them.
    If you are implementing TLB,  always check first the presence of translation
    in TLB before proceeding forward */

	 va = unoffset(va);
    if(va==-1){
      printf("Va error\n");
      return;
    }
    char* val_ptr=(char*)val;
    //Check that all memory regions in the range are allocated
    int allocated=checkAllocated(va,size);
    if(allocated!=0){
      printf("Not valid memory addresses\n");
      return;
    }
    // Translate VA to PA TLB, if miss, the searchTLB function calls Translate()
    pte_t* pa=searchTLB(va);
    if(pa==NULL){
      printf("Not valid memory address\n");
      return;
    }
    pthread_mutex_lock(&lock);
    char* pa_ptr=(char*)pa;
    // For i: 0 to Size
    int i=0;
    for(i=0;i<size;i++){
      *(val_ptr+i)=*(pa_ptr+i);
    }
    pthread_mutex_unlock(&lock);
}



/*
This function receives two matrices mat1 and mat2 as an argument with size
argument representing the number of rows and columns. After performing matrix
multiplication, copy the result to answer.
*/
void MatMult(void *mat1, void *mat2, int size, void *answer) {

    /* Hint: You will index as [i * size + j] where  "i, j" are the indices of the
    matrix accessed. Similar to the code in test.c, you will use GetVal() to
    load each element and perform multiplication. Take a look at test.c! In addition to 
    getting the values from two matrices, you will perform multiplication and 
    store the result to the "answer array"*/

       
}









/* correct */


#include "my_vm.h"
pthread_mutex_t lock;

int log_2(int x){
  unsigned int ans = 0 ;
  while( x>>=1 ) {
    ans++;
  }
  return ans ;
}
unsigned int getNextPowerOfTwo(int x){
  unsigned int next=pow(2,ceil(log(x)/log(2)));
  printf("next power of 2 of %d is %d\n",x,next);
  return next;
}
void *unoffset (void *va){
  if(va<20){
    printf("Bad Address. Cannot process. Returning -1\n");
    return -1;
  }
  unsigned int new_va = (unsigned int)va - OFFSET;
  return (void*)new_va;
}
int checkAllocated(void *va, int size){
  if(va<0){
    printf("!!! Bad Virtual address: 0x%X\n",va);

    return -1;
  }
  int counter=0;
  pte_t** pa=malloc( (size/PGSIZE+2)*sizeof(pte_t*)  );
  //count pages
  do{
    pa[counter]=Translate(page_dir, va+virt_page_size*counter);
    // printf("found translation virtual 0x%x to physical 0x%x\n",va+virt_page_size*counter,pa[counter] );
    if(pa[counter]==NULL){
      printf("!!! Virtual address: 0x%X was not allocated.\n",va+virt_page_size*counter);
      free(pa);
      return -1;
    }
    counter+=1;
    size-=PGSIZE;
  }while(size>0);
  pthread_mutex_lock(&lock);
  int i=0;
  int pageNums[counter];
  for(i=0;i<counter;i++){
    int offset=getPageOffset(va+i*virt_page_size);
    //printf("difference from base%d\n",(int)pa[i]);
    pageNums[i]=((unsigned int)pa[i]-offset-(unsigned int)physical_mem)/PGSIZE;
    //printf("testing page number: %d\n",pageNums[i]);
    if(testBit(pageNums[i])==0){
      printf("!!! Page %d was not allocated.\n", pageNums[i]);
      pthread_mutex_unlock(&lock);
      free(pa);
      return-1;
    }
  }
  free(pa);
  pthread_mutex_unlock(&lock);
  return 0;
}
int getOptimalVacantPages(int pagesToAllocate){//returns index of starting page
  // bitmap
  int i = 0;
  int counter = 0;
  int leastRegionFound = numPages+1;
  int index = -1;

  for(i=0; i<numPages; i++){
    if(testBit(i)==0){ // not set
      counter++;
    }
    else{
      if(counter == pagesToAllocate){
        index = i - counter;//FOUND OPTIMAL SPOT
        return index;
      }
      if(counter>=pagesToAllocate && counter<leastRegionFound){
        leastRegionFound = counter;
        index = i - counter;
      }
      counter = 0;
    }
  }
  //last place is best
  if(counter>=pagesToAllocate && counter<leastRegionFound){
    index = i - counter;
    printf("last place best\n");
  }
  printf("pages to allocate: %d, i is: %d counter is %d and least region found is: %d\n",pagesToAllocate,i, counter,leastRegionFound);
  printf("index to alocate pages %d\n",index);
  return index;
}

void setBit(int bit){
  bitmap[(bit/32)] |= (1 << (bit%32));
}
void clearBit(int bit){
  bitmap[(bit/32)] &= ~(1 << (bit%32));
}
int testBit(int bit){
  return ( (bitmap[bit/32] & (1 << (bit%32) )) != 0 );
}
unsigned int getPageOffset(void* va){
  unsigned int page_offset = (((unsigned int)va)&lower_bitmask);
  return page_offset;
}
unsigned int getTableIndex(void* va){
  unsigned int page_table_index = ((((unsigned int)va)&middle_bitmask) >> numOffsetBits);
  return page_table_index;
}
unsigned int getDirIndex(void* va){
  unsigned int page_directory_index = ((((unsigned int)va)&upper_bitmask) >> (numOffsetBits+numPageTableBits));
  return page_directory_index;
}

unsigned int getTLBIndex(void* va){
  unsigned int tlb_index = ((((unsigned int)va)&tlb_bitmask) >> (numOffsetBits));
  return tlb_index;
}

void SetPhysicalMem() {
    //allocate physical memory using mmap or malloc
    unsigned long mem_size=MEMSIZE;
    unsigned long max_mem_size=MAX_MEMSIZE;
    if(mem_size>max_mem_size || mem_size<0){
      mem_size=MAX_MEMSIZE;
       printf("Asked to allocate %u, which is greater than Max Memory available, so setting it to Max = %u\n",mem_size,max_mem_size);
    }
    else{
      ;
      // mem_size=MEMSIZE;
      // printf("physical mem size =%u < max=%u\n",mem_size, max_mem_size);
    }
    physical_mem =(char*) mmap(NULL, mem_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANON,  -1, 0);
    // physical_mem =(char*) mmap(NULL, MEMSIZE, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANON,  -1, 0);
    if((int)physical_mem==-1){
      printf("allocating memory failed\n");
      perror("error: ");
      exit(-1);
    }
    // printf("physical mem located at: 0x%X\n",physical_mem);
    //Calculate bits (needed and create bitmasks needed for translation
    numTotalBits=(int)ceil(log2(mem_size));
    // printf("total bits being used: %d\n",numTotalBits);
    numPages=(int)ceil((mem_size)/(PGSIZE));
    // numPagesBits=(int)ceil(log2(numPages));
    numOffsetBits = (int)ceil(log2(PGSIZE));
    numPagesBits=(int)(numTotalBits-numOffsetBits);
    numPageDirBits = numPagesBits/2; //Floor division
    printf("Num page dir bits: %d\n",numPageDirBits);

    numPageTableBits = numPagesBits - numPageDirBits;
    printf("numPageTableBits: %d\n",numPageTableBits);
    numTLBBits= (int)ceil(log2(TLB_SIZE));
    numDirEntries=pow(2,numPageDirBits);
    numTableEntries=pow(2,numPageTableBits);
    lower_bitmask= (int) pow(2,numOffsetBits)-1;
    middle_bitmask= (int) (pow(2, numPageTableBits)-1 ) << numOffsetBits;
    upper_bitmask=(int) (pow(2, numPageDirBits)-1 ) << (numOffsetBits+numPageTableBits);
    printf("loer bitmask: 0x%X\n",lower_bitmask);
    printf("middle bitmask: 0x%X\n",middle_bitmask);
    printf("upper bitmask: 0x%X\n",upper_bitmask);
    tlb_bitmask=(int) (pow(2,numTLBBits)-1)<<(numOffsetBits);
    // printf("tlb bitmask: 0x%X\n",tlb_bitmask);
    //initialize page directory to point to 2^(numbits) entries
    page_dir=(pde_t*) calloc(numDirEntries,PAGETABLEENTRYSIZE);
    int numEntriesInBitmap=(numPages%32)==0?ceil(numPages/32): (numPages/32)+1;
    // printf("%s\n", );
    bitmap=(int*) calloc(numEntriesInBitmap,sizeof(int));
    // printf("bitmap initialized to size: %d\n", (numPages/32));
    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        printf("!!! Failed to initialize mutex\n");
        return;
    //dick 
    
    }
    //initialize TLB
    tlb_store=(struct tlb*) malloc(sizeof(struct tlb));
    tlb_store->translations=(pte_t*) calloc(TLB_SIZE,sizeof(pte_t));
    tlb_store->virtual_tags=(int*) calloc(TLB_SIZE,sizeof(int));
    tlb_store->hits=0;
    tlb_store->misses=0;
    int i=0;
    for (i=0;i<TLB_SIZE;i++){
      tlb_store->virtual_tags[i]=-1;
    }
    // printf("done set physical mem\n");
}

pte_t * Translate(pde_t *pgdir, void *va)
{
    //you are given a page directory pointer and a virtual address
    //walk the page directory to get the address of the second level page table
    //then use the second level page directory and offset in virtual address to return the
    //physical address
    //page offset is taken from the LSB of the va

    // printf("***Translating address: 0x%x\n", va);

    unsigned int page_offset = getPageOffset(va);
    //get the index of the page table
    unsigned int page_table_index = getTableIndex(va);
    //get index of page directory;
    unsigned int page_directory_index = getDirIndex(va);
      // printf("page offset: 0x%X\n",page_offset);
      // printf("page table index: 0x%X\n",page_table_index);
      // printf("page directory index: 0x%X\n",page_directory_index);
    pde_t *addrOfPageDirEntry = pgdir + page_directory_index;
    pthread_mutex_lock(&lock);
    if(*addrOfPageDirEntry==NULL){
      printf("directory entry unallocated, returning NULL\n");
      pthread_mutex_unlock(&lock);
      return NULL;
    }
    pte_t *addrOfPageTable = (pte_t*)   *addrOfPageDirEntry;
    pte_t *addrOfPageTableEntry = addrOfPageTable + page_table_index;
    if(*addrOfPageTableEntry==NULL){
      printf("table entry unallocated, exiting\n");
      pthread_mutex_unlock(&lock);
      return NULL;
    }

    pte_t *physicalPageAddr = (pte_t*)(*addrOfPageTableEntry + page_offset);//can do this because *addrOfPageTableEntry is an unsigned long
    pthread_mutex_unlock(&lock);
    return physicalPageAddr;
}

int PageMap(pde_t *pgdir, void *va, void *pa)
{
    //walk the page directory to see if the virtual address is present or not
    //if not store the entry
    //you might have to reserve some extra space for page table if its not already allocated
    if(pgdir == NULL){
      printf("page directory not set, returning -1\n");
      return -1;
    }
    va=unoffset(va);
    //extract the directory index from the address
    unsigned int directory_index=getDirIndex(va);
    //pthread_mutex_lock(&lock);
    // printf("directory_index: 0x%X\n",directory_index);
    //if the table at the index has not been initialized
    if(pgdir[directory_index]==0){
      //create the page table;
      pgdir[directory_index]=(pde_t) calloc(numTableEntries,PAGETABLEENTRYSIZE);
    }
    //get the beginning of the inner page table
    pte_t* page_table=(pte_t*)pgdir[directory_index];
    //get the index of the inner page table
    unsigned int table_index=getTableIndex(va);
    //if this page table entry isn't mapped to anything yet, map it to the physical addr
    if(page_table[table_index]==0){
      page_table[table_index]=(pte_t)pa;
    }
    else{
      printf("Can't map to something that already is translated at table index %d\n",table_index);
      printf("what is there already: 0x%X\n",page_table[table_index]);
    }
    //pthread_mutex_unlock(&lock);

    return 0;
}

int page_unmap(pde_t *pgdir, void *va)
{
    //walk the page directory to see if the virtual address is present or not
    //if not store the entry
    //you might have to reserve some extra space for page table if its not already allocated
    if(pgdir == NULL){
      printf("page directory not set, returning -1\n");
      return -1;
    }

    //extract the directory index from the address
    unsigned int directory_index=getDirIndex(va);
    //pthread_mutex_lock(&lock);

    //if the table at the index has not been initialized
    if(pgdir[directory_index]==0){
      //create the page table;
      printf("!!! Can't unmap an unallocated memory address, returning -1\n");
      // pthread_mutex_unlock(&lock);
      return -1;
    }
    //get the beginning of the inner page table
    pte_t* page_table=(pte_t*)pgdir[directory_index];
    //get the index of the inner page table
    unsigned int table_index=getTableIndex(va);
    //if this page table entry isn't mapped to anything yet, map it to the physical addr
    if(page_table[table_index]==(pte_t)NULL){
      printf("!!! Can't unmap an unallocated memory address, returning -1\n");
      // pthread_mutex_unlock(&lock);
      return -1;
    }
    page_table[table_index]=(pte_t)NULL;
    //remove from TLB if present in it
    // printf("BEFORE:\n");
    // printTLB();
    removeFromTLB(va);
    // printf("AFTER:\n");
    // printTLB();
    // pthread_mutex_unlock(&lock);
    return 0;
}

void removeFromTLB(void *va){

  unsigned int tlb_index = getTLBIndex(va);
  unsigned int offset = getPageOffset(va);
  if(tlb_store->virtual_tags[tlb_index] == (unsigned int)va-offset){// this page was in the TLB
    // printf("Freeing va ==> 0x%x\n This page starts at ==> 0x%x\n", va, (unsigned int)va - offset);
    tlb_store->virtual_tags[tlb_index] = -1;//(unsigned int)NULL;
    tlb_store->translations[tlb_index] = (unsigned int)NULL;
  }
}

void* m_alloc(unsigned int num_bytes) {
    pthread_mutex_lock(&lock);
    //you will allocate the pages required to store the given number of bytes as
    //continuous virtual address.
    //you will have to store the page table entries
    //you will also have to mark which physical pages have been used
    // Step 1) Check if page directory has been initialized, if not, call SetPhysicalMem()


    if(page_dir==NULL){
      SetPhysicalMem();
    }
    // Step 2) Convert num_bytes to allocate into numPages to allocate
    unsigned int pg_size = PGSIZE;
    unsigned int pages_to_allocate=((int)(num_bytes%pg_size))==0 ? ceil(num_bytes/pg_size) : (num_bytes/pg_size)+1;
    // unsigned int pages_to_allocate= ceil(num_bytes/pg_size);

    // pthread_mutex_lock(&lock);
    // printf("Lock!\n" );

    // Step 3) Get Shortest Continuous Memory Region
    int pageIndex=getOptimalVacantPages(pages_to_allocate);
    printf("Page index to malloc %d pages: %d\n",pages_to_allocate,pageIndex );

    if(pageIndex==-1){
      printf("!!! No space to allocate. Returning NULL\n");
      pthread_mutex_unlock(&lock);
      return NULL;
    }
    // Step 4) Allocate pages found by setting their value to \u201c1\u201d in the bitmap.
    int i=0;
    for(i=0;i<pages_to_allocate;i++){
      setBit(pageIndex+i);
    }
    // printf("set bit!\n" );

    // Step 5) Choose the virtual addr and map it to the physical pages.
    //Virtual address = OFFSET + PGSIZE*INDEX_OF_ALLOCATED_PAGE
    void* va=(void*) (virt_page_size*pageIndex + OFFSET);

    //Physical Page to allocate
    void* pa=physical_mem+PGSIZE*pageIndex;
    for(i=0;i<pages_to_allocate;i++){
      int status=PageMap(page_dir, va+virt_page_size*i, pa+PGSIZE*i);
      // printf("mapped 0x%X to 0x%X\n",va+virt_page_size*i, pa+PGSIZE*i );

      if(status==-1){
        printf("!!! Error mapping page. Returning NULL \n");
        pthread_mutex_unlock(&lock);
        return NULL;
      }
    }
    // Step 6) return virtual addr of first page in this contiguous block to user.
    pthread_mutex_unlock(&lock);
    // printf("returning from malloc!\n" );

    return va;
}

void a_free(void *va, unsigned int size) {
    //free the page table entries starting from this virtual address and uptill size
    //mark the pages which you recently free
    //only free if the memory from va till va+size is valid
    if(size<=0){
      printf("Cannot free memory of size: %d. This may be due to an integer overflow error.\n", size);
      return;
    }
    va = unoffset(va);
    if(va==-1){
      printf("!!! Virtual address is bad. Returning and not freeing.\n");
      return;
    }

    pte_t** pa=malloc( (size/PGSIZE+2)*sizeof(pte_t*)  );
    int counter=0;
    do{
      printf("checking if there is a translation for: 0x%x\n",va+virt_page_size*counter);
      pa[counter]=Translate(page_dir, va+virt_page_size*counter);
      if(pa[counter]==NULL){
        printf("!!! Virtual address: 0x%X was not allocated. Returning and not freeing.\n",va+virt_page_size*counter);
        return;
      }
      counter+=1;
      size-=PGSIZE;
    }while(size>0);
    // printf("asking to free %d pages\n",counter);
    pthread_mutex_lock(&lock);
    int i=0;
    int page_nums_to_free[counter];
    for(i=0;i<counter;i++){
      int offset=getPageOffset(va+i*virt_page_size);
      page_nums_to_free[i]=((int)pa[i]-offset-(int)physical_mem)/PGSIZE;
      if(testBit(page_nums_to_free[i])==0){
        //should never get here
        printf("!!! Page (%d) has a translation but was not allocated. Weird. Returning.\n",page_nums_to_free[i]);
        pthread_mutex_unlock(&lock);
        free(pa);
        return;
      }
    }
    free(pa);
    //We are guaranteed that there was a translation and that the pages were allocated
    //Unmap the translations and mark them as unallocated in the bitmap
    for(i=0;i<counter;i++){
      int status=page_unmap(page_dir,va+i*virt_page_size);
      if(status==-1){
        printf("!!! Tried to unmap unallocated page. Returning\n");
        pthread_mutex_unlock(&lock);
        return;
      }
      // printf("free pagenum: %d\n");
      clearBit(page_nums_to_free[i]);

    }
    pthread_mutex_unlock(&lock);
}

void PutVal(void *va, void *val, int size) {
    //put the values pointed to by val inside the physical memory at given virtual address
    //assume you can access val address directly by derefencing it
    //Also, it has the capability to put values inside the tlb

    va = unoffset(va);
    if(va==-1){
      printf("!!! Virtual address is bad. Returning and not freeing.\n");
      return;
    }
    //printf("writing 4 bytes to virtual address 0x%X\n",((unsigned int)va));

    char* val_ptr=(char*)val;
    // Translate VA to PA by checking TLB, if it misses, the searchTLB function calls Translate()
    // pte_t* pa=searchTLB(va);
    //Check that all memory regions in the range are allocated
    int allocated=checkAllocated(va,size);
    if(allocated!=0){
      printf("!!! Not valid memory addresses. Returning\n");
      return;
    }

    char* pa=(char*)searchTLB(va);
    // printf("physical address 0x%X virtual addr: 0x%X\n",((unsigned int)pa-(unsigned int)physical_mem),va);
    if(pa==NULL){
      printf("!!! Not valid memory address. Returning\n");
      return;
    }

    // For i: 0 to Size
    int i=0;
    pthread_mutex_lock(&lock);
    for(i=0;i<size;i++){
      *(pa+i)=*(val_ptr+i);
    }
    pthread_mutex_unlock(&lock);


}

void printTLB(){
  int i = 0;
  for(i=0; i<TLB_SIZE; i++){
    printf("\ntlb_store->virtual_tags[%d] = 0x%x\n", i, tlb_store->virtual_tags[i]);
    printf("tlb_store->translations[%d] = 0x%x\n", i, tlb_store->translations[i]);
  }
}

void GetVal(void *va, void *val, int size) {
    //put the values pointed to by va inside the physical memory at given val address
    //assume you can access val address directly by derefencing them
    //always check first the presence of translation inside the tlb before proceeding forward
    //for testing purpose:

    //reset va to unoffsetted value
    va = unoffset(va);
    if(va==-1){
      printf("!!! Virtual address is bad. Returning and not freeing.\n");
      return;
    }
    char* val_ptr=(char*)val;
    //Check that all memory regions in the range are allocated
    int allocated=checkAllocated(va,size);
    if(allocated!=0){
      printf("!!! Not valid memory addresses. Returning\n");
      return;
    }
    // Translate VA to PA by checking TLB, if it misses, the searchTLB function calls Translate()
    pte_t* pa=searchTLB(va);
    if(pa==NULL){
      printf("!!! Not valid memory address. Returning\n");
      return;
    }


    pthread_mutex_lock(&lock);
    char* pa_ptr=(char*)pa;
    // For i: 0 to Size
    int i=0;
    for(i=0;i<size;i++){
      *(val_ptr+i)=*(pa_ptr+i);
    }
    pthread_mutex_unlock(&lock);

}

pte_t* searchTLB(pte_t* va){


  unsigned int tlb_index=getTLBIndex(va);
  unsigned int offset=getPageOffset(va);
  if(tlb_store->virtual_tags[tlb_index]==(unsigned int)va-offset){
    //TLB HIT
    pte_t* physical_address=tlb_store->translations[tlb_index];
    tlb_store->hits+=1;

    //delete page_num, its just for debugging
    unsigned int page_num=((unsigned int)physical_address-(unsigned int)physical_mem)/PGSIZE;

    //printf("TLB HIT! Physical address 0x%X     page number: %d\n",physical_address,page_num);

    return (char*)physical_address+offset;

  }
  else{
    //TLB MISS
    tlb_store->virtual_tags[tlb_index]=(unsigned int)va-offset;
    pte_t* physical_address=Translate(page_dir,(unsigned int)(va)-offset);
    tlb_store->translations[tlb_index]=physical_address;

    //delete page_num, its just for debugging
    unsigned int page_num=((unsigned int)physical_address-(unsigned int)physical_mem)/PGSIZE;

    //printf("TLB MISS! Physical Address 0x%X    page number:%d\n",physical_address,page_num);
    tlb_store->misses+=1;
    // printf("offset: %d\n",offset);
     //printf("physical_address without offset: 0x%X\n",physical_address);

    return (unsigned int)physical_address+offset;

  }
}

void MatMult(void *mat1, void *mat2, int size, void *answer) {
    //given two arrays of length: size * size
    //multiply them as matrices and store the computed result in answer
    //Hint: You will do indexing as [i * size + j] where i, j are the indices of matrix being accessed
    int *mat1_ptr = (int*)mat1;
    int *mat2_ptr = (int*)mat2;
    int *answer_ptr = (int*)answer;
    int ans=0;
    int i=0,j=0,k=0;
    printf("multiplying matrices!\n");
    for(i=0;i<size;i++){
      for(j=0;j<size;j++){
        for(k=0;k<size;k++){
          int mat1_val=0,mat2_val=0, old_answer_val=0;
          GetVal(mat1+(i*size+k)*sizeof(int),&mat1_val,sizeof(int));
          GetVal(mat2+(k*size+j)*sizeof(int),&mat2_val,sizeof(int));
          //printf("adding %d*%d\n",mat1_val,mat2_val);
          ans+=mat1_val*mat2_val;
        }
        PutVal(answer+(i*size+j)*sizeof(int), &ans,sizeof(int));
        int a=0;
        GetVal(answer+(i*size+j)*sizeof(int), &a,sizeof(int));

        ans=0;
      }

    }
}




	void *v_a = (void*)(((long)va) + (long)size);
    unsigned long va_l = (long)va;
    unsigned long va_l_un = va_l>>(offset_bits);
    unsigned long index = floor((double)va_l_un / 8);
    unsigned long offset = va_l_un % 8;
    unsigned long va_l2 = (long)v_a;
    unsigned long va_l_un2 = va_l2>>(offset_bits);
    unsigned long index2 = floor((double)va_l_un2 / 8);
    unsigned long offset2 = (va_l_un2 % 8);
    unsigned long i, j;
    unsigned char *byte;
    unsigned char bmask;
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
