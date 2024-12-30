#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#define PORT 8080
#define BUFFER_SIZE 1024

// グローバル変数: 終了フラグ
volatile sig_atomic_t quit_flag = 0;

// JSON パーサーの基本的な構造体 (非常に簡略化された例)
typedef struct {
    char* key;
    char* value;
} JsonPair;

typedef struct {
    JsonPair* pairs;
    int count;
} JsonObject;

// 簡略化された JSON パーサー
JsonObject* parse_json(const char* json_str) {
    JsonObject* obj = (JsonObject*)malloc(sizeof(JsonObject));
    if (!obj) return NULL;
    obj->pairs = NULL;
    obj->count = 0;

    // 簡単な例として、"key":"value" の形式のみを処理
    const char *current = json_str;
    while (*current) {
        if (*current == '{') {
            current++;
            while (*current) {
                if (*current == '"') {
                    current++;
                    const char* key_start = current;
                    while (*current && *current != '"') current++;
                    if (*current == '"') {
                        int key_len = current - key_start;
                        char* key = (char*)malloc(key_len + 1);
                        strncpy(key, key_start, key_len);
                        key[key_len] = '\0';
                        current++; // Skip "
                        while (*current && *current != ':') current++;
                        if (*current == ':') {
                            current++;
                            while (*current == ' ') current++; // Skip spaces
                            if (*current == '"') {
                                current++;
                                const char* value_start = current;
                                while (*current && *current != '"') current++;
                                if (*current == '"') {
                                    int value_len = current - value_start;
                                    char* value = (char*)malloc(value_len + 1);
                                    strncpy(value, value_start, value_len);
                                    value[value_len] = '\0';

                                    obj->count++;
                                    obj->pairs = (JsonPair*)realloc(obj->pairs, sizeof(JsonPair) * obj->count);
                                    obj->pairs[obj->count - 1].key = key;
                                    obj->pairs[obj->count - 1].value = value;

                                    current++; // Skip "
                                } else {
                                    free(key);
                                    free(obj);
                                    return NULL; // Invalid JSON
                                }
                            } else {
                                free(key);
                                free(obj);
                                return NULL; // Invalid JSON
                            }
                        } else {
                            free(key);
                            free(obj);
                            return NULL; // Invalid JSON
                        }
                    } else {
                        free(obj);
                        return NULL; // Invalid JSON
                    }
                } else if (*current == '}') {
                    current++;
                    break;
                }
                while (*current == ' ' || *current == ',' ) current++;
            }
            break;
        }
        current++;
    }
    return obj;
}

// JSON オブジェクトを文字列化する (非常に簡略化された例)
char* stringify_json(const JsonObject* obj) {
    if (!obj) return NULL;
    char* json_str = strdup("{");
    for (int i = 0; i < obj->count; i++) {
        char* temp;
        asprintf(&temp, "\"%s\":\"%s\"", obj->pairs[i].key, obj->pairs[i].value);
        if (i > 0) {
            char* old_json_str = json_str;
            asprintf(&json_str, "%s,%s", old_json_str, temp);
            free(old_json_str);
        } else {
            char* old_json_str = json_str;
            asprintf(&json_str, "%s%s", old_json_str, temp);
            free(old_json_str);
        }
        free(temp);
    }
    char* old_json_str = json_str;
    asprintf(&json_str, "%s}", old_json_str);
    free(old_json_str);
    return json_str;
}

// JSON オブジェクトの解放
void free_json_object(JsonObject* obj) {
    if (obj) {
        for (int i = 0; i < obj->count; i++) {
            free(obj->pairs[i].key);
            free(obj->pairs[i].value);
        }
        free(obj->pairs);
        free(obj);
    }
}

// エラー処理関数
void handle_error(const char *msg) {
    perror(msg);
}

// 文字列が特定のプレフィックスで始まるかどうかを確認する関数
int starts_with(const char *str, const char *prefix) {
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

// HTTPレスポンスを送信する関数
void send_response(int client_socket, const char *status, const char *content_type, const char *body) {
    char headers[BUFFER_SIZE];
    size_t body_len = body ? strlen(body) : 0;
    snprintf(headers, sizeof(headers),
             "HTTP/1.1 %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n", status, content_type ? content_type : "", body_len);

    send(client_socket, headers, strlen(headers), 0);
    if (body && body_len > 0) {
        send(client_socket, body, body_len, 0);
    }
    printf("レスポンスを送信しました:\n%s%s\n", headers, body ? body : "");
}

// ファイルを開く関数
FILE* open_file(const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (fp == NULL) {
        handle_error("ファイルを開けませんでした");
    }
    return fp;
}

// ファイルサイズを取得する関数
long get_file_size(FILE* fp) {
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    rewind(fp);
    return file_size;
}

// ファイル内容をバッファに読み込む関数
size_t read_file_to_buffer(FILE* fp, char* buffer, long file_size) {
    size_t bytes_read = fread(buffer, 1, file_size, fp);
    if (bytes_read != (size_t)file_size) {
        handle_error("ファイルの読み込みエラー");
    }
    return bytes_read;
}

// ファイル読み込み処理をまとめた関数
char* read_file(const char* filename) {
    FILE* fp = open_file(filename);
    if (fp == NULL) {
        return NULL;
    }

    long file_size = get_file_size(fp);
    char* buffer = (char*)malloc(file_size + 1);
    if (buffer == NULL) {
        handle_error("メモリ割り当てエラー");
        fclose(fp);
        return NULL;
    }

    if (read_file_to_buffer(fp, buffer, file_size) != (size_t)file_size) {
        free(buffer);
        fclose(fp);
        return NULL;
    }

    buffer[file_size] = '\0';
    fclose(fp);
    return buffer;
}

// ソケットを作成する関数
int create_socket() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        handle_error("ソケットの作成に失敗しました");
    }
    return sockfd;
}

// アドレス構造体を初期化する関数
void initialize_address(struct sockaddr_in *address) {
    address->sin_family = AF_INET;
    address->sin_addr.s_addr = INADDR_ANY;
    address->sin_port = htons(PORT);
}

// ソケットにアドレスをバインドする関数
int bind_socket(int sockfd, struct sockaddr_in *address) {
    if (bind(sockfd, (struct sockaddr *)address, sizeof(*address)) == -1) {
        handle_error("バインドに失敗しました");
        return -1;
    }
    return 0;
}

// リッスンを開始する関数
int listen_socket(int sockfd) {
    if (listen(sockfd, 3) == -1) {
        handle_error("リッスンに失敗しました");
        return -1;
    }
    return 0;
}

// クライアント接続を受け入れる関数
int accept_connection(int server_fd, struct sockaddr_in *client_address, socklen_t *client_len) {
    int client_socket = accept(server_fd, (struct sockaddr *)client_address, client_len);
    if (client_socket == -1 && !quit_flag) {
        handle_error("接続の受け入れに失敗しました");
    }
    return client_socket;
}

// リクエストを受信する関数
ssize_t receive_request(int client_socket, char *buffer) {
    ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received == -1) {
        handle_error("リクエストの受信に失敗しました");
    } else if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        printf("リクエスト:\n%s\n", buffer);
    }
    return bytes_received;
}

// HTTPメソッドを解析する関数
char* parse_http_method(const char *request) {
    if (starts_with(request, "GET")) return "GET";
    if (starts_with(request, "POST")) return "POST";
    return NULL;
}

// GETリクエストかどうかを判定する関数
int is_get_request(const char *request) {
    return strcmp(parse_http_method(request), "GET") == 0;
}

// POSTリクエストかどうかを判定する関数
int is_post_request(const char *request) {
    return strcmp(parse_http_method(request), "POST") == 0;
}

// 不明なリクエストを処理する関数
void handle_unknown_request(int client_socket) {
    send_response(client_socket, "501 Not Implemented", NULL, "");
}

// index.htmlを送信する関数
void send_index_html(int client_socket) {
    char* html_content = read_file("index.html");
    if (html_content == NULL) {
        send_response(client_socket, "404 Not Found", "text/plain", "404 Not Found");
    } else {
        send_response(client_socket, "200 OK", "text/html; charset=utf-8", html_content);
        free(html_content);
    }
}

// GETリクエストを処理する関数
void handle_get_request(int client_socket) {
    send_index_html(client_socket);
}

// Content-Lengthを取得する関数
int get_content_length(const char *request) {
    char *content_length_str = strstr(request, "Content-Length: ");
    if (content_length_str != NULL) {
        content_length_str += strlen("Content-Length: ");
        return atoi(content_length_str);
    }
    return -1;
}

// POSTリクエストボディの開始位置を特定する関数
char* find_post_body(const char *request) {
    char *body_start = strstr(request, "\r\n\r\n");
    if (body_start != NULL) {
        return body_start + 4;
    }
    return NULL;
}

// POSTデータを処理する関数 (JSONとして処理)
void process_post_data(int client_socket, const char *body_start) {
    JsonObject* json_obj = parse_json(body_start);
    if (json_obj) {
        printf("POST データ (JSON):\n");
        for (int i = 0; i < json_obj->count; i++) {
            printf("  %s: %s\n", json_obj->pairs[i].key, json_obj->pairs[i].value);
            // ここで JSON データに基づいて処理を行う
        }
        free_json_object(json_obj);
        send_response(client_socket, "200 OK", "application/json", "{\"status\": \"success\", \"message\": \"POST request processed\"}");
    } else {
        send_response(client_socket, "400 Bad Request", "text/plain", "Invalid JSON");
    }
}

// POSTリクエストを処理する関数
void handle_post_request(int client_socket, const char *request) {
    char *body_start = find_post_body(request);
    if (body_start != NULL) {
        process_post_data(client_socket, body_start);
    } else {
        send_response(client_socket, "400 Bad Request", "text/plain", "Bad Request: Missing body");
    }
}

// クライアントからの接続を処理する関数
void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE] = {0};
    ssize_t bytes_received = receive_request(client_socket, buffer);
    if (bytes_received > 0) {
        if (is_post_request(buffer)) {
            handle_post_request(client_socket, buffer);
        } else if (is_get_request(buffer)) {
            handle_get_request(client_socket);
        } else {
            handle_unknown_request(client_socket);
        }
    }
}

// 終了コマンドを処理する関数
void handle_quit_command() {
    printf("終了コマンドを受け付けました...\n");
    quit_flag = 1;
}

// 不明なコマンドを処理する関数
void handle_unknown_command() {
    printf("不明なコマンドです。\n");
}

// コマンドを処理する関数
void process_command(const char *command) {
    if (strcmp(command, "quit") == 0) {
        handle_quit_command();
    } else {
        handle_unknown_command();
    }
}

// コマンド入力を取得する関数
char* get_command(char *buffer, size_t size) {
    printf(">> ");
    if (fgets(buffer, size, stdin) == NULL) {
        return NULL;
    }
    buffer[strcspn(buffer, "\n")] = 0;
    return buffer;
}

// コマンド入力監視スレッドの関数
void* command_handler(void* arg) {
    char command[BUFFER_SIZE];
    while (1) {
        if (get_command(command, sizeof(command)) == NULL) {
            break;
        }
        process_command(command);
        if (quit_flag) {
            break;
        }
    }
    return NULL;
}

int main() {
    int server_fd;
    pthread_t command_thread;

    // コマンド入力監視スレッドの作成
    if (pthread_create(&command_thread, NULL, command_handler, NULL) != 0) {
        handle_error("コマンド監視スレッドの作成に失敗しました");
        return EXIT_FAILURE;
    }

    // ソケットの作成
    server_fd = create_socket();
    if (server_fd == -1) {
        return EXIT_FAILURE;
    }

    struct sockaddr_in address;
    initialize_address(&address);

    // ソケットにアドレスをバインド
    if (bind_socket(server_fd, &address) == -1) {
        close(server_fd);
        return EXIT_FAILURE;
    }

    // リッスン開始
    if (listen_socket(server_fd) == -1) {
        close(server_fd);
        return EXIT_FAILURE;
    }

    printf("ポート %d でリッスン中...\n", PORT);

    while (!quit_flag) {
        struct sockaddr_in client_address;
        socklen_t client_len = sizeof(client_address);
        int client_socket = accept_connection(server_fd, &client_address, &client_len);
        if (client_socket != -1) {
            handle_client(client_socket);
            close(client_socket);
        }
    }

    printf("サーバーをシャットダウンしています...\n");
    pthread_join(command_thread, NULL);
    close(server_fd);
    printf("サーバーを終了しました。\n");
    return 0;
}