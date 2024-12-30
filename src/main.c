#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>

#define PORT 8080
#define BUFFER_SIZE 65536

char* create_http_response(char* dst, size_t dst_size, const char* body, const char* content_type) {
    snprintf(dst, dst_size,
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %ld\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             content_type, strlen(body), body);
    return dst;
}

char* read_file(char* dst, size_t dst_size, const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (fp == NULL) {
        return NULL; // Indicate failure, though error handling is supposed to be removed
    }
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (file_size >= dst_size) {
        fclose(fp);
        return NULL; // Indicate failure if file is too large
    }
    fread(dst, 1, file_size, fp);
    dst[file_size] = '\0';
    fclose(fp);
    return dst;
}

char* handle_request(char* response_dst, size_t response_dst_size, const char* request) {
    char method[10];
    char path[BUFFER_SIZE];
    if (sscanf(request, "%s %s", method, path) != 2 || strcmp(method, "GET") != 0) {
        return NULL; // Or some default error response if needed
    }

    char filename[BUFFER_SIZE];
    if (strcmp(path, "/") == 0) {
        strcpy(filename, "index.html");
    } else {
        strncpy(filename, path + 1, sizeof(filename) - 1);
        filename[sizeof(filename) - 1] = '\0';
    }

    static char file_content_buffer[BUFFER_SIZE]; // Still need a static buffer here
    char* file_content = read_file(file_content_buffer, sizeof(file_content_buffer), filename);
    if (file_content == NULL) {
        // Handle file reading failure, though error handling is supposed to be removed
        return NULL;
    }

    return create_http_response(response_dst, response_dst_size, file_content, "text/html");
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    static char request_buffer[BUFFER_SIZE];
    static char response_buffer[BUFFER_SIZE * 2];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr*)&address, sizeof(address));

    listen(server_fd, 3);

    printf("Listening on port %d...\n", PORT);

    while (1) {
        new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        ssize_t bytes_received = recv(new_socket, request_buffer, sizeof(request_buffer) - 1, 0);
        request_buffer[bytes_received] = '\0';
        printf("Received request:\n%s\n", request_buffer);
        char* response = handle_request(response_buffer, sizeof(response_buffer), request_buffer);
        if (response != NULL) {
            send(new_socket, response, strlen(response), 0);
        }
        close(new_socket);
    }

    close(server_fd);
    return 0;
}