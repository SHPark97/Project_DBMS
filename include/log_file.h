#ifndef __LOG_H__
#define __LOG_H__

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

// 로그 타입
#define BEGIN (0)
#define UPDATE (1)
#define COMMIT (2)
#define ROLLBACK (3)
#define COMPENSATE (4)

// 로그 메세지 타입
#define ANALYSIS_PASS_START (1)
#define ANALYSIS_PASS_SUCCESS (2)
#define REDO_PASS_START (3)
#define UNDO_PASS_START (4)
#define PASS_BEGIN (5)
#define REDO_PASS_UPDATE (6)
#define UNDO_PASS_UPDATE (7)
#define PASS_COMMIT (8)
#define PASS_ROLLBACK (9)
#define UNDO_PASS_COMPENSATE (10)
#define REDO_PASS_CONSIDER_REDO (11)
#define REDO_PASS_END (12)
#define UNDO_PASS_END (13)

#define LOG_SIZE (1000) // Log File에 있는 Log 개수가 1000개라고 가정

#define COMPENSATE_RECORD_SIZE (296)
#define UPDATE_RECORD_SIZE (288)
#define BCR_RECORD_SIZE (28)

#define ALREADYMADE (0)
#define UNMADE (1)

// 292 Byte [통합 Type]
// Type으로 구분 - BEGIN(0), UPDATE(1), COMMIT(2), ROLLBACK(3), COMPENSATE(4)
struct Log_type {
    // 공용
    int log_size;
    uint64_t LSN;
    uint64_t prev_LSN;
    int trx_id;
    int type;

    // Update, Compensate 용
    int table_id;
    uint64_t page_number;
    int offset;
    int data_length; 
    char old_image[120];
    char new_image[120];

    // Compensate용
    uint64_t next_undo_LSN;


    bool operator=(Log_type& log)
    {
        this->log_size = log.log_size;
        this->LSN = log.LSN;
        this->prev_LSN = log.prev_LSN;
        this->trx_id = log.trx_id;
        this->type = log.type;

        if (this->type == UPDATE || this->type == COMPENSATE)
        {
            this->table_id = log.table_id;
            this->page_number = log.page_number;
            this->offset = log.offset;
            this->data_length = log.data_length;
            memcpy(&this->old_image[0], &log.old_image[0], 120);
            memcpy(&this->new_image[0], &log.new_image[0], 120);
        }

        if (this->type == COMPENSATE)
        {
            this->next_undo_LSN = log.next_undo_LSN;
        }
    }
};



int log_path_file_open(char* log_path);
int logmsg_path_file_open(char* logmsg_path);
int log_file_read(char* log_path);
int log_file_write(Log_type& log);
int logmsg_write(int type, Log_type* log);

#endif