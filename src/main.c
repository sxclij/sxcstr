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
#define BUFFER_SIZE 1024 * 1024 * 16
#define SAVEDATA_CAPACITY 1024 * 1024 * 128

struct global {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen;
};

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
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    fread(dst, 1, file_size, fp);
    dst[file_size] = '\0';
    fclose(fp);
    return dst;
}

char* handle_get_request(char* response_dst, size_t response_dst_size, const char* path) {
    char filename[BUFFER_SIZE];
    strcpy(filename, "index.html");
    static char file_content_buffer[BUFFER_SIZE];
    char* file_content = read_file(file_content_buffer, sizeof(file_content_buffer), filename);
    return create_http_response(response_dst, response_dst_size, file_content, "text/html");
}

char* handle_post_request(char* response_dst, size_t response_dst_size, const char* request) {
    char* body_start = strstr(request, "\r\n\r\n");
    if (body_start != NULL) {
        body_start += 4;
        printf("Received POST data:\n%s\n", body_start);
        return create_http_response(response_dst, response_dst_size, body_start, "text/plain");
    } else {
        snprintf(response_dst, response_dst_size, "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
        return response_dst;
    }
}

char* handle_request(char* response_dst, size_t response_dst_size, const char* request) {
    char method[10];
    char path[BUFFER_SIZE];

    if (sscanf(request, "%s %s", method, path) >= 1) {
        if (strcmp(method, "GET") == 0) {
            return handle_get_request(response_dst, response_dst_size, path);
        } else if (strcmp(method, "POST") == 0) {
            return handle_post_request(response_dst, response_dst_size, request);
        } else {
            snprintf(response_dst, response_dst_size, "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
            return response_dst;
        }
    } else {
        snprintf(response_dst, response_dst_size, "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
        return response_dst;
    }
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    static char request_buffer[BUFFER_SIZE];
    static char response_buffer[BUFFER_SIZE];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == -1) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) == -1) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Listening on port %d...\n", PORT);

    while (1) {
        new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        ssize_t bytes_received = recv(new_socket, request_buffer, sizeof(request_buffer) - 1, 0);
        if (bytes_received > 0) {
            request_buffer[bytes_received] = '\0';
            printf("Received request:\n%s\n", request_buffer);
            char* response = handle_request(response_buffer, sizeof(response_buffer), request_buffer);
            if (response != NULL) {
                send(new_socket, response, strlen(response), 0);
            }
        }
        close(new_socket);
    }

    close(server_fd);
    return 0;
}