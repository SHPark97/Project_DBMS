#ifndef __LOG_BUFFER_H__
#define __LOG_BUFFER_H__

#include "log_file.h"

// 인자로 전해진 로그를 로그 버퍼에 추가함
int log_buffer_add(Log_type& log);

// 로그 버퍼에 있는 값을 로그 DB로 내려줌
// [Commit, Page Eviction, Full log Buffer]시 이 함수 사용
int log_buffer_flush();

int get_log_lsn(int type);

#endif