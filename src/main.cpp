#include <bits/stdc++.h>
using namespace std;
 
const int PORT = 8080;
const int BUFFER_SIZE = 65536;
const char *FILE_DEFAULT = "index.html";
const char *HTTP_OK = "HTTP/1.0 200 OK\r\n";
const char *HTTP_NOT_FOUND = "HTTP/1.0 404 Not Found\r\n";
const char *CONTENT_TYPE_HTML = "Content-Type: text/html\r\n\r\n";
const char *CONTENT_TYPE_PLAIN = "Content-Type: text/plain\r\n\r\n";
const char *HTML_PATH = "routes";
const char *HTML_INDEX = "index";

void handle_connection(int client_socket) {
    char *buffer = (char *)malloc(BUFFER_SIZE);
    ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
        free(buffer);
        close(client_socket);
        return;
    }
    buffer[bytes_received] = '\0';
    free(buffer);
    close(client_socket);
}

int server_init() {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, BACKLOG) == -1) {
        perror("listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);
    return server_socket;
}

int main() {
    int server_socket = server_init();
    while (1) {
        struct sockaddr_in client_address;
        socklen_t client_len = sizeof(client_address);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_len);
        if (client_socket == -1) {
            perror("accept failed");
            continue;
        }
        handle_connection(client_socket);
    }
    close(server_socket); // unreachable, but for completeness
    return 0;
}