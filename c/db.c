// sqlite archtecture
// tokenizer -> parser -> code generator
// -> virtual machine -> b-tree -> pager -> os interface

// front-end
// - tokenizer
// - parser
// - code generator

// back-end
// - virtual machine
// - b-tree
// - pager
// - os interface

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 255
#define size_of_attribute(Struct, Attribute) sizeof(((Struct*)0)->Attribute)

typedef struct {
    char* buffer;
    size_t buffer_length;
    ssize_t input_length;
} InputBuffer;

typedef enum {
    META_COMMAND_SUCCESS,
    META_COMMAND_UNRECOGNIZED_COMMAND,
} MetaCommandResult;

typedef enum {
    PREPARE_SUCCESS,
    PREPARE_NEGATIVE_ID,
    PREPARE_STRING_TOO_LONG,
    PREPARE_SYNTAX_ERROR,
    PREPARE_UNRECOGNIZED_STATEMENT
} PrepareResult;

typedef enum {
    STATEMENT_INSERT,
    STATEMENT_SELECT
} StatementType;

// 以下のテーブルのデータを表す構造体
// 内部表現として使う
typedef struct {
    uint32_t id;
    // null文字を含むため+1
    char username[COLUMN_USERNAME_SIZE + 1];
    char email[COLUMN_EMAIL_SIZE + 1];
} Row;

// 列のコンパクト表現のためのデータサイズ
// column   | size(bytes) | offset
// ---------+-------------+---------
// id       | 4           | 0       
// username | 32          | 4       
// email    | 255         | 36      
// total    | 291         |         

const uint32_t ID_SIZE = size_of_attribute(Row, id);
const uint32_t USERNAME_SIZE = size_of_attribute(Row, username);
const uint32_t EMAIL_SIZE = size_of_attribute(Row, email);
const uint32_t ID_OFFSET = 0;
const uint32_t USERNAME_OFFSET = ID_OFFSET + ID_SIZE;
const uint32_t EMAIL_OFFSET = USERNAME_OFFSET + USERNAME_SIZE;
const uint32_t ROW_SIZE = ID_SIZE + USERNAME_SIZE + EMAIL_SIZE;

// pageサイズは4Kbyte
// ほとんどのコンピューターアーキテクチャの仮想メモリシステムで使用されているページと同じサイズであるため、ページサイズを4キロバイトにしている
// これはデータベースの1ページがOSで使われる1ページに対応することを意味する
// OSは、ページを分割するのではなく、ページ全体をユニット全体としてメモリに出し入れする
const uint32_t PAGE_SIZE = 4096;
#define TABLE_MAX_PAGES 100
const uint32_t ROWS_PER_PAGE = PAGE_SIZE / ROW_SIZE;
const uint32_t TABLE_MAX_ROWS = ROWS_PER_PAGE * TABLE_MAX_PAGES;

typedef struct {
    // ファイルディスクリプタについて
    // http://e-words.jp/w/%E3%83%95%E3%82%A1%E3%82%A4%E3%83%AB%E3%83%87%E3%82%A3%E3%82%B9%E3%82%AF%E3%83%AA%E3%83%97%E3%82%BF.html
    int file_descriptor;
    uint32_t file_length;
    // void* は汎用ポインタ型
    // あらゆるポインタの方に変換できる
    // anyっぽさある
    void* pages[TABLE_MAX_PAGES];
} Pager;

typedef struct {
    uint32_t num_rows;
    Pager* pager;
} Table;

typedef struct {
    StatementType type;
    Row row_to_insert; // insertの時のみ使う
} Statement;

typedef enum {
    EXECUTE_TABLE_FULL,
    EXECUTE_SUCCESS,
} ExecuteResult;


// Cursor オブジェクト
// テーブル内の位置を表す
// やることは以下
// - テーブルの先頭にカーソルを作成する
// - テーブルの終端にカーソルを作成する
// - カーソルが指し示すデータ列へアクセスする
// - カーソルを次の行に進める
typedef struct {
    Table* table;
    uint32_t row_num;
    bool end_of_table; // 最後の要素の一つ後の位置を指し示す
} Cursor;

// ========= part1 start ===========
InputBuffer* new_input_buffer();
void print_prompt();
void read_input(InputBuffer*);
void close_input_buffer(InputBuffer*);
// ========= part1 end ===========

// ========= part2 start ===========
MetaCommandResult do_meta_command(InputBuffer*, Table*);
PrepareResult prepare_statement(InputBuffer*, Statement*);
ExecuteResult execute_statement(Statement*, Table*);
// ========= part2 end ===========

// ========= part3 start ===========
void serialize_row(Row*, void*);
void deserialize_row(void*, Row*);

// void* row_slot(Table*, uint32_t);
void print_row(Row*);
// 実際のところ，sqliteではデータをb-treeにするが，今回は簡単のため，配列で保持することにする
// 実装計画は以下
// - ページと呼ばれるメモリのブロックに列（データ）を格納する
// - 各ページには収まるだけの列を格納する(あまりなどが出ないようにする)
// - 各行はページに格納する際にコンパクトにするためシリアライズする
// - ページは必要なだけメモリの割り当てがなされる(１つしか必要ない場合に二つ以上ページを持たない)
// - ページへのポインタの固定長配列を保持する
// ========= part3 end ===========


// ========= part4 start ===========
PrepareResult prepare_insert(InputBuffer*, Statement*);
// ========= part4 end ===========


// ========= part5 start ===========
Pager* pager_open(const char*);
void* get_page(Pager*, uint32_t);

Table* db_open(const char*);
void db_close(Table*);
void pager_flush(Pager*, uint32_t, uint32_t);
// ========= part5 end ===========

// ========= part6 start ===========
Cursor* table_start(Table*);
Cursor* table_end(Table*);
void cursor_advance(Cursor* cursor);


void* cursor_value(Cursor* cursor);
// ========= part6 end ===========

InputBuffer* new_input_buffer() {
    InputBuffer* input_buffer = (InputBuffer *)malloc(sizeof(InputBuffer));
    input_buffer->buffer = NULL;
    input_buffer->buffer_length = 0;
    input_buffer->input_length = 0;

  return input_buffer;
}

void print_prompt() { printf("db > "); }

void read_input(InputBuffer* input_buffer) {
    // ssize_t getline(char **lineptr, size_t *n, FILE *stream);
    // lineptr: バッファへのポインタ
    // NULLに設定されている場合、getlineによって不正に割り当てられているため、コマンドが失敗した場合でもユーザーが解放する必要がある
    // n: バッファサイズへのポインタ
    // stream: input stream, 標準入力をつかう想定
    ssize_t bytes_read = getline(&(input_buffer-> buffer), &(input_buffer->buffer_length), stdin);

    if (bytes_read <= 0) {
        printf("Error reading input\n");
        exit(EXIT_FAILURE);
    }
    // Ignore trailing newline
    // bufferは最初NULLとして扱われるので
    input_buffer->input_length = bytes_read - 1;
    input_buffer->buffer[bytes_read - 1] = 0;
}

void close_input_buffer(InputBuffer* input_buffer) {
    free(input_buffer->buffer);
    free(input_buffer);
}

MetaCommandResult do_meta_command(InputBuffer* input_buffer, Table* table) {
    if (strcmp(input_buffer->buffer, ".exit") == 0) {
        db_close(table);
        exit(EXIT_SUCCESS);
    } else {
        return META_COMMAND_UNRECOGNIZED_COMMAND;
    }
}

PrepareResult prepare_insert(InputBuffer* input_buffer, Statement* statement) {
    statement->type = STATEMENT_INSERT;

    // strtok(char *s1, const char *s2): split関数みたいなもん
    // 呼び出しごとにトークンのポインタを一つずつ返す
    // 2回目以降の呼び出しではs1にNULLを指定する
    char* keyword = strtok(input_buffer->buffer, " ");
    char* id_string = strtok(NULL, " ");
    char* username = strtok(NULL, " ");
    char* email = strtok(NULL, " ");

    bool valid = !(id_string == NULL || username == NULL || email == NULL);

    if (!valid) {
        return PREPARE_SYNTAX_ERROR;
    }

    int id = atoi(id_string);
    if (id < 0) {
        return PREPARE_NEGATIVE_ID;
    }
    // strlen: ヌル文字を含めない文字列の長さ
    if (strlen(username) > COLUMN_USERNAME_SIZE) {
        return PREPARE_STRING_TOO_LONG;
    }
    if (strlen(username) > COLUMN_EMAIL_SIZE) {
        return PREPARE_STRING_TOO_LONG;
    }

    statement->row_to_insert.id = id;
    strcpy(statement->row_to_insert.username, username);
    strcpy(statement->row_to_insert.email, email);

    return PREPARE_SUCCESS;
}

PrepareResult prepare_statement(InputBuffer* input_buffer, Statement* statement) {
    if (strncmp(input_buffer->buffer, "insert", 6) == 0) {
        // 次のようなSQLに対応
        // insert 1 cstack foo@bar.com

        // sscanfの問題点
        // https://stackoverflow.com/questions/2430303/disadvantages-of-scanf
        return prepare_insert(input_buffer, statement);
    }
    if (strncmp(input_buffer->buffer, "select", 6) == 0) {
        statement->type = STATEMENT_SELECT;
        return PREPARE_SUCCESS;
    }
    return PREPARE_UNRECOGNIZED_STATEMENT;
}



ExecuteResult execute_insert(Statement* statement, Table* table) {
    if (table->num_rows >= TABLE_MAX_ROWS) {
        return EXECUTE_TABLE_FULL;
    }

    Row* row_to_insert = &(statement->row_to_insert);
    Cursor* cursor = table_end(table);

    serialize_row(row_to_insert, cursor_value(cursor));
    table->num_rows += 1;

    free(cursor);

    return EXECUTE_SUCCESS;
}

ExecuteResult execute_select(Statement* statement, Table* table) {
    Cursor* cursor = table_start(table);

    Row row;
    while (!(cursor->end_of_table)) {
        deserialize_row(cursor_value(cursor), &row);
        print_row(&row);
        cursor_advance(cursor);
    }

    free(cursor);

    return EXECUTE_SUCCESS;
}

ExecuteResult execute_statement(Statement* statement, Table* table) {
    switch (statement->type) {
        case (STATEMENT_INSERT):
            return execute_insert(statement, table);
        case (STATEMENT_SELECT):
            return execute_select(statement, table);
    }
}

// destinationにはpageの要素のポインタが入る
// メモリレイアウト:
// | id1 | username1 | email1 | id2 | username2 | email2 | ... 
// ^
// destination

// となるような使い方をする
// xxxSIZEで確保するメモリの大きさを
// destination + xxx_OFFSETで格納場所を決める
void serialize_row(Row* source, void* destination) {
    memcpy(destination + ID_OFFSET, &(source->id), ID_SIZE);
    memcpy(destination + USERNAME_OFFSET, &(source->username), USERNAME_SIZE);
    memcpy(destination + EMAIL_OFFSET, &(source->email), EMAIL_SIZE);
}

// serializeの逆
// pageのポインタから1行分のデータをRowとして扱えるようにする
void deserialize_row(void* source, Row* destination) {
    memcpy(&(destination->id), source + ID_OFFSET, ID_SIZE);
    memcpy(&(destination->username), source + USERNAME_OFFSET, USERNAME_SIZE);
    memcpy(&(destination->email), source + EMAIL_OFFSET, EMAIL_SIZE);
}

// row_slotは次に割り当てる列のpageのメモリアドレスを返す
// 列は複数のページに跨って存在しないほうが良い
// あるページが割り当てられているメモリ番地の次の番地に次のページが割り当てられているとは限らないので扱いにくい
void* cursor_value(Cursor* cursor) {
    uint32_t row_num = cursor->row_num;
    uint32_t page_num = row_num / ROWS_PER_PAGE;
    
    void* page = get_page(cursor->table->pager, page_num);
    uint32_t row_offset = row_num % ROWS_PER_PAGE;
    uint32_t byte_offset = row_offset * ROW_SIZE;

    return page + byte_offset;
}

void print_row(Row* row) {
    printf("(%d, %s, %s)\n", row->id, row->username, row->email);
}

// databaseファイルを開く
// pager構造体の初期化
// table構造体の初期化
Table* db_open(const char* filename) {
    Pager* pager = pager_open(filename);
    uint32_t num_rows = pager->file_length / ROW_SIZE;

    Table* table = malloc(sizeof(Table));
    table->pager = pager;
    table->num_rows = num_rows;

    return table;
}

Pager* pager_open(const char* filename) {
    int fd = open(filename,
        O_RDWR | O_CREAT, // O_RDWR: Read/Write モード, O_CREAT: ファイルがなければ作成する
        S_IWUSR | S_IRUSR // S_IWUSR: ユーザに書き込み権限を与える, S_IRUSR: ユーザに読み込み権限を与える
    );

    if (fd == -1) {
        printf("Unable to open file\n");
        exit(EXIT_FAILURE);
    }

    // lseek(int fd, off_t offset, int whence): ファイルの読み書きオフセットの位置を変更する
    // fd: ファイルデスクリプタ
    // offset: whenceからのオフセット
    // whence: オフセットの開始位置
    // SEEK_END: ファイルの終端
    off_t file_length = lseek(fd, 0, SEEK_END);

    Pager* pager = malloc(sizeof(Pager));

    pager->file_descriptor = fd;
    pager->file_length = file_length;

    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
        pager->pages[i] = NULL;
    }

    return pager;
}

void* get_page(Pager* pager, uint32_t page_num) {
    if (page_num > TABLE_MAX_PAGES) {
        printf("Tried to fetch page number out of bounds. %d > %d\n", page_num, TABLE_MAX_PAGES);
        exit(EXIT_FAILURE);
    }

    if (pager->pages[page_num] == NULL) {
        // キャッシュヒットしない場合, ファイルからロードしてpageをメモリ確保する
        void* page = malloc(PAGE_SIZE);
        uint32_t num_pages = pager->file_length / PAGE_SIZE;

        // ファイルの最後にページの一部を保存する可能性あり
        if (pager->file_length % PAGE_SIZE) {
            num_pages += 1;
        }

        if (page_num <= num_pages) {
            // SEEK_SET オフセットはファイルの先頭から
            lseek(pager->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);
            // read(int fd, void* buf, size_t count): ファイルディスクリプタから読み込む
            // fd: ファイルディスクリプタ, buf: 読み込み先バッファ, count: 読み込むバイト数
            ssize_t bytes_read = read(pager->file_descriptor, page, PAGE_SIZE);
            if (bytes_read == -1) {
                printf("Error reading file: %d\n", errno);
                exit(EXIT_FAILURE);
            }
        }

        pager->pages[page_num] = page;
    }

    return pager->pages[page_num];
}

void db_close(Table* table) {
    Pager* pager = table->pager;
    uint32_t num_full_pages = table->num_rows / ROWS_PER_PAGE;

    for (uint32_t i = 0; i < num_full_pages; i++) {
        if (pager->pages[i] == NULL) {
            continue;
        }

        pager_flush(pager, i, PAGE_SIZE);
        free(pager->pages[i]);
        pager->pages[i] = NULL;
    }

    // ページの一部をファイルの終端に書き込む可能性がある
    // b-treeの実装後は不要になる
    uint32_t num_additional_rows = table->num_rows % ROWS_PER_PAGE;
    if (num_additional_rows > 0) {
        uint32_t page_num = num_full_pages;
        if (pager->pages[page_num] != NULL) {
            pager_flush(pager, page_num, num_additional_rows * ROW_SIZE);
            free(pager->pages[page_num]);

            pager->pages[page_num] = NULL;
        }
    }

    int result = close(pager->file_descriptor);
    if (result == -1) {
        printf("Error closing db file.\n");
        exit(EXIT_FAILURE);
    }

    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
        void* page = pager->pages[i];
        if (page) {
            free(page);
            pager->pages[i] = NULL;
        }
    }

    free(pager);
    free(table);
}

void pager_flush(Pager* pager, uint32_t page_num, uint32_t size) {
    if (pager->pages[page_num] == NULL) {
        printf("Tried to flush null page.\n");
        exit(EXIT_FAILURE);
    }

    off_t offset = lseek(pager->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);

    if (offset == -1) {
        printf("Error seeking: %d\n", errno);
        exit(EXIT_FAILURE);
    }

    ssize_t bytes_written = 
        write(pager->file_descriptor, pager->pages[page_num], size);

    if (bytes_written == -1) {
        printf("error writing: %d\n", errno);
        exit(EXIT_FAILURE);
    }
}

Cursor* table_start(Table* table) {
    Cursor* cursor = malloc(sizeof(Cursor));
    cursor->table = table;
    cursor->row_num = 0;
    cursor->end_of_table = (table->num_rows == 0);

    return cursor;
}

Cursor* table_end(Table* table) {
    Cursor* cursor = malloc(sizeof(Cursor));
    cursor->table = table;
    cursor->row_num = table->num_rows;
    cursor->end_of_table = true;

    return cursor;
}

void cursor_advance(Cursor* cursor) {
    cursor->row_num += 1;
    if (cursor->row_num >= cursor->table->num_rows) {
        cursor->end_of_table = true;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Must supply a database filename.\n");
        exit(EXIT_FAILURE);
    }

    char* filename = argv[1];
    Table* table = db_open(filename);

    InputBuffer* input_buffer = new_input_buffer();
    while(true) {
        print_prompt();
        read_input(input_buffer);

        // meta commandは'.'から始まる
        if (input_buffer->buffer[0] == '.') {
            switch (do_meta_command(input_buffer, table)) {
                case (META_COMMAND_SUCCESS):
                    continue;
                case (META_COMMAND_UNRECOGNIZED_COMMAND):
                    printf("Unrecognized command '%s'.\n", input_buffer->buffer);
                    continue;
            }
        }

        Statement statement;
        switch (prepare_statement(input_buffer, &statement)) {
            case (PREPARE_SUCCESS):
                break;
            case (PREPARE_NEGATIVE_ID):
                printf("ID must be positive.\n");
                continue;
            case (PREPARE_STRING_TOO_LONG):
                printf("String is too long.\n");
                continue;
            case (PREPARE_SYNTAX_ERROR):
                printf("Syntax error. Could not parse statement.\n");
                continue;
            case (PREPARE_UNRECOGNIZED_STATEMENT):
                printf("Unrecognized keyword at start of '%s'.\n", input_buffer->buffer);
                continue;
        }

        switch (execute_statement(&statement, table)) {
            case (EXECUTE_SUCCESS):
                printf("Executed.\n");
                break;
            case (EXECUTE_TABLE_FULL):
                printf("Error: Table full.\n");
                break;
        }
    }
}
