#include "recovery.h"

// Analysis 단계에서 쓸 변수 선언
int analysis_trx_begin[NUMBER_OF_TRX];   // Begin된 TRX ID 배열
int analysis_trx_winner[NUMBER_OF_TRX];  // Winner인 TRX ID 배열
int analysis_trx_loser[NUMBER_OF_TRX];   // Loser인 TRX ID 배열

// Winner TRX 개수와 Loser TRX 개수
int winner_cnt = 0;
int loser_cnt = 0;

// log들이 저장된 배열을 가져옴
extern Log_type logs[LOG_SIZE];
extern int log_cnt; //log가 얼마나 저장한 변수

// Recovery의 Analysis 진행
int do_analysis_pass()
{
    // Analysis Pass의 시작을 알림
    logmsg_write(ANALYSIS_PASS_START, nullptr);

    int trx_cnt = 0;

    // 0에서(처음) 부터 시작
    for (int i = 0; i < log_cnt; i++)
    {
        if (logs[i].type == BEGIN)
        {
            // BEGIN Type이 있으면 해당 TRX ID를 begin 배열에 저장
            analysis_trx_begin[trx_cnt++] = logs[i].trx_id;
        }

        else if (logs[i].type == COMMIT)
        {
            // Commit Type이 있으면 해당 TRX ID를 Winner 배열에 저장
            analysis_trx_winner[winner_cnt++] = logs[i].trx_id;   
        }

        else if (logs[i].type == ROLLBACK)
        {
            // Rollback Type이 있으면 해당 TRX ID가 Abort이후 Roll Back이 완료된 것이므로 Winner 배열에 저장
            analysis_trx_winner[winner_cnt++] = logs[i].trx_id;
        }
    }

    // Loser가 몇개인지 찾음
    for (int i = 0; i < trx_cnt; i++)
    {
        // loser 인지 체크하기 위한 변수
        // loser 이면 check 는 0으로 유지됨
        int loser_check = 0;

        for (int j = 0; j < winner_cnt; j++)
        {
            // winner 이면 check 가 1로 바뀜
            if (analysis_trx_begin[i] == analysis_trx_winner[j])
                loser_check = 1;
        }

        if (loser_check == 0)
            analysis_trx_loser[loser_cnt++] = analysis_trx_begin[i];
    }


    // Log File의 끝까지 돌고 난 뒤 다 찾으면 Log Msg Write 해줌
    logmsg_write(ANALYSIS_PASS_SUCCESS, nullptr);
}


// Recovery의 Redo 진행
// flag가 1인 경우 log_limit_번째 redo 진행 이후엔 init_db가 Crash 나야함
// flag가 0인 경우 Crash 나지 않고 정상 진행
int do_redo_pass(int flag, int log_limit_)
{
    // Redo Pass의 시작을 알림
    logmsg_write(REDO_PASS_START, nullptr);
    
    int log_limit = log_cnt;

    page_t temp_page;
    char Buffer8[8];
    string str;
    char* path = new char[100];
    int page_LSN;
    int fd;

    // flag가 1이면 인자로 받은 수 까지만 Redo를 진행한다.
    if (flag == 1 && log_cnt > log_limit_)
        log_limit = log_limit_;

    for (int i = 0; i < log_limit; i++)
    {
        switch(logs[i].type)
        {
            case BEGIN :
                trx_begin(); // TRX를 Begin 시킴
                logmsg_write(PASS_BEGIN, &logs[i]); // LOG MSG를 써줌
                // Recovery Begin 은 Log를 발행하지 않음
                break;

            case COMMIT :
                trx_commit(logs[i].trx_id); // TRX를 Commit 시킴
                logmsg_write(PASS_COMMIT, &logs[i]); // LOG MSG를 써줌
                // Recovery Commit 은 Log를 발행하지 않음
                break;
            
            case UPDATE :
                sprintf(path, "DATA%02d.db", logs[i].table_id);
                fd = open(path, O_SYNC | O_RDWR, 0644);
                file_read_page_for_recovery(logs[i].page_number, &temp_page, fd);
                memcpy(&Buffer8[0], &temp_page.Page_Buffer[24], 8);
                page_LSN = atoi(Buffer8);
                
                // Redo 하는 LSN이 현재 Page의 LSN 보다 작거나 같으면 Consider-Redo 진행
                if (logs[i].LSN <= page_LSN)
                {
                    logmsg_write(REDO_PASS_CONSIDER_REDO, &logs[i]); // LOG MSG를 써줌
                    // Recovery Consider-Redo 은 Log를 발행하지 않음
                }

                // Redo 하는 LSN이 현재 Page의 LSN 보다 크면 UPDATE 진행
                else 
                {
                    logmsg_write(REDO_PASS_UPDATE, &logs[i]); // LOG MSG를 써줌
                    str = to_string(logs[i].LSN);
                    strcpy(Buffer8, str.c_str());
                    memcpy(&temp_page.Page_Buffer[24], &Buffer8[0], 8); // UPDATE한 LSN을 넣어줌
                    memcpy(&temp_page.Page_Buffer[logs[i].offset], &logs[i].new_image[0], 120); // UPDATE할 값을 넣어줌
                    file_write_page_for_recovery(logs[i].page_number, &temp_page, fd);
                    // Recovery Redo 는 Log를 발행하지 않음
                }

                close(fd);
                break;
            
            case COMPENSATE :
                sprintf(path, "DATA%02d.db", logs[i].table_id);
                fd = open(path, O_SYNC | O_RDWR, 0644);
                file_read_page_for_recovery(logs[i].page_number, &temp_page, fd);
                memcpy(&Buffer8[0], &temp_page.Page_Buffer[24], 8);
                page_LSN = atoi(Buffer8);
                
                // Redo 하는 LSN이 현재 Page의 LSN 보다 작거나 같으면 Consider-Redo 진행
                if (logs[i].LSN <= page_LSN)
                {
                    logmsg_write(REDO_PASS_CONSIDER_REDO, &logs[i]); // LOG MSG를 써줌
                    // Recovery Consider-Redo 은 Log를 발행하지 않음
                }

                // Redo 하는 LSN이 현재 Page의 LSN 보다 작거나 같으면 UPDATE 진행
                else 
                {
                    logmsg_write(REDO_PASS_UPDATE, &logs[i]); // LOG MSG를 써줌
                    str = to_string(logs[i].LSN);
                    strcpy(Buffer8, str.c_str());
                    memcpy(&temp_page.Page_Buffer[24], &Buffer8[0], 8); // UPDATE한 LSN을 넣어줌
                    memcpy(&temp_page.Page_Buffer[logs[i].offset], &logs[i].old_image[0], 120); // UPDATE할 값을 넣어줌
                    file_write_page_for_recovery(logs[i].page_number, &temp_page, fd);
                    // Recovery Redo 는 Log를 발행하지 않음
                }

                break;

            // Redo에선 Rollback으로 아무것도 하지 않음
            case ROLLBACK :
            default :
                break;
        }
    }


    if (flag == 1)
    {
        return 1; // 강제 종료를 의미함
    }

    // Redo Pass의 끝을 알림
    logmsg_write(REDO_PASS_END, nullptr);
    return 0; //정상 종료를 의미
}

// Recovery의 Undo 진행
// flag가 2인 경우 log_limit_번째 undo 진행 이후엔 init_db가 Crash 나야함
// flag가 0인 경우 Crash 나지 않고 정상 진행
int do_undo_pass(int flag, int log_limit_)
{
    // Undo Pass의 시작을 알림
    logmsg_write(UNDO_PASS_START, nullptr);
    
    int undo_log_cnt = 0; //100번째 로그를 찾기 위해 사용

    page_t temp_page;
    char Buffer8[8];
    string str;
    int page_LSN;
    int fd;
    int next_undo_LSN = 0;
    int k;
    char* path = new char[100];
    int lsn;

    // Undo는 뒤에서 부터 시작하며 Loser들에 대해서만 진행한다.
    for (int i = log_cnt - 1; i >= 0; i--)
    {
        if (undo_log_cnt >= log_limit_) // 100번째 로그가 나오면 종료
            break;


        // loser인지 확인하기 위한 변수로 0인 경우 loser임
        int loser_check = 0;

        for (int j = 0; j < loser_cnt; j++)
        {
            if (logs[i].trx_id == analysis_trx_loser[j])
                loser_check = 1;
        }


        // loser인 경우에만 Undo를 실행
        if (loser_check == 0)
        {
            switch(logs[i].type)
            {   
                case UPDATE :
                    sprintf(path, "DATA%02d.db", logs[i].table_id);
                    fd = open(path, O_SYNC | O_RDWR, 0644);
                    file_read_page_for_recovery(logs[i].page_number, &temp_page, fd);

                    // Compensate 과정 진행          
                    lsn = get_log_lsn(COMPENSATE);      
                    str = to_string(lsn);
                    strcpy(Buffer8, str.c_str());
                    memcpy(&temp_page.Page_Buffer[24], &Buffer8[0], 8); // UPDATE한 LSN을 넣어줌
                    memcpy(&temp_page.Page_Buffer[logs[i].offset], &logs[i].old_image[0], 120); // UPDATE할 값을 넣어줌
                    file_write_page_for_recovery(logs[i].page_number, &temp_page, fd);

                    Log_type log;
                    log.log_size = COMPENSATE_RECORD_SIZE;
                    log.LSN = lsn;
                    log.prev_LSN = trx_get_last_lsn(logs[i].trx_id, lsn);
                    log.trx_id = logs[i].trx_id;
                    log.type = COMPENSATE;
                    log.table_id = logs[i].table_id;
                    log.page_number = logs[i].page_number;
                    log.offset = logs[i].offset;
                    log.data_length = 120;
                    memcpy(&log.old_image[0], &logs[i].old_image[0], 120);
                    memcpy(&log.new_image[0], &logs[i].new_image[0], 120);

                    // 현재 Compensate 중인 Trx id가 가진 이전 write가 있는지 확인
                    // 이 작업은 next undo LSN을 찾기 위한 작업임
                    for (k = i - 1; k >= 0; k--)
                    {
                        if (logs[k].trx_id == logs[i].trx_id && logs[k].type == UPDATE)
                        {
                            log.next_undo_LSN = logs[k].LSN;
                        }

                        else if (logs[k].trx_id == logs[i].trx_id && logs[k].type == BEGIN)
                        {
                            log.next_undo_LSN = 0; // NIL을 의미함
                        }
                    }

                    // log buffer에 추가해줌
                    log_buffer_add(log);
                    logmsg_write(UNDO_PASS_UPDATE, &log); // LOG MSG를 써줌
                    undo_log_cnt++;

                    



                    // 만약 새로 발급한 compensate log record 에 next_undo_LSN이 없으면 Rollback Msg도 띄워줌
                    if (log.next_undo_LSN == 0)
                    {
                        Log_type rollback_log;
                        rollback_log.log_size = BCR_RECORD_SIZE;
                        rollback_log.LSN = get_log_lsn(ROLLBACK);
                        rollback_log.prev_LSN = trx_get_last_lsn(logs[i].trx_id, rollback_log.LSN);
                        rollback_log.trx_id = logs[i].trx_id;
                        rollback_log.type = ROLLBACK;

                        logmsg_write(PASS_ROLLBACK, &rollback_log); // LOG MSG를 써줌
                        log_buffer_add(rollback_log);
                        undo_log_cnt++;
                    }

                    close(fd);
                    break;

                
                case COMPENSATE :
                    // Next Undo LSN으로 찾아감
                    for (k = 0; k < log_cnt; k++)
                        if (logs[k].LSN == logs[i].next_undo_LSN)
                            break;

                    sprintf(path, "DATA%02d.db", logs[k].table_id);
                    fd = open(path, O_SYNC | O_RDWR, 0644);
                    file_read_page_for_recovery(logs[k].page_number, &temp_page, fd);

                    // Compensate 과정 진행                   
                    lsn = get_log_lsn(COMPENSATE);
                    str = to_string(lsn);
                    strcpy(Buffer8, str.c_str());
                    memcpy(&temp_page.Page_Buffer[24], &Buffer8[0], 8); // UPDATE한 LSN을 넣어줌
                    memcpy(&temp_page.Page_Buffer[logs[k].offset], &logs[k].old_image[0], 120); // UPDATE할 값을 넣어줌
                    file_write_page_for_recovery(logs[k].page_number, &temp_page, fd);

                    Log_type comp_log;
                    comp_log.log_size = COMPENSATE_RECORD_SIZE;
                    comp_log.LSN = lsn;
                    comp_log.prev_LSN = trx_get_last_lsn(logs[i].trx_id, lsn);
                    comp_log.trx_id = logs[k].trx_id;
                    comp_log.type = COMPENSATE;
                    comp_log.table_id = logs[k].table_id;
                    comp_log.page_number = logs[k].page_number;
                    comp_log.offset = logs[k].offset;
                    comp_log.data_length = 120;
                    memcpy(&comp_log.old_image[0], &logs[k].old_image[0], 120);
                    memcpy(&comp_log.new_image[0], &logs[k].new_image[0], 120);

                    // 현재 Compensate 중인 Trx id가 가진 이전 write가 있는지 확인
                    // 이 작업은 next undo LSN을 찾기 위한 작업임
                    for (int l = k - 1; l >= 0; l--)
                    {
                        if (logs[l].trx_id == logs[k].trx_id && logs[l].type == UPDATE)
                        {
                            comp_log.next_undo_LSN = logs[l].LSN;
                        }

                        else if (logs[l].trx_id == logs[k].trx_id && logs[l].type == BEGIN)
                        {
                            comp_log.next_undo_LSN = 0; // NIL을 의미함
                        }
                    }

                    // log buffer에 추가해줌
                    log_buffer_add(comp_log);
                    logmsg_write(UNDO_PASS_COMPENSATE, &comp_log); // LOG MSG를 써줌
                    undo_log_cnt++;


                    // 만약 새로 발급한 compensate log record 에 next_undo_LSN이 없으면 Rollback Msg도 띄워줌
                    if (comp_log.next_undo_LSN == 0)
                    {
                        Log_type rollback_log;
                        rollback_log.log_size = BCR_RECORD_SIZE;
                        rollback_log.LSN = get_log_lsn(ROLLBACK);
                        rollback_log.prev_LSN = trx_get_last_lsn(logs[i].trx_id, rollback_log.LSN);
                        rollback_log.trx_id = logs[k].trx_id;
                        rollback_log.type = ROLLBACK;
                        logmsg_write(PASS_ROLLBACK, &rollback_log); // LOG MSG를 써줌
                        log_buffer_add(rollback_log);
                        undo_log_cnt++;
                    }


                    break;


                // Begin, Commit, Rollback은 Undo에선 아무것도 하지 않음
                case BEGIN :
                case COMMIT :
                case ROLLBACK :
                default :
                     break;
            }
        }
    }


    if (flag == 2)
    {
        return 1; // 강제 종료를 의미함
    }

    // Undo Pass의 끝을 알림
    logmsg_write(UNDO_PASS_END, nullptr);
    return 0; //정상 종료를 의미
}