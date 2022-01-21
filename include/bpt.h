#ifndef BPT_H
#define BPT_H

#include "buffer.h"
#include "trx.h"

// 찾는 Key가 있을법한 페이지를 반환
pagenum_t bpt_find_page(int64_t key, char* ret_val);

// Find 성공시 값이 있는 Page를, 찾는 값이 없으면 0을 반환
pagenum_t bpt_find(int64_t key, char* ret_val, int trx_id);

// Insert 성공시 1을, 실패시 -1을 반환함 (중복일 시)
int bpt_insert(int64_t key, char* value);

// 삽입시 Internal 페이지에 값을 넣는 경우 사용
pagenum_t bpt_insert_into_parent(pagenum_t left, uint64_t key, pagenum_t right);


// Delete 성공시 1을, 실패시 -1을 반환함
int bpt_delete(int64_t key);

// 아래는 bpt_delete 관련 함수들
void bpt_delete_entry(pagenum_t Page_Number, int64_t key);

void bpt_remove_entry_from_page(pagenum_t Page_Number, int64_t key);

void bpt_adjust_root_page(pagenum_t Page_Number);

void bpt_delayed_merge(pagenum_t Page_Number, pagenum_t Neighbor_Page_Number, 
                int Neighbor_index, int k_prime);

void bpt_redistribute_page(pagenum_t Page_Number, pagenum_t Neighbor_Page_Number, 
                        int neighbor_index, int k_prime_index, int k_prime);

// 기존 bpt_find 를 bpt_find_original로 남겨둠
pagenum_t bpt_find_original(int64_t key, char* ret_val);



// project5 추가 함수
int bpt_update(int table_id, int64_t key, char* value, int trx_id);

#endif