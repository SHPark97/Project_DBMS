#ifndef FILE_H
#define FILE_H

#define HEADER_PAGE (0)
#define EXPAND_SIZE (3)

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <iostream>
#include <sstream>
using namespace std;

#define MAX_TABLE 10

typedef uint64_t pagenum_t;
typedef uint32_t uint32t;

struct page_t {
	char Page_Buffer[4096];
};

// Allocate an on-disk page from the free page list
// Header Page Num은 1로 지정
pagenum_t file_alloc_page();

// Free an on-disk page to the free page list
void file_free_page(pagenum_t pagenum);

// Read an on-disk page into the in-memory page structure(dest)
void file_read_page(pagenum_t pagenum, page_t* dest);

// Write an in-memory page(src) to the on-disk page
void file_write_page(pagenum_t pagenum, const page_t* src);


// 추가 함수

// pathname을 열어줌
// 여는 것에 성공시 열린 fd를, 실패시 -1 반환
int file_open(const char* pathname);

// 현재 사용하는 Table이 몇 번째 Table인지 알려줌
// 확인 가능할 시 Table id를, 아무 것도 없으면 0을 반환
int file_unique_table_id();

void file_header_alloc();


//Recovery를 위한 함수
void file_read_page_for_recovery(pagenum_t pagenum, page_t* dest, int fd);

void file_write_page_for_recovery(pagenum_t pagenum, const page_t* src, int fd);


#endif