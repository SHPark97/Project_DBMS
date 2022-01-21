#include "trx.h"
#include "db.h"
#include "log_buffer.h"

int global_trx_id = 0;
map<int, trx_t*> trx_table;
pthread_mutex_t trx_manager_latch = PTHREAD_MUTEX_INITIALIZER;


int trx_begin(void) {
    pthread_mutex_lock(&trx_manager_latch);    
    trx_t* trx_obj =            new trx_t;


    // trx object 초기화
    trx_obj->trx_id =           ++global_trx_id;
    trx_obj->head =             NULL;
    trx_obj->last_lsn =         0;
    trx_obj->active =           ON;

    // log buffer에 추가해줌
    Log_type log;
    log.log_size = BCR_RECORD_SIZE;
    log.LSN = get_log_lsn(BEGIN);
    log.prev_LSN = 0;
    log.trx_id = trx_obj->trx_id;
    log.type = BEGIN;
    log_buffer_add(log);
    logmsg_write(PASS_BEGIN, &log); // LOG MSG를 써줌

    trx_table.insert(pair<int, trx_t*>(trx_obj->trx_id, trx_obj));
    pthread_mutex_unlock(&trx_manager_latch);
    return trx_obj->trx_id;
}




int trx_commit(int trx_id) {
    pthread_mutex_lock(&trx_manager_latch);
    lock_t* temp = trx_table[trx_id]->head;
    lock_t* move;


    // trx list가 빌 때 까지 lock release를 진행해줌
    while (temp != NULL) {
        move = temp->trx_next_lock_ptr;
        pthread_mutex_unlock(&trx_manager_latch);
        lock_release(temp);
        pthread_mutex_lock(&trx_manager_latch);
        temp = move;
    }


    // 빈 trx list를 해제한 뒤, trx table에서 없애줌
    delete trx_table[trx_id];
    trx_table.erase(trx_id);

    // log buffer에 추가해줌
    Log_type log;
    log.log_size = BCR_RECORD_SIZE;
    log.LSN = get_log_lsn(COMMIT);
    log.prev_LSN = trx_table[trx_id]->last_lsn;
    trx_table[trx_id]->last_lsn = log.LSN;
    log.trx_id = trx_id;
    log.type = COMMIT;
    log_buffer_add(log);
    logmsg_write(PASS_COMMIT, &log); // LOG MSG를 써줌

    // log buffer를 Flush해줌 (WAL에 의해)
    log_buffer_flush();
    
    pthread_mutex_unlock(&trx_manager_latch);
}




void trx_delete(lock_t* lock_obj) {
    // trx list에서 lock object 제거 [trx table 변경]
    // 단순히 trx list에서만 제거, lock object 자체는 lock release에서 제거
    pthread_mutex_lock(&trx_manager_latch);


    // trx list에 달린 맨 앞 lock object(head)가 없어지는 경우
    if (lock_obj->trx_prev_lock_ptr == NULL) {
        // trx list에 혼자 달린 경우
        if (lock_obj->trx_next_lock_ptr == NULL) {
            trx_table[lock_obj->trx_id]->head =                 NULL;
        }


        // trx list에서 뒤에 무언가 달린 경우
        else {
            lock_obj->trx_next_lock_ptr->trx_prev_lock_ptr =    NULL;
            trx_table[lock_obj->trx_id]->head =                 lock_obj->trx_next_lock_ptr;
        }
    }


    // trx list에 달린 마지막 lock object(tail)가 없어지는 경우
    else if (lock_obj->trx_next_lock_ptr == NULL) {
        // trx list에 혼자 있는 건 위에서 먼저 처리하기 때문에 고려하지 않아도 됨
        lock_obj->trx_prev_lock_ptr->trx_next_lock_ptr =        NULL;
    }


    // trx list에 달린 중간 lock object가 없어지는 경우
    else {
        lock_obj->trx_next_lock_ptr->trx_prev_lock_ptr =        lock_obj->trx_prev_lock_ptr;
        lock_obj->trx_prev_lock_ptr->trx_next_lock_ptr =        lock_obj->trx_next_lock_ptr;
    }


    pthread_mutex_unlock(&trx_manager_latch);
}




void trx_link(lock_t* lock_obj) {
    //trx table과 lock object 연결 [trx table 변경]
    pthread_mutex_lock(&trx_manager_latch);

    // 처음 달리는 경우
    if (trx_table[lock_obj->trx_id]->head == NULL)
    {
        trx_table[lock_obj->trx_id]->head = lock_obj;
        lock_obj->trx_prev_lock_ptr = NULL;
        lock_obj->trx_next_lock_ptr = NULL;
    }

    else
    {
        lock_obj->trx_next_lock_ptr = 	                	    trx_table[lock_obj->trx_id]->head;
        lock_obj->trx_next_lock_ptr->trx_prev_lock_ptr =        lock_obj;
        trx_table[lock_obj->trx_id]->head =		                lock_obj;
        lock_obj->trx_prev_lock_ptr =		                    NULL;
    }

    pthread_mutex_unlock(&trx_manager_latch);
}




int trx_abort(int trx_id) {
    pthread_mutex_lock(&trx_manager_latch);
    lock_t* temp = trx_table[trx_id]->head;
    lock_t* move;

    while(temp != NULL) {
        // EXCLUSIVE 중 대기가 아닌 lock을 획득해 값을 변경한 경우에만
        // 값을 Roll back 해줌
        if (temp->lock_mode == EXCLUSIVE && temp->lock_acquired == ACQUIRED) {
            pthread_mutex_unlock(&trx_manager_latch);

            // log buffer에 추가해줌 (CLR 발급)
            Log_type clog;
            clog.log_size = COMPENSATE_RECORD_SIZE;
            clog.LSN = get_log_lsn(COMPENSATE);
            clog.prev_LSN = trx_table[trx_id]->last_lsn;
            trx_table[trx_id]->last_lsn = clog.LSN;
            clog.trx_id = trx_id;
            clog.type = COMPENSATE;
            clog.table_id = temp->sentinel_pointer->table_id;
            clog.page_number = temp->sentinel_pointer->record_id;
            clog.offset = temp->offset;
            clog.data_length = 120;
            memcpy(&clog.old_image[0], &temp->temp_record_value[0], 120);
            memcpy(&clog.new_image[0], &temp->temp_new_value[0], 120);
            log_buffer_add(clog);
            logmsg_write(UNDO_PASS_COMPENSATE, &clog); // LOG MSG를 써줌

            rollback_results(temp);
            pthread_mutex_lock(&trx_manager_latch);
        }

        move = temp->trx_next_lock_ptr;
        pthread_mutex_unlock(&trx_manager_latch);
        lock_release(temp);
        pthread_mutex_lock(&trx_manager_latch);
        temp = move;
    }

    // log buffer에 추가해줌
    Log_type log;
    log.log_size = BCR_RECORD_SIZE;
    log.LSN = get_log_lsn(ROLLBACK);
    log.prev_LSN = trx_table[trx_id]->last_lsn;
    trx_table[trx_id]->last_lsn = log.LSN;
    log.trx_id = trx_id;
    log.type = ROLLBACK;
    log_buffer_add(log);
    logmsg_write(PASS_ROLLBACK, &log); // LOG MSG를 써줌

    // log buffer를 Flush해줌 (WAL에 의해)
    log_buffer_flush();

    pthread_mutex_unlock(&trx_manager_latch);
}




void rollback_results(lock_t* lock_obj) {
    // trx_abort가 실행하는 함수로, 이미 mutex를 잡고 있기 때문에
    // 다시 mutex를 잡지 않아도 됨

    // 이미 trx id가 잡아둔 EXCLUSIVE lock을 다시 EXCLUSIVE로 잡는 것이므로
    // lock acquire는 lock 을 매달지 않고 먼저 잡은 EXCLUSIVE lock을 사용해 바로 진행할 수 있음

    char* temp = new char[120];
    memcpy(&temp[0], &lock_obj->temp_record_value[0], 120);

    db_update(lock_obj->sentinel_pointer->table_id, 
                lock_obj->temp_record_id, temp, lock_obj->trx_id);

}




int trx_deadlock_detect(lock_t* lock_obj) {
    pthread_mutex_lock(&trx_manager_latch);

    // Self Deadlock이 있는지 확인
	// 앞에 새로 붙는 lock obj와 trx id가 같은 lock obj 가 있다면
	// lock release 함수 내에서 자체적으로 deadlock 처리
	lock_t* self_temp = lock_obj->sentinel_pointer->head;
	int same_check = 0;
	while (self_temp != lock_obj) {
		if (self_temp->trx_id == lock_obj->trx_id)
        {
            pthread_mutex_unlock(&trx_manager_latch);
			return 0;
        }
		
		if (self_temp->next_pointer != NULL)
			self_temp = self_temp->next_pointer;
		else
		{
			break;
		}
		
	}


    /////
    // EXCLUSIVE인 경우 바로 DEADLOCK 확인
    if (lock_obj->lock_mode == EXCLUSIVE) {
        lock_t* temp1 = lock_obj->sentinel_pointer->head;


        while(temp1 != lock_obj) {
            lock_t* temp2 = trx_table[temp1->trx_id]->head;
            
            while(temp2 != NULL) {
                lock_t* temp3 = temp2;

                // 그 Record의 붙어있는 Shared 중 가장 앞으로 감
                while (temp3->lock_mode == SHARED && temp3->prev_pointer != NULL && temp3->prev_pointer->lock_mode == SHARED) {
                    temp3 = temp3->prev_pointer;
                }

                while (temp3 != NULL && temp3->prev_pointer != NULL) {
                    if (temp3->prev_pointer->trx_id == lock_obj->trx_id) {
                        pthread_mutex_unlock(&trx_manager_latch);
                        return 1;
                    }
                    

                    temp3 = temp3->prev_pointer;
                }

                if (temp2->trx_next_lock_ptr != NULL)
                    temp2 = temp2->trx_next_lock_ptr;
                else
                    break;
            }
            temp1 = temp1->next_pointer;
        }
    }

    // SHARED 인 경우 A B / B A 더라도 DEADLOCK이 아닌 경우가 있으므로
    // [AB 와 BA가 사이에 EXCLUSIVE가 없이 붙어 있는 경우]
    // 그 경우를 제외한 후 DEADLOCK 확인
    else {
        lock_t* temp = lock_obj;
        while (temp->lock_mode == SHARED && temp->prev_pointer != NULL && temp->prev_pointer->lock_mode == SHARED) {
            temp = temp->prev_pointer;
        }

        lock_t* temp1 = lock_obj->sentinel_pointer->head;
        while(temp1 != lock_obj) {
            lock_t* temp2 = trx_table[temp1->trx_id]->head;
            
            while(temp2 != NULL) {
                lock_t* temp3 = temp2;

                // 그 Record의 붙어있는 Shared 중 가장 앞으로 감
                while (temp3->lock_mode == SHARED && temp3->prev_pointer != NULL && temp3->prev_pointer->lock_mode == SHARED) {
                    temp3 = temp3->prev_pointer;
                }

                while (temp3 != NULL && temp3->prev_pointer != NULL) {
                    if (temp3->prev_pointer->trx_id == lock_obj->trx_id) {
                        pthread_mutex_unlock(&trx_manager_latch);
                        return 1;
                    }
                    

                    temp3 = temp3->prev_pointer;
                }

                if (temp2->trx_next_lock_ptr != NULL)
                    temp2 = temp2->trx_next_lock_ptr;
                else
                    break;
            }
            temp1 = temp1->next_pointer;
        }
    }

    pthread_mutex_unlock(&trx_manager_latch);
    return 0; // Deadlock 없으면 0 반환
}


int trx_get_last_lsn(int trx_id, int lsn)
{
    pthread_mutex_lock(&trx_manager_latch);

    int return_lsn = trx_table[trx_id]->last_lsn;
    trx_table[trx_id]->last_lsn = lsn;

    pthread_mutex_unlock(&trx_manager_latch);
    return return_lsn;
}