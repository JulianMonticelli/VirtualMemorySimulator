/*
 * CS 1550: Header file for Virtual Memory skeleton code
 * with a single level 32-Bit page table and
 * fifo page replacement algorithm
 * (c) Mohammad H. Mofrad, 2017
 * (e) hasanzadeh@cs.pitt.edu
 *
 * Level 1 Page Table   PAGE FRAME
 * 31------------- 12 | 11 ------------- 0
 * |PAGE TABLE ENTRY  | PHYSICAL OFFSET  |
 * -------------------------------------
 * <-------20-------> | <-------12------->
 *
*/

#ifndef _VMSIM_INCLUDED_H
#define _VMSIM_INCLUDED_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define MAX(x, y) ((x) > (y) ? (x) : (y))

// Debug levels
#define ALL
#define DEBUG
#define INFO

#define OPT_DEBUG
#define NRU_TEST
#define CHECK_PTE
#define OPT_INFO
#define REFRESH_INFO

// 32-Bit page table constants
#define PAGE_SIZE_4KB   4096
#define PT_SIZE_1MB     1048576
#define PAGE_SIZE_BYTES 4
#define PTE_SIZE_BYTES  4

// Macros to extract pte/frame index
#define PTE32_INDEX(x)  (((x) >> 12) & 0xfffff)
#define FRAME_INDEX(x)  ( (x)        &   0xfff)

// 32-Bit memory frame data structure
struct frame_struct
{
   unsigned int frame_number;      // Number of physical frame ??
   unsigned int *physical_address; // Address of physical frame
   unsigned int virtual_address;   // Virtual address
   struct pte_32 * pte_pointer;    // Pointer of page table entry struct 
   struct frame_struct * next;     // Pointer to next frame_struct (linked list)
};

// 32-Bit Root level Page Table Entry (PTE) 
struct pte_32
{
   unsigned int present;           // Is the page present in allotted number of frames?
   unsigned int dirty;             // Is the page 'dirty' - meaning, do we need to write to hard disk if we swap out?
   unsigned int referenced;        // Referenced byte - can also be used for aging scheme
   unsigned int *physical_address; // The actualy physical address of the frame
};

// Opt algorithm node for memory lists, guarantees O(1) traversal
struct opt_list
{
   struct opt_node * head;
   struct opt_node * tail;
};

// Opt algorithm node
struct opt_node
{
   unsigned int line;
   struct opt_node * next;
};

// Handle page fault function
struct frame_struct * handle_page_fault(unsigned int);

// Page replacement algorithms
int fifo_alg();
int rand_alg();
int nru_alg(struct frame_struct * head);
int clock_alg(struct frame_struct * head, struct frame_struct * hand);
int opt_alg(struct frame_struct * head);
#endif
