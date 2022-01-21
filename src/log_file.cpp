#include "log_file.h"
#include "recovery.h"

// LOG DB FILE을 읽어온 LOG FILE 배열
Log_type logs[LOG_SIZE];
int log_cnt = 0;

int log_fd;
FILE* fp;
int log_fd_made = UNMADE;
int fp_made = UNMADE;


// Winner 배열과 Loser 배열, 각각의 개수를 가져온다
extern int analysis_trx_winner[NUMBER_OF_TRX];  // Winner인 TRX ID 배열
extern int analysis_trx_loser[NUMBER_OF_TRX];   // Loser인 TRX ID 배열
extern int winner_cnt;                          // Winner TRX 개수
extern int loser_cnt;                           // Loser TRX 개수

int log_LSN;





// 성공시 0 반환 / 실패시 -1 반환
int log_path_file_open(char* log_path) 
{
    log_fd = open(log_path, O_SYNC | O_RDWR, 0644);
    log_fd_made = ALREADYMADE; // log file은 이미 만들어져 있었는 경우

	// 여는 것이 실패할 경우
	if (log_fd == -1) 
    {
        log_fd_made = UNMADE; // log file이 없어서 새로 만든 경우
        log_fd = open(log_path, O_CREAT | O_SYNC | O_RDWR, 0644);
		
        if (log_fd == -1) 
        {
            cout << "cannot open files in pathname" << endl;
            return -1; // 여는 것 실패
        }
	}

	// 여는 것이 성공한 경우 0 반환
    return 0;
}


// 성공시 0 반환 / 실패시 -1 반환
int logmsg_path_file_open(char* logmsg_path) 
{
    fp = fopen(logmsg_path, "r+t");
    fp_made = ALREADYMADE; // log file은 이미 만들어져 있었는 경우

	// 여는 것이 실패할 경우
	if (fp == NULL) 
    {
        fp_made = UNMADE; // log file이 없어서 새로 만든 경우
        fp = fopen(logmsg_path, "w+t");
		
        if (fp == NULL) 
        {
            cout << "cannot open files in pathname" << endl;
            return -1; // 여는 것 실패
        }
	}

	// 여는 것이 성공한 경우 0 반환
    return 0;
}



int log_file_read(char* log_path)
{
    // 읽은 Log를 저장할 변수 선언
    Log_type Temp;

    // 읽을 지점을 나타냄
    int read_point = 0;

    // 파일의 끝을 위해 사용
    int read_cnt = 0;

    char Buffer[COMPENSATE_RECORD_SIZE];
    char Buffer4[4], Buffer8[8];

    log_LSN = 0;

    lseek(log_fd, read_point, SEEK_SET);
    // 파일이 끝이 아닌 것을 읽었을 때만 실행
    while ( (read_cnt = read( log_fd, Buffer, sizeof(Buffer) ) > 0 ) )
    {
        memcpy(&Buffer4[0], &Buffer[0], 4);
        Temp.log_size = atoi(Buffer4);

        memcpy(&Buffer8[0], &Buffer[4], 8);
        Temp.LSN = atoi(Buffer8);

        memcpy(&Buffer8[0], &Buffer[12], 8);
        Temp.prev_LSN = atoi(Buffer8);

        memcpy(&Buffer4[0], &Buffer[20], 4);
        Temp.trx_id = atoi(Buffer4);

        memcpy(&Buffer4[0], &Buffer[24], 4);
        Temp.type = atoi(Buffer4);


        // Type이 Update나 Compensate인 경우 정보를 더 저장함
        if (Temp.type == UPDATE || Temp.type == COMPENSATE)
        {
            memcpy(&Buffer4[0], &Buffer[28], 4);
            Temp.table_id = atoi(Buffer4);

            memcpy(&Buffer8[0], &Buffer[32], 8);
            Temp.page_number = atoi(Buffer8);

            memcpy(&Buffer4[0], &Buffer[40], 4);
            Temp.offset = atoi(Buffer4);

            memcpy(&Buffer4[0], &Buffer[44], 4);
            Temp.data_length = atoi(Buffer4);

            memcpy(&Temp.old_image[0], &Buffer[48], 120);
            memcpy(&Temp.new_image[0], &Buffer[168], 120);
        }


        // Type이 Compensate인 경우 정보를 더 저장함
        if (Temp.type == COMPENSATE) 
        {
            memcpy(&Buffer8[0], &Buffer[288], 8);
            Temp.next_undo_LSN = atoi(Buffer8);
        }


        // Type에 따라 다음 읽을 위치 offset을 정해줌
        switch(Temp.type)
        {
            case BEGIN :
            case COMMIT : 
            case ROLLBACK :
                read_point = read_point + BCR_RECORD_SIZE;
                break;

            case UPDATE :
                read_point = read_point + UPDATE_RECORD_SIZE;
                break;

            case COMPENSATE :
                read_point = read_point + COMPENSATE_RECORD_SIZE;
                break;
        }


        // 읽은 Log를 Log 배열에 넣어줌
        logs[log_cnt++] = Temp;

        // 다음 읽을 위치로 이동
        lseek(log_fd, read_point, SEEK_SET);
        log_LSN = read_point;
    }
}

// log_file_write는 log를 1개 1개 LOG DB에 써주는 함수임
int log_file_write(Log_type& log)
{
    char Buffer296[COMPENSATE_RECORD_SIZE];
    char Buffer4[4], Buffer8[8];
    string str;
    int write_size = 0;

    switch(log.type)
    {
        // Compensate 인 경우 COMPENSATE_RECORD_SIZE Byte 써주면 됨
        case COMPENSATE :
            str = to_string(log.next_undo_LSN);
            strcpy(Buffer8, str.c_str());
            memcpy(&Buffer296[288], &Buffer8[0], 8);

        // Update 인 경우 284 Byte 써주면 됨
        case UPDATE :
            memcpy(&Buffer296[168], &log.new_image[0], 120);
            memcpy(&Buffer296[48], &log.old_image[0], 120);

            str = to_string(log.data_length);
            strcpy(Buffer4, str.c_str());
            memcpy(&Buffer296[44], &Buffer4[0], 4);

            str = to_string(log.offset);
            strcpy(Buffer4, str.c_str());
            memcpy(&Buffer296[40], &Buffer4[0], 4);

            str = to_string(log.page_number);
            strcpy(Buffer8, str.c_str());
            memcpy(&Buffer296[32], &Buffer8[0], 8);

            str = to_string(log.table_id);
            strcpy(Buffer4, str.c_str());
            memcpy(&Buffer296[28], &Buffer4[0], 4);

        // Begin, Commit, Rollback 인 경우 24 Byte 써주면 됨
        case BEGIN :
        case COMMIT :
        case ROLLBACK :
            str = to_string(log.type);
            strcpy(Buffer4, str.c_str());
            memcpy(&Buffer296[24], &Buffer4[0], 4);

            str = to_string(log.trx_id);
            strcpy(Buffer4, str.c_str());
            memcpy(&Buffer296[20], &Buffer4[0], 4);

            str = to_string(log.prev_LSN);
            strcpy(Buffer8, str.c_str());
            memcpy(&Buffer296[12], &Buffer8[0], 8);

            str = to_string(log.LSN);
            strcpy(Buffer8, str.c_str());
            memcpy(&Buffer296[4], &Buffer8[0], 8);

            str = to_string(log.log_size);
            strcpy(Buffer4, str.c_str());
            memcpy(&Buffer296[0], &Buffer4[0], 4);

            // LOG FILE의 마지막에 이어서 써줌
            lseek(log_fd, 0, SEEK_END);

            // Type 별로 적어줄 사이즈 정해줌
            if (log.type == BEGIN || log.type == COMMIT || log.type == ROLLBACK) write_size = BCR_RECORD_SIZE;
            else if (log.type == UPDATE) write_size = UPDATE_RECORD_SIZE;
            else if (log.type == COMPENSATE) write_size = COMPENSATE_RECORD_SIZE;

            write(log_fd, Buffer296, write_size);
    }
}


int logmsg_write(int type, Log_type* log) {
    switch (type)
    {
        case ANALYSIS_PASS_START:
            fprintf(fp, "[ANALYSIS] Analysis pass start\n");
            break;


        case ANALYSIS_PASS_SUCCESS:
            fprintf(fp, "[ANALYSIS] Analysis success. Winner: ");
            for (int i = 0; i < winner_cnt; i++)
            {
                fprintf(fp, "%d ", analysis_trx_winner[i]);    
            }
            fprintf(fp, ", Loser: ");
            for (int i = 0; i < loser_cnt; i++)
            {
                fprintf(fp, "%d ", analysis_trx_loser[i]);    
            }
            fprintf(fp, "\n");
            break;



        case PASS_BEGIN:
            fprintf(fp, "LSN %lu [BEGIN] Transaction id %d\n", log->LSN, log->trx_id);
            break;

        case PASS_COMMIT:
            fprintf(fp, "LSN %lu [COMMIT] Transaction id %d\n", log->LSN, log->trx_id);
            break;

        case PASS_ROLLBACK:
            fprintf(fp, "LSN %lu [ROLLBACK] Transaction id %d\n", log->LSN, log->trx_id);
            break;



        case REDO_PASS_START:
            fprintf(fp, "[REDO] Redo pass start\n");
            break;

        case REDO_PASS_UPDATE:
            fprintf(fp, "LSN %lu [UPDATE] Transaction id %d redo apply\n", log->LSN, log->trx_id);
            break;

        case REDO_PASS_CONSIDER_REDO:
            fprintf(fp, "LSN %lu [CONSIDER-REDO] Transaction id %d\n", log->LSN, log->trx_id);
            break;

        case REDO_PASS_END:
            fprintf(fp, "[REDO] Redo pass end\n");
            break;




        case UNDO_PASS_START:
            fprintf(fp, "[UNDO] Undo pass start\n");
            break;

        case UNDO_PASS_UPDATE:
            fprintf(fp, "LSN %lu [UPDATE] Transaction id %d undo apply\n", log->LSN, log->trx_id);
            break;

        case UNDO_PASS_COMPENSATE:
            fprintf(fp, "LSN %lu [CLR] next undo lsn %lu\n", log->LSN, log->next_undo_LSN);
            break;

        case UNDO_PASS_END:
            fprintf(fp, "[UNDO] Undo pass end\n");
            break;
        
        default:
            break;
    }
}