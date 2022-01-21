#include "log_buffer.h"

#define LOG_BUFFER_SIZE (5000) // 5000개의 Log_Type을 담을수 있는 버퍼 설정

pthread_mutex_t log_buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
Log_type log_buffer_array[LOG_BUFFER_SIZE];
int log_buffer_cnt = 0; // 현재 로그 버퍼에 있는 개수

extern int log_LSN;

// 인자로 전해진 로그를 로그 버퍼에 추가함
int log_buffer_add(Log_type& log)
{
    // Lock을 잡고 로그 추가 함수 진행
    pthread_mutex_lock(&log_buffer_mutex);

    
    // 로그 버퍼가 다 찬 경우 log_buffer_flush로 Log File에 내려준 뒤 다시 처음부터 사용함
    if (log_buffer_cnt == LOG_BUFFER_SIZE)
    {
        log_buffer_flush();
        log_buffer_cnt = 0;
    }


    // 로그 버퍼에 로그를 넣어주고 개수를 1개 추가해줌
    log_buffer_array[log_buffer_cnt] = log;
    log_buffer_cnt++;


    // 로그 추가 함수가 끝났으면 잡은 Lock을 풀어줌
    pthread_mutex_unlock(&log_buffer_mutex);
}

// 로그 버퍼에 있는 값을 로그 DB로 내려줌
// [Commit, Page Eviction, Full log Buffer]시 이 함수 사용
int log_buffer_flush()
{
    // Lock을 잡고 플러시 함수 진행
    pthread_mutex_lock(&log_buffer_mutex);

    for (int i = 0; i < log_buffer_cnt; i++)
    {
        // 로그 버퍼 배열에 있는 로그를 0번부터 있는 개수까지 1개씩 다 써줌
        log_file_write(log_buffer_array[i]);
    }


    // 진행이 완료되면 log buffer가 비었으므로 cnt를 0으로 내려줌
    log_buffer_cnt = 0;

    // 플러시 함수가 끝났으면 잡은 Lock을 풀어줌
    pthread_mutex_unlock(&log_buffer_mutex);
}


int get_log_lsn(int type)
{
    pthread_mutex_lock(&log_buffer_mutex);

    int LSN = log_LSN;

    switch(type)
    {
        case BEGIN:
        case COMMIT:
        case ROLLBACK:
            log_LSN = log_LSN + BCR_RECORD_SIZE;
            break;

        case UPDATE:
            log_LSN = log_LSN + UPDATE_RECORD_SIZE;
            break;

        case COMPENSATE:
            log_LSN = log_LSN + COMPENSATE_RECORD_SIZE;
            break;
            
        default:
            break;
    }
    pthread_mutex_unlock(&log_buffer_mutex);

    return LSN;
}