#include <stdlib.h>
#include <time.h>

#include "vmsim.h"


// Comment below to see more logs
#undef ALL
#undef DEBUG
#undef INFO

#undef NRU_TEST
#undef CHECK_PTE
#undef OPT_INFO
#undef OPT_DEBUG
#undef REFRESH_INFO
int numframes;
unsigned int *physical_frames;

// Page Table
unsigned int *page_table = NULL;
// Page Table Entry
struct pte_32 *pte = NULL;
// Opt Linked List
struct opt_list * * opt_table = NULL;

// Fifo page replacement current index
int current_index = -1;

int main(int argc, char *argv[])
{
   // Initiate random seed
   srand(time(NULL));

  /*
   * Add sanity check for input arguments
   * Open the memory trace file 
   * and store it in an array
   */
   
   FILE *file;
   if((argc >= 6) && (!strcmp(argv[1],"-n")) && (!strcmp(argv[3], "-a")) && (!strcmp(argv[4], "fifo") \
                  || !strcmp(argv[4], "rand") || !strcmp(argv[4], "opt") || !strcmp(argv[4], "nru")   \
                  || !strcmp(argv[4], "clock")))
   {
      numframes = atoi(argv[2]);
      if (!strcmp(argv[5], "-r"))
      {
         file = fopen(argv[7], "rb");
      }
      else
      {
         file = fopen(argv[5], "rb");
      }
      if(!file)
      {
         fprintf(stderr, "USAGE: %s -n <numframes> -a <fifo|opt|clock|nru|rand> <tracefile>\n", argv[0]);
         fprintf(stderr, "Error on opening the trace file\n");
         exit(1); 
      }
   }
   else
   {
      fprintf(stderr, "USAGE: %s -n <numframes> -a <fifo|opt|clock|nru|rand> <tracefile>\n", argv[0]);
      exit(1); 
   }

   //
   int flag_refresh = 0; // Simple 0/1 for whether or not we care about refreshing R bits
   int refresh_rate = 0; // Refresh rate.
   if( !strcmp(argv[4], "nru") )
   {
      flag_refresh = 1;
      if (!strcmp(argv[5], "-r"))
      {
         int rate = atoi(argv[6]);
         if(rate <= 0)
         {
            fprintf(stderr, "-r <refresh rate> should be > 0!");
            exit(-1);
         }
         else
         {
            refresh_rate = rate;
         }
      }
	  else {
		  printf("Refresh rate needed to run NRU algorithm!");
		  exit(-2);
	  }
   } 
   /* 
    * Calculate the trace file's length
    * and read in the trace file
    * and write it into addr_arr and mode_arr arrays 
    */
   unsigned int numaccesses = 0;
   unsigned int addr = 0;
   unsigned char mode = NULL;
   
   
   // Opt algorithm table setup
   if( !strcmp(argv[4], "opt") )
   {
      opt_table = malloc(PT_SIZE_1MB * sizeof(struct opt_list *));
      if(!opt_table)
      {
         fprintf(stderr, "Error on mallocing opt table\n");
      }
      memset(opt_table, 0, PT_SIZE_1MB * PTE_SIZE_BYTES);
   }
   
   // Calculate number of lines in the trace file
   // Also, if we need the clairvoyant optimal page replacement
   // algorithm's metaphorical crystal ball, while counting the
   // number of lines, also store the next-occurrence nodes
   // 
   while(fscanf(file, "%x %c", &addr, &mode) == 2)
   {
      if (!strcmp(argv[4], "opt"))
      {
         struct opt_list * ot = opt_table[PTE32_INDEX(addr)];
		 if(!ot)
         {
            opt_table[PTE32_INDEX(addr)] = malloc(sizeof(struct opt_list));
            ot = opt_table[PTE32_INDEX(addr)];
			ot->head = malloc(sizeof(struct opt_node));
			ot->tail = ot->head;
			ot->head->line = numaccesses;
			ot->head->next = NULL;
         }
		 else
         {
            ot->tail->next = malloc(sizeof(struct opt_node));
			ot->tail = ot->tail->next;
			ot->tail->line = numaccesses;
			ot->tail->next = NULL;
         }
      }
      numaccesses++;
   }
   rewind(file);

   unsigned int address_array[numaccesses];
   unsigned char mode_array[numaccesses];
   unsigned int i = 0;

   // Store the memory accesses 
   while(fscanf(file, "%x %c", &address_array[i], &mode_array[i]) == 2)
   {
      #ifdef ALL
         printf("\'0x%08x %c\'\n", address_array[i], mode_array[i]);
      #endif
      i++;
   }

   if(!fclose(file))
   {
      ; // Noop
   }
   else
   {
      fprintf(stderr, "Error on closing the trace file\n");
      exit(1);
   }

   // Initialize the physical memory address space
   physical_frames = malloc(PAGE_SIZE_4KB * numframes);
   if(!physical_frames)
   {
      fprintf(stderr, "Error on mallocing physical frames\n");
      exit(1);
   }
   memset(physical_frames, 0, PAGE_SIZE_4KB * numframes);

   // Create the first frame of the frames linked list
   struct frame_struct *frame = malloc(sizeof(struct frame_struct));
   if(!frame)
   {
      fprintf(stderr, "Error on mallocing frame struct\n");
      exit(1);
   }
   memset(frame, 0, sizeof(struct frame_struct));

   // Store the head of frames linked list
   struct frame_struct *head = frame;
   struct frame_struct *hand = frame; // Clock hand for clock algorithm
   struct frame_struct *curr;


   // Initialize page table
   page_table = malloc(PT_SIZE_1MB * PTE_SIZE_BYTES);
   if(!page_table)
   {
      fprintf(stderr, "Error on mallocing page table\n");
   }
   memset(page_table, 0, PT_SIZE_1MB * PTE_SIZE_BYTES);

   struct pte_32 *new_pte = NULL;

   // Initialize the frames linked list
   for(i = 0; i < numframes; i++)
   {
      frame->frame_number = i;
      frame->physical_address = physical_frames + (i * PAGE_SIZE_4KB) / PAGE_SIZE_BYTES;
      frame->virtual_address = 0;
      frame->pte_pointer = NULL;
      #ifdef INFO
         printf("Frame#%d: Adding a new frame at memory address %ld(0x%08x)\n", i, frame->physical_address, frame->physical_address);
      #endif
      frame->next = malloc(sizeof(struct frame_struct));
      frame = frame->next;
      memset(frame, 0, sizeof(struct frame_struct));

   }
   
   unsigned int fault_address = 0;
   unsigned int previous_fault_address = 0;
   unsigned char mode_type = NULL;
   int hit = 0;
   int page2evict = 0;
   int numfaults = 0;
   int numwrites = 0;
   //numaccesses = 100;
   // Main loop to process memory accesses
   for(i = 0; i < numaccesses; i++)
   {
	   
      // Do we need to refresh referenced bits?
      if (flag_refresh && i % refresh_rate == 0)
      {
		 #ifdef REFRESH_INFO
		    printf("Refresh triggered: i: %d\n", i);
		 #endif
         curr = head;
         while(curr->next) {
            if(curr->pte_pointer) {
               #ifdef REFRESH_INFO
			      printf("Changing frame %d ref from %d to 0\n", curr->frame_number, curr->pte_pointer->referenced);
               #endif
               curr->pte_pointer->referenced = 0;
            }
            curr = curr->next;
         }
      }

      fault_address = address_array[i];
      mode_type = mode_array[i];
      hit = 0;
    
      // Perform the page walk for the fault address
      new_pte = (struct pte_32 *) handle_page_fault(fault_address);
      
      /*
       * Traverse the frames linked list    
       * to see if the requested page is present in
       * the frames linked list.
       */

      curr = head;
      while(curr->next)
      {
         if(curr->physical_address == new_pte->physical_address)
         {
            if(new_pte->present)
            {
               curr->virtual_address = fault_address;
			   new_pte->referenced = 1; // Set Ref bit?
               hit = 1;
            }
            break;
         }
         else
         {
            curr = curr->next;
         }

      }

      /* 
       * if the requested page is not present in the
       * frames linked list use the fifo page replacement
       * to evict the victim frame and
       * swap in the requested frame
       */  

      if(!hit)
      {
         // Fifo page replacement algorithm
         if(!strcmp(argv[4], "fifo"))
         {
            page2evict = fifo();
         }
         else if(!strcmp(argv[4], "rand"))
         {
            page2evict = rand_alg();
         }
         else if(!strcmp(argv[4], "clock"))
         {
            page2evict = clock_alg(head, hand);
         }
         else if(!strcmp(argv[4], "nru"))
         {
            page2evict = nru_alg(head);
         }
         else if(!strcmp(argv[4], "opt"))
         {
            page2evict = opt_alg(head);
         }

         /* Traverse the frames linked list to
          * find the victim frame and swap it out
          * Set the present, referenced, and dirty bits
          * and collect some statistics
          */

         curr = head;
         while(curr->next)
         {
            if(curr->frame_number == page2evict)
            {

               previous_fault_address = curr->virtual_address;
			   #ifdef OPT_DEBUG
                  printf("numfaults before increment: %d\n", numfaults);
               #endif
               numfaults++;
               #ifdef OPT_DEBUG
                  printf("numfaults after increment: %d\n", numfaults);
               #endif
			   // Make sure that we set present bit of pte_pointer before we change pte_pointer
			   if(curr->pte_pointer)
               {
                  curr->pte_pointer->present = 0;
                  if(curr->pte_pointer->dirty)
                  {
                     curr->pte_pointer->dirty = 0; // No longer dirty - we wrote to disk
                     numwrites++; // We wrote this to disk
                     #ifdef DEBUG
                        printf("%5d: page fault – evict dirty(0x%08x)accessed(0x%08x)\n", i, previous_fault_address, fault_address);
                     #endif
                  }
                  else
                  {
                     #ifdef DEBUG
                        printf("%5d: page fault – evict clean(0x%08x)accessed(0x%08x)\n", i, previous_fault_address, fault_address);
                     #endif
                  }
               }
               curr->pte_pointer = (struct pte_32 *) new_pte; // Set current PTE pointer to appropriate PTE
               new_pte->physical_address = curr->physical_address;
               new_pte->present = 1;
			   new_pte->referenced = 1; // We referenced this by accessing it
               curr->virtual_address = fault_address;
               if(mode_type == 'W')
               {
                  new_pte->dirty = 1; // A write in the page makes this page dirty
               }
			   break; // Don't traverse the linked list unneccessarily!
            }
            curr = curr->next; 
         }
      }
      else
      {
         curr->pte_pointer->referenced = 1; // Set referenced bit
         #ifdef DEBUG
            printf("%5d: page hit   – no eviction(0x%08x)\n", i, fault_address);
            printf("%5d: page hit   - keep page  (0x%08x)accessed(0x%08x)\n", i, new_pte->physical_address, curr->virtual_address);
         #endif         
      }
	  // OPT part
	  if(!strcmp(argv[4], "opt"))
      {
         #ifdef OPT_INFO
            printf("(B) VA : %d | FA: %d | OTH: %d\n", curr->virtual_address, fault_address, opt_table[PTE32_INDEX(fault_address)]->head);
         #endif
         struct opt_list * onh = opt_table[PTE32_INDEX(fault_address)];
		 onh->head = onh->head->next; // Not even concerned with freeing memory - no dynamic memory 
         #ifdef OPT_INFO
            printf("(A) VA : %d | FA: %d | OTH: %d\n", curr->virtual_address, fault_address, opt_table[PTE32_INDEX(fault_address)]->head);
         #endif
		 // I know this is probably frowned upon, but the program only ever needs
		 // memory that we initialize on the heap at the beginning and space on the stack
      }
   }

   printf("Algorithm:             %s\n", argv[4]);
   printf("Number of frames:      %d\n", numframes);
   if(flag_refresh)
   {
	   printf("Refresh rate:          %d\n", refresh_rate);
   }
   printf("Total memory accesses: %d\n", i);
   printf("Total page faults:     %d\n", numfaults);
   printf("Total writes to disk:  %d\n", numwrites);

   return(0);
}

struct frame_struct * handle_page_fault(unsigned int fault_address)
{
   pte = (struct pte_32 *) page_table[PTE32_INDEX(fault_address)];

   if(!pte)
   {
      pte = malloc(sizeof(struct pte_32));
      memset(pte, 0, sizeof(struct pte_32));
      pte->present = 0;
      pte->physical_address = NULL;
      page_table[PTE32_INDEX(fault_address)] = (unsigned int) pte;
   }

   #ifdef INFO
      printf("Page fault handler\n");
      printf("Fault address %d(0x%08x)\n", (unsigned int) fault_address, fault_address);
      printf("Page table base address %ld(0x%08x)\n", (unsigned int) page_table, page_table);
      printf("PTE offset %ld(0x%03x)\n", PTE32_INDEX(fault_address), PTE32_INDEX(fault_address));
      printf("PTE index %ld(0x%03x)\n",  PTE32_INDEX(fault_address) * PTE_SIZE_BYTES, PTE32_INDEX(fault_address) * PTE_SIZE_BYTES);
      printf("PTE virtual address %ld(0x%08x)\n", (unsigned int) page_table + PTE32_INDEX(fault_address), page_table + PTE32_INDEX(fault_address));

      printf("PAGE table base address %ld(0x%08x)\n", pte->physical_address, pte->physical_address);
      printf("PAGE offset %ld(0x%08x)\n", FRAME_INDEX(fault_address), FRAME_INDEX(fault_address));
      printf("PAGE index %ld(0x%08x)\n", FRAME_INDEX(fault_address) * PTE_SIZE_BYTES, FRAME_INDEX(fault_address) * PTE_SIZE_BYTES);
      printf("PAGE physical address %ld(0x%08x)\n", pte->physical_address + FRAME_INDEX(fault_address), pte->physical_address  + FRAME_INDEX(fault_address));
   #endif
   #ifdef CHECK_PTE
      printf("[PTE]: Fault address %d returns %d\n", fault_address, pte);
   #endif
   return ((struct frame_struct *) pte);
}

int fifo()
{
   current_index++;
   current_index = current_index % numframes;            
   return (current_index);
}


int rand_alg() // EZPZ:)
{
   current_index = rand() % numframes;
   return current_index;
}


// Find the best match based on R and D bits
int nru_alg(struct frame_struct * head)
{
   struct frame_struct * curr;
   // Search for R & D bits both being zero
   int best_option_r = 0;
   int best_option_d = 0; 
   
   while(1)
   {
      curr = head; // Restart at the beginning of the linked list
	  
	  // Iterate all indexed pages for a page that matches the best possible match
      while(curr->next) {
         if(!curr->pte_pointer) {
            curr->pte_pointer = (struct pte_32 *)handle_page_fault(curr->virtual_address); // Added this to deal with NPR at start of program
         }
         if(curr->pte_pointer->referenced == best_option_r && curr->pte_pointer->dirty == best_option_d) {
            #ifdef NRU_TEST
			   printf("[NRU]: Frame %d %dR %dD found\n", curr->frame_number, curr->pte_pointer->referenced, curr->pte_pointer->dirty);
			#endif
			return curr->frame_number;
         }
		 #ifdef NRU_TEST
			   printf("[NRU]: Frame %d traversed, found %dR %dD\n", curr->frame_number, curr->pte_pointer->referenced, curr->pte_pointer->dirty);
	     #endif
         curr = curr->next;
      }
	  
	  // 
      if(!best_option_r)
      {
         if(!best_option_d) { // Traversed 0R & 0D?
		    #ifdef NRU_TEST
			   printf("[NRU]: Finished 0R 0D traversal\n");
			#endif
            best_option_d = 1; // Traverse 0R & 1D
         } else { // Traversed 0R & 1D?
		    #ifdef NRU_TEST
			   printf("[NRU]: Finished 0R 1D traversal\n");
			#endif
            best_option_d = 0; // Traverse 0D
            best_option_r = 1; // 1R
         }
      }
      else // We traversed 1R & 0D
      {
         #ifdef NRU_TEST
			   printf("[NRU]: Frame %d %dR %dD (Default)\n", curr->frame_number, curr->pte_pointer->referenced, curr->pte_pointer->dirty);
         #endif
         return 0; // Don't even bother traversing. Use the first frame because all frames are 1R1D at this point (Using 0 is predictable - same result as traversal)
      }
   }
}

// In order to preserve single-node data structure
// I utilized a simple hack that shouldn't affect runtime
// that much - tradeoff of an extra O(1) operation is 
// to not have to create a separate linked list data structure
// - Essentially, I make a singly linked list a doubly linked list
// by redirecting null pointers to the head of the singly-linked list
int clock_alg(struct frame_struct * head, struct frame_struct * hand)
{
   struct frame_struct * curr = hand;
   while(1) { // Will be exited when we encounter a null reference
	   
      // In the beginning of the program, we need to be concerned about referencing the NULL pointer
      if(!hand->pte_pointer) {
         hand->pte_pointer = (struct pte_32 *)handle_page_fault(curr->virtual_address); // Deal with Null Pointer Reference possibility
      }
	  
	  // Handle encountering an unreferenced page
	  if(!hand->pte_pointer->referenced)
	  {
         int frame_number = hand->frame_number;
		 // Adjust clock hand before returning frame to replace (pseudo-doubly-linked list)
		 if(!hand->next)
         {
            hand = head;
         }
		 else
         {
            hand = hand->next;
         }
         return frame_number;
      }
	  // If we haven't found an unreferenced page
	  else
      {
         hand->pte_pointer->referenced = 0; // Dereference, but leave it a second chance!
		 // Pseudo-doubly-linked list
		 if(!hand->next)
         {
            hand = head;
         }
		 else
         {
            hand = hand->next;
         }
      }
   } // end while
}

// Utilizes future knowledge via opt_table opt_lists
int opt_alg(struct frame_struct * head)
{
   unsigned int frame = 0;
   unsigned int last_max = 0;
   unsigned int max = 0; // max of 0 so that nearly every comparison triggers the change
   struct frame_struct * curr = head;
   int current_frame = 0; // needed for iterating frames?
   while(curr)
   {
      #ifdef OPT_INFO
         printf("[OPT]:VA : %d    F#: %d\n", curr->virtual_address, curr->frame_number);
      #endif
      
	  // Get opt_list from opt_table
	  struct opt_list * op = opt_table[PTE32_INDEX(curr->virtual_address)];
	  
	  // If there are NO more accesses of a given frame
	  if(!op->head) 
	  {
         return curr->frame_number; // This frame will never be used again, return it!
      }
	  
      #ifdef OPT_INFO
         printf("[OPT]: max = %d      op->head->line = %d\n", max, op->head->line);
      #endif
	  
	  
	  max = MAX(max, op->head->line);
	  
	  if(max != last_max)
      {
         frame = curr->frame_number;
		 last_max = max;
      }
	  
	  curr = curr->next; // Set current frame to be the 
	  current_frame++; // Increment current frame count
	  if(!curr->next)
      {
         #ifdef OPT_INFO
            printf("Encountered !curr->next, returning frame %d\n", frame);
         #endif
         return frame;
	  }
   }
   //return ;
}


