#include "buffer.h"

using namespace std;

extern int Table_Count;
extern int Current_TABLE_ID;
extern int Table_Count;
extern int TABLE[MAX_TABLE + 1];
extern string Path_Table[MAX_TABLE + 1];
extern char Buffer8[8], Buffer4096[4096];

buffer_info_t Buffer_Info;
LRU_List_t LRU_List;
pthread_mutex_t buffer_manager_latch = PTHREAD_MUTEX_INITIALIZER;

// 사용할 페이지를 버퍼에 요청
// 사용할 페이지 번호가 버퍼풀에 있으면 Page에 값 저장해줌
// 사용할 페이지 번호가 버퍼풀에 없다면 file_read로 버퍼풀에 디스크 페이지 저장
void buffer_request(pagenum_t Page_Number, page_t* Page) {
    // buffer manager latch 획득

    pthread_mutex_lock(&buffer_manager_latch);

    // 만약 요청한 페이지가 프레임에 있다면 그 페이지를 읽어서 보내줌 (Cache Hit 인 경우)
    for (int i = 0; i < Buffer_Info.Number_of_Buffer; i++) {
        // 페이지가 clean 페이지 인 경우 그냥 읽어서 보내줌
        if (Buffer_Info.Buffer_List[i].Page_Number == Page_Number &&
                Buffer_Info.Buffer_List[i].is_dirty == Clean && Buffer_Info.Buffer_List[i].table_id == Current_TABLE_ID) {

            // 해당 page latch 획득
            pthread_mutex_lock(&Buffer_Info.Buffer_List[i].page_latch);


            Buffer_Info.Buffer_List[i].is_pinned = Pinned;
            memcpy(&Page->Page_Buffer[0], &Buffer_Info.Buffer_List[i].Frame[0], 4096);

            //LRU 연결
            if (LRU_List.LRU_Tail == LRU_List.LRU_Head) {
                //빈 공간
            }
            else if (i == LRU_List.LRU_Tail) {

                LRU_List.LRU_Tail = Buffer_Info.Buffer_List[LRU_List.LRU_Tail].LRU_Prev;
                
                Buffer_Info.Buffer_List[i].LRU_Next = LRU_List.LRU_Head;
                Buffer_Info.Buffer_List[i].LRU_Prev = -1;
                Buffer_Info.Buffer_List[LRU_List.LRU_Head].LRU_Prev = i;

                LRU_List.LRU_Head = i;
                Buffer_Info.Buffer_List[LRU_List.LRU_Tail].LRU_Next = -1;
            }
            else if (i == LRU_List.LRU_Head) {
                //빈 공간
            }
            else {
                Buffer_Info.Buffer_List[Buffer_Info.Buffer_List[i].LRU_Prev].LRU_Next = Buffer_Info.Buffer_List[i].LRU_Next;
                Buffer_Info.Buffer_List[Buffer_Info.Buffer_List[i].LRU_Next].LRU_Prev = Buffer_Info.Buffer_List[i].LRU_Prev;

                Buffer_Info.Buffer_List[i].LRU_Next = LRU_List.LRU_Head;
                Buffer_Info.Buffer_List[i].LRU_Prev = -1;
                Buffer_Info.Buffer_List[LRU_List.LRU_Head].LRU_Prev = i;
                LRU_List.LRU_Head = i;
            }


            // LRU 변경 후 buffer manager latch 제거
            pthread_mutex_unlock(&buffer_manager_latch);


            return;
        }

        // 페이지가 dirty 페이지 인 경우 disk에 sync를 해 주고, clean표시한 뒤, 보내줌
        else if (Buffer_Info.Buffer_List[i].Page_Number == Page_Number && 
                Buffer_Info.Buffer_List[i].is_dirty == Dirty && Buffer_Info.Buffer_List[i].table_id == Current_TABLE_ID) {
            // 해당 page latch 획득
            pthread_mutex_lock(&Buffer_Info.Buffer_List[i].page_latch);


            // disk에 sync를 먼저 해줌
            page_t Temp_Page;
            memcpy(&Temp_Page.Page_Buffer[0], &Buffer_Info.Buffer_List[i].Frame[0], 4096);
            // 어차피 현재 테이블에 페이지를 쓰니 테이블을 바꿀 필요 없음
            file_write_page(Buffer_Info.Buffer_List[i].Page_Number, &Temp_Page);

            // 그 후, 페이지를 clean 하게 만들고 인자로 받은 페이지 내용을 써줌
            Buffer_Info.Buffer_List[i].is_dirty = Clean;
            Buffer_Info.Buffer_List[i].is_pinned = Pinned;
            memcpy(&Page->Page_Buffer[0], &Buffer_Info.Buffer_List[i].Frame[0], 4096);

            //LRU 연결
            if (LRU_List.LRU_Tail == LRU_List.LRU_Head) {
                // 빈 공간
            }
            else if (i == LRU_List.LRU_Tail) {

                LRU_List.LRU_Tail = Buffer_Info.Buffer_List[LRU_List.LRU_Tail].LRU_Prev;

                Buffer_Info.Buffer_List[i].LRU_Next = LRU_List.LRU_Head;
                Buffer_Info.Buffer_List[i].LRU_Prev = -1;
                Buffer_Info.Buffer_List[LRU_List.LRU_Head].LRU_Prev = i;

                LRU_List.LRU_Head = i;
                Buffer_Info.Buffer_List[LRU_List.LRU_Tail].LRU_Next = -1;
            }
            else if (i == LRU_List.LRU_Head) {
                // 빈 공간
            }
            else {
                Buffer_Info.Buffer_List[Buffer_Info.Buffer_List[i].LRU_Prev].LRU_Next = Buffer_Info.Buffer_List[i].LRU_Next;
                Buffer_Info.Buffer_List[Buffer_Info.Buffer_List[i].LRU_Next].LRU_Prev = Buffer_Info.Buffer_List[i].LRU_Prev;

                Buffer_Info.Buffer_List[i].LRU_Next = LRU_List.LRU_Head;
                Buffer_Info.Buffer_List[i].LRU_Prev = -1;
                Buffer_Info.Buffer_List[LRU_List.LRU_Head].LRU_Prev = i;
                LRU_List.LRU_Head = i;
            }


            // LRU 변경 후 buffer manager latch 제거
            pthread_mutex_unlock(&buffer_manager_latch);


            return;
        }
    }

    // Cache Miss 인 경우 해당 페이지를 file_read로 읽어 버퍼 풀에 유지
    
    // 요청한 페이지가 해당 프레임에 없는 경우
    // 빈 프레임이 존재해 바로 읽어올 수 있는 경우
    for (int i = 0; i < Buffer_Info.Number_of_Buffer; i++) {
        if (Buffer_Info.Buffer_List[i].Page_Number == (PAGE_NUMBER_INITIALIZER)) {

            // 해당 page latch 획득
            pthread_mutex_lock(&Buffer_Info.Buffer_List[i].page_latch);



            page_t Temp_Page;
            // 현재 테이블을 읽는 것이므로 FD 바꿀 필요 없음
            file_read_page(Page_Number, &Temp_Page);

            memcpy(&Buffer_Info.Buffer_List[i].Frame[0], &Temp_Page.Page_Buffer[0], 4096);
            Buffer_Info.Buffer_List[i].is_pinned = Pinned;
            Buffer_Info.Buffer_List[i].is_dirty = Clean;
            Buffer_Info.Buffer_List[i].Page_Number = Page_Number;
            Buffer_Info.Buffer_List[i].table_id = Current_TABLE_ID;

            memcpy(&Page->Page_Buffer[0], &Buffer_Info.Buffer_List[i].Frame[0], 4096);
            
            // LRU 연결해 주기
            Buffer_Info.Buffer_List[i].LRU_Next = LRU_List.LRU_Head;
            Buffer_Info.Buffer_List[LRU_List.LRU_Head].LRU_Prev = i;
            LRU_List.LRU_Head = i;
            Buffer_Info.Buffer_List[LRU_List.LRU_Tail].LRU_Next = -1;
            Buffer_Info.Buffer_List[LRU_List.LRU_Head].LRU_Prev = -1;


            // LRU 변경 후 buffer manager latch 제거
            pthread_mutex_unlock(&buffer_manager_latch);


            return;
        }
    }

    // 현재 Current_table_id 유지
    int For_Maintain = Current_TABLE_ID;

    

    // 빈 프레임이 존재하지 않는 경우 LRU policy 적용
    int victim;
    victim = LRU_List.LRU_Tail;

    while (Buffer_Info.Buffer_List[victim].is_pinned == Pinned) {
        victim = Buffer_Info.Buffer_List[victim].LRU_Prev;
        if(victim == -1) {
            cout << "Buffer 부족 에러"<< endl;
            exit(0);
        }
    }



    // 해당 page latch 획득
    pthread_mutex_lock(&Buffer_Info.Buffer_List[victim].page_latch);

    page_t Temp_Page;


    // dirty 프레임인 경우 디스크에 써준뒤 사용한다.
    if (Buffer_Info.Buffer_List[victim].is_dirty == Dirty) {
        memcpy(&Temp_Page.Page_Buffer[0], &Buffer_Info.Buffer_List[victim].Frame[0], 4096);

        // 필요 없었던 테이블로 가서 써주고
        Current_TABLE_ID = Buffer_Info.Buffer_List[victim].table_id;

        file_write_page(Buffer_Info.Buffer_List[victim].Page_Number, &Temp_Page);

        // 다시 현재 테이블로 돌아옴
        Current_TABLE_ID = For_Maintain;
    }

    // 써준뒤 필요한 페이지를 읽어옴
    file_read_page(Page_Number, &Temp_Page);

    memcpy(&Buffer_Info.Buffer_List[victim].Frame[0], &Temp_Page.Page_Buffer[0], 4096);
    Buffer_Info.Buffer_List[victim].is_pinned = Pinned;
    Buffer_Info.Buffer_List[victim].is_dirty = Clean;
    Buffer_Info.Buffer_List[victim].Page_Number = Page_Number;
    Buffer_Info.Buffer_List[victim].table_id = Current_TABLE_ID;

    memcpy(&Page->Page_Buffer[0], &Buffer_Info.Buffer_List[victim].Frame[0], 4096);


    // 다음 victim 선정을 위해 사용
    if (victim == LRU_List.LRU_Tail) {
        LRU_List.LRU_Tail = Buffer_Info.Buffer_List[victim].LRU_Prev;
        Buffer_Info.Buffer_List[LRU_List.LRU_Tail].LRU_Next = -1;
        Buffer_Info.Buffer_List[victim].LRU_Prev = -1;
        Buffer_Info.Buffer_List[victim].LRU_Next = LRU_List.LRU_Head;
        Buffer_Info.Buffer_List[LRU_List.LRU_Head].LRU_Prev = victim;
        LRU_List.LRU_Head = victim;
    }

    else if (victim == LRU_List.LRU_Head) {
        //할거 없음
    }

    else {
        Buffer_Info.Buffer_List[Buffer_Info.Buffer_List[victim].LRU_Prev].LRU_Next = Buffer_Info.Buffer_List[victim].LRU_Next;
        Buffer_Info.Buffer_List[Buffer_Info.Buffer_List[victim].LRU_Next].LRU_Prev = Buffer_Info.Buffer_List[victim].LRU_Prev;
        Buffer_Info.Buffer_List[LRU_List.LRU_Head].LRU_Prev = victim;
        Buffer_Info.Buffer_List[victim].LRU_Next = LRU_List.LRU_Head;
        Buffer_Info.Buffer_List[victim].LRU_Prev = -1;
        LRU_List.LRU_Head = victim;
    }


    // LRU 변경 후 buffer manager latch 제거
    pthread_mutex_unlock(&buffer_manager_latch);

    return;
}

// 사용한 페이지를 버퍼에 저장
void buffer_write(pagenum_t Page_Number, page_t* Page) {

    for (int i = 0; i < Buffer_Info.Number_of_Buffer; i++) {
        if (Buffer_Info.Buffer_List[i].Page_Number == Page_Number && Buffer_Info.Buffer_List[i].table_id == Current_TABLE_ID) {
            Buffer_Info.Buffer_List[i].is_dirty = Dirty;
            memcpy(&Buffer_Info.Buffer_List[i].Frame[0], &Page->Page_Buffer[0], 4096);
            return;
        }
    }    
}

// 사용한 페이지를 버퍼에 저장 (기존의 buffer request + buffer write 합친 함수)
void buffer_page_eviction(pagenum_t Page_Number, page_t* Page) {
    // buffer manager latch 획득
    pthread_mutex_lock(&buffer_manager_latch);


    // 만약 요청한 페이지가 프레임에 있다면 그 페이지에 써줌 (Cache Hit 인 경우)
    for (int i = 0; i < Buffer_Info.Number_of_Buffer; i++) {
        // 페이지가 clean 페이지 인 경우 그냥 씀 (disk에 쓰지 않음)
        if (Buffer_Info.Buffer_List[i].Page_Number == Page_Number &&
                Buffer_Info.Buffer_List[i].is_dirty == Clean && Buffer_Info.Buffer_List[i].table_id == Current_TABLE_ID) {
            // 해당 page latch 획득
            pthread_mutex_lock(&Buffer_Info.Buffer_List[i].page_latch);


            Buffer_Info.Buffer_List[i].is_pinned = Pinned;

            //LRU 연결
            if (LRU_List.LRU_Tail == LRU_List.LRU_Head) {
                //빈 공간
            }
            else if (i == LRU_List.LRU_Tail) {

                LRU_List.LRU_Tail = Buffer_Info.Buffer_List[LRU_List.LRU_Tail].LRU_Prev;
                
                Buffer_Info.Buffer_List[i].LRU_Next = LRU_List.LRU_Head;
                Buffer_Info.Buffer_List[i].LRU_Prev = -1;
                Buffer_Info.Buffer_List[LRU_List.LRU_Head].LRU_Prev = i;

                LRU_List.LRU_Head = i;
                Buffer_Info.Buffer_List[LRU_List.LRU_Tail].LRU_Next = -1;
            }
            else if (i == LRU_List.LRU_Head) {
                //빈 공간
            }
            else {
                Buffer_Info.Buffer_List[Buffer_Info.Buffer_List[i].LRU_Prev].LRU_Next = Buffer_Info.Buffer_List[i].LRU_Next;
                Buffer_Info.Buffer_List[Buffer_Info.Buffer_List[i].LRU_Next].LRU_Prev = Buffer_Info.Buffer_List[i].LRU_Prev;

                Buffer_Info.Buffer_List[i].LRU_Next = LRU_List.LRU_Head;
                Buffer_Info.Buffer_List[i].LRU_Prev = -1;
                Buffer_Info.Buffer_List[LRU_List.LRU_Head].LRU_Prev = i;
                LRU_List.LRU_Head = i;
            }


            // Page Eviction
            Buffer_Info.Buffer_List[i].is_dirty = Dirty;
            memcpy(&Buffer_Info.Buffer_List[i].Frame[0], &Page->Page_Buffer[0], 4096);


            // LRU 변경 후 buffer manager latch 제거
            pthread_mutex_unlock(&buffer_manager_latch);


            return;
        }

        // 페이지가 dirty 페이지 인 경우 disk에 sync를 해 주고, clean표시한 뒤 page에 씀
        else if (Buffer_Info.Buffer_List[i].Page_Number == Page_Number && 
                Buffer_Info.Buffer_List[i].is_dirty == Dirty && Buffer_Info.Buffer_List[i].table_id == Current_TABLE_ID) {
            // 해당 page latch 획득
            pthread_mutex_lock(&Buffer_Info.Buffer_List[i].page_latch);

            Buffer_Info.Buffer_List[i].is_pinned = Pinned;


            // disk에 sync를 먼저 해줌
            page_t Temp_Page;
            memcpy(&Temp_Page.Page_Buffer[0], &Buffer_Info.Buffer_List[i].Frame[0], 4096);
            // 어차피 현재 테이블에 페이지를 쓰니 테이블을 바꿀 필요 없음
            file_write_page(Buffer_Info.Buffer_List[i].Page_Number, &Temp_Page);


            //LRU 연결
            if (LRU_List.LRU_Tail == LRU_List.LRU_Head) {
                // 빈 공간
            }
            else if (i == LRU_List.LRU_Tail) {

                LRU_List.LRU_Tail = Buffer_Info.Buffer_List[LRU_List.LRU_Tail].LRU_Prev;

                Buffer_Info.Buffer_List[i].LRU_Next = LRU_List.LRU_Head;
                Buffer_Info.Buffer_List[i].LRU_Prev = -1;
                Buffer_Info.Buffer_List[LRU_List.LRU_Head].LRU_Prev = i;

                LRU_List.LRU_Head = i;
                Buffer_Info.Buffer_List[LRU_List.LRU_Tail].LRU_Next = -1;
            }
            else if (i == LRU_List.LRU_Head) {
                // 빈 공간
            }
            else {
                Buffer_Info.Buffer_List[Buffer_Info.Buffer_List[i].LRU_Prev].LRU_Next = Buffer_Info.Buffer_List[i].LRU_Next;
                Buffer_Info.Buffer_List[Buffer_Info.Buffer_List[i].LRU_Next].LRU_Prev = Buffer_Info.Buffer_List[i].LRU_Prev;

                Buffer_Info.Buffer_List[i].LRU_Next = LRU_List.LRU_Head;
                Buffer_Info.Buffer_List[i].LRU_Prev = -1;
                Buffer_Info.Buffer_List[LRU_List.LRU_Head].LRU_Prev = i;
                LRU_List.LRU_Head = i;
            }


            // 페이지를 다시 dirty 하게 만들고, Page Eviction 진행
            Buffer_Info.Buffer_List[i].is_dirty = Dirty;
            memcpy(&Buffer_Info.Buffer_List[i].Frame[0], &Page->Page_Buffer[0], 4096);


            // LRU 변경 후 buffer manager latch 제거
            pthread_mutex_unlock(&buffer_manager_latch);


            return;
        }
    }

    // Cache Miss 인 경우 해당 페이지를 file_read로 읽어 버퍼 풀에 유지
    
    // 요청한 페이지가 해당 프레임에 없는 경우
    // 빈 프레임이 존재해 바로 읽어올 수 있는 경우
    for (int i = 0; i < Buffer_Info.Number_of_Buffer; i++) {
        if (Buffer_Info.Buffer_List[i].Page_Number == (PAGE_NUMBER_INITIALIZER)) {
            // 해당 page latch 획득
            pthread_mutex_lock(&Buffer_Info.Buffer_List[i].page_latch);


            page_t Temp_Page;
            // 현재 테이블을 읽는 것이므로 FD 바꿀 필요 없음
            file_read_page(Page_Number, &Temp_Page);

            memcpy(&Buffer_Info.Buffer_List[i].Frame[0], &Temp_Page.Page_Buffer[0], 4096);
            Buffer_Info.Buffer_List[i].is_pinned = Pinned;
            Buffer_Info.Buffer_List[i].Page_Number = Page_Number;
            Buffer_Info.Buffer_List[i].table_id = Current_TABLE_ID;

            
            // LRU 연결해 주기
            Buffer_Info.Buffer_List[i].LRU_Next = LRU_List.LRU_Head;
            Buffer_Info.Buffer_List[LRU_List.LRU_Head].LRU_Prev = i;
            LRU_List.LRU_Head = i;
            Buffer_Info.Buffer_List[LRU_List.LRU_Tail].LRU_Next = -1;
            Buffer_Info.Buffer_List[LRU_List.LRU_Head].LRU_Prev = -1;


            // Page Eviction
            Buffer_Info.Buffer_List[i].is_dirty = Dirty;
            memcpy(&Buffer_Info.Buffer_List[i].Frame[0], &Page->Page_Buffer[0], 4096);


            // LRU 변경 후 buffer manager latch 제거
            pthread_mutex_unlock(&buffer_manager_latch);


            return;
        }
    }

    
    // 현재 Current_table_id 유지
    int For_Maintain = Current_TABLE_ID;

    

    // 빈 프레임이 존재하지 않는 경우 LRU policy 적용
    int victim;
    victim = LRU_List.LRU_Tail;

    while (Buffer_Info.Buffer_List[victim].is_pinned == Pinned) {
        victim = Buffer_Info.Buffer_List[victim].LRU_Prev;
        if(victim == -1) {
            cout << "Buffer 부족 에러"<< endl;
            exit(0);
        }
    }


    // 해당 page latch 획득
    pthread_mutex_lock(&Buffer_Info.Buffer_List[victim].page_latch);

    page_t Temp_Page;


    // dirty 프레임인 경우 디스크에 써준뒤 사용한다.
    if (Buffer_Info.Buffer_List[victim].is_dirty == Dirty) {
        memcpy(&Temp_Page.Page_Buffer[0], &Buffer_Info.Buffer_List[victim].Frame[0], 4096);

        // 필요 없었던 테이블로 가서 써주고
        Current_TABLE_ID = Buffer_Info.Buffer_List[victim].table_id;

        file_write_page(Buffer_Info.Buffer_List[victim].Page_Number, &Temp_Page);

        // 다시 현재 테이블로 돌아옴
        Current_TABLE_ID = For_Maintain;
    }

    Buffer_Info.Buffer_List[victim].is_pinned = Pinned;
    Buffer_Info.Buffer_List[victim].Page_Number = Page_Number;
    Buffer_Info.Buffer_List[victim].table_id = Current_TABLE_ID;



    // 다음 victim 선정을 위해 사용
    if (victim == LRU_List.LRU_Tail) {
        LRU_List.LRU_Tail = Buffer_Info.Buffer_List[victim].LRU_Prev;
        Buffer_Info.Buffer_List[LRU_List.LRU_Tail].LRU_Next = -1;
        Buffer_Info.Buffer_List[victim].LRU_Prev = -1;
        Buffer_Info.Buffer_List[victim].LRU_Next = LRU_List.LRU_Head;
        Buffer_Info.Buffer_List[LRU_List.LRU_Head].LRU_Prev = victim;
        LRU_List.LRU_Head = victim;
    }

    else if (victim == LRU_List.LRU_Head) {
        //할거 없음
    }

    else {
        Buffer_Info.Buffer_List[Buffer_Info.Buffer_List[victim].LRU_Prev].LRU_Next = Buffer_Info.Buffer_List[victim].LRU_Next;
        Buffer_Info.Buffer_List[Buffer_Info.Buffer_List[victim].LRU_Next].LRU_Prev = Buffer_Info.Buffer_List[victim].LRU_Prev;
        Buffer_Info.Buffer_List[LRU_List.LRU_Head].LRU_Prev = victim;
        Buffer_Info.Buffer_List[victim].LRU_Next = LRU_List.LRU_Head;
        Buffer_Info.Buffer_List[victim].LRU_Prev = -1;
        LRU_List.LRU_Head = victim;
    }



    // Page Eviction
    Buffer_Info.Buffer_List[victim].is_dirty = Dirty;
    memcpy(&Buffer_Info.Buffer_List[victim].Frame[0], &Page->Page_Buffer[0], 4096);


    // LRU 변경 후 buffer manager latch 제거
    pthread_mutex_unlock(&buffer_manager_latch);

    return;  
}

// 버퍼 사용 완료시 Pinned 값 내려주는 함수
void buffer_close(pagenum_t Page_Number) {
    for (int i = 0; i < Buffer_Info.Number_of_Buffer; i++) {
        if (Buffer_Info.Buffer_List[i].Page_Number == Page_Number && Buffer_Info.Buffer_List[i].table_id == Current_TABLE_ID) {
            Buffer_Info.Buffer_List[i].is_pinned = Unpinned;
            
            // page latch 해제
            pthread_mutex_unlock(&Buffer_Info.Buffer_List[i].page_latch);
            return;
        }
    }   
}


// 버퍼에 File_alloc_page 요청
pagenum_t buffer_alloc_page() {
    uint64_t Header_Free_Page_Number;
	uint64_t Header_Root_Page_Number;
	uint64_t Header_Number_Of_Pages;
	uint64_t Free_Next_Free_Page_Number;
	page_t Header_Page;
	page_t Page;
	page_t Free_Page;


	// Data File 확인용 변수 선언
	buffer_request(HEADER_PAGE, &Header_Page);

	memcpy(Buffer8, &Header_Page.Page_Buffer[0], 8);
	int Free_Page_Number = atoi(Buffer8);

	memcpy(Buffer8, &Header_Page.Page_Buffer[16], 8);
	int Number_of_Pages = atoi(Buffer8);



	// Data File이 없는 경우(Header Page 없음)
	if (Number_of_Pages == 0) {

		// Header Page 를 하나 만듦
		Header_Free_Page_Number = 0; // [0~7]Header Page 가 1, 다음 Free Page 가 2
		Header_Root_Page_Number = 0; // [8~15]
		Header_Number_Of_Pages = 2; // [16~23]

		strcpy(Buffer8, to_string(Header_Free_Page_Number).c_str());
		memcpy(&Header_Page.Page_Buffer[0], Buffer8, 8);
		strcpy(Buffer8, to_string(Header_Root_Page_Number).c_str());
		memcpy(&Header_Page.Page_Buffer[8], Buffer8, 8);
		strcpy(Buffer8, to_string(Header_Number_Of_Pages).c_str());
		memcpy(&Header_Page.Page_Buffer[16], Buffer8, 8);

		buffer_write(HEADER_PAGE, &Header_Page);


        buffer_request(HEADER_PAGE + 1, &Free_Page);

		string Empty = "";
		strcpy(Buffer4096, Empty.c_str());
		memcpy(&Free_Page.Page_Buffer[0], Buffer4096, 4096);


		// 새로운 Free Page 를 하나 할당해 그 곳을 가리키게 함
		// 새로운 Free Page 할당

		Free_Next_Free_Page_Number = 0;
		strcpy(Buffer8, to_string(Free_Next_Free_Page_Number).c_str());

		memcpy(&Free_Page.Page_Buffer[0], Buffer8, 8);

		buffer_write(HEADER_PAGE + 1, &Free_Page);
        buffer_close(HEADER_PAGE);
        buffer_close(HEADER_PAGE + 1);

		return (HEADER_PAGE + 1);
	}


	// Data File이 있는 경우(Header Page 존재) - 1. Free page 존재하는 경우
	else if (Number_of_Pages != 0 && Free_Page_Number != 0) {
		// First Free Page 의 다음 Free Page 가 어디에 존재하는지 확인
		// 그 후 Header Page의 Free Page 를 가리키던 정보를 그 뒷 Free Page 를 가리키게 한 뒤, Free Page Number 반환
		buffer_request(Free_Page_Number, &Free_Page);

		memcpy(&Header_Page.Page_Buffer[0], &Free_Page.Page_Buffer[0], 8);

		buffer_write(HEADER_PAGE, &Header_Page);

        buffer_close(HEADER_PAGE);
        buffer_close(Free_Page_Number);

		return Free_Page_Number;
	}

	// Data File이 있는 경우(Header Page 존재) - 2. Free page 가 존재하지 않는 경우
	else if (Number_of_Pages != 0 && Free_Page_Number == 0) {

		// 3개의 Free Page 를 만들고 연결시켜줌
		page_t Temp_Page;

		for (int i = 0; i < EXPAND_SIZE; i++) {
            buffer_request(Number_of_Pages, &Temp_Page);
			// 4096 바이트 할당만 먼저 해줌
			string Empty = "";
			strcpy(Buffer4096, Empty.c_str());
			memcpy(&Temp_Page.Page_Buffer[0], Buffer4096, 4096);

			strcpy(Buffer8, to_string(Number_of_Pages + 1).c_str());
			memcpy(&Temp_Page.Page_Buffer[0], Buffer8, 8);

			buffer_write(Number_of_Pages, &Temp_Page);

            buffer_close(Number_of_Pages);

			Number_of_Pages++;
		}

		// 마지막이 의미없는 페이지를 가리키므로 0을 가리켜 다음 프리 페이지가 없음을 나타내게 함
		buffer_request(Number_of_Pages - 1, &Temp_Page);
		strcpy(Buffer8, to_string(0).c_str());
		memcpy(&Temp_Page.Page_Buffer[0], Buffer8, 8);

		buffer_write(Number_of_Pages - 1, &Temp_Page);

		// 처음 Free Page 와 Header Page 연결
		strcpy(Buffer8, to_string(Number_of_Pages - (EXPAND_SIZE - 1)).c_str());
		memcpy(&Header_Page.Page_Buffer[0], Buffer8, 8);

		// 바뀐 Number of Pages 수정
		strcpy(Buffer8, to_string(Number_of_Pages).c_str());
		memcpy(&Header_Page.Page_Buffer[16], Buffer8, 8);

		buffer_write(HEADER_PAGE, &Header_Page);

        buffer_close(HEADER_PAGE);
        buffer_close(Number_of_Pages - 1);

		return (Number_of_Pages - EXPAND_SIZE);
	}

    buffer_close(HEADER_PAGE);
	// 전부 실패시 0반환
	return 0;
}

// 버퍼에 file_free 요청
void buffer_file_free_page(pagenum_t Page_Number) {
    // 버퍼에 해당 페이지가 있는 프레임 존재시 프레임을 비워준 후 페이지 초기화
    for (int i = 0; i < Buffer_Info.Number_of_Buffer; i++) {
        if (Buffer_Info.Buffer_List[i].Page_Number == Page_Number && Buffer_Info.Buffer_List[i].table_id == Current_TABLE_ID) {
            Buffer_Info.Buffer_List[i].Frame[0] = '\0';
            Buffer_Info.Buffer_List[i].is_dirty = Clean;
            Buffer_Info.Buffer_List[i].Page_Number = 0;
            Buffer_Info.Buffer_List[i].table_id = 0;
            Buffer_Info.Buffer_List[i].is_pinned = Unpinned;

            page_t Page, Header_Page;
            buffer_request(Page_Number, &Page);

            // 헤더 페이지의 다음 프리 페이지 확인용 변수 선언
            buffer_request(HEADER_PAGE, &Header_Page);

            // 다른 Free Page 가 있는 경우 - 할당 해제한 Free Page가 원래 Header가 가리키던 Free Page를 가리키고 (0이었으면 0을 가리킴)
            // Header는 할당 해제한 Free Page를 가리키게함

            // 할당 해제할 페이지를 비움
            string Empty = "";
            strcpy(Buffer4096, Empty.c_str());
            memcpy(&Page.Page_Buffer[0], Buffer4096, 4096);

            // 할당 해제한 Free Page가 원래 Header가 가리키던 Free Page를 가리키게 함
            memcpy(&Page.Page_Buffer[0], &Header_Page.Page_Buffer[0], 8);

            // Header 가 할당 해제한 Free Page를 가리키게 함
            strcpy(Buffer8, to_string(Page_Number).c_str());
            memcpy(&Header_Page.Page_Buffer[0], Buffer8, 8);

            buffer_write(HEADER_PAGE, &Header_Page);
            buffer_close(HEADER_PAGE);

            buffer_write(Page_Number, &Page);
            buffer_close(Page_Number);
            return;
        }
    }   


    // 버퍼에 해당 페이지가 없는 경우 그냥 페이지 초기화
    page_t Page, Header_Page;
	buffer_request(Page_Number, &Page);

	// 헤더 페이지의 다음 프리 페이지 확인용 변수 선언
	buffer_request(HEADER_PAGE, &Header_Page);

	// 다른 Free Page 가 있는 경우 - 할당 해제한 Free Page가 원래 Header가 가리키던 Free Page를 가리키고 (0이었으면 0을 가리킴)
	// Header는 할당 해제한 Free Page를 가리키게함

	// 할당 해제할 페이지를 비움
	string Empty = "";
	strcpy(Buffer4096, Empty.c_str());
	memcpy(&Page.Page_Buffer[0], Buffer4096, 4096);

	// 할당 해제한 Free Page가 원래 Header가 가리키던 Free Page를 가리키게 함
	memcpy(&Page.Page_Buffer[0], &Header_Page.Page_Buffer[0], 8);

	// Header 가 할당 해제한 Free Page를 가리키게 함
	strcpy(Buffer8, to_string(Page_Number).c_str());
	memcpy(&Header_Page.Page_Buffer[0], Buffer8, 8);

	buffer_write(Page_Number, &Page);
	buffer_write(HEADER_PAGE, &Header_Page);

    buffer_close(HEADER_PAGE);
    buffer_close(Page_Number);
}


void how_frame_used() {
    cout << "[Head=" << LRU_List.LRU_Head << ", Tail=" << LRU_List.LRU_Tail << ", 순서 = ";

    int temp = LRU_List.LRU_Head;
    while(temp != -1) {
        cout << temp << "->";
        temp = Buffer_Info.Buffer_List[temp].LRU_Next;
    }
    cout << endl;
    for(int i=0; i<Buffer_Info.Number_of_Buffer; i++) {
        cout << "[Frame " << i << "]" << endl;
        cout << "[Table " << Buffer_Info.Buffer_List[i].table_id << ", Page ";
        if (Buffer_Info.Buffer_List[i].Page_Number == (PAGE_NUMBER_INITIALIZER))
            cout << "X";
        else
            cout << Buffer_Info.Buffer_List[i].Page_Number;
        
        
        if (Buffer_Info.Buffer_List[i].is_dirty == Dirty)
            cout << ", Dirty";
        else
            cout << ", Clean";

        if (Buffer_Info.Buffer_List[i].is_pinned == Pinned)
            cout << ", Pinned";
        else 
            cout << ", Unpinned";

        cout << ", Prev " << Buffer_Info.Buffer_List[i].LRU_Prev << ", Next " << Buffer_Info.Buffer_List[i].LRU_Next;
        cout << "]" << endl;
    }
    cout << endl;
}