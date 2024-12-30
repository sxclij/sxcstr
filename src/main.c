#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 1024

// ファイルから内容を読み込む関数
char* read_file(const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("ファイルを開けませんでした");
        return NULL;
    }

    // ファイルサイズの取得
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    rewind(fp);

    char* buffer = (char*)malloc(file_size + 1);
    if (buffer == NULL) {
        perror("メモリ割り当てエラー");
        fclose(fp);
        return NULL;
    }

    size_t bytes_read = fread(buffer, 1, file_size, fp);
    if (bytes_read != (size_t)file_size) {
        perror("ファイルの読み込みエラー");
        free(buffer);
        fclose(fp);
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
    char buffer[BUFFER_SIZE] = {0};

    // ソケットの作成
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("ソケットの作成に失敗しました");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // ソケットにアドレスをバインド
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("バインドに失敗しました");
        exit(EXIT_FAILURE);
    }

    // リッスン開始
    if (listen(server_fd, 3) < 0) {
        perror("リッスンに失敗しました");
        exit(EXIT_FAILURE);
    }

    printf("ポート %d でリッスン中...\n", PORT);

    while (1) {
        // クライアントからの接続を受け入れる
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("接続の受け入れに失敗しました");
            exit(EXIT_FAILURE);
        }

        // リクエストの読み込み
        ssize_t bytes_received = recv(new_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received < 0) {
            perror("リクエストの受信に失敗しました");
            close(new_socket);
            continue;
        }
        buffer[bytes_received] = '\0';
        printf("リクエスト:\n%s\n", buffer);

        // index.html ファイルの読み込み
        char* html_content = read_file("index.html");
        if (html_content == NULL) {
            const char* not_found_response = "HTTP/1.1 404 Not Found\r\nContent-Length: 13\r\nConnection: close\r\n\r\n404 Not Found";
            send(new_socket, not_found_response, strlen(not_found_response), 0);
            close(new_socket);
            continue;
        }

        // HTTPレスポンスの作成
        char response[BUFFER_SIZE * 2]; // 大きめのバッファを確保
        snprintf(response, sizeof(response),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: text/html\r\n"
                 "Content-Length: %zu\r\n"
                 "Connection: close\r\n"
                 "\r\n"
                 "%s", strlen(html_content), html_content);

        // レスポンスの送信
        send(new_socket, response, strlen(response), 0);
        printf("レスポンスを送信しました:\n%s\n", response);

        // 後処理
        free(html_content);
        close(new_socket);
    }

    // ここには到達しないはずですが、念のためソケットをクローズ
    close(server_fd);
    return 0;
}