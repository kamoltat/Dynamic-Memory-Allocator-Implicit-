// Initialize 1 Megabyte of memory and then allocate characters to see how space is taken up
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef char *addrs_t;
typedef void *any_t; 

#define WSIZE               4       /* Word and header/footer size (bytes) */   
#define DSIZE               8           /* Doubleword size (bytes) */
#define DEFAULT_MEM_SIZE    1<<20

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))   

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p)) 
#define PUT(p, val)  (*(unsigned int *)(p) = (val)) 

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)    
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)   
#define FTRP(bp)             ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))    
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

#define rdtsc(x)      __asm__ __volatile__("rdtsc \n\t" : "=A" (*(x)))
/* Global variables */
addrs_t baseptr;    /* Pointer to first block */

/* Function prototypes for internal helper routines */

static void place(void *bp, size_t asize);
static void *find_first_fit(size_t size);
static void *coalesce(void *bp);


int bloc_alloc = 0; //number of blocks allocated (done)
int bloc_free = 1; //number of free blocks (done)
int raw_byt_alloc = 0; //actuall bytes requested(done)
int pd_byt_alloc = 0; //padded total bytes allocated (done)
int raw_byt_free = 0; //
int al_byt_free = 0; //done
int tot_malloc = 0;//done
int tot_free = 0;//done
int tot_fail = 0;//done
int avg_malloc = 0;
int avg_free = 0;
int tot_clock = 0;


/***
 *    ██████╗  █████╗ ██████╗ ████████╗     ██╗
 *    ██╔══██╗██╔══██╗██╔══██╗╚══██╔══╝    ███║
 *    ██████╔╝███████║██████╔╝   ██║       ╚██║
 *    ██╔═══╝ ██╔══██║██╔══██╗   ██║        ██║
 *    ██║     ██║  ██║██║  ██║   ██║        ██║
 *    ╚═╝     ╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝        ╚═╝
 *                                             
 */

    /* Use the system malloc() routine (or new in C++) only to allocate size bytes for
      the initial memory area, M1. baseptr is the starting address of M1. */

void Init (size_t size) {

  size_t asize = size - (5 * WSIZE); /* adjusted size of free allocated memory, taking into account the size of the initialized headers */ 
  baseptr = (addrs_t) malloc (size); 
  PUT (baseptr, 0);                                         /* Alignment padding */
  PUT (baseptr + (1 * WSIZE), PACK (DSIZE, 1));             /* Prologue header */
  PUT (baseptr + (2 * WSIZE), PACK (DSIZE, 1));             /* Prologue footer */
  PUT (baseptr + (3 * WSIZE), PACK (asize, 0));             /* New malloc header */
  PUT (baseptr + asize + (3 * WSIZE), PACK (asize, 0));         /* New malloc footer */
  PUT (baseptr + size, PACK (0, 1));                        /* Epilogue header*/
  baseptr += (4 * WSIZE);                           /* Moves the baseptr to the start of the malloc header */
}

  /*
    Implement your own memory allocation routine here. 
    This should allocate the first contiguous size bytes available in M1. 
    Since some machine architectures are 64-bit, it should be safe to allocate space starting 
    at the first address divisible by 8. Hence align all addresses on 8-byte boundaries!

    If enough space exists, allocate space and return the base address of the memory. 
    If insufficient space exists, return NULL. 
  */

addrs_t Malloc (size_t size) {
  tot_malloc++; //update the malloc call counter
  // Calculates the final size of allocation so that it is aligned
  size_t asize;
  // pointer to iterate over freeList
  addrs_t bp;
  
  if (baseptr == 0) {
    Init (size);
  }
  // catches requests that can break code
  if (size <= 0) {
    tot_fail++;
    return NULL;
  }
  // adjusts block size to make it aligned + have enough space for overhead
  if (size <= DSIZE)            
        asize = 2 * DSIZE;      
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    
  if((bp = find_first_fit(asize)) != NULL) {
      bloc_alloc++;
      raw_byt_alloc += (size); //update the raw bytes allocated
      pd_byt_alloc += (asize-DSIZE); //update padded bytes
      place(bp,asize);
      return bp;
    }
  tot_fail++;
  return NULL;
    
  }

  
/* find_first_fit - Iterates over the blocks in the heap to see if any of them
              are free, and if they have a size bigger than size. If so,
              returns the address. If not, returns NULL.*/ 
 
static void *find_first_fit(size_t size) {
    
    void *bp;
    
    // Iterates over the different blocks in the heap
    for (bp = baseptr; GET_SIZE (HDRP (bp)) > 0; bp = NEXT_BLKP (bp)) {
        // If not allocated AND GET_SIZE > size: 
        if (!GET_ALLOC (HDRP (bp)) && (size <= GET_SIZE (HDRP (bp)))) {
            return bp;
        }
    }
    // If there is not enough space found in the heap
    return NULL; 
}
  
/* 
 * place - Place block of asize bytes at start of free block bp 
 *         and split if remainder would be at least minimum block size
 */

static void place (void *bp, size_t size)
{
    // csize is calculated from the size of the currently selected unallocated block.
    size_t csize = GET_SIZE (HDRP (bp));

    // checks to see if there is enough space to split the unallocated block.
    if ((csize - size) >= (2 * DSIZE)) {
        PUT (HDRP (bp), PACK (size, 1));            // overwrites free header with alloc header
        PUT (FTRP (bp), PACK (size, 1));            // overwrites free footer with alloc footer
        bp = NEXT_BLKP (bp);                        // address moves to the remainder of the free block.
        PUT (HDRP (bp), PACK (csize - size, 0));    // creates updated free header
        PUT (FTRP (bp), PACK (csize - size, 0));    // creates updated free footer
    }
    else {                                          // Else overwrite the header and footer to be allocated.
        PUT (HDRP (bp), PACK (csize, 1));           
        PUT (FTRP (bp), PACK (csize, 1));
        bloc_free--;
    }
}

  /*
    This frees the previously allocated size bytes starting from address addr in the 
    memory area, M1. You can assume the size argument is stored in a data structure after 
    the Malloc() routine has been called, just as with the UNIX free() command. 
  */
void Free (addrs_t addr) {
    tot_free++;
    if (addr == 0){
        tot_fail++;
        return;
    }
    size_t size = GET_SIZE (HDRP (addr));
    if (baseptr == 0) {
          Init(size);
    }
    PUT (HDRP (addr), PACK (size, 0));
    PUT (FTRP (addr), PACK (size, 0));
    bloc_alloc--;
    pd_byt_alloc -= (size-DSIZE); //update the padded bytes allocated
    coalesce (addr);
}


/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block
 */
static void *coalesce (void *bp)
{
    size_t prev_alloc = GET_ALLOC (FTRP (PREV_BLKP (bp)));
    size_t next_alloc = GET_ALLOC (HDRP (NEXT_BLKP (bp)));
    size_t size = GET_SIZE (HDRP (bp));

    if (prev_alloc && next_alloc) { // Case 1 : Both the previous and next blocks are allocated, no coalescing.
        bloc_free++;  
        return bp;
    }

    else if (prev_alloc && !next_alloc) {   // Case 2 : Only next block is free, forward coalescing.
        size += GET_SIZE (HDRP (NEXT_BLKP (bp)));
        PUT (HDRP (bp), PACK (size, 0));
        PUT (FTRP (NEXT_BLKP (bp)), PACK (size, 0));
    }

    else if (!prev_alloc && next_alloc) {   // Case 3 : Only previous block is free, backward coalescing.
        size += GET_SIZE (HDRP (PREV_BLKP (bp)));
        PUT (FTRP (bp), PACK (size, 0));
        PUT (HDRP (PREV_BLKP (bp)), PACK (size, 0));
        bp = PREV_BLKP (bp);
    }

    else {                      // Case 4 : Both previous and next blocks are free, forwards and backwards coalescing.
        size += GET_SIZE  (HDRP (PREV_BLKP (bp)));
        size += GET_SIZE  (HDRP (NEXT_BLKP (bp)));
        PUT (HDRP (PREV_BLKP (bp)), PACK (size, 0));
        PUT (FTRP (NEXT_BLKP (bp)), PACK (size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}



  /*
   Allocate size bytes from M1 using Malloc(). 
   Copy size bytes of data into Malloc'd memory. 
   You can assume data is a storage area outside M1. 
   Return starting address of data in Malloc'd memory. 
  */
addrs_t Put (any_t data, size_t size) {
 
  addrs_t bp = Malloc(size);
  if(bp == NULL){
      return 0;
  }
  memcpy(bp,data,size);         // copies data into the heap
  return bp;
}

/*
    Copy size bytes from addr in the memory area, M1, to data address. 
    As with Put(), you can assume data is a storage area outside M1. 
    De-allocate size bytes of memory starting from addr using Free(). 
  */

void Get (any_t return_data, addrs_t addr, size_t size) {

   //char ret[size];          // char array to copy the data to.
   memcpy(return_data,addr,size);   // copies data from heap to ret
   //return_data = ret;       // assigns ret to return_data
   Free(addr);  
   raw_byt_alloc -= (size);  
}


 /*******************************************************************************
  *                                                                             *
  *                                                                             *
  *                                                                             *
  *                 ██████╗  █████╗ ██████╗ ████████╗    ██████╗                *
  *                 ██╔══██╗██╔══██╗██╔══██╗╚══██╔══╝    ╚════██╗               *
  *                 ██████╔╝███████║██████╔╝   ██║        █████╔╝               *
  *                 ██╔═══╝ ██╔══██║██╔══██╗   ██║       ██╔═══╝                *
  *                 ██║     ██║  ██║██║  ██║   ██║       ███████╗               *
  *                 ╚═╝     ╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝       ╚══════╝               *
  *                                                                             *
  *                                                                             *
  *                                                                             *
  *******************************************************************************/



addrs_t *RT = NULL;
addrs_t heap_base;
int RT_size; // Measured in number of elements, not bytes


void VInit(size_t size)  {
  size_t asize = size - (5 * WSIZE);  /* adjusted size of free allocated memory, taking into account the size of the initialized headers */   
  heap_base = (addrs_t) malloc (size); 
  PUT (heap_base, 0);                                         /* Alignment padding */
  PUT (heap_base + (1 * WSIZE), PACK (DSIZE, 1));             /* Prologue header */
  PUT (heap_base + (2 * WSIZE), PACK (DSIZE, 1));             /* Prologue footer */
  PUT (heap_base + (3 * WSIZE), PACK (asize, 0));             /* New malloc header */
  PUT (heap_base + asize + (3 * WSIZE), PACK (asize, 0));         /* New malloc footer */
  PUT (heap_base + size, PACK (0, 1));                        /* Epilogue header*/
  heap_base += (4 * WSIZE);                           /* Moves the heap_base to the start of the malloc header */
  
  RT_size = asize / (2 * DSIZE);
  RT = malloc(RT_size * sizeof(*RT));                                                 /* Initializing the Redirection Table */
}



/* Vfind_first_fit - Iterates over the blocks in the heap to see if any of them
              are free, and if they have a size bigger than size. If so,
              returns the address. If not, returns NULL.*/ 
 
static void *Vfind_first_fit(size_t size) {
    
    void *bp;
    
    // Iterates over the different blocks in the heap
    for (bp = heap_base; GET_SIZE (HDRP (bp)) > 0; bp = NEXT_BLKP (bp)) {
        // If not allocated AND GET_SIZE > size: 
        if (!GET_ALLOC (HDRP (bp)) && (size <= GET_SIZE (HDRP (bp)))) {
            return bp;
        }
    }
    // If there is not enough space found in the heap
    return NULL; 
}


// Iterates over the Redirection Table to look for a free slot
int RT_find_space_ind() {
  int i;
  for(i = 0; i < RT_size; i++) {
    if (RT[i] == NULL) {
      return i;
    }
  }
  return -1;
}

// VMalloc - Allocates a block of size t while adjusting its size. Also creates and returns pointer to its corresponding entry in the 
// redirection table.
addrs_t *VMalloc(size_t size) {
   tot_malloc++; //update the malloc call counter
   // Calculates the final size of allocation so that it is aligned
  size_t asize;                            // aligned size + overhead
  addrs_t bp;                              // pointer to iterate over freeList
  int i = RT_find_space_ind();             // Index of possible RT entry for VMalloc
  addrs_t *RTbp;                           // pointer pointing to the RT entry containing pointer to malloc block
  if (heap_base == 0) {
    VInit (size);
  }
  // catches requests that can break code
  if (size <= 0){ 
      tot_fail++;
      return NULL;
  }
  // adjusts block size to make it aligned + have enough space for overhead
  if (size <= DSIZE)            
        asize = 2 * DSIZE;      
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
  
  
  if(i >= 0) {                                      // Checks if RT_find_space_ind() actually found something. AKA if RT has an available entry.
    if ((bp = Vfind_first_fit(asize)) != NULL) {    // Iterates over the heap to find an unallocated block with enough size.
        place(bp,asize);                            // Allocates the block.
        bloc_alloc++;
        raw_byt_alloc += (size); //update the raw bytes allocated
        pd_byt_alloc += (asize-DSIZE); //update padded bytes
        RT[i] = bp;
        RTbp = &RT[i];
        return RTbp;
    }
}
  tot_fail++;
  return NULL;                                      // Returns NULL if no space found to allocate.
  

}

/*  VFree frees both a block of memory with address addr, and its
    corresponding RT entry. Also performs the coalescing and compaction
    of the heap if it leaves a gap.
*/
void VFree (addrs_t *addr) {
    // REMEMBER: *addr dereferences the pointer to the RT element.
    tot_free++;
     if (*addr == 0){
        tot_fail++;
        return;
     }
    // Gets address of next block
    addrs_t next_bp = NEXT_BLKP(*addr);     // Creates address to the block after addr.
    addrs_t curr_bp = *addr;                // Copies the addr_t that addr is pointing to for loop purposes.
    if (addr == NULL || *addr == NULL)      // If addr or *addr is null, return to avoid changes. Avoid errors when trying to dereference a null pointer.
        return;

    size_t size = GET_SIZE (HDRP (*addr));  // Gets size of *addr (adjusted to be aligned with overhead) from its header
    if (heap_base == 0) {
         VInit(size);
    }
    // Updates the header and footer of *addr to be unallocated.
    PUT (HDRP (*addr), PACK (size, 0));     
    PUT (FTRP (*addr), PACK (size, 0));

    // COMPACTION LOOP: While loop runs until it detects there are no more allocated blocks after its current block. AKA, it compacts.
    // Essentially swaps the free block forward with the allocated blocks until it reaches the last unallocated block.
    while(GET_ALLOC(HDRP (next_bp)) != 0) {
        size_t next_bp_size = GET_SIZE(HDRP (next_bp));     // Gets size of next_bp
        size_t curr_size = GET_SIZE(HDRP (curr_bp));        // Gets size of curr_bp
        // This code swaps blocks.
        PUT (HDRP (curr_bp), PACK (next_bp_size, 1));       // Calculates and puts the header of next_bp in curr_bp's location.
        PUT (FTRP (curr_bp), PACK (next_bp_size, 1));       // Same as above, but with the footer.
        memcpy(curr_bp, next_bp, next_bp_size);             // Copies next_bp's data and moves it to curr_bp's location.
        next_bp = NEXT_BLKP(curr_bp);                       // next_bp updates its location to the new next block of curr_bp (which is overwritten with next_bp's data).
        PUT (HDRP (next_bp), PACK (curr_size, 0));          // Calculates and puts the header of curr_bp into next_bp's location.
        PUT (FTRP (next_bp), PACK (curr_size, 0));          // Same as above, but with footer.
    
        curr_bp = next_bp;                                  // curr_bp now points to its next block.
        next_bp = NEXT_BLKP(next_bp);                       // increments loop, next_bp points to its next block.
         
    }
    // RT update loop
    // This loop goes over RT. If it detects an address that is more than *addr, it subtracts size from it.
    // This is to reflect the changed addresses of the allocated blocks due to compaction.
    int i;
    for(i = 0; i < RT_size; i++) {
        if(RT[i] > *addr) {
            RT[i] -= size;
        }
    }
    *addr = NULL;           // Sets RT entry to NULL.
    bloc_alloc--;
    pd_byt_alloc -= (size-DSIZE); //update the padded bytes allocated
    coalesce (next_bp);     // Coalesces free'd block with the big unallocated block if it exists.

}

/* Allocate size bytes from M2 using VMalloc(). 
   Copy size bytes of data into Malloc'd memory. 
   You can assume data is a storage area outside M2. 
   Return pointer to redirection table for Malloc'd memory.
*/
addrs_t *VPut (any_t data, size_t size) {
    
    addrs_t *RTbp = VMalloc(size);
    if(RTbp == NULL) {
        return 0;
    }
    memcpy(*RTbp,data,size);        // copies data from data to *RTbp.
    return RTbp;

    
}

/* Copy size bytes from the memory area, M2, to data address. The
    addr argument specifies a pointer to a redirection table entry. 
    As with VPut(), you can assume data is a storage area outside M2. 
    Finally, de-allocate size bytes of memory using VFree() with addr 
    pointing to a redirection table entry. 
*/
void VGet (any_t return_data, addrs_t *addr, size_t size) {
   memcpy(return_data,*addr,size);   // copies data from *addr to return_data.
   VFree(addr);  
   raw_byt_alloc -= (size);

}
 


/***
 *    ██████╗  █████╗ ██████╗ ████████╗     ██╗    ████████╗███████╗███████╗████████╗
 *    ██╔══██╗██╔══██╗██╔══██╗╚══██╔══╝    ███║    ╚══██╔══╝██╔════╝██╔════╝╚══██╔══╝
 *    ██████╔╝███████║██████╔╝   ██║       ╚██║       ██║   █████╗  ███████╗   ██║   
 *    ██╔═══╝ ██╔══██║██╔══██╗   ██║        ██║       ██║   ██╔══╝  ╚════██║   ██║   
 *    ██║     ██║  ██║██║  ██║   ██║        ██║       ██║   ███████╗███████║   ██║   
 *    ╚═╝     ╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝        ╚═╝       ╚═╝   ╚══════╝╚══════╝   ╚═╝   
 *                                                                                   
 */



/***
 *    ██████╗  █████╗ ██████╗ ████████╗    ██████╗     ████████╗███████╗███████╗████████╗
 *    ██╔══██╗██╔══██╗██╔══██╗╚══██╔══╝    ╚════██╗    ╚══██╔══╝██╔════╝██╔════╝╚══██╔══╝
 *    ██████╔╝███████║██████╔╝   ██║        █████╔╝       ██║   █████╗  ███████╗   ██║   
 *    ██╔═══╝ ██╔══██║██╔══██╗   ██║       ██╔═══╝        ██║   ██╔══╝  ╚════██║   ██║   
 *    ██║     ██║  ██║██║  ██║   ██║       ███████╗       ██║   ███████╗███████║   ██║   
 *    ╚═╝     ╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝       ╚══════╝       ╚═╝   ╚══════╝╚══════╝   ╚═╝   
 *                                                                                       
 */




/***
 *      _   _                  _             _              ____       _      _         _   _     _       __  __       _        ____  
 *     | | | | ___ _   _      | |_   _ _ __ (_) ___  _ __  |  _ \  ___| | ___| |_ ___  | |_| |__ (_)___  |  \/  | __ _(_)_ __  / /\ \ 
 *     | |_| |/ _ \ | | |  _  | | | | | '_ \| |/ _ \| '__| | | | |/ _ \ |/ _ \ __/ _ \ | __| '_ \| / __| | |\/| |/ _` | | '_ \| |  | |
 *     |  _  |  __/ |_| | | |_| | |_| | | | | | (_) | |    | |_| |  __/ |  __/ ||  __/ | |_| | | | \__ \ | |  | | (_| | | | | | |  | |
 *     |_| |_|\___|\__, |  \___/ \__,_|_| |_|_|\___/|_|    |____/ \___|_|\___|\__\___|  \__|_| |_|_|___/ |_|  |_|\__,_|_|_| |_| |  | |
 *                 |___/                                                                                                       \_\/_/ 
 */
void main (int argc, char **argv) {

///////////////////////////////////////////////////////////////////////////////

                        //PART 1 HEAP CHECKER//

//////////////////////////////////////////////////////////////////////////////
  int i, n;
  char s[80];
  addrs_t addr1, addr2,addr3;
  char data[80];
  int mem_size = 1000000; // Set DEFAULT_MEM_SIZE to 1<<20 bytes for a heap region
  unsigned long tot_alloc_time, tot_free_time;
  unsigned long start, finish;
  tot_alloc_time = 0;
  tot_free_time = 0;
  if  (argc > 2) {
    fprintf (stderr, "Usage: %s [memory area size in bytes]\n", argv[0]);
    exit (1);
  }
  else if (argc == 2)
    mem_size = atoi (argv[1]);

  Init (mem_size);

  for (i = 0;i<(mem_size); i++) {
    n = sprintf (s, "String 1, the current count is %d\n", i);
    rdtsc(&start);
    addr1 = Put (s, n+1);
    rdtsc(&finish);
    tot_alloc_time += finish - start;
    
    if (addr1){
    rdtsc(&start);
    Get ((any_t)data, addr1, n+1);
    rdtsc(&finish);
    tot_free_time += finish - start;
    }
    }



   printf("\n<<Part 1 for Region M1>>\n");
   printf("Number of allocated blocks: %ld\n",bloc_alloc);

   printf("Number of free blocks: %ld\n",bloc_free);
   printf("Raw total number of bytes allocated: %d\n",raw_byt_alloc);
   printf("Padded total number of bytes allocated: %d\n",pd_byt_alloc);
   raw_byt_free = mem_size-(12)-raw_byt_alloc;
   printf("Raw total number of bytes free: %d\n",raw_byt_free);
   al_byt_free = mem_size-(12)-pd_byt_alloc;
   printf("Aligned total number of bytes free: %d\n",al_byt_free);
   printf("Total number of Malloc requests: %d\n",tot_malloc);
   printf("Total number of Free requests: %d\n",tot_free);
   printf("Total number of request failures: %d\n",tot_fail);
   printf("Average clock cycles for a Malloc request: %d\n",tot_alloc_time/tot_malloc);
   int x;
   if(tot_free == 0) {         //to avoid flaoting point exception we have to make sure denominator is not zero
        x = 0;
    }
    else {
        x = tot_free_time/tot_free;
    }
   printf("Average clock cycles for a Free request: %d\n",x);
   printf("Total clock cycles for all requests: %d\n",(tot_free_time + tot_alloc_time)); 



//RESET ALL VARIABLES//////////

bloc_alloc = 0; //number of blocks allocated
bloc_free = 1; //number of free blocks
raw_byt_alloc = 0; //actuall bytes requested
pd_byt_alloc = 0; //padded total bytes allocated 
raw_byt_free = 0;
al_byt_free = 0;
tot_malloc = 0;
tot_free = 0;
tot_fail = 0;
avg_malloc = 0;
avg_free = 0;
tot_clock = 0;
tot_alloc_time = 0;
tot_free_time = 0;
start = 0;
finish = 0;

///////////////////////////////////////////////////////////////////////////////

                        //PART 2 HEAP CHECKER//

//////////////////////////////////////////////////////////////////////////////

  char s2[80];
  char data2[80];
  if  (argc > 2) {
    fprintf (stderr, "Usage: %s [memory area size in bytes]\n", argv[0]);
    exit (1);
  }
  else if (argc == 2)
    mem_size = atoi (argv[1]);

  VInit (mem_size);

  for (i = 0;i<(mem_size); i++) {
    n = sprintf (s2, "String 1, the current count is %d\n", i);
    rdtsc(&start);
    addr2 = VPut (s, n+1);
    rdtsc(&finish);
    tot_alloc_time += finish - start;
    
    if (addr2){
    rdtsc(&start);
    VGet ((any_t)data2, addr2, n+1);
    rdtsc(&finish);
    tot_free_time += finish - start;
    }
    }

   printf("\n<<Part 2 for Region M2>>\n");
   printf("Number of allocated blocks: %ld\n",bloc_alloc);
   printf("Number of free blocks: %ld\n",bloc_free);
   printf("Raw total number of bytes allocated: %d\n",raw_byt_alloc);
   printf("Padded total number of bytes allocated: %d\n",pd_byt_alloc);
   raw_byt_free = mem_size-(12)-raw_byt_alloc;
   printf("Raw total number of bytes free: %d\n",raw_byt_free);
   al_byt_free = mem_size-(12)-pd_byt_alloc;
   printf("Aligned total number of bytes free: %d\n",al_byt_free);
   printf("Total number of VMalloc requests: %d\n",tot_malloc);
   printf("Total number of VFree requests: %d\n",tot_free);
   printf("Total number of request failures: %d\n",tot_fail);
   printf("Average clock cycles for a VMalloc request: %d\n",tot_alloc_time/tot_malloc);
   if(tot_free == 0) {         //to avoid flaoting point exception we have to make sure denominator is not zero
        x = 0;
    }
    else {
        x = tot_free_time/tot_free;
    }
   printf("Average clock cycles for a VFree request: %d\n",x);
   printf("Total clock cycles for all requests: %d\n",(tot_free_time + tot_alloc_time)); 


   
}





