#include "lock_table.h"
#include "trx.h"

pthread_mutex_t lock_table_latch;
map<key_entry, table_entry> lock_table;


int
init_lock_table()
{
	lock_table_latch = PTHREAD_MUTEX_INITIALIZER;

	return 0;
}

lock_t*
lock_acquire(int table_id, int64_t key, int trx_id, int lock_mode)
{
	pthread_mutex_lock(&lock_table_latch);

	map<key_entry, table_entry>::iterator 		iter;
	key_entry hash_key = 						{table_id, key};
	iter = 										lock_table.find(hash_key);

	// Case1) table_entry이 빈 경우 혹은 table_entry의 [head]와 [tail]이 빈 경우
	if (iter == lock_table.end() || 
			(iter != lock_table.end() &&
			 lock_table[hash_key].tail == NULL)) {

		if (iter == lock_table.end()) {
			table_entry* hash_value = 				new table_entry;
			hash_value->record_id =	 				hash_key.record_id;
			hash_value->table_id = 					hash_key.table_id;
			hash_value->head = 						NULL;
			hash_value->tail = 						NULL;
			hash_value->condition = 				PTHREAD_COND_INITIALIZER;

			lock_table.insert(pair<key_entry, table_entry>(hash_key, *hash_value));
		}

		lock_t* lock_object = 					new lock_t;

		lock_object->next_pointer = 			NULL;
		lock_object->prev_pointer = 			NULL;
		lock_object->sentinel_pointer =			&lock_table[hash_key];
		lock_object->trx_id = 					trx_id;
		lock_object->lock_mode = 				lock_mode;
		lock_object->lock_acquired = 			ACQUIRED;

		// trx 연결
		trx_link(lock_object);

		lock_table[hash_key].head = 			lock_object;
		lock_table[hash_key].tail = 			lock_object;


		pthread_mutex_unlock(&lock_table_latch);
		return lock_object;
	}


	// Case2) [tail]이 존재하는 경우 = 앞에 Lock Object가 있는 경우
	if (lock_table[hash_key].tail != NULL) {
		lock_t* new_lock_object =	 					new lock_t;
		new_lock_object->next_pointer = 				NULL;
		new_lock_object->prev_pointer = 				lock_table[hash_key].tail;
		new_lock_object->sentinel_pointer = 			&lock_table[hash_key];
		new_lock_object->lock_mode =					lock_mode;
		new_lock_object->trx_id = 						trx_id;
		new_lock_object->lock_acquired = 				NONACQUIRED;


		// trx 연결
		trx_link(new_lock_object);


		lock_table[hash_key].tail->next_pointer = 		new_lock_object;
		lock_table[hash_key].tail = 					new_lock_object;


		// DeadLock 확인용 - deadlock 걸리면 null 반환
		// trx_deadlock_detect는 deadlock 있으면 1, 없으면 0 반환
		// Deadlock은 새로운 lock object를 붙인 뒤 검사 진행
		if (trx_deadlock_detect(new_lock_object)) {
			pthread_mutex_unlock(&lock_table_latch);
			return NULL;
		}


		// 새로 붙는 Lock Object가 Shared 인 경우
		if (new_lock_object->lock_mode == SHARED) {
			int check_exclusive = 0;
			int check_same_trx_id = 0;
			lock_t* Temp;
			lock_t* same_trx_lock;
			Temp = new_lock_object->sentinel_pointer->head;

			// 앞에 Exclusive가 있는지 확인
			while (Temp != new_lock_object) {
				if (Temp->lock_mode == EXCLUSIVE)
					check_exclusive++;

				if (Temp->trx_id == new_lock_object->trx_id) {
					check_same_trx_id++;
					same_trx_lock = Temp;
				}
				
				if (Temp->next_pointer != NULL)
					Temp = Temp->next_pointer;
				else
					break;
			}

			// 앞에 같은 trx id가 있는 경우
			if (check_same_trx_id != 0) {
				// (lock 을 매달지 않음)
				lock_table[hash_key].tail = 					new_lock_object->prev_pointer;
				lock_table[hash_key].tail->next_pointer = 		nullptr;


				// 앞서 trx table에 연결했던 new lock object를 제거해줌
				trx_delete(new_lock_object);

				delete new_lock_object;


				// cond wait 하지 말고 find를 진행할 수 있게 함
				// lock object는 앞서 획득한 lock object를 반환


				pthread_mutex_unlock(&lock_table_latch);
				return same_trx_lock;
			}


			// 앞에 Shared만 있으면 그냥 사용
	

			// 앞에 Exclusive가 있으면 대기
			else if (check_exclusive != 0) {

				// 바로 앞에 같은 trx id가 있으면 wait 하지 말고 실행 (wait은 앞의 같은 trx id가 진행하므로)
				if (new_lock_object->trx_id == new_lock_object->prev_pointer->trx_id) {
					// 행동 없음
				}


				// 바로 앞에 같은 trx id가 없다면 대기
				else {

					while(check_exclusive != 0) {
						pthread_cond_wait(&lock_table[hash_key].condition, &lock_table_latch);

						check_exclusive = 0;
						Temp = new_lock_object->sentinel_pointer->head;
						while (Temp != new_lock_object) {
							if (Temp->lock_mode == EXCLUSIVE)
								check_exclusive++;
							
							if (Temp->next_pointer != NULL)
								Temp = Temp->next_pointer;
							else
								break;
						}
					}
				}
			}
		}

		// 새로 붙는 Lock Object가 Exclusive 인 경우 - 기존과 동일하게 Head가 될 때 일어나서 사용
		else if (new_lock_object->lock_mode == EXCLUSIVE) {
			// 앞에 같은 trx id가 있다면 SHARED 인지 EXCLUSIVE 인지 확인
			int check_same_trx_id = 0;
			int mode = SHARED;
			lock_t* Temp;
			lock_t* same_exclusive_lock;
			lock_t* shared_lock;
			Temp = new_lock_object->sentinel_pointer->head;


			// 앞에 trx id가 같은 Exclusive가 있는지 확인
			while (Temp != new_lock_object) {
				if (Temp->trx_id == new_lock_object->trx_id) {
					check_same_trx_id++;

					// 앞에 EXCLUSIVE Mode 가 한번이라도 있는 지 확인
					if (Temp->lock_mode == EXCLUSIVE) {
						same_exclusive_lock = Temp;
						mode = EXCLUSIVE;
					}

					// 앞에 SHARED Mode가 있으면 저장
					if (Temp->lock_mode == SHARED)
						shared_lock = Temp;
				}
				
				Temp = Temp->next_pointer;
			}


			// 앞에 동일한 trx id가 EXCLUSIVE가 있는 경우 => 앞의 같은 trx id의 lock으로 대체
			if (check_same_trx_id != 0 && mode == EXCLUSIVE) {
				// 새로 단 lock 제거 (lock 을 매달지 않음)
				lock_table[hash_key].tail = 					new_lock_object->prev_pointer;
				lock_table[hash_key].tail->next_pointer = 		nullptr;



				// 앞서 trx table에 연결했던 new lock object를 제거해줌
				trx_delete(new_lock_object);

				delete new_lock_object;

				// cond wait 하지 말고 update를 진행할 수 있게 함
				pthread_mutex_unlock(&lock_table_latch);
				return same_exclusive_lock;
			}


// 앞에 동일한 trx id가 있는데 SHARED 인 경우 => 조건 부로 사용해야 하므로 조건 검사
			else if (check_same_trx_id != 0 && mode == SHARED) {
				// lock을 매달기만 함
				
				// 앞의 trx id가 같은 Shared lock의 앞 쪽 검사
				lock_t* temp1 = new_lock_object->sentinel_pointer->head;
				int exclude_check1 = 0, exist_check1 = 0;
				while (temp1 != NULL && temp1 != shared_lock) {
					if (temp1->lock_mode == EXCLUSIVE)
						exclude_check1++;

					exist_check1++;
					temp1 = temp1->next_pointer;
				}


				// 앞의 trx id가 같은 Shared lock의 Shared lock 뒤 쪽 검사
				lock_t* temp2 = shared_lock->next_pointer;
				int exclude_check2 = 0, exist_check2 = 0;
				while (temp2 != NULL && temp2 != new_lock_object) {
					if (temp2->lock_mode == EXCLUSIVE) {
						exclude_check2++;
					}

					exist_check2++;
					temp2 = temp2->next_pointer;
				}


				// S X 인 경우 => 그냥 사용
				if (exist_check1 == NONEXIST && exist_check2 == NONEXIST) {
					// 그냥 사용
				}


				// s S X 인 경우 => 기존과 같은 cond wait
				// x S X 인 경우 => 기존과 같은 cond wait
				// S s X 인 경우 => 기존과 같은 cond wait
				// s S s X 인 경우 => 기존과 같은 cond wait
				// x S s X 인 경우 => 기존과 같은 cond wait
				else if ( (exist_check1 != NONEXIST && exclude_check1 == NONEXIST && exist_check2 == NONEXIST) 
					|| (exist_check1 != NONEXIST && exclude_check1 != NONEXIST && exist_check2 == NONEXIST)
					|| (exist_check1 == NONEXIST && exist_check2 != NONEXIST && exclude_check2 == NONEXIST)
					|| (exist_check1 != NONEXIST && exclude_check1 == NONEXIST && exist_check2 != NONEXIST && exclude_check2 == NONEXIST)
					|| (exist_check1 != NONEXIST && exclude_check1 != NONEXIST && exist_check2 != NONEXIST && exclude_check2 == NONEXIST)
					)  {
					// S X 혹은 X 가 될 때 까지 대기



					//debugging용
					int cnt = 0;
					// 여기까지

					while( ( (lock_table[hash_key].head->next_pointer != new_lock_object )
						|| (lock_table[hash_key].head->trx_id != new_lock_object->trx_id) )
						&& (lock_table[hash_key].head != new_lock_object) ) {

						pthread_cond_wait(&lock_table[hash_key].condition, &lock_table_latch);
					}
				}

				// S x X 인 경우 => deadlock
				// s S x X 인 경우 => deadlock
				// x S x X 인 경우 => deadlock
				else if ( (exist_check1 == NONEXIST && exist_check2 != NONEXIST && exclude_check2 != NONEXIST) 
					|| (exist_check1 != NONEXIST && exclude_check1 == NONEXIST && exist_check2 != NONEXIST && exclude_check2 != NONEXIST)
				 	|| (exist_check1 != NONEXIST && exclude_check1 != NONEXIST && exist_check2 != NONEXIST && exclude_check2 != NONEXIST)
					)  {
						
					pthread_mutex_unlock(&lock_table_latch);
					return NULL;
				}
			}


			// 바로 앞에 같은 trx id가 없다면 대기
			else {
				while(lock_table[hash_key].head != new_lock_object) {
					pthread_cond_wait(&lock_table[hash_key].condition, &lock_table_latch);
				}
			}
		}


		new_lock_object->lock_acquired = ACQUIRED;
		pthread_mutex_unlock(&lock_table_latch);
		return new_lock_object;
	}

	pthread_mutex_unlock(&lock_table_latch);
	return nullptr;
}

int
lock_release(lock_t* lock_obj)
{
	pthread_mutex_lock(&lock_table_latch);
	// Case1 ) release하는 lock object가 제일 앞이고, Lock List에 Lock Object 자신만 있을 경우
	if (lock_obj->sentinel_pointer->head == lock_obj && lock_obj->next_pointer == NULL) {
		lock_t* temp_obj = 							lock_obj->sentinel_pointer->head;
		lock_obj->sentinel_pointer->head = 			NULL;
		lock_obj->sentinel_pointer->tail = 			NULL;

		// trx 연결 해제
		trx_delete(temp_obj);

		
		// Conditional varaible 처리
		delete temp_obj;
		pthread_mutex_unlock(&lock_table_latch);
		return 0;
	}


	// Case2 ) release하는 lock object가 제일 앞이고, Lock List에 Lock Object가 뒤에 더 있는 경우
	else if (lock_obj->sentinel_pointer->head == lock_obj && lock_obj->next_pointer != NULL){
		lock_t* temp_obj = 							lock_obj->sentinel_pointer->head;
		lock_obj->sentinel_pointer->head = 			lock_obj->sentinel_pointer->head->next_pointer;
		lock_obj->next_pointer->prev_pointer = 		NULL;


		// trx 연결 해제
		trx_delete(temp_obj);


		pthread_cond_broadcast(&lock_obj->sentinel_pointer->condition);
		delete temp_obj;
		pthread_mutex_unlock(&lock_table_latch);
		return 0;
	}

	// Case3 ) release하는 lock object가 tail에 있는 경우
	else if (lock_obj->sentinel_pointer->tail == lock_obj) {
		lock_t* temp_obj = 							lock_obj->sentinel_pointer->tail;
		lock_obj->sentinel_pointer->tail = 			lock_obj->sentinel_pointer->tail->prev_pointer;
		lock_obj->prev_pointer->next_pointer = 		NULL;

		
		// trx 연결 해제
		trx_delete(temp_obj);


		delete temp_obj;
		pthread_mutex_unlock(&lock_table_latch);
		return 0;
	}

	// Case4 ) release하는 lock object가 중간에 있는 경우
	else if (lock_obj->next_pointer != NULL && lock_obj->prev_pointer != NULL) {
		lock_t* temp_obj = 							lock_obj;
		lock_obj->prev_pointer->next_pointer = 		lock_obj->next_pointer;
		lock_obj->next_pointer->prev_pointer = 		lock_obj->prev_pointer;


		// trx 연결 해제
		trx_delete(temp_obj);


		pthread_cond_broadcast(&lock_obj->sentinel_pointer->condition);
		delete temp_obj;
		pthread_mutex_unlock(&lock_table_latch);
		return 0;
	}


	return -1;
}