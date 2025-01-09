#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE (1024 * 1024 * 16)
#define SAVEDATA_CAPACITY (1024 * 1024 * 128)

// Enum for result types
enum result_type {
    result_type_ok,
    result_type_err,
};

// Struct for string management
struct string {
    char* data;
    uint32_t size;
};

// Global struct containing all necessary buffers and configurations
struct global {
    char buf_recv_data[BUFFER_SIZE];
    char buf_send_data[BUFFER_SIZE];
    char buf_file_data[BUFFER_SIZE];
    char buf_path_data[BUFFER_SIZE];
    struct string buf_recv;
    struct string buf_send;
    struct string buf_file;
    struct string buf_path;
    int http_server;
    int http_client;
    struct sockaddr_in http_address;
    int http_addrlen;
};

// String utilities
const struct string string_make(char* const data, const uint32_t size) {
    return (const struct string){.data = data, .size = size};
}

const struct string string_make_str(char* const data) {
    return string_make(data, strlen(data));
}

void string_clear(struct string* const dst) {
    dst->size = 0;
}

void string_cpy(struct string* const dst, const struct string src) {
    memcpy(dst->data, src.data, src.size);
    dst->size = src.size;
}

void string_cpy_str(struct string* const dst, const char* const src) {
    string_cpy(dst, string_make((char*)src, strlen(src)));
}

void string_cat(struct string* const dst, const struct string src) {
    memcpy(dst->data + dst->size, src.data, src.size);
    dst->size += src.size;
}

void string_cat_str(struct string* const dst, const char* const src) {
    string_cat(dst, string_make((char*)src, strlen(src)));
}

void string_tostr(struct string* const dst) {
    dst->data[dst->size] = '\0';
}

int string_cmp(const struct string s1, const struct string s2) {
    if (s1.size == s2.size) {
        return memcmp(s1.data, s2.data, s1.size);
    } else {
        return s1.size - s2.size;
    }
}

int string_cmp_str(const struct string s1, const char* const s2) {
    return string_cmp(s1, string_make((char*)s2, strlen(s2)));
}

// File read utility
enum result_type file_read(struct string* const dst, const char* const path) {
    FILE* const fp = fopen(path, "r");
    if (!fp) {
        printf("Error opening file: %s\n", path);
        dst->size = 0;
        return result_type_err;
    }
    fseek(fp, 0, SEEK_END);
    dst->size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    fread(dst->data, 1, dst->size, fp);
    fclose(fp);
    return result_type_ok;
}

// Determine HTTP content type
const char* http_contenttype(const char* const path) {
    const char* const ext = strrchr(path, '.');
    if (!ext) {
        return "application/octet-stream";
    }
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
    if (strcmp(ext, ".txt") == 0) return "text/plain";
    if (strcmp(ext, ".xml") == 0) return "application/xml";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".ico") == 0) return "image/x-icon";
    if (strcmp(ext, ".svg") == 0) return "image/svg+xml";
    if (strcmp(ext, ".json") == 0) return "application/json";
    if (strcmp(ext, ".woff") == 0) return "font/woff";
    if (strcmp(ext, ".woff2") == 0) return "font/woff2";
    if (strcmp(ext, ".ttf") == 0) return "font/ttf";
    if (strcmp(ext, ".otf") == 0) return "font/otf";
    if (strcmp(ext, ".mp4") == 0) return "video/mp4";
    if (strcmp(ext, ".webm") == 0) return "video/webm";
    return "application/octet-stream";
}

// HTTP response finalization
void http_response_finalize(struct string* const buf_send, const struct string body, const char* const content_type) {
    buf_send->size = sprintf(buf_send->data, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %u\r\n\r\n", content_type, body.size);
    memcpy(buf_send->data + buf_send->size, body.data, body.size);
    buf_send->size += body.size;
}

// HTTP GET request handler
void http_handle_get(struct string* const buf_file, struct string* const buf_path, const char** const contenttype, const struct string buf_recv) {
    const char* const path_start = strstr(buf_recv.data, "/");
    const char* const path_end = strstr(path_start, " ");
    const int path_size = path_end - path_start;
    const struct string path_string = string_make((char*)path_start, path_size);

    if (string_cmp_str(path_string, "/") == 0) {
        string_cpy_str(buf_path, "./routes/index");
    } else if (string_cmp_str(path_string, "/favicon.ico") == 0) {
        string_cpy_str(buf_path, "./routes/favicon.svg");
    } else {
        string_cpy_str(buf_path, "./routes");
        string_cat(buf_path, path_string);
    }

    string_tostr(buf_path);
    if (file_read(buf_file, buf_path->data) == result_type_ok) {
        *contenttype = http_contenttype(buf_path->data);
        return;
    }
    
    string_cat_str(buf_path, ".html");
    string_tostr(buf_path);
    if (file_read(buf_file, buf_path->data) == result_type_ok) {
        *contenttype = http_contenttype(buf_path->data);
    } else {
        *contenttype = "text/html";
    }
}

// HTTP request handler
void http_handle_request(struct string* const buf_send, const struct string buf_recv, struct string* const buf_file, struct string* const buf_path) {
    const char* contenttype = "text/html";

    if (strncmp(buf_recv.data, "GET", 3) == 0) {
        http_handle_get(buf_file, buf_path, &contenttype, buf_recv);
    } else {
        string_clear(buf_file);
    }

    http_response_finalize(buf_send, *buf_file, contenttype);
}

// Read HTTP request
enum result_type http_read(struct string* const buf_recv, const int http_client) {
    const int bytes_read = read(http_client, buf_recv->data, BUFFER_SIZE);
    if (bytes_read <= 0) {
        return result_type_err;
    }
    buf_recv->size = bytes_read;
    return result_type_ok;
}

// Main server loop
void global_loop(struct global* const global) {
    while (1) {
        global->http_client = accept(global->http_server, (struct sockaddr*)&global->http_address, &global->http_addrlen);
        if (global->http_client < 0) continue;

        if (http_read(&global->buf_recv, global->http_client) == result_type_ok) {
            http_handle_request(&global->buf_send, global->buf_recv, &global->buf_file, &global->buf_path);
            send(global->http_client, global->buf_send.data, global->buf_send.size, 0);
        }
        close(global->http_client);
    }
}

// Global initialization
enum result_type global_init(struct global* const global) {
    memset(global, 0, sizeof(struct global));
    global->buf_recv = string_make(global->buf_recv_data, 0);
    global->buf_send = string_make(global->buf_send_data, 0);
    global->buf_file = string_make(global->buf_file_data, 0);
    global->buf_path = string_make(global->buf_path_data, 0);

    global->http_server = socket(AF_INET, SOCK_STREAM, 0);
    if (global->http_server == 0) return result_type_err;

    const int opt = 1;
    if (setsockopt(global->http_server, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) return result_type_err;

    global->http_address.sin_family = AF_INET;
    global->http_address.sin_addr.s_addr = INADDR_ANY;
    global->http_address.sin_port = htons(PORT);
    global->http_addrlen = sizeof(global->http_address);

    if (bind(global->http_server, (struct sockaddr*)&global->http_address, sizeof(global->http_address)) < 0) return result_type_err;
    if (listen(global->http_server, 3) < 0) return result_type_err;

    return result_type_ok;
}

int main() {
    static struct global global;

    if (global_init(&global) == result_type_ok) {
        printf("Server listening on port %d\n", PORT);
        global_loop(&global);
    }
    return 0;
}