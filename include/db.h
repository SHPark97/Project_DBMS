#ifndef DB_H
#define DB_H
#define ABORT (1)
#define SUCCESS (0)

#include "bpt.h"

// pathname으로 Table을 엶
// 여는것이 성공하면, unique table id를 반환
// 여는것이 실패하면, -1 반환
int open_table(char* pathname);

// Key, Value를 데이터 파일의 맞는 위치에 삽입
// 삽입에 성공하면 0을, 실패하면 non-zero(-1) 값을 반환
int db_insert(int table_id, int64_t key, char* value);

// Key에 해당하는 Record를 찾고, 그 Value를 반환
// 찾고 값을 가져오는게 성공하면 0을, 실패하면 non-zero(-1) 값을 반환
// Record Structure를 위한 메모리 할당은 Caller Function에서 해야 함
int db_find(int table_id, int64_t key, char* ret_val, int trx_id);

int db_update(int table_id, int64_t key, char* values, int trx_id);

// Key에 해당하는 Record를 찾고, 찾았으면 지움
// 지우는게 성공하면 0을, 실패하면 non-zero(-1) 값을 반환
int db_delete(int table_id, int64_t);

// 사용할 fd 적용
int init_db (int buf_num, int flag, int log_num, char* log_path, char* logmsg_path);
int close_table(int table_id);
int shutdown_db();


#endif