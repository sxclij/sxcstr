#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>

#define PORT 8080
#define BUFFER_SIZE 1024

// HTTPレスポンスを作成する関数
char* create_http_response(const char* body, const char* content_type) {
    char* response = malloc(BUFFER_SIZE * 2); // 十分なサイズを確保
    if (response == NULL) {
        perror("malloc failed");
        return NULL;
    }
    snprintf(response, BUFFER_SIZE * 2,
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %ld\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             content_type, strlen(body), body);
    return response;
}

// エラーレスポンスを作成する関数
char* create_error_response(int status_code, const char* message) {
    char* response = malloc(BUFFER_SIZE);
    if (response == NULL) {
        perror("malloc failed");
        return NULL;
    }
    snprintf(response, BUFFER_SIZE,
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: text/plain\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%d %s",
             status_code, message, status_code, message);
    return response;
}

// ファイルの内容を読み込む関数
char* read_file(const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (fp == NULL) {
        return NULL;
    }

    // ファイルサイズの取得
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char* buffer = malloc(file_size + 1);
    if (buffer == NULL) {
        fclose(fp);
        return NULL;
    }

    size_t bytes_read = fread(buffer, 1, file_size, fp);
    if (bytes_read != file_size) {
        fclose(fp);
        free(buffer);
        return NULL;
    }
    buffer[file_size] = '\0';
    fclose(fp);
    return buffer;
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE];

    // ソケットの作成
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // アドレスの設定
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // ソケットとアドレスのバインド
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // 接続の待ち受け
    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Listening on port %d...\n", PORT);

    while (1) {
        // クライアントからの接続を受け入れる
        if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept failed");
            exit(EXIT_FAILURE);
        }

        // リクエストの受信
        ssize_t bytes_received = recv(new_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received < 0) {
            perror("recv failed");
            close(new_socket);
            continue;
        }
        buffer[bytes_received] = '\0';
        printf("Received request:\n%s\n", buffer);

        // リクエストラインの解析 (GETメソッドとファイル名のみ対応)
        char method[10];
        char path[BUFFER_SIZE];
        if (sscanf(buffer, "%s %s", method, path) != 2 || strcmp(method, "GET") != 0) {
            char* response = create_error_response(400, "Bad Request");
            send(new_socket, response, strlen(response), 0);
            free(response);
            close(new_socket);
            continue;
        }

        // ファイル名の決定 (ルートパスの場合は index.html を返す)
        char filename[BUFFER_SIZE];
        if (strcmp(path, "/") == 0) {
            strcpy(filename, "index.html");
        } else {
            // パスの先頭の '/' を削除
            strncpy(filename, path + 1, BUFFER_SIZE - 1);
            filename[BUFFER_SIZE - 1] = '\0';
        }

        // ファイルの読み込み
        char* file_content = read_file(filename);
        if (file_content == NULL) {
            char* response = create_error_response(404, "Not Found");
            send(new_socket, response, strlen(response), 0);
            free(response);
        } else {
            // Content-Typeの決定 (簡単な実装として .html の場合のみ対応)
            char* content_type = "text/html";
            char* response = create_http_response(file_content, content_type);
            send(new_socket, response, strlen(response), 0);
            free(response);
            free(file_content);
        }

        // ソケットのクローズ
        close(new_socket);
    }

    // ここには到達しないはずですが、念のため
    close(server_fd);
    return 0;
}