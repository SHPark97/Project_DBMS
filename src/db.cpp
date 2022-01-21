// Headers
#include "db.h"
#include "recovery.h"
extern int Table_Count;
extern int Current_TABLE_ID;
extern int TABLE[MAX_TABLE + 1];
extern string Path_Table[MAX_TABLE + 1];
extern buffer_info_t Buffer_Info;
extern LRU_List_t LRU_List;

// pathname으로 Table을 엶
// 여는것이 성공하면, unique table id를 반환
// 여는것이 실패하면, -1 반환
int open_table(char* pathname) {
    string Pathname = pathname;

    if (Pathname.length() > 100) {
        cout << "pathname 길이가 100을 초과합니다. " << endl;
        return -1;
    }

    for (int i=1; i <= Table_Count; i++) {
        if (strcmp(Path_Table[i].c_str(), Pathname.c_str()) == 0) {
            
            // 현재 사용하는 FD는 i번째 Table Id사용
            Current_TABLE_ID = i;
            return -1;   
        }
    }

    if (Table_Count == 10) {
        cout << "table 개수가 10개라 더 추가할 수 없습니다. " << endl;
        return -1;
    }

    file_open(pathname);
    
    int table_id = file_unique_table_id();

    if (table_id == 0) {
        cout << "table_id 가 0으로 데이터파일이 열리지 않았습니다." << endl;
        return -1;
    }

    Path_Table[Table_Count] = Pathname;

    return table_id;
}

// Key, Value를 데이터 파일의 맞는 위치에 삽입
// 삽입에 성공하면 0을, 실패하면 non-zero(-1) 값을 반환
int db_insert(int table_id, int64_t key, char* value) {
    Current_TABLE_ID = table_id;

    if (bpt_insert(key, value) == -1) {
        return -1;
    }

    close_table(table_id);
    
    //Insert 성공
    return 0;
}

// Key에 해당하는 Record를 찾고, 그 Value를 반환
// 찾고 값을 가져오는게 성공하면 0을, 실패하면 non-zero(-1) 값을 반환
// Record Structure를 위한 메모리 할당은 Caller Function에서 해야 함
int db_find(int table_id, int64_t key, char* ret_val, int trx_id) {
    Current_TABLE_ID = table_id;

    
    if (bpt_find(key, ret_val, trx_id) == 0) {
        //deadlock 발생 혹은 값을 찾지 못한 경우
        cout << "ABORT1" << endl;
        trx_abort(trx_id);
        cout << "ABORT2" << endl;
        return ABORT;
    }
    
    // find 성공
    return SUCCESS;
}

int db_update(int table_id, int64_t key, char* values, int trx_id) {
    Current_TABLE_ID = table_id;

    // log record는 bpt_update 함수 안에서 진행

    if (bpt_update(table_id, key, values, trx_id) == 0) {
        //deadlock 발생 혹은 값을 찾지 못한 경우
        cout << "ABORT1" << endl;
        trx_abort(trx_id);
        cout << "ABORT2" << endl;
        return ABORT;
    }

    //update 성공
    return SUCCESS;
}

// Key에 해당하는 Record를 찾고, 찾았으면 지움
// 지우는게 성공하면 0을, 실패하면 non-zero(-1) 값을 반환
int db_delete(int table_id, int64_t key) {
    Current_TABLE_ID = table_id;
    
    if (bpt_delete(key) == -1) {
        return -1;
    }

    // Delete 성공 시
    return 0;
}


// project3 추가함수
int init_db (int buf_num, int flag, int log_num, char* log_path, char* logmsg_path) {
    if (buf_num <= 0) {
        std::cout << "init_db 오류" << endl;
        return -1;
    }

   Buffer_Info.Buffer_List = new buffer_t[buf_num];
   Buffer_Info.Number_of_Buffer = buf_num;

   LRU_List.LRU_Head = 0;
   LRU_List.LRU_Tail = 0;

   for (int i = 0; i < buf_num; i++) {
        // frame을 붙여서 할당함
        Buffer_Info.Buffer_List[i].Frame;
        Buffer_Info.Buffer_List[i].table_id = 0;
        Buffer_Info.Buffer_List[i].Page_Number = PAGE_NUMBER_INITIALIZER;
        Buffer_Info.Buffer_List[i].is_dirty = Clean;
        Buffer_Info.Buffer_List[i].is_pinned = Unpinned;
        Buffer_Info.Buffer_List[i].LRU_Next = -1;
        Buffer_Info.Buffer_List[i].LRU_Prev = -1;
    }


    log_path_file_open(log_path);
    logmsg_path_file_open(logmsg_path);
    do_analysis_pass();
    if (do_redo_pass(flag, log_num)) {
        return 0;
    }
    if (do_undo_pass(flag, log_num)) {
        return 0;
    }
    
    return 0;
}

// Write all pages of this table from buffer to disk
// 성공시 0을, 실패시 non-zero(-1) 값 반환
int close_table(int table_id) {
    if (table_id <= 0 || Buffer_Info.Number_of_Buffer <= 0) {
        std::cout << "close table의 table id 가 0보다 작거나 같은 오류 혹은 버퍼 개수가 0이하인 오류" << endl;
        return -1;
    }

    for (int i = 0; i < Buffer_Info.Number_of_Buffer; i++) {
        if (Buffer_Info.Buffer_List[i].table_id == table_id && Buffer_Info.Buffer_List[i].is_pinned == Pinned) {
            std::cout << "pinned 된 페이지가 있어 쓸 수 없습니다." << endl;
            return -1;
        }
    }

    int For_Maintain = Current_TABLE_ID;

    for (int i = 0; i < Buffer_Info.Number_of_Buffer; i++) {
        // 프레임에 해당 테이블의 페이지가 존재하고 해당 페이지가 더티 페이지인 경우 그 페이지를 디스크에 써줌
        if (Buffer_Info.Buffer_List[i].table_id == table_id && Buffer_Info.Buffer_List[i].is_dirty == Dirty) {
            page_t Page;
            memcpy(&Page.Page_Buffer[0], &Buffer_Info.Buffer_List[i].Frame[0], 4096);

            // 쓸 테이블로 Id 변경 해줌
            Current_TABLE_ID = table_id;

            file_write_page(Buffer_Info.Buffer_List[i].Page_Number, &Page);

            // 써줬으니 dirty 만 clean으로 바꿔줌
            Buffer_Info.Buffer_List[i].is_dirty = Clean;

        }
    }

    // 다시 현재 테이블로 원상복구
    Current_TABLE_ID = For_Maintain;
    return 0;
}

// Flush all data from buffer and destroy allocated buffer
// 성공시 0을, 실패시 non-zero(-1) 값 반환
int shutdown_db() {
   if (Buffer_Info.Number_of_Buffer <= 0) {
        std::cout << "버퍼 개수가 0이하인 오류" << endl;
        return -1;
    }

    for (int i = 0; i < Buffer_Info.Number_of_Buffer; i++) {
        if (Buffer_Info.Buffer_List[i].is_pinned == Pinned) {
            std::cout << "pinned 된 페이지가 있어 쓸 수 없습니다." << endl;
            return -1;
        }
    }

    page_t Page;
    int For_Maintain = Current_TABLE_ID;

    for (int i = 0; i < Buffer_Info.Number_of_Buffer; i++) {
        // 프레임에 더티 페이지가 존재하는 경우 그 페이지를 디스크에 써줌
        if (Buffer_Info.Buffer_List[i].is_dirty == Dirty) {
            memcpy(Page.Page_Buffer, Buffer_Info.Buffer_List[i].Frame, 4096);

            // 쓸 테이블로 Id 변경 해줌
            Current_TABLE_ID = Buffer_Info.Buffer_List[i].table_id;

            file_write_page(Buffer_Info.Buffer_List[i].Page_Number, &Page);
        }
    }

    // 해당 프레임을 deallocate 해줌
    delete[] Buffer_Info.Buffer_List;

    // 다시 현재 테이블로 원상복구
    Current_TABLE_ID = For_Maintain;

    return 0;
}