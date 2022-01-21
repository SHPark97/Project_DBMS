#include "file.h"

int TABLE[MAX_TABLE + 1];
string Path_Table[MAX_TABLE + 1];
int Current_TABLE_ID = 0;
int Table_Count = 0;

// 사용할 버퍼 선언
char Buffer4[4], Buffer8[8], Buffer120[120], Buffer4096[4096];
int FD;
int New_Open[MAX_TABLE + 1];


// Allocate an on-disk page from the free page list
// Header Page Num은 1로 지정
pagenum_t file_alloc_page() {
	// 구현을 위한 변수 설정
	uint64_t Header_Free_Page_Number;
	uint64_t Header_Root_Page_Number;
	uint64_t Header_Number_Of_Pages;
	uint64_t Free_Next_Free_Page_Number;
	page_t Check_Page;
	page_t Page;
	page_t Free_Page;


	// Data File 확인용 변수 선언
	file_read_page(HEADER_PAGE, &Check_Page);

	memcpy(Buffer8, &Check_Page.Page_Buffer[0], 8);
	int Free_Page_Number = atoi(Buffer8);

	memcpy(Buffer8, &Check_Page.Page_Buffer[16], 8);
	int Number_of_Pages = atoi(Buffer8);


	// Data File이 없는 경우(Header Page 없음)
	if (Number_of_Pages == 0) {
		file_read_page(HEADER_PAGE, &Page);

		// Header Page 를 하나 만듦
		Header_Free_Page_Number = 0; // [0~7]Header Page 가 1, 다음 Free Page 가 2
		Header_Root_Page_Number = 0; // [8~15]
		Header_Number_Of_Pages = 2; // [16~23]

		strcpy(Buffer8, to_string(Header_Free_Page_Number).c_str());
		memcpy(&Page.Page_Buffer[0], Buffer8, 8);
		strcpy(Buffer8, to_string(Header_Root_Page_Number).c_str());
		memcpy(&Page.Page_Buffer[8], Buffer8, 8);
		strcpy(Buffer8, to_string(Header_Number_Of_Pages).c_str());
		memcpy(&Page.Page_Buffer[16], Buffer8, 8);

		file_write_page(HEADER_PAGE, &Page);

        file_read_page(HEADER_PAGE + 1, &Free_Page);

		string Empty = "";
		strcpy(Buffer4096, Empty.c_str());
		memcpy(&Free_Page.Page_Buffer[0], Buffer4096, 4096);

		// 새로운 Free Page 를 하나 할당해 그 곳을 가리키게 함
		// 새로운 Free Page 할당
		file_read_page(HEADER_PAGE + 1, &Free_Page);

		Free_Next_Free_Page_Number = 0;
		strcpy(Buffer8, to_string(Free_Next_Free_Page_Number).c_str());

		memcpy(&Free_Page.Page_Buffer[0], Buffer8, 8);

		file_write_page(HEADER_PAGE + 1, &Free_Page);

		return (HEADER_PAGE + 1);
	}


	// Data File이 있는 경우(Header Page 존재) - 1. Free page 존재하는 경우
	else if (Number_of_Pages != 0 && Free_Page_Number != 0) {

		// First Free Page 의 다음 Free Page 가 어디에 존재하는지 확인
		// 그 후 Header Page의 Free Page 를 가리키던 정보를 그 뒷 Free Page 를 가리키게 한 뒤, Free Page Number 반환
		file_read_page(Free_Page_Number, &Free_Page);
		file_read_page(HEADER_PAGE, &Page);

		memcpy(&Page.Page_Buffer[0], &Free_Page.Page_Buffer[0], 8);

		file_write_page(HEADER_PAGE, &Page);

		return Free_Page_Number;
	}

	// Data File이 있는 경우(Header Page 존재) - 2. Free page 가 존재하지 않는 경우
	else if (Number_of_Pages != 0 && Free_Page_Number == 0) {

		// 3개의 Free Page 를 만들고 연결시켜줌
		page_t Temp_Page;

		for (int i = 0; i < EXPAND_SIZE; i++) {
            file_read_page(Number_of_Pages, &Temp_Page);
			// 4096 바이트 할당만 먼저 해줌
			string Empty = "";
			strcpy(Buffer4096, Empty.c_str());
			memcpy(&Temp_Page.Page_Buffer[0], Buffer4096, 4096);
			file_write_page(Number_of_Pages, &Temp_Page);


			file_read_page(Number_of_Pages, &Temp_Page);

			strcpy(Buffer8, to_string(Number_of_Pages + 1).c_str());
			memcpy(&Temp_Page.Page_Buffer[0], Buffer8, 8);

			file_write_page(Number_of_Pages, &Temp_Page);

			Number_of_Pages++;
		}

		// 마지막이 의미없는 페이지를 가리키므로 0을 가리켜 다음 프리 페이지가 없음을 나타내게 함
		file_read_page(Number_of_Pages - 1, &Temp_Page);
		strcpy(Buffer8, to_string(0).c_str());
		memcpy(&Temp_Page.Page_Buffer[0], Buffer8, 8);

		file_write_page(Number_of_Pages - 1, &Temp_Page);

		// 처음 Free Page 와 Header Page 연결
        file_read_page(HEADER_PAGE, &Page);
		strcpy(Buffer8, to_string(Number_of_Pages - (EXPAND_SIZE - 1)).c_str());
		memcpy(&Page.Page_Buffer[0], Buffer8, 8);

		// 바뀐 Number of Pages 수정
		strcpy(Buffer8, to_string(Number_of_Pages).c_str());
		memcpy(&Page.Page_Buffer[16], Buffer8, 8);

		file_write_page(HEADER_PAGE, &Page);

		return (Number_of_Pages - EXPAND_SIZE);
	}

	// 전부 실패시 0반환
	return 0;
}


// Free an on-disk page to the free page list
void file_free_page(pagenum_t pagenum) {
	page_t Page, Header_Page;
	file_read_page(pagenum, &Page);

	// 헤더 페이지의 다음 프리 페이지 확인용 변수 선언
	file_read_page(HEADER_PAGE, &Header_Page);

	// 다른 Free Page 가 있는 경우 - 할당 해제한 Free Page가 원래 Header가 가리키던 Free Page를 가리키고 (0이었으면 0을 가리킴)
	// Header는 할당 해제한 Free Page를 가리키게함

	// 할당 해제할 페이지를 비움
	string Empty = "";
	strcpy(Buffer4096, Empty.c_str());
	memcpy(&Page.Page_Buffer[0], Buffer4096, 4096);

	// 할당 해제한 Free Page가 원래 Header가 가리키던 Free Page를 가리키게 함
	memcpy(&Page.Page_Buffer[0], &Header_Page.Page_Buffer[0], 8);

	// Header 가 할당 해제한 Free Page를 가리키게 함
	strcpy(Buffer8, to_string(pagenum).c_str());
	memcpy(&Header_Page.Page_Buffer[0], Buffer8, 8);

	file_write_page(pagenum, &Page);
	file_write_page(HEADER_PAGE, &Header_Page);
}


// Read an on-disk page into the in-memory page structure(dest)
void file_read_page(pagenum_t pagenum, page_t* dest) {
    ostringstream ss;
    string stream;
    uint64_t Temp[512];

    lseek(TABLE[Current_TABLE_ID], 4096 * (pagenum), SEEK_SET);
    read(TABLE[Current_TABLE_ID], Temp, 4096);

    for (int i = 0; i < 16; i++) {
        ss << Temp[i];
        stream = ss.str();
        strcpy(Buffer8, stream.c_str());
        memcpy(&dest->Page_Buffer[8 * i], Buffer8, 8);
        ss.str(""); ss.clear();
    }

    uint32_t b8_11 = (uint32_t)(Temp[1] & 0xFFFFFFFF);
    uint32_t b12_15 = (uint32_t)((Temp[1] >> 32) & 0xFFFFFFFF);

    ss << b8_11;
    stream = ss.str();
    strcpy(Buffer4, stream.c_str());
    memcpy(&dest->Page_Buffer[8], Buffer4, 4);
    ss.str(""); ss.clear();

    ss << b12_15;
    stream = ss.str();
    strcpy(Buffer4, stream.c_str());
    memcpy(&dest->Page_Buffer[12], Buffer4, 4);
    ss.str(""); ss.clear();

    //leaf
    if (b8_11 == 1) {
        for (int i = 0; i < 31; i++) {
            ss << Temp[16 + 16 * i];
            stream = ss.str();
            strcpy(Buffer8, stream.c_str());
            memcpy(&dest->Page_Buffer[128 + 128 * i], Buffer8, 8);
            ss.str(""); ss.clear();

            for (int j = 0; j < 15; j++) {
                memcpy(&dest->Page_Buffer[136 + 128 * i + 8 * j], &Temp[17+16*i+j], 8);
            }
        }
    }

    //internal
    if (b8_11 == 0) {
        for (int i = 16; i < 512; i++) {
            ss << Temp[i];
            stream = ss.str();
            strcpy(Buffer8, stream.c_str());
            memcpy(&dest->Page_Buffer[8 * i], Buffer8, 8);
            ss.str(""); ss.clear();
        }
    }
}


// Write an in-memory page(src) to the on-disk page
void file_write_page(pagenum_t pagenum, const page_t* src) {
	istringstream iss;
    iss.clear();
    string stream;
    uint64_t Temp[512];

    for (int i = 0; i < 16; i++) {
        memcpy(Buffer8, &src->Page_Buffer[8 * i], 8);
        stream = Buffer8;
        iss.str(stream);
        iss >> Temp[i];
        iss.str(""); iss.clear();
    }

	uint32_t b8_11;
    uint32_t b12_15;

	memcpy(Buffer4, &src->Page_Buffer[12], 4);
	stream = Buffer4;
	iss.str(stream);
	iss >> b12_15;
	iss.str(""); iss.clear();

	memcpy(Buffer4, &src->Page_Buffer[8], 4);
	stream = Buffer4;
	iss.str(stream);
	iss >> b8_11;
	iss.str(""); iss.clear();
	
	Temp[1] = (((uint64_t)b12_15) << 32) | (uint64_t)b8_11;

    //leaf
    if (b8_11 == 1) {
        for (int i = 0; i < 31; i++) {

			memcpy(Buffer8, &src->Page_Buffer[128 + 128 * i], 8);
			stream = Buffer8;
			iss.str(stream);
			iss >> Temp[16 + 16 * i];
			iss.str(""); iss.clear();

            for (int j = 0; j < 15; j++) {
				memcpy(&Temp[17+16*i+j], &src->Page_Buffer[136+128*i + 8 * j], 8);
            }
        }
    }

    //internal
    if (b8_11 == 0) {
        for (int i = 16; i < 512; i++) {
			memcpy(Buffer8, &src->Page_Buffer[8 * i], 8);
			stream = Buffer8;
			iss.str(stream);
			iss >> Temp[i];
			iss.str(""); iss.clear();
        }
    }

    lseek(TABLE[Current_TABLE_ID], 4096 * (pagenum), SEEK_SET);
    write(TABLE[Current_TABLE_ID], Temp, 4096);
}


// 추가 함수
// pathname을 열어줌
// 여는 것에 성공시 열린 fd를, 실패시 -1 반환
int file_open(const char* pathname) {
	// path에 있는 녀석 있으면 열고 없으면 만듦(O_CREAT)
    Table_Count++;
    TABLE[Table_Count] = open(pathname, O_SYNC | O_RDWR, 0644);

	// 여는 것이 실패할 경우
	if (TABLE[Table_Count] == -1) {
        New_Open[Table_Count] = 1;
        TABLE[Table_Count] = open(pathname, O_CREAT | O_SYNC | O_RDWR, 0644);
		if (TABLE[Table_Count] == -1) {
            cout << "cannot open files in pathname" << endl;
            return -1;
        }
	}

	// 여는 것이 성공한 경우 TABLE[Current_TABLE_ID] 반환
    Current_TABLE_ID = Table_Count;
    //Table_ON_OFF++;
	return TABLE[Table_Count]; 
}

// 몇 번째 Table인지 알려줌
// 확인 가능할 시 Table id를, 아무 것도 없으면 0을 반환
int file_unique_table_id() {
	if (0 < Current_TABLE_ID && Current_TABLE_ID <= MAX_TABLE)
		return Current_TABLE_ID;
	
	else 
		return 0;
}

void file_header_alloc() {
    page_t Page;

    strcpy(Buffer8, to_string(0).c_str());
    memcpy(&Page.Page_Buffer[0], Buffer8, 8);

    strcpy(Buffer8, to_string(0).c_str());
    memcpy(&Page.Page_Buffer[8], Buffer8, 8);

    strcpy(Buffer8, to_string(0).c_str());
    memcpy(&Page.Page_Buffer[16], Buffer8, 8);

    file_write_page(HEADER_PAGE, &Page);
}


//Recovery를 위한 함수
// Read an on-disk page into the in-memory page structure(dest)
void file_read_page_for_recovery(pagenum_t pagenum, page_t* dest, int fd) {
    ostringstream ss;
    string stream;
    uint64_t Temp[512];

    lseek(fd, 4096 * (pagenum), SEEK_SET);
    read(fd, Temp, 4096);

    for (int i = 0; i < 16; i++) {
        ss << Temp[i];
        stream = ss.str();
        strcpy(Buffer8, stream.c_str());
        memcpy(&dest->Page_Buffer[8 * i], Buffer8, 8);
        ss.str(""); ss.clear();
    }

    uint32_t b8_11 = (uint32_t)(Temp[1] & 0xFFFFFFFF);
    uint32_t b12_15 = (uint32_t)((Temp[1] >> 32) & 0xFFFFFFFF);

    ss << b8_11;
    stream = ss.str();
    strcpy(Buffer4, stream.c_str());
    memcpy(&dest->Page_Buffer[8], Buffer4, 4);
    ss.str(""); ss.clear();

    ss << b12_15;
    stream = ss.str();
    strcpy(Buffer4, stream.c_str());
    memcpy(&dest->Page_Buffer[12], Buffer4, 4);
    ss.str(""); ss.clear();

    //leaf
    if (b8_11 == 1) {
        for (int i = 0; i < 31; i++) {
            ss << Temp[16 + 16 * i];
            stream = ss.str();
            strcpy(Buffer8, stream.c_str());
            memcpy(&dest->Page_Buffer[128 + 128 * i], Buffer8, 8);
            ss.str(""); ss.clear();

            for (int j = 0; j < 15; j++) {
                memcpy(&dest->Page_Buffer[136 + 128 * i + 8 * j], &Temp[17+16*i+j], 8);
            }
        }
    }

    //internal
    if (b8_11 == 0) {
        for (int i = 16; i < 512; i++) {
            ss << Temp[i];
            stream = ss.str();
            strcpy(Buffer8, stream.c_str());
            memcpy(&dest->Page_Buffer[8 * i], Buffer8, 8);
            ss.str(""); ss.clear();
        }
    }
}


// Write an in-memory page(src) to the on-disk page
void file_write_page_for_recovery(pagenum_t pagenum, const page_t* src, int fd) {
	istringstream iss;
    iss.clear();
    string stream;
    uint64_t Temp[512];

    for (int i = 0; i < 16; i++) {
        memcpy(Buffer8, &src->Page_Buffer[8 * i], 8);
        stream = Buffer8;
        iss.str(stream);
        iss >> Temp[i];
        iss.str(""); iss.clear();
    }

	uint32_t b8_11;
    uint32_t b12_15;

	memcpy(Buffer4, &src->Page_Buffer[12], 4);
	stream = Buffer4;
	iss.str(stream);
	iss >> b12_15;
	iss.str(""); iss.clear();

	memcpy(Buffer4, &src->Page_Buffer[8], 4);
	stream = Buffer4;
	iss.str(stream);
	iss >> b8_11;
	iss.str(""); iss.clear();
	
	Temp[1] = (((uint64_t)b12_15) << 32) | (uint64_t)b8_11;

    //leaf
    if (b8_11 == 1) {
        for (int i = 0; i < 31; i++) {

			memcpy(Buffer8, &src->Page_Buffer[128 + 128 * i], 8);
			stream = Buffer8;
			iss.str(stream);
			iss >> Temp[16 + 16 * i];
			iss.str(""); iss.clear();

            for (int j = 0; j < 15; j++) {
				memcpy(&Temp[17+16*i+j], &src->Page_Buffer[136+128*i + 8 * j], 8);
            }
        }
    }

    //internal
    if (b8_11 == 0) {
        for (int i = 16; i < 512; i++) {
			memcpy(Buffer8, &src->Page_Buffer[8 * i], 8);
			stream = Buffer8;
			iss.str(stream);
			iss >> Temp[i];
			iss.str(""); iss.clear();
        }
    }

    lseek(fd, 4096 * (pagenum), SEEK_SET);
    write(fd, Temp, 4096);
}