#include "bpt.h"

extern int Current_TABLE_ID;
extern int New_Open[MAX_TABLE + 1];



// Find 성공시 값이 있는 Page를, 찾는 값이 없으면 0을 반환
pagenum_t bpt_find_page(int64_t key, char* ret_val) {
    char Buffer8[8], Buffer4[4];

    // 루트 노드(루트 페이지)를 찾음
    page_t Header_Page, Page, Root_Page;
    pagenum_t Root_Page_Number;
    uint32_t Is_Leaf;
    uint32_t Number_of_Keys;
    uint64_t Page_Key;

    int table_id = file_unique_table_id();

    if (table_id == 0) {
        cout << "table id가 0" << endl;
        exit(0);
    }

    // File에서 Header 페이지를 찾아서 루트 페이지를 찾음
    buffer_request(HEADER_PAGE, &Header_Page);

    memcpy(Buffer8, &Header_Page.Page_Buffer[8], 8);
    Root_Page_Number = atoi(Buffer8);

    buffer_close(HEADER_PAGE);

    if (Root_Page_Number == 0) {
        return 0;
    }

    // 루트 페이지를 읽어옴
    buffer_request(Root_Page_Number, &Root_Page);

    memcpy(Buffer4, &Root_Page.Page_Buffer[8], 4);
    Is_Leaf = atoi(Buffer4);

    memcpy(Buffer4, &Root_Page.Page_Buffer[12], 4);
    Number_of_Keys = atoi(Buffer4);

    memcpy(Buffer8, &Root_Page.Page_Buffer[128], 8);
    Page_Key = atoi(Buffer8);

    // case1 - 루트 페이지가 Internal Page
    // 리프 페이지를 찾을 때 까지 반복
    


    int i = 0;
    while (Is_Leaf == 0) {
        i = 0;
        while (i < Number_of_Keys) {
            memcpy(Buffer8, &Root_Page.Page_Buffer[128 + 16 * i], 8);
            int Root_Page_Key = atoi(Buffer8);
            

            if (key >= Root_Page_Key) i++;
            else 
                break;
        }

        buffer_close(Root_Page_Number);

        memcpy(Buffer8, &Root_Page.Page_Buffer[120 + 16 * i], 8);
        Root_Page_Number = atoi(Buffer8);

        buffer_request(Root_Page_Number, &Root_Page);

        memcpy(Buffer4, &Root_Page.Page_Buffer[8], 4);
        Is_Leaf = atoi(Buffer4);

        memcpy(Buffer4, &Root_Page.Page_Buffer[12], 4);
        Number_of_Keys = atoi(Buffer4);

        memcpy(Buffer8, &Root_Page.Page_Buffer[128], 8);
        Page_Key = atoi(Buffer8);
    }

    buffer_close(Root_Page_Number);

    return Root_Page_Number;
}

// Find 성공시 값이 있는 Page를, 찾는 값이 없으면 0을 반환
pagenum_t bpt_find(int64_t key, char* ret_val, int trx_id) {
    char Buffer4[4], Buffer120[120];
    
    // 루트 노드(루트 페이지)를 찾음
    page_t Header_Page, Page, Root_Page;
    pagenum_t Root_Page_Number;
    uint32_t Is_Leaf;
    uint32_t Number_of_Keys;
    uint64_t Page_Key;
    char Buffer8[8];

    // File에서 Header 페이지를 찾아서 루트 페이지를 찾음
    buffer_request(HEADER_PAGE, &Header_Page);

    memcpy(Buffer8, &Header_Page.Page_Buffer[8], 8);
    Root_Page_Number = atoi(Buffer8);
    


    if (Root_Page_Number == 0) {
        buffer_close(HEADER_PAGE);
        return 0;
    }

    buffer_close(HEADER_PAGE);

    // 루트 페이지를 읽어옴
    buffer_request(Root_Page_Number, &Root_Page);
    

    memcpy(Buffer4, &Root_Page.Page_Buffer[8], 4);
    Is_Leaf = atoi(Buffer4);

    memcpy(Buffer4, &Root_Page.Page_Buffer[12], 4);
    Number_of_Keys = atoi(Buffer4);

    memcpy(Buffer8, &Root_Page.Page_Buffer[128], 8);
    Page_Key = atoi(Buffer8);

    // case1 - 루트 페이지가 Internal Page
    // 리프 페이지를 찾을 때 까지 반복

    int i = 0;
    while (Is_Leaf == 0) {
        i = 0;

        while (i < Number_of_Keys) {
            memcpy(Buffer8, &Root_Page.Page_Buffer[128 + 16 * i], 8);
            int Root_Page_Key = atoi(Buffer8);

            if (key >= Root_Page_Key) i++;
            else 
                break;
        }

        memcpy(Buffer8, &Root_Page.Page_Buffer[120 + 16 * i], 8);
        buffer_close(Root_Page_Number);

        Root_Page_Number = atoi(Buffer8);


        buffer_request(Root_Page_Number, &Root_Page);


        memcpy(Buffer4, &Root_Page.Page_Buffer[8], 4);
        Is_Leaf = atoi(Buffer4);

        memcpy(Buffer4, &Root_Page.Page_Buffer[12], 4);
        Number_of_Keys = atoi(Buffer4);

        memcpy(Buffer8, &Root_Page.Page_Buffer[128], 8);
        Page_Key = atoi(Buffer8);

    }


    // 루트 페이지가 Leaf Page 인 경우 그냥 값을 찾아줌
    // 루트 페이지에는 Find 한 Key 의 page 정보가 담겨있음
    for (int i = 0; i < Number_of_Keys; i++) {
        memcpy(Buffer8, &Root_Page.Page_Buffer[128*(i+1)], 8);
        Page_Key = atoi(Buffer8);

        if (Page_Key == key) {
            buffer_close(Root_Page_Number);
            if(lock_acquire(Current_TABLE_ID, key, trx_id, SHARED) == NULL) {
                //deadlock 발생시
                return 0;
            }

            buffer_request(Root_Page_Number, &Root_Page);

            memcpy(Buffer120, &Root_Page.Page_Buffer[128*(i+1) + 8], 120);
            string Temp(Buffer120);
            strncpy(ret_val, Temp.c_str(), 120);
            buffer_close(Root_Page_Number);
            return Root_Page_Number; // Leaf Page 를 내타냄
        }
    }

    buffer_close(Root_Page_Number);


    // 값이 없으면 0 반환
    return 0;
}
















// Insert 성공시 1을, 실패시 -1을 반환함 (중복일 시)
int bpt_insert(int64_t key, char* value) {
    char Buffer4[4], Buffer8[8], Buffer120[120];

    char* temp_char = new char[120];
	string Value(value);



    // 중복 제거
    int Find_Page = bpt_find_original(key, temp_char); // Find Page 는 Leaf Page



    if (Find_Page != 0) {
        delete[] temp_char;
        return -1;
    }

    // 만약 insert할 페이지가 없다면 insert 할 페이지를 만들고,
    // Header Page 의 Root Page 와 연결해줌
    page_t Header_Page, Page, New_Page;
    pagenum_t New_Page_Number;
    buffer_request(HEADER_PAGE, &Header_Page);
    memcpy(Buffer8, &Header_Page.Page_Buffer[8], 8);
    pagenum_t Root_Page_Number = atoi(Buffer8);
    buffer_close(HEADER_PAGE);


    // 아무 것도 없는 곳에 넣을 때 사용
    if (Root_Page_Number == 0) {
        // New Page 는 Leaf Page
        New_Page_Number = buffer_alloc_page();

        buffer_request(HEADER_PAGE, &Header_Page);


        // Header Page - Root Page 정보 변경
        strcpy(Buffer8, (to_string(New_Page_Number)).c_str());
        memcpy(&Header_Page.Page_Buffer[8], Buffer8, 8);

        // Header Page - Number of Pages 정보 변경
        //strcpy(Buffer8, (to_string(2)).c_str());
        //memcpy(&Header_Page.Page_Buffer[16], Buffer8, 8);

        buffer_write(HEADER_PAGE, &Header_Page);

        // Leaf Page의 정보 변경
        buffer_request(New_Page_Number, &Page);

        // Leaf Page - Parent Page Number 정보 변경
        uint64_t Parent_Page_Number = 0;
        strcpy(Buffer8, (to_string(Parent_Page_Number)).c_str());
        memcpy(&Page.Page_Buffer[0], Buffer8, 8);

        // Leaf Page - Is Leaf 정보 변경
        uint64_t Is_Leaf = 1;
        strcpy(Buffer4, (to_string(Is_Leaf)).c_str());
        memcpy(&Page.Page_Buffer[8], Buffer4, 4);

        // Leaf Page - Number of Keys 정보 변경
        uint64_t Number_of_Keys = 1;
        strcpy(Buffer4, (to_string(Number_of_Keys)).c_str());
        memcpy(&Page.Page_Buffer[12], Buffer4, 4);

        // Leaf Page - Right Sibling Page Number 정보 변경
        uint64_t Right_Sibling_Page_Number = 0;
        strcpy(Buffer8, (to_string(Right_Sibling_Page_Number)).c_str());
        memcpy(&Page.Page_Buffer[120], Buffer8, 8);

        // Leaf Page - Key, Value 추가
        strcpy(Buffer8, (to_string(key)).c_str());
        strcpy(Buffer120, Value.c_str());
        memcpy(&Page.Page_Buffer[128], Buffer8, 8);
        memcpy(&Page.Page_Buffer[128 + 8], Buffer120, 120);

        buffer_write(New_Page_Number, &Page);


        delete[] temp_char;
        buffer_close(HEADER_PAGE);
        buffer_close(New_Page_Number);
        return 1;
    }


    // Root Page가 아닌 경우 찾은 Find Page(리프페이지)의 Number of Keys 정보를 읽어옴
	Find_Page = bpt_find_page(key, temp_char);
    buffer_request(HEADER_PAGE, &Header_Page);
    buffer_request(Find_Page, &Page);
    memcpy(Buffer4, &Page.Page_Buffer[12], 4);
    uint64_t Number_of_Keys = atoi(Buffer4);

    // Page 에 있는 Key 와 Value 값을 배열로 만듦
    uint64_t Keys[35];
    uint64_t Temp_Keys[36];
    char Values[35][121];
    char Temp_Values[36][121];

    for (int i = 0; i < Number_of_Keys; i++) {
        memcpy(Buffer8, &Page.Page_Buffer[128 + 128*i], 8);
        Keys[i] = atoi(Buffer8);

        memcpy(Buffer120, &Page.Page_Buffer[128 + 8 + 128*i], 120);
		strcpy(Values[i], Buffer120);
    }


    // Leaf Page Key 개수가 d <= LeafPage < 2d 인 경우 그냥 넣음 (여기서 2d는 32임)
    if (Number_of_Keys < 32 - 1) {
        // 어디에 넣어야 할 지 찾음
        int insertion_point = 0;

        while (insertion_point < Number_of_Keys && Keys[insertion_point] < key) {
            insertion_point++;
        }

        // Insert 하기 위해 한칸씩 이동
        for (int i = Number_of_Keys; i > insertion_point; i--) {
            Keys[i] = Keys[i-1];
            strcpy(Values[i], Values[i-1]);
        }

        // Key, Value 넣음
        strcpy(Buffer8, (to_string(key)).c_str());
        memcpy(&Page.Page_Buffer[128 * (insertion_point + 1)], Buffer8, 8);
        strcpy(Buffer120, Value.c_str());
        memcpy(&Page.Page_Buffer[128 * (insertion_point + 1) + 8], Buffer120, 120);

        // 밀어 놓은 Key, Value 넣음
         for (int i = Number_of_Keys; i > insertion_point; i--) {
            strcpy(Buffer8, (to_string(Keys[i])).c_str());
            memcpy(&Page.Page_Buffer[128 * (i + 1)], Buffer8, 8);
            strcpy(Buffer120, Values[i]);
            memcpy(&Page.Page_Buffer[128 * (i + 1) + 8], Buffer120, 120);
        }

        // Number of Keys 정보 변경
        Number_of_Keys++;
        strcpy(Buffer4, (to_string(Number_of_Keys)).c_str());
        memcpy(&Page.Page_Buffer[12], Buffer4, 4);

        buffer_write(Find_Page, &Page);

        delete[] temp_char;
        buffer_close(HEADER_PAGE);
        buffer_close(Find_Page);
        return 1;
    }


    // Leaf Page Key 개수가 2d 인 경우 Split 실행
    else {

        // 새로운 리프 페이지 할당
        buffer_close(HEADER_PAGE);
        buffer_close(Find_Page);

        New_Page_Number = buffer_alloc_page();
        buffer_request(HEADER_PAGE, &Header_Page);
        buffer_request(Find_Page, &Page);

        // Leaf Page의 정보 변경
        buffer_request(New_Page_Number, &New_Page);

        // Leaf Page - Parent Page Number 정보 변경
        uint64_t new_Parent_Page_Number = 0;
        strcpy(Buffer8, (to_string(new_Parent_Page_Number)).c_str());
        memcpy(&New_Page.Page_Buffer[0], Buffer8, 8);

        // Leaf Page - Is Leaf 정보 변경
        uint32_t new_Is_Leaf = 1;
        strcpy(Buffer4, (to_string(new_Is_Leaf)).c_str());
        memcpy(&New_Page.Page_Buffer[8], Buffer4, 4);

        // Leaf Page - Number of Keys 정보 변경
        uint32_t new_Number_of_Keys = 0;
        strcpy(Buffer4, (to_string(new_Number_of_Keys)).c_str());
        memcpy(&New_Page.Page_Buffer[12], Buffer4, 4);

        // Leaf Page - Right Sibling Page Number 정보 변경
        uint64_t new_Right_Sibling_Page_Number = 0;
        strcpy(Buffer8, (to_string(new_Right_Sibling_Page_Number)).c_str());
        memcpy(&New_Page.Page_Buffer[120], Buffer8, 8);

        buffer_write(New_Page_Number, &New_Page);


        // 어디에 넣어야 할 지 찾음
        int insertion_point = 0;

        while (insertion_point < Number_of_Keys && Keys[insertion_point] < key) {
            insertion_point++;
        }

        for (int i = 0, j = 0; i < Number_of_Keys ; i++, j++) {
            if (j == insertion_point) j++;
            Temp_Keys[j] = Keys[i];
            strcpy(Temp_Values[j], Values[i]);
        }

        // insertion_point 에 Key 와 Value 값을 넣어줌
        // 기존 리프 페이지의 Number of Keys 를 0으로 초기화
        Temp_Keys[insertion_point] = key;
        strcpy(Buffer120, Value.c_str());
        strcpy(Temp_Values[insertion_point], Buffer120);
        Number_of_Keys = 0;

        int split = 16;


        // Split 전 값까지는 기존 리프 페이지에 넣어줌
        for (int i = 0; i < split; i++) {
            strcpy(Buffer8, (to_string(Temp_Keys[i])).c_str());
            memcpy(&Page.Page_Buffer[128 * (i + 1)], Buffer8, 8);
            strcpy(Buffer120, Temp_Values[i]);
            memcpy(&Page.Page_Buffer[128 * (i + 1) + 8], Buffer120, 120);
            Number_of_Keys++;
        }
        

        // Split 이후 값부터는 새로 할당한 리프 페이지에 넣어줌
        for (int i = split, j = 0; i < 32; i++, j++) {
            strcpy(Buffer8, (to_string(Temp_Keys[i])).c_str());
            memcpy(&New_Page.Page_Buffer[128 * (j + 1)], Buffer8, 8);
            strcpy(Buffer120, Temp_Values[i]);
            memcpy(&New_Page.Page_Buffer[128 * (j + 1) + 8], Buffer120, 120);
            new_Number_of_Keys++;
        }

        // 각각의 페이지에 Key값을 넣어줌
        strcpy(Buffer4, to_string(Number_of_Keys).c_str());
        memcpy(&Page.Page_Buffer[12], Buffer4, 4);
        strcpy(Buffer4, to_string(new_Number_of_Keys).c_str());
        memcpy(&New_Page.Page_Buffer[12], Buffer4, 4);


        // 새로운 리프 페이지의 Right Sibling은 기존 리프 페이지의 Right Sibling 을,
        // 기존 리프 페이지의 마지막은 새로운 리프 페이지의 페이지 넘버를 가리키게 함
        memcpy(Buffer8, &Page.Page_Buffer[120], 8);
        uint64_t Right_Sibling_Page_Number = atoi(Buffer8);
        new_Right_Sibling_Page_Number = Right_Sibling_Page_Number;
        Right_Sibling_Page_Number = New_Page_Number;

        strcpy(Buffer8, (to_string(Right_Sibling_Page_Number)).c_str());
        memcpy(&Page.Page_Buffer[120], Buffer8, 8);

        strcpy(Buffer8, (to_string(new_Right_Sibling_Page_Number)).c_str());
        memcpy(&New_Page.Page_Buffer[120], Buffer8, 8);


        // Split 한 부분중 기존 리프 페이지에서 사용되지 않는 부분은 초기화해줌
        for (int i = Number_of_Keys; i < 31; i++) {
            string str = "";
            strcpy(Buffer8, str.c_str());
            strcpy(Buffer120, str.c_str());
            memcpy(&Page.Page_Buffer[128 * (i + 1)], Buffer8, 8);
            memcpy(&Page.Page_Buffer[128 * (i + 1) + 8], Buffer120, 120);
        }

        // 새로운 리프 페이지의 부모 페이지는 기존 리프 페이지의 부모 페이지를 가리키게 함
        // 부모 페이지에 줄 Key는 새로운 리프 페이지의 첫 번째 Key 값을 줌
        memcpy(Buffer8, &Page.Page_Buffer[0], 8);
        new_Parent_Page_Number = atoi(Buffer8);

        strcpy(Buffer8, (to_string(new_Parent_Page_Number)).c_str());
        memcpy(&New_Page.Page_Buffer[0], Buffer8, 8);

        memcpy(Buffer8, &New_Page.Page_Buffer[128], 8);
        uint64_t New_Key = atoi(Buffer8);

        buffer_write(New_Page_Number, &New_Page);
        buffer_write(Find_Page, &Page);

        buffer_close(HEADER_PAGE);
        buffer_close(Find_Page);
        buffer_close(New_Page_Number);


        // 부모 페이지에 키 값을 넣어줌
        bpt_insert_into_parent(Find_Page, New_Key, New_Page_Number);

        delete[] temp_char;
        return 1;
    }

}

// 삽입시 Internal 페이지에 값을 넣는 경우 사용
pagenum_t bpt_insert_into_parent(pagenum_t left, uint64_t key, pagenum_t right) {
    char Buffer4[4], Buffer8[8], Buffer120[120];

    page_t Page, New_Page, Parent_Page, Header_Page;
    buffer_request(left, &Page);
    buffer_request(right, &New_Page);

    
    // 부모 페이지를 읽어옴
    memcpy(Buffer8, &Page.Page_Buffer[0], 8);
    pagenum_t Parent_Page_Number = atoi(Buffer8);

    if (Parent_Page_Number != 0)
        buffer_request(Parent_Page_Number, &Parent_Page);

    memcpy(Buffer4, &Parent_Page.Page_Buffer[12], 4);
    uint64_t Parent_Number_of_Keys = atoi(Buffer4);

    // 부모 페이지가 존재하지 않을 경우 새로운 부모 페이지를 할당
    // 헤더 페이지랑 부모 페이지도 연결해줌(부모 페이지가 루트 페이지가 됨)
    if (Parent_Page_Number == 0) {
        buffer_close(left);
        buffer_close(right);

        // 새로운 부모 페이지 할당
        Parent_Page_Number = buffer_alloc_page();

        buffer_request(left, &Page);
        buffer_request(right, &New_Page);

		// 헤더 페이지에 부모 페이지가 새로운 루트 페이지가 되게함
		buffer_request(HEADER_PAGE, &Header_Page);
		strcpy(Buffer8, to_string(Parent_Page_Number).c_str());
		memcpy(&Header_Page.Page_Buffer[8], Buffer8, 8);
		buffer_write(HEADER_PAGE, &Header_Page);


        // Internal Page의 정보 변경
        buffer_request(Parent_Page_Number, &Parent_Page);

        // Internal Page - Parent Page Number 정보 변경
        uint64_t new_Parent_Page_Number = 0;
        strcpy(Buffer8, (to_string(new_Parent_Page_Number)).c_str());
        memcpy(&Parent_Page.Page_Buffer[0], Buffer8, 8);

        // Internal Page - Is Leaf 정보 변경
        uint64_t new_Is_Leaf = 0;
        strcpy(Buffer4, (to_string(new_Is_Leaf)).c_str());
        memcpy(&Parent_Page.Page_Buffer[8], Buffer4, 4);

        // Internal Page - Number of Keys 정보 변경
        uint64_t new_Number_of_Keys = 1;
        strcpy(Buffer4, (to_string(new_Number_of_Keys)).c_str());
        memcpy(&Parent_Page.Page_Buffer[12], Buffer4, 4);

        // Internal Page - One More Page Number 정보 변경 (Key 값 기준 왼쪽 페이지 정보)
        uint64_t new_One_more_Page_Number = left;
        strcpy(Buffer8, (to_string(new_One_more_Page_Number)).c_str());
        memcpy(&Parent_Page.Page_Buffer[120], Buffer8, 8);

        // Key 값 삽입
        strcpy(Buffer8, (to_string(key)).c_str());
        memcpy(&Parent_Page.Page_Buffer[128], Buffer8, 8);

        // Key 값 기준 오른쪽 페이지 정보 삽입
        strcpy(Buffer8, (to_string(right)).c_str());
        memcpy(&Parent_Page.Page_Buffer[128 + 8], Buffer8, 8);

        strcpy(Buffer8, to_string(Parent_Page_Number).c_str());
        memcpy(&Page.Page_Buffer[0], Buffer8, 8);
        buffer_write(left, &Page);

        strcpy(Buffer8, to_string(Parent_Page_Number).c_str());
        memcpy(&New_Page.Page_Buffer[0], Buffer8, 8);
        buffer_write(right, &New_Page);

        buffer_write(Parent_Page_Number, &Parent_Page);

        buffer_close(HEADER_PAGE);
        buffer_close(Parent_Page_Number);
        buffer_close(left);
        buffer_close(right);

        return Parent_Page_Number;
    }

    // Parent Page 에 있는 Key 와 Value 값을 배열로 만듦
    uint64_t Keys[248];
    uint64_t Temp_Keys[249];
    pagenum_t Page_Numbers[249];
    pagenum_t Temp_Page_Numbers[250];
    
    memcpy(Buffer8, &Parent_Page.Page_Buffer[120], 8);
    Page_Numbers[0] = atoi(Buffer8);

    for (int i = 0; i < Parent_Number_of_Keys; i++) {
        memcpy(Buffer8, &Parent_Page.Page_Buffer[128 + 16*i], 8);
        Keys[i] = atoi(Buffer8);

        memcpy(Buffer8, &Parent_Page.Page_Buffer[120 + 16 * (i + 1)], 8);
        Page_Numbers[i+1] = atoi(Buffer8);
    }
    
    // 부모 페이지가 존재하는 경우 Key가 들어가야할 Index 의 왼쪽 Index 를 찾음
    int left_index = 0;
    while (left_index <= Parent_Number_of_Keys && Page_Numbers[left_index] != left)
        left_index++;

    // 부모 페이지가 존재하고, 그 페이지 안에 Key 가 들어갈 수 있는 경우
    // Key 값을 부모 페이지에 넣어만 줌
    if (Parent_Number_of_Keys < 248) {
        // Left index 를 기준으로 큰 값들은 이동시킴
        for (int i = Parent_Number_of_Keys; i > left_index; i--) {
            Page_Numbers[i+1] = Page_Numbers[i];
            Keys[i] = Keys[i-1];
        }

        // 빈 자리에 Key 값과 페이지 번호를 삽입
        Page_Numbers[left_index+1] = right;
        Keys[left_index] = key;
        Parent_Number_of_Keys++;

        strcpy(Buffer4, to_string(Parent_Number_of_Keys).c_str());
        memcpy(&Parent_Page.Page_Buffer[12], Buffer4, 4);


        // 부모 페이지에 각 값들을 적어줌
        for (int i = 0; i < Parent_Number_of_Keys; i++) {
            strcpy(Buffer8, (to_string(Page_Numbers[i])).c_str());
            memcpy(&Parent_Page.Page_Buffer[120 + 16 * i], Buffer8, 8);

            strcpy(Buffer8, (to_string(Keys[i])).c_str());
            memcpy(&Parent_Page.Page_Buffer[128 + 16 * i], Buffer8, 8);
        }

        // 맨 오른쪽 페이지 넘버도 적어줌
        strcpy(Buffer8, (to_string(Page_Numbers[Parent_Number_of_Keys])).c_str());
        memcpy(&Parent_Page.Page_Buffer[120 + 16 * Parent_Number_of_Keys], Buffer8, 8);

        buffer_write(Parent_Page_Number, &Parent_Page);

        buffer_close(left);
        buffer_close(right);
        buffer_close(Parent_Page_Number);


        return Parent_Page_Number;
    }

    // 부모 페이지가 존재하고, 그 페이지 안에 Key 가 들어갈 수 없는 경우
    // split 진행
    else {

        for (int i = 0, j = 0; i < Parent_Number_of_Keys + 1; i++, j++) {
            if (j == left_index + 1) j++;
            Temp_Page_Numbers[j] = Page_Numbers[i];
        }

        for (int i = 0, j = 0; i < Parent_Number_of_Keys; i++, j++) {
            if (j == left_index) j++;
            Temp_Keys[j] = Keys[i];
        }

        Temp_Page_Numbers[left_index + 1] = right;
        Temp_Keys[left_index] = key;

        int split = 124;

        buffer_close(left);
        buffer_close(right);
        buffer_close(Parent_Page_Number);

        //새로운 페이지를 만듦
        pagenum_t New_Parent_Sibling_Page = buffer_alloc_page();
        Parent_Number_of_Keys = 0;

        buffer_request(left, &Page);
        buffer_request(right, &New_Page);
        buffer_request(Parent_Page_Number, &Parent_Page);

        // split 기준 전까지 기존 부모 페이지에 temp 값 저장
        for (int i = 0; i < split-1; i++) {
            strcpy(Buffer8, to_string(Temp_Page_Numbers[i]).c_str());
            memcpy(&Parent_Page.Page_Buffer[120 + 16 * i], Buffer8, 8);
            strcpy(Buffer8, to_string(Temp_Keys[i]).c_str());
            memcpy(&Parent_Page.Page_Buffer[128 + 16 * i], Buffer8, 8);
            Parent_Number_of_Keys++;
        }

        strcpy(Buffer8, to_string(Temp_Page_Numbers[split-1]).c_str());
        memcpy(&Parent_Page.Page_Buffer[120 + 16 * (split-1)], Buffer8, 8);

        int k_prime = Temp_Keys[split-1];

        strcpy(Buffer4, to_string(Parent_Number_of_Keys).c_str());
        memcpy(&Parent_Page.Page_Buffer[12], Buffer4, 4);

        // Split 한 부분중 기존 리프 페이지에서 사용되지 않는 부분은 초기화해줌
        for (int i = Parent_Number_of_Keys; i < 248; i++) {
            string str = "";
            strcpy(Buffer8, str.c_str());
            memcpy(&Parent_Page.Page_Buffer[128 + 16 * i], Buffer8, 8);
            memcpy(&Parent_Page.Page_Buffer[128 + 16 * i + 8], Buffer8, 8);
        }

        buffer_write(Parent_Page_Number, &Parent_Page);


        // split 기준 이후부터는 새로운 인터널 페이지에 temp 값 저장
        page_t Temp;
        buffer_request(New_Parent_Sibling_Page, &Temp);
        int new_sibling_number_of_keys = 0;
        for (int i = split, j = 0; i < 249; i++, j++) {
            strcpy(Buffer8, to_string(Temp_Page_Numbers[i]).c_str());
            memcpy(&Temp.Page_Buffer[120 + 16 * j], Buffer8, 8);
            strcpy(Buffer8, to_string(Temp_Keys[i]).c_str());
            memcpy(&Temp.Page_Buffer[128 + 16 * j], Buffer8, 8);
            new_sibling_number_of_keys++;
        }

        strcpy(Buffer8, to_string(Temp_Page_Numbers[249]).c_str());
        memcpy(&Temp.Page_Buffer[120 + 16 * 125], Buffer8, 8);

        strcpy(Buffer4, to_string(new_sibling_number_of_keys).c_str());
        memcpy(&Temp.Page_Buffer[12], Buffer4, 4);

        memcpy(&Temp.Page_Buffer[0], &Parent_Page.Page_Buffer[0], 8);

        strcpy(Buffer4, to_string(0).c_str());
        memcpy(&Temp.Page_Buffer[8], Buffer4, 4);

        buffer_write(New_Parent_Sibling_Page, &Temp);


        buffer_close(left);
        buffer_close(right);
        buffer_close(Parent_Page_Number);

        // 기존 부모 페이지를 부모로 가리키던 새로운 페이지의 자식들을 새로운 페이지가 부모가 되게 해줌
        for (int i = 0; i <= new_sibling_number_of_keys; i++) {
            page_t Child_Page;
            pagenum_t Child_Page_Number;
            memcpy(Buffer8, &Temp.Page_Buffer[120 + 16 * i], 8);
            Child_Page_Number = atoi(Buffer8);

            buffer_request(Child_Page_Number, &Child_Page);

            strcpy(Buffer8, to_string(New_Parent_Sibling_Page).c_str());
            memcpy(&Child_Page.Page_Buffer[0], Buffer8, 8);

            buffer_write(Child_Page_Number, &Child_Page);

            buffer_close(Child_Page_Number);
        }

        buffer_close(New_Parent_Sibling_Page);

        bpt_insert_into_parent(Parent_Page_Number, k_prime, New_Parent_Sibling_Page);

        return Parent_Page_Number;
    }
}













// Delete 성공시 1을, 실패시 -1을 반환함 [Delayed Merge 구현]
int bpt_delete(int64_t key) {
    char* ret_val = new char[120];
    pagenum_t Find_Page = bpt_find_original(key, ret_val);
    page_t Page;


    if (Find_Page == 0) {
        return -1;
    }

    // Leaf Page, Internal Page Key 가 전부 지워질 때 까지 Merge 하지 않음

    bpt_delete_entry(Find_Page, key);

    delete[] ret_val;
    return 1;
}


void bpt_delete_entry(pagenum_t Page_Number, int64_t key) {
    char Buffer4[4], Buffer8[8], Buffer120[120];

    // 변수 설정
    page_t Page;
    buffer_request(Page_Number, &Page);
    bpt_remove_entry_from_page(Page_Number, key);
    buffer_request(Page_Number, &Page);

    // 만약 제거한 페이지가 루트 페이지라면 루트 페이지를 조정해줌
    page_t Header_Page;
    buffer_request(HEADER_PAGE, &Header_Page);
    memcpy(Buffer8, &Header_Page.Page_Buffer[8], 8);
    pagenum_t Root_Page = atoi(Buffer8);

    if(Page_Number == Root_Page) {
        buffer_close(Page_Number);
        buffer_close(HEADER_PAGE);
        bpt_adjust_root_page(Page_Number);
        return ;
    }

    // 루트 페이지보다 밑 페이지에서 제거한 경우
    // Delayed Merge 구현을 위해 최소 유지 크기를 1로 정함
    int min_keys = 1;

    // 만약 제거한 후 페이지에 있는 Key 개수가 0이 아니면 그대로 끝
    memcpy(Buffer4, &Page.Page_Buffer[12], 4);
    if (atoi(Buffer4) >= min_keys) {
        buffer_close(Page_Number);
        buffer_close(HEADER_PAGE);
        return;
    }

    // 만약 제거한 후 페이지에 있는 Key 개수가 0이면 Delayed Merge 구현
    // Redistribution은 internal page merge 할 때, 형제 페이지가 꽉 차있을 경우에만 진행
    // Merge 할 형제 페이지를 구하고
    // 형제 페이지와 기존 페이지 사이를 가리키던 부모 페이지의 Key 값 K_prime 값을 구함

    // 가장 가까운 왼쪽 형제 페이지가 존재할 경우 할당
    // 만약 페이지가 가장 왼족 자식 페이지일 경우 -1 로 할당
    page_t Parent_Page;
    pagenum_t Parent_Page_Number;
    memcpy(Buffer8, &Page.Page_Buffer[0], 8);
    Parent_Page_Number = atoi(Buffer8);
    buffer_request(Parent_Page_Number, &Parent_Page);
    memcpy(Buffer4, &Parent_Page.Page_Buffer[12], 4);
    uint32_t Parent_Page_Number_of_Keys = atoi(Buffer4);
    int neighbor_index;
    int k_prime_index;
    int64_t k_prime;
    int neighbor;
    int capacity;
    uint32_t Is_Leaf;
    


    for (int i = 0; i <= Parent_Page_Number_of_Keys; i++) {
        memcpy(Buffer8, &Parent_Page.Page_Buffer[120 + 16 * i], 8);
        if (atoi(Buffer8) == Page_Number) {
            neighbor_index = i - 1;
            break;
        }
        neighbor_index = -1;   
    }

    // Delayed Merge를 위한 각종 변수 선언
    k_prime_index = neighbor_index == -1 ? 0 : neighbor_index;
    memcpy(Buffer8, &Parent_Page.Page_Buffer[128 + 16 * k_prime_index], 8);
    k_prime = atoi(Buffer8);
    int tempA, tempB;
    memcpy(Buffer8, &Parent_Page.Page_Buffer[136], 8);
    tempA = atoi(Buffer8);
    memcpy(Buffer8, &Parent_Page.Page_Buffer[120 + 16 * neighbor_index], 8);
    tempB = atoi(Buffer8);
    neighbor = neighbor_index == -1 ? tempA : tempB;
    memcpy(Buffer4, &Page.Page_Buffer[8], 4);
    Is_Leaf = atoi(Buffer4);

    page_t Neighbor_Page;
    buffer_request(neighbor, &Neighbor_Page);
    memcpy(Buffer4, &Neighbor_Page.Page_Buffer[12], 4);
    uint32_t Neighbor_Number_of_Keys = atoi(Buffer4);


    // Delayed Merge 사용
    if (Is_Leaf == 1 || (Is_Leaf == 0 && Neighbor_Number_of_Keys < 248)) {
        buffer_close(Page_Number);
        buffer_close(HEADER_PAGE);
        buffer_close(Parent_Page_Number);
        buffer_close(neighbor);
        bpt_delayed_merge(Page_Number, neighbor, neighbor_index, k_prime);
        return;
    }

    // Internal Page 를 지울 때, 형제 페이지 Key 개수도 꽉차있으면 
    // Redistribute 사용

    buffer_close(Page_Number);
    buffer_close(HEADER_PAGE);
    buffer_close(Parent_Page_Number);
    buffer_close(neighbor);
    bpt_redistribute_page(Page_Number, neighbor, neighbor_index, k_prime_index, k_prime);
    return;
}
    

void bpt_remove_entry_from_page(pagenum_t Page_Number, int64_t key) {
    char Buffer4[4], Buffer8[8], Buffer120[120];

    // 변수 선언
    page_t Page;
    buffer_request(Page_Number, &Page);

    // 우선 Delete를 한번 진행
    memcpy(Buffer4, &Page.Page_Buffer[12], 4);
    uint32_t Number_of_Keys = atoi(Buffer4);
    memcpy(Buffer4, &Page.Page_Buffer[8], 4);
    uint32_t Is_Leaf = atoi(Buffer4);


    // 리프 페이지에서 제거하는 경우
    if (Is_Leaf == 1) {
        uint64_t Keys[31];
        char Values[31][120];

        for (int i = 0; i < Number_of_Keys; i++) {
            memcpy(Buffer8, &Page.Page_Buffer[128 + 128*i], 8);
            Keys[i] = atoi(Buffer8);

            memcpy(Values[i], &Page.Page_Buffer[128 + 8 + 128*i], 120);
        }

        // Key가 있는 index를 찾음
        int i = 0;
        while (Keys[i] != key) i++;


        for (++i; i < Number_of_Keys; i++) {
            memcpy(&Page.Page_Buffer[128 * i], &Page.Page_Buffer[128 * (i + 1)], 8);
            memcpy(&Page.Page_Buffer[128 * i + 8], &Page.Page_Buffer[128 * (i + 1) + 8], 120);
        }

        // 밀고 남은 Key와 Value 값은 초기화 시켜줌
        string str = "";
        strcpy(Buffer120, str.c_str());
        strcpy(Buffer8, str.c_str());
        memcpy(&Page.Page_Buffer[128 * (Number_of_Keys)], Buffer8, 8);
        memcpy(&Page.Page_Buffer[128 * (Number_of_Keys) + 8], Buffer120, 120);

    }

    // 인터널 페이지에서 제거하는 경우
    else {
        uint64_t Keys[248];
        uint64_t Pages[249];

        for (int i = 0; i < Number_of_Keys; i++) {
            memcpy(Buffer8, &Page.Page_Buffer[128 + 16 * i], 8);
            Keys[i] = atoi(Buffer8);

            memcpy(Buffer8, &Page.Page_Buffer[120 + 16 * i], 8);
            Pages[i] = atoi(Buffer8);
        }

        memcpy(Buffer8, &Page.Page_Buffer[120 + 16 * Number_of_Keys], 8);
        Pages[Number_of_Keys] = atoi(Buffer8);

        // Key가 있는 index를 찾음
        int i = 0;
        while (Keys[i] != key) i++;

        // 찾은 Index 값보다 1큰 값을 기준으로 Key값을 한칸씩 옮김
        for (++i; i < Number_of_Keys; i++) {
            memcpy(&Page.Page_Buffer[128 + 16 * (i - 1)], &Page.Page_Buffer[128 + 16 * i], 8);
            memcpy(&Page.Page_Buffer[128 + 8 + 16 * (i - 1)], &Page.Page_Buffer[128 + 8 + 16 * i], 8);
        }

        // 밀고 남은 Key와 Value 값은 초기화 시켜줌
        string str = "";
        strcpy(Buffer8, str.c_str());
        memcpy(&Page.Page_Buffer[128 + 16 * (Number_of_Keys - 1)], Buffer8, 8);
        memcpy(&Page.Page_Buffer[128 + 16 * (Number_of_Keys - 1) + 8], Buffer8, 8);
    }


    Number_of_Keys--;
    strcpy(Buffer4, to_string(Number_of_Keys).c_str());
    memcpy(&Page.Page_Buffer[12], Buffer4, 4);

    buffer_write(Page_Number, &Page);
    buffer_close(Page_Number);
    return ;
}

void bpt_adjust_root_page(pagenum_t Page_Number) {
    char Buffer4[4], Buffer8[8], Buffer120[120];

    // 변수 설정
    page_t Page;
    buffer_request(Page_Number, &Page);

    // 제거 후 루트 페이지가 비어있지 않은 경우 adjust가 필요 없음
    memcpy(Buffer4, &Page.Page_Buffer[12], 4);
    uint64_t Number_of_Pages = atoi(Buffer4);
    if (Number_of_Pages > 0)
        return ;

    // 제거 후, 루트 페이지가 Internal 페이지인 경우 (자식 페이지가 있는 경우)
    // 첫 번째 자식 페이지를 새로운 루트 페이지로 사용
    memcpy(Buffer4, &Page.Page_Buffer[8], 4);
    uint64_t Is_Leaf = atoi(Buffer4);

    if (Is_Leaf == 0) {
        page_t Header_Page, Child_Page;
        memcpy(Buffer8, &Page.Page_Buffer[120], 8);
        uint64_t Child_Page_Number = atoi(Buffer8);

        // Header Page의 루트 페이지 정보 변경
        buffer_request(HEADER_PAGE, &Header_Page);
        memcpy(&Header_Page.Page_Buffer[8], Buffer8, 8);

        // Child Page의 부모 페이지 정보 변경
        buffer_request(Child_Page_Number, &Child_Page);
        strcpy(Buffer8, to_string(0).c_str());
        memcpy(&Child_Page.Page_Buffer[0], Buffer8, 8);

        buffer_write(HEADER_PAGE, &Header_Page);
        buffer_write(Child_Page_Number, &Child_Page);
        buffer_close(Child_Page_Number);
    }

    // 자식 페이지가 없다면 전체 Tree가 빈 경우임
    else {
        // Header Page의 루트 페이지 정보 변경
        page_t Header_Page;
        buffer_request(HEADER_PAGE, &Header_Page);
        strcpy(Buffer8, to_string(0).c_str());
        memcpy(&Header_Page.Page_Buffer[8], Buffer8, 8);
        buffer_write(HEADER_PAGE, &Header_Page);
    }

    // 기존 사용하던 아무것도 없던 Root Page는 할당 해제해줌
    buffer_file_free_page(Page_Number);
    buffer_close(Page_Number);
    buffer_close(HEADER_PAGE);
}

void bpt_delayed_merge(pagenum_t Page_Number, pagenum_t Neighbor_Page_Number, int Neighbor_index, int k_prime) {
    char Buffer4[4], Buffer8[8], Buffer120[120];

    pagenum_t Temp_Page_Number;
    page_t Page, Neighbor_Page;
    int neighbor_insertion_index;

    // 기존 페이지가 맨 왼쪽에 있다면, 오른쪽 형제 페이지와 바꿈
    if (Neighbor_index == -1){
        Temp_Page_Number = Page_Number;
        Page_Number = Neighbor_Page_Number;
        Neighbor_Page_Number = Temp_Page_Number;
    }

    // 형제 페이지의 어느 인덱스부터 Merge 할지 정함
    buffer_request(Neighbor_Page_Number, &Neighbor_Page);
    memcpy(Buffer4, &Neighbor_Page.Page_Buffer[12], 4);
    uint32_t neighbor_Number_of_Keys = atoi(Buffer4);
    neighbor_insertion_index = neighbor_Number_of_Keys;
    

    // Merge 하는 페이지가 리프 페이지인지 인터널 페이지인지 확인
    buffer_request(Page_Number, &Page);
    memcpy(Buffer4, &Page.Page_Buffer[8], 4);
    uint32_t Is_Leaf= atoi(Buffer4);

    // Parent Page 정보 가져옴
    memcpy(Buffer8, &Page.Page_Buffer[0], 8);
    int Parent_Page_Number = atoi(Buffer8);

    // Merge 하는 페이지가 인터널 페이지 인 경우
    if (Is_Leaf == 0) {
        // 형제 페이지의 합쳐지는 부분에 k_prime 값을 넣어줌
        strcpy(Buffer8, to_string(k_prime).c_str());
        memcpy(&Neighbor_Page.Page_Buffer[128 + 16 * neighbor_insertion_index], Buffer8, 8);
        neighbor_Number_of_Keys++;
        strcpy(Buffer4, to_string(neighbor_Number_of_Keys).c_str());
        memcpy(&Neighbor_Page.Page_Buffer[12], Buffer4, 4);

        // Key가 0개가 되었을 때 바뀌는 것이므로 넣어줄 포인터는 한개 뿐임
        memcpy(&Neighbor_Page.Page_Buffer[120 + 16 * (neighbor_insertion_index + 1)], &Page.Page_Buffer[120], 8);

        // 기존 페이지의 자식 페이지가 형제 페이지를 가리키게 해줌
        page_t Child_Page;
        memcpy(Buffer8, &Page.Page_Buffer[120], 8);
        uint64_t Child_Page_Number = atoi(Buffer8);
        buffer_request(Child_Page_Number, &Child_Page);
        strcpy(Buffer8, to_string(Neighbor_Page_Number).c_str());
        memcpy(&Child_Page.Page_Buffer[0], Buffer8, 8);

        // 기존 페이지는 free page 로 만들어줌
        buffer_file_free_page(Page_Number);

        buffer_write(Child_Page_Number, &Child_Page);
        buffer_close(Child_Page_Number);
    }

    // Merge 하는 페이지가 리프 페이지인 경우
    else {
        // 기존 페이지의 Number of keys = 0 를 구함
        memcpy(Buffer4, &Page.Page_Buffer[12], 4);
        uint32_t Number_of_Keys = atoi(Buffer4);

        // Right Sibling 값을 넣어줌
        memcpy(&Neighbor_Page.Page_Buffer[120], &Page.Page_Buffer[120], 8);

        // 기존 페이지는 free page 로 만들어줌
        buffer_file_free_page(Page_Number);
    }

    // 페이지 저장
    buffer_write(Neighbor_Page_Number, &Neighbor_Page);

    // Merge 완료 후 delete_entry 로 부모 노드에서 사용하던 Key 값을 제거해줌
    bpt_delete_entry(Parent_Page_Number, k_prime);

    buffer_close(Page_Number);
    buffer_close(Neighbor_Page_Number);
}

void bpt_redistribute_page(pagenum_t Page_Number, pagenum_t Neighbor_Page_Number,
             int neighbor_index, int k_prime_index, int k_prime) {
    char Buffer4[4], Buffer8[8], Buffer120[120];

    // Internal Page Delete 중 형제 페이지가 꽉 차서 merge를 못하는 경우 사용함
    page_t Page, Neighbor_Page, Parent_Page;
    buffer_request(Page_Number, &Page);
    buffer_request(Neighbor_Page_Number, &Neighbor_Page);

    // 부모 페이지도 읽어옴
    memcpy(Buffer8, &Page.Page_Buffer[0], 8);
    pagenum_t Parent_Page_Number = atoi(Buffer8);

    buffer_request(Parent_Page_Number, &Parent_Page);

    // Key 개수들을 가져옴
    memcpy(Buffer4, &Neighbor_Page.Page_Buffer[12], 4);
    uint32_t Neighbor_Number_of_Keys = atoi(Buffer4);

    // 기존 페이지의 왼쪽에 형제 페이지가 있는 경우
    if (neighbor_index != -1) {
        // 가장 왼쪽을 가리키던 페이지가 두번째를 가리키게 해줌
        memcpy(&Page.Page_Buffer[120 + 16], &Page.Page_Buffer[120], 8);

        // 가장 왼쪽을 가리키던 페이지는 왼쪽 형제 페이지의 가장 오른쪽을 가리키던 페이지 번호로 바꿈
        memcpy(&Page.Page_Buffer[120], &Neighbor_Page.Page_Buffer[120 + 16 * Neighbor_Number_of_Keys], 8);

        // 바꾼 자식 페이지의 부모 페이지를 기본 페이지를 가리키게 해줌
        page_t Child_Page;
        pagenum_t Child_Page_Number;
        memcpy(Buffer8, &Neighbor_Page.Page_Buffer[120 + 16 * Neighbor_Number_of_Keys], 8);
        Child_Page_Number = atoi(Buffer8);
        buffer_request(Child_Page_Number, &Child_Page);
        strcpy(Buffer8, to_string(Page_Number).c_str());
        memcpy(&Child_Page.Page_Buffer[0], Buffer8, 8);
        buffer_write(Child_Page_Number, &Child_Page);
        buffer_close(Child_Page_Number);

        // 왼쪽 형제 페이지에서 빌려온 값은 할당 해제 해줌
        string Empty = "";
        strcpy(Buffer8, Empty.c_str());
        memcpy(&Neighbor_Page.Page_Buffer[120 + 16 * Neighbor_Number_of_Keys], Buffer8, 8);

        // 기존 페이지의 가장 왼쪽 Key 값은 k_prime값을 넣어줌
        strcpy(Buffer8, to_string(k_prime).c_str());
        memcpy(&Page.Page_Buffer[128], Buffer8, 8);

        // 이후 부모 페이지의 k_prime 대신 할 값은 왼쪽 형제 페이지의 가장 오른쪽 Key 값으로 설정해줌
        memcpy(&Parent_Page.Page_Buffer[128 + 16 * k_prime_index], &Neighbor_Page.Page_Buffer[128 + 16 * (Neighbor_Number_of_Keys - 1)], 8);

        // 형제 페이지에서 올라간 부분을 비워줌
        strcpy(Buffer8, Empty.c_str());
        memcpy(&Neighbor_Page.Page_Buffer[128 + 16 * (Neighbor_Number_of_Keys - 1)], Buffer8, 8);
    }

    // 왼쪽 형제 페이지가 없는 경우
    else {
        // 기존 페이지의 Key 값을 k_prime 으로 해줌
        strcpy(Buffer8, to_string(k_prime).c_str());
        memcpy(&Page.Page_Buffer[128], Buffer8, 8);
        
        // 기존 페이지의 오른쪽이 가리키는 페이지가 오른쪽 형제 페이지의 첫번째 페이지를 가리키게 해줌
        memcpy(&Page.Page_Buffer[128 + 8], &Neighbor_Page.Page_Buffer[120], 8);

        // 바꾼 자식 페이지의 부모 페이지를 기본 페이지를 가리키게 해줌
        page_t Child_Page;
        pagenum_t Child_Page_Number;
        memcpy(Buffer8, &Neighbor_Page.Page_Buffer[120], 8);
        Child_Page_Number = atoi(Buffer8);
        buffer_request(Child_Page_Number, &Child_Page);
        strcpy(Buffer8, to_string(Page_Number).c_str());
        memcpy(&Child_Page.Page_Buffer[0], Buffer8, 8);
        buffer_write(Child_Page_Number, &Child_Page);
        buffer_close(Child_Page_Number);

        // 이후 부모 페이지의 k_prime 대신 할 값은 오른쪽 형제 페이지의 가장 왼쪽 Key 값으로 설정해줌
        memcpy(&Parent_Page.Page_Buffer[128 + 16 * k_prime_index], &Neighbor_Page.Page_Buffer[128], 8);
        
        // 형제 페이지에서 하나를 부모 페이지로 올렸으므로 형제 페이지는 한개 삭제된 것과 동일
        // 따라서 그에 따른 값들을 옮겨줌
        for (int i = 0; i < Neighbor_Number_of_Keys - 1; i++) {
            memcpy(&Neighbor_Page.Page_Buffer[128 + 16 * i], &Neighbor_Page.Page_Buffer[128 + 16 * (i+1)], 8);
            memcpy(&Neighbor_Page.Page_Buffer[120 + 16 * i], &Neighbor_Page.Page_Buffer[120 + 16 * (i+1)], 8);
        }

        memcpy(&Neighbor_Page.Page_Buffer[120 + 16 * (Neighbor_Number_of_Keys-1)], &Neighbor_Page.Page_Buffer[120 + 16 * (Neighbor_Number_of_Keys)], 8);

        // 형제 페이지에서 남은 부분을 비워줌
        string Empty = "";
        strcpy(Buffer8, Empty.c_str());
        memcpy(&Neighbor_Page.Page_Buffer[128 + 16 * (Neighbor_Number_of_Keys - 1)], Buffer8, 8);
        strcpy(Buffer8, Empty.c_str());
        memcpy(&Neighbor_Page.Page_Buffer[120 + 16 * (Neighbor_Number_of_Keys)], Buffer8, 8);
    }


    int Number_of_Keys = 1;
    strcpy(Buffer4, to_string(Number_of_Keys).c_str());
    memcpy(&Page.Page_Buffer[12], Buffer4, 4);
    Neighbor_Number_of_Keys--;
    strcpy(Buffer4, to_string(Neighbor_Number_of_Keys).c_str());
    memcpy(&Neighbor_Page.Page_Buffer[12], Buffer4, 4);

    buffer_write(Page_Number, &Page);
    buffer_write(Neighbor_Page_Number, &Neighbor_Page);
    buffer_write(Parent_Page_Number, &Parent_Page);

    buffer_close(Page_Number);
    buffer_close(Neighbor_Page_Number);
    buffer_close(Parent_Page_Number);
}



// project5 함수 추가
int bpt_update(int table_id, int64_t key, char* value, int trx_id) {
    char Buffer4[4], Buffer8[8], Buffer120[120];

    // 루트 노드(루트 페이지)를 찾음
    page_t Header_Page, Page, Root_Page;
    pagenum_t Root_Page_Number;
    uint32_t Is_Leaf;
    uint32_t Number_of_Keys;
    uint64_t Page_Key;


    // File에서 Header 페이지를 찾아서 루트 페이지를 찾음
    buffer_request(HEADER_PAGE, &Header_Page);

    memcpy(Buffer8, &Header_Page.Page_Buffer[8], 8);
    Root_Page_Number = atoi(Buffer8);

    buffer_close(HEADER_PAGE);

    if (Root_Page_Number == 0) {
        return 0;
    }

    // 루트 페이지를 읽어옴
    buffer_request(Root_Page_Number, &Root_Page);


    memcpy(Buffer4, &Root_Page.Page_Buffer[8], 4);
    Is_Leaf = atoi(Buffer4);

    memcpy(Buffer4, &Root_Page.Page_Buffer[12], 4);
    Number_of_Keys = atoi(Buffer4);

    memcpy(Buffer8, &Root_Page.Page_Buffer[128], 8);
    Page_Key = atoi(Buffer8);

    // case1 - 루트 페이지가 Internal Page
    // 리프 페이지를 찾을 때 까지 반복

    int i = 0;
    while (Is_Leaf == 0) {
        i = 0;

        while (i < Number_of_Keys) {
            memcpy(Buffer8, &Root_Page.Page_Buffer[128 + 16 * i], 8);
            int Root_Page_Key = atoi(Buffer8);

            if (key >= Root_Page_Key) i++;
            else 
                break;
        }

        buffer_close(Root_Page_Number);

        memcpy(Buffer8, &Root_Page.Page_Buffer[120 + 16 * i], 8);
        Root_Page_Number = atoi(Buffer8);

        buffer_request(Root_Page_Number, &Root_Page);

        memcpy(Buffer4, &Root_Page.Page_Buffer[8], 4);
        Is_Leaf = atoi(Buffer4);

        memcpy(Buffer4, &Root_Page.Page_Buffer[12], 4);
        Number_of_Keys = atoi(Buffer4);

        memcpy(Buffer8, &Root_Page.Page_Buffer[128], 8);
        Page_Key = atoi(Buffer8);
    }

    // 루트 페이지가 Leaf Page 인 경우 그냥 값을 찾아줌
    // 루트 페이지에는 Find 한 Key 의 page 정보가 담겨있음
    for (int i = 0; i < Number_of_Keys; i++) {
        memcpy(Buffer8, &Root_Page.Page_Buffer[128*(i+1)], 8);
        Page_Key = atoi(Buffer8);
        if (Page_Key == key) {
            buffer_close(Root_Page_Number);

            lock_t* lock_obj = lock_acquire(Current_TABLE_ID, key, trx_id, EXCLUSIVE);


            // deadlock 발생시 0 반환
            if(lock_obj == NULL) {
                return 0;
            }

            // log buffer에 추가해줌
            Log_type log;
            log.log_size = UPDATE_RECORD_SIZE;
            log.LSN = get_log_lsn(UPDATE);
            log.prev_LSN = trx_get_last_lsn(trx_id, log.LSN);
            log.trx_id = trx_id;
            log.type = UPDATE;
            log.table_id = table_id;
            log.page_number = Root_Page_Number;
            log.offset = 128*(i+1) + 8;
            log.data_length = 120;
            memcpy(&log.old_image[0], &Root_Page.Page_Buffer[128*(i+1) + 8], 120);
            memcpy(&log.new_image[0], &value[0], 120);
            log_buffer_add(log);

            lock_obj->temp_record_id = key;
            memcpy(&lock_obj->temp_record_value[0], &Root_Page.Page_Buffer[128*(i+1) + 8], 120);
            memcpy(&lock_obj->temp_new_value[0], &value[0], 120);
            lock_obj->offset = 128*(i+1) + 8;



            memcpy(&Root_Page.Page_Buffer[128*(i+1) + 8], &value[0], 120);
            buffer_page_eviction(Root_Page_Number, &Root_Page);
            buffer_close(Root_Page_Number);
            return 1; // 바꾸는게 성공시 1 반환
        }
    }

    buffer_close(Root_Page_Number);

    // 값이 없으면 0 반환
    return 0;
}


// bpt find를 변경했기 때문에 기존 bpt_find는 bpt_original로 남겨둠
// 이는 bpt_insert delete를 성공적으로 하기 위함임
// Find 성공시 값이 있는 Page를, 찾는 값이 없으면 0을 반환
pagenum_t bpt_find_original(int64_t key, char* ret_val) {
    char Buffer4[4], Buffer8[8], Buffer120[120];


    // 루트 노드(루트 페이지)를 찾음
    page_t Header_Page, Page, Root_Page;
    pagenum_t Root_Page_Number;
    uint32_t Is_Leaf;
    uint32_t Number_of_Keys;
    uint64_t Page_Key;

    if(New_Open[Current_TABLE_ID] == 1) {
        New_Open[Current_TABLE_ID] = 0;
        file_header_alloc();
    }

    int table_id = file_unique_table_id();

    if (table_id == 0) {
        cout << "table id가 0인 에러" << endl;
        exit(0);
    }

    // File에서 Header 페이지를 찾아서 루트 페이지를 찾음
    buffer_request(HEADER_PAGE, &Header_Page);

    memcpy(Buffer8, &Header_Page.Page_Buffer[8], 8);
    Root_Page_Number = atoi(Buffer8);

    buffer_close(HEADER_PAGE);

    if (Root_Page_Number == 0) {
        return 0;
    }

    // 루트 페이지를 읽어옴
    buffer_request(Root_Page_Number, &Root_Page);

    memcpy(Buffer4, &Root_Page.Page_Buffer[8], 4);
    Is_Leaf = atoi(Buffer4);

    memcpy(Buffer4, &Root_Page.Page_Buffer[12], 4);
    Number_of_Keys = atoi(Buffer4);

    memcpy(Buffer8, &Root_Page.Page_Buffer[128], 8);
    Page_Key = atoi(Buffer8);

    // case1 - 루트 페이지가 Internal Page
    // 리프 페이지를 찾을 때 까지 반복


    int i = 0;
    while (Is_Leaf == 0) {
        i = 0;

        while (i < Number_of_Keys) {
            memcpy(Buffer8, &Root_Page.Page_Buffer[128 + 16 * i], 8);
            int Root_Page_Key = atoi(Buffer8);

            if (key >= Root_Page_Key) i++;
            else 
                break;
        }

        buffer_close(Root_Page_Number);

        memcpy(Buffer8, &Root_Page.Page_Buffer[120 + 16 * i], 8);
        Root_Page_Number = atoi(Buffer8);

        buffer_request(Root_Page_Number, &Root_Page);

        memcpy(Buffer4, &Root_Page.Page_Buffer[8], 4);
        Is_Leaf = atoi(Buffer4);

        memcpy(Buffer4, &Root_Page.Page_Buffer[12], 4);
        Number_of_Keys = atoi(Buffer4);

        memcpy(Buffer8, &Root_Page.Page_Buffer[128], 8);
        Page_Key = atoi(Buffer8);
    }

    buffer_close(Root_Page_Number);

    buffer_request(Root_Page_Number, &Page);

    // 루트 페이지가 Leaf Page 인 경우 그냥 값을 찾아줌
    // 루트 페이지에는 Find 한 Key 의 page 정보가 담겨있음
    for (int i = 0; i < Number_of_Keys; i++) {
        memcpy(Buffer8, &Page.Page_Buffer[128*(i+1)], 8);
        Page_Key = atoi(Buffer8);
        if (Page_Key == key) {
            memcpy(Buffer120, &Page.Page_Buffer[128*(i+1) + 8], 120);
            string Temp(Buffer120);
            strncpy(ret_val, Temp.c_str(), 120);
            buffer_close(Root_Page_Number);
            return Root_Page_Number; // Leaf Page 를 내타냄
        }
    }

    buffer_close(Root_Page_Number);

    // 값이 없으면 0 반환
    return 0;
}