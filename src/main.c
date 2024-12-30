#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_POSTS 10

// グローバル変数: 終了フラグ
volatile sig_atomic_t quit_flag = 0;

typedef struct {
    char timestamp[30];
    char message[256];
} Post;

Post posts[MAX_POSTS];
int post_count = 0;
pthread_mutex_t posts_mutex = PTHREAD_MUTEX_INITIALIZER;

// 現在時刻の文字列を取得
void get_current_timestamp(char *timestamp) {
    time_t timer;
    char buffer[30];
    time(&timer);
    strftime(buffer, 30, "%Y-%m-%d %H:%M:%S", localtime(&timer));
    strcpy(timestamp, buffer);
}

// 投稿を追加 (スレッドセーフ)
void add_post(const char *message) {
    pthread_mutex_lock(&posts_mutex);
    get_current_timestamp(posts[post_count % MAX_POSTS].timestamp);
    strncpy(posts[post_count % MAX_POSTS].message, message, sizeof(posts[post_count % MAX_POSTS].message) - 1);
    posts[post_count % MAX_POSTS].message[sizeof(posts[post_count % MAX_POSTS].message) - 1] = '\0';
    post_count++;
    pthread_mutex_unlock(&posts_mutex);
}

// 投稿リストをJSON形式で取得 (スレッドセーフ)
char *get_posts_json() {
    pthread_mutex_lock(&posts_mutex);
    char *json_str = strdup("{\"posts\":[");
    for (int i = 0; i < MAX_POSTS; i++) {
        int index = (post_count - MAX_POSTS + i) % MAX_POSTS;
        if (index < 0) index += MAX_POSTS;
        if (posts[index].message[0] != '\0') {
            char *temp;
            asprintf(&temp, "{\"timestamp\":\"%s\", \"message\":\"%s\"}", posts[index].timestamp, posts[index].message);
            if (i > 0 && posts[(post_count - MAX_POSTS + i - 1) % MAX_POSTS].message[0] != '\0') {
                char *old_json_str = json_str;
                asprintf(&json_str, "%s,%s", old_json_str, temp);
                free(old_json_str);
            } else {
                char *old_json_str = json_str;
                asprintf(&json_str, "%s%s", old_json_str, temp);
                free(old_json_str);
            }
            free(temp);
        }
    }
    char *old_json_str = json_str;
    asprintf(&json_str, "%s]}", old_json_str);
    free(old_json_str);
    pthread_mutex_unlock(&posts_mutex);
    return json_str;
}

// HTTPレスポンスを送信
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
}

// GETリクエストの処理
void handle_get_request(int client_socket, const char *uri) {
    if (strcmp(uri, "/posts") == 0) {
        char *json_response = get_posts_json();
        send_response(client_socket, "200 OK", "application/json", json_response);
        free(json_response);
    } else if (strcmp(uri, "/") == 0) {
        FILE *fp = fopen("index.html", "r");
        if (fp == NULL) {
            send_response(client_socket, "404 Not Found", "text/plain", "File not found");
            return;
        }
        fseek(fp, 0, SEEK_END);
        long file_size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        char *file_content = (char *)malloc(file_size + 1);
        if (file_content == NULL) {
            send_response(client_socket, "500 Internal Server Error", "text/plain", "Internal Server Error");
            fclose(fp);
            return;
        }
        fread(file_content, 1, file_size, fp);
        file_content[file_size] = '\0';
        fclose(fp);
        send_response(client_socket, "200 OK", "text/html", file_content);
        free(file_content);
    } else {
        send_response(client_socket, "404 Not Found", "text/plain", "Not Found");
    }
}

// POSTリクエストの処理 (簡易的なJSON解析)
void handle_post_request(int client_socket, const char *uri, const char *body) {
    if (strcmp(uri, "/posts") == 0) {
        if (body != NULL && strlen(body) > 0) {
            const char *message_key = "\"message\":\"";
            const char *start = strstr(body, message_key);
            if (start != NULL) {
                start += strlen(message_key);
                const char *end = strchr(start, '"');
                if (end != NULL) {
                    size_t message_len = end - start;
                    char message[256];
                    if (message_len < sizeof(message)) {
                        strncpy(message, start, message_len);
                        message[message_len] = '\0';
                        add_post(message);
                        send_response(client_socket, "201 Created", "application/json", "{\"status\": \"success\"}");
                        return;
                    }
                }
            }
        }
        send_response(client_socket, "400 Bad Request", "text/plain", "Invalid JSON");
    } else {
        send_response(client_socket, "404 Not Found", "text/plain", "Not Found");
    }
}

// クライアント接続を処理する関数
void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE] = {0};
    ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        printf("Received request:\n%s\n", buffer);

        char *method = strtok(buffer, " ");
        char *uri = strtok(NULL, " ");
        // 簡単なURIデコード
        if (uri != NULL) {
            for (int i = 0; uri[i] != '\0'; i++) {
                if (uri[i] == '%') {
                    if (uri[i+1] == '2' && uri[i+2] == '0') {
                        uri[i] = ' ';
                        memmove(&uri[i+1], &uri[i+3], strlen(&uri[i+3]) + 1);
                    }
                }
            }
        }
        char *http_version = strtok(NULL, "\r\n");
        char *body = NULL;

        if (strcmp(method, "GET") == 0) {
            handle_get_request(client_socket, uri);
        } else if (strcmp(method, "POST") == 0) {
            // Content-Lengthを探す
            int content_length = 0;
            char *current_line = buffer;
            while ((current_line = strtok(current_line == buffer ? buffer : NULL, "\r\n")) != NULL) {
                if (strncmp(current_line, "Content-Length: ", 16) == 0) {
                    content_length = atoi(current_line + 16);
                    break;
                }
            }

            // ボディ部分を探す
            char *body_start = strstr(buffer, "\r\n\r\n");
            if (body_start != NULL) {
                body_start += 4;
                if (strlen(body_start) > 0) {
                    body = body_start;
                } else if (content_length > 0) {
                    // recvでボディ全体を受け取る（簡略化のため、エラー処理は省略）
                    char *full_request = malloc(bytes_received + content_length + 1);
                    strcpy(full_request, buffer);
                    int remaining = content_length - (bytes_received - (body_start - buffer));
                    if (remaining > 0) {
                        recv(client_socket, full_request + bytes_received, remaining, 0);
                    }
                    full_request[bytes_received + remaining] = '\0';
                    body_start = strstr(full_request, "\r\n\r\n");
                    if (body_start != NULL) {
                        body = body_start + 4;
                    }
                    free(full_request);
                }
            }
            handle_post_request(client_socket, uri, body);
        } else {
            send_response(client_socket, "400 Bad Request", "text/plain", "Bad Request");
        }
    }
    close(client_socket);
}

// エラー処理関数
void handle_error(const char *msg) {
    perror(msg);
}

// 終了コマンドを処理する関数
void handle_quit_command() {
    printf("Terminating server...\n");
    quit_flag = 1;
}

// コマンド入力を監視するスレッド
void *command_handler(void *arg) {
    char command[BUFFER_SIZE];
    while (!quit_flag) {
        printf(">> ");
        if (fgets(command, sizeof(command), stdin) != NULL) {
            command[strcspn(command, "\n")] = 0; // 改行を削除
            if (strcmp(command, "quit") == 0) {
                handle_quit_command();
            } else {
                printf("Unknown command: %s\n", command);
            }
        } else {
            break;
        }
    }
    return NULL;
}

// SIGINTハンドラ
void sigint_handler(int sig) {
    handle_quit_command();
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address, client_address;
    socklen_t client_len = sizeof(client_address);
    pthread_t command_thread;

    // SIGINTシグナルを設定
    signal(SIGINT, sigint_handler);

    // コマンド監視スレッドを開始
    if (pthread_create(&command_thread, NULL, command_handler, NULL) != 0) {
        handle_error("Failed to create command thread");
        return 1;
    }

    // ソケットを作成
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        handle_error("Socket creation failed");
        return 1;
    }

    // アドレスを設定
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // ソケットにアドレスをバインド
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        handle_error("Bind failed");
        return 1;
    }

    // リッスン開始
    if (listen(server_fd, 3) < 0) {
        handle_error("Listen failed");
        return 1;
    }

    printf("Listening on port %d...\n", PORT);

    while (!quit_flag) {
        if ((client_socket = accept(server_fd, (struct sockaddr *)&client_address, &client_len)) < 0 && !quit_flag) {
            handle_error("Accept failed");
            continue;
        }
        if (!quit_flag) {
            handle_client(client_socket);
        }
    }

    // コマンド監視スレッドを待機
    pthread_join(command_thread, NULL);

    // サーバーソケットを閉じる
    close(server_fd);
    printf("Server shutting down.\n");
    return 0;
}