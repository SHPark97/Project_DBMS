#ifndef BUFFER_H
#define BUFFER_H

#include "file.h"
#include "log_buffer.h"
#define PAGE_NUMBER_INITIALIZER (1<<20)

enum Is_Dirty {Clean = 0, Dirty};
enum Is_Pinned {Unpinned = 0, Pinned};

struct buffer_t {
    char Frame[4096];
    int table_id;
    pagenum_t Page_Number;
    int is_dirty;
    int is_pinned;
    pthread_mutex_t page_latch;
    int LRU_Next;
    int LRU_Prev;
};

struct buffer_info_t {
    buffer_t* Buffer_List;
    int Number_of_Buffer;
};

struct LRU_List_t {
    int LRU_Head;
    int LRU_Tail;
};

void buffer_request(pagenum_t Page_Number, page_t* Page);
void buffer_write(pagenum_t Page_Number, page_t* Page);
void buffer_page_eviction(pagenum_t Page_Number, page_t* Page);
void buffer_close(pagenum_t Page_Number);
pagenum_t buffer_alloc_page();
void buffer_file_free_page(pagenum_t Page_Number);

#endif