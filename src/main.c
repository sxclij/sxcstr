#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE (1024 * 1024 * 16)
#define SAVEDATA_CAPACITY (1024 * 1024 * 128)

struct string {
    char* data;
    uint32_t size;
};

struct global {
    char buf_recv_data[BUFFER_SIZE];
    char buf_send_data[BUFFER_SIZE];
    char buf_html_data[BUFFER_SIZE];
    char buf_path_data[BUFFER_SIZE];
    struct string buf_recv;
    struct string buf_send;
    struct string buf_html;
    struct string buf_path;
    int server_fd;
    int current_socket;
    struct sockaddr_in address;
    int addrlen;
};

void string_cat(struct string* dst, struct string src) {
    memcpy(dst->data + dst->size, src.data, src.size);
    dst->size += src.size;
}
void string_cat_str(struct string* dst, char* src) {
    string_cat(dst, (struct string){.data = src, .size = strlen(src)});
}

void file_read(struct string* dst, const char* path) {
    FILE* fp = fopen(path, "r");
    if (!fp) {
        printf("Error opening file. %s\n", path);
        dst->size = 0;
        return;
    }
    fseek(fp, 0, SEEK_END);
    dst->size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    fread(dst->data, 1, dst->size, fp);
    fclose(fp);
}

void http_response_finalize(struct string* buf_send, struct string* body, const char* content_type) {
    buf_send->size = sprintf(buf_send->data, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %u\r\n\r\n", content_type, body->size);
    memcpy(buf_send->data+buf_send->size,body->data,body->size);
    buf_send->size += body->size;
}

void http_handle_get(struct string* buf_send, struct string* buf_html, struct string* buf_path, struct string* buf_recv) {
    char* path_start = strstr(buf_recv->data, "/");
    char* path_end = strstr(path_start, " ");
    int path_size = path_end - path_start;
    if (path_start[0] == '/' && path_size == 1) {
        file_read(buf_html, "./routes/index.html");
    } else {
        buf_path->size = 0;
        string_cat_str(buf_path, "./routes");
        string_cat(buf_path, (struct string){.data = path_start, .size = path_size});
        string_cat_str(buf_path, ".html");
        buf_path->data[buf_path->size] = '\0';
        file_read(buf_html, buf_path->data);
    }
    http_response_finalize(buf_send, buf_html, "text/html");
}

void http_handle_post(struct string* buf_send, struct string* buf_recv) {
    // Echo received data for POST request
    struct string body = { .data = buf_recv->data, .size = buf_recv->size };
    http_response_finalize(buf_send, &body, "text/plain");
}

void http_handle_request(struct string* buf_send, struct string* buf_recv, struct string* buf_html, struct string* buf_path) {
    if (strncmp(buf_recv->data, "GET", 3) == 0) {
        http_handle_get(buf_send, buf_html, buf_path, buf_recv);
    } else if (strncmp(buf_recv->data, "POST", 4) == 0) {
        http_handle_post(buf_send, buf_recv);
    } else {
        const char* body_text = "<html><body><h1>Method Not Allowed</h1></body></html>";
        struct string body = { .data = (char*)body_text, .size = strlen(body_text) };
        http_response_finalize(buf_send, &body, "text/html");
    }
}

void global_init(struct global* global) {
    memset(global, 0, sizeof(struct global));
    global->buf_recv.data = global->buf_recv_data;
    global->buf_send.data = global->buf_send_data;
    global->buf_html.data = global->buf_html_data;
    global->buf_path.data = global->buf_path_data;
    global->buf_recv.size = 0;
    global->buf_send.size = 0;
    global->buf_html.size = 0;
    global->buf_path.size = 0;

    file_read(&global->buf_html, "index.html");

    global->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (global->server_fd == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(global->server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    global->address.sin_family = AF_INET;
    global->address.sin_addr.s_addr = INADDR_ANY;
    global->address.sin_port = htons(PORT);
    global->addrlen = sizeof(global->address);

    if (bind(global->server_fd, (struct sockaddr*)&global->address, sizeof(global->address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(global->server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
}

int main() {
    static struct global global;
    global_init(&global);
    printf("Server listening on port %d\n", PORT);
    while (1) {
        global.current_socket = accept(global.server_fd, (struct sockaddr*)&global.address, &global.addrlen);
        if (global.current_socket < 0) {
            perror("Accept failed");
            continue;
        }
        int bytes_read = read(global.current_socket, global.buf_recv_data, BUFFER_SIZE);
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                printf("Client disconnected.\n");
            } else {
                perror("Read failed");
            }
            close(global.current_socket);
            continue;
        }
        global.buf_recv.size = bytes_read;
        http_handle_request(&global.buf_send, &global.buf_recv, &global.buf_html, &global.buf_path);
        send(global.current_socket, global.buf_send.data, global.buf_send.size, 0);
        close(global.current_socket);
    }

    return 0;
}