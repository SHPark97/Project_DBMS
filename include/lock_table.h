#ifndef __LOCK_TABLE_H__
#define __LOCK_TABLE_H__

#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <iostream>
#include <map>

using namespace std;

#define SHARED (0)
#define EXCLUSIVE (1)
#define NONEXIST (0)
#define CASE2 (2)
#define CASE3 (3)
#define CASE4 (4)
#define ACQUIRED (1)
#define NONACQUIRED (0)

typedef struct table_entry table_entry;
typedef struct lock_t lock_t;

struct lock_t {
	/* NO PAIN, NO GAIN. */
	lock_t* 		prev_pointer;
	lock_t* 		next_pointer;
	table_entry* 	sentinel_pointer;

	int 			lock_mode;
	int 			trx_id;
	

	// trx api 추가 변수
	lock_t*			trx_next_lock_ptr;
	lock_t*			trx_prev_lock_ptr;


	// roll back 추가 변수
	int 			lock_acquired;
	int 			temp_record_id;
	char 			temp_record_value[120];
	char 			temp_new_value[120];
	int 			offset;
};

struct table_entry {
	int64_t 			table_id;
	int64_t 			record_id;
	lock_t* 			tail;
	lock_t* 			head;
	pthread_cond_t 		condition;
	pthread_mutex_t 	mutex;
};

struct key_entry {
    int64_t             table_id;
    int64_t             record_id;

	key_entry(int64_t _table_id, int64_t _record_id) : table_id(_table_id), record_id(_record_id) {}
	
	bool operator<(const key_entry& rhs) const {
		if (table_id != rhs.table_id) {
			return table_id < rhs.table_id;
		}
		return record_id < rhs.record_id;
	}
};

/* APIs for lock table */

// Lock Table 구현에 필요한 Data Structure나 Mutex Resource, Hash Table, Table Latch 등을 초기화
// 성공시 0을, 실패시 non-zero(-1) 반환
int 
init_lock_table();

// Record Lock을 획득하는데 사용되는 함수이며 인자로 Table index key를 받음
// 동작 : 하나의 Lock Object를 생성해 해당 Record의 Lock List에 매달아 준 뒤 다음을 진행
// -1. 매단 Record에 대한 Lock이 이미 다른 쓰레드에 의해 잡혀있는 경우 [대기(Sleep) - 이 전 쓰레드가 Lock을 Release할 때 까지]
// -2. 매단 Record가 다른 쓰레드에 의해 잡혀있지 않는 경우 - [Lock이 획득 - 생성했던 Lock Object의 주소값을 리턴]
// 에러 발생시 NULL 리턴
lock_t* 
lock_acquire(int table_id, int64_t key, int trx_id, int lock_mode);

// 획득했던 Record Lock을 해제할 때 사용. Lock Object를 넘겨받아 이 Lock Object가 소속되어 있던 Lock List로 부터 제거
// -1. 만약 해제한 Lock Object 뒤에 다른 Lock Object가 있는 경우 - [Wake Up - 그 Lock Object의 주인 쓰레드를 깨워줌]
// 성공시 0을, 실패시 non-zero(-1) 반환
int 
lock_release(lock_t* lock_obj);

#endif /* __LOCK_TABLE_H__ */
