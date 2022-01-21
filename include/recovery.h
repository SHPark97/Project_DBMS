#ifndef __RECOVERY_H__
#define __RECOVERY_H__
#include "log_buffer.h"
#include "db.h"

#define NUMBER_OF_TRX (100)


// Recovery의 Analysis 진행
int do_analysis_pass();

// Recovery의 Redo 진행
int do_redo_pass(int flag, int log_limit_);

// Recovery의 Undo 진행
int do_undo_pass(int flag, int log_limit_);




#endif