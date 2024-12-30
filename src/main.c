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

static char response_buffer[BUFFER_SIZE * 2];
static char file_buffer[BUFFER_SIZE];

char* create_http_response(const char* body, const char* content_type) {
    snprintf(response_buffer, sizeof(response_buffer),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %ld\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             content_type, strlen(body), body);
    return response_buffer;
}

char* read_file(const char* filename) {
    FILE* fp = fopen(filename, "r");

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    fread(file_buffer, 1, file_size, fp);
    file_buffer[file_size] = '\0';
    fclose(fp);
    return file_buffer;
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    static char request_buffer[BUFFER_SIZE];

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

        char method[10];
        char path[BUFFER_SIZE];
        sscanf(request_buffer, "%s %s", method, path);

        char filename[BUFFER_SIZE];
        if (strcmp(path, "/") == 0) {
            strcpy(filename, "index.html");
        } else {
            strncpy(filename, path + 1, sizeof(filename) - 1);
            filename[sizeof(filename) - 1] = '\0';
        }

        char* file_content = read_file(filename);
        char* content_type = "text/html";
        char* response = create_http_response(file_content, content_type);
        send(new_socket, response, strlen(response), 0);

        close(new_socket);
    }

    close(server_fd);
    return 0;
}