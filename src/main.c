#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE (1024 * 1024 * 16)
#define SAVEDATA_CAPACITY (1024 * 1024 * 128)

enum result_type {
    result_type_ok,
    result_type_err,
};
struct string {
    char* data;
    uint32_t size;
};

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

struct string string_make(char* data, uint32_t size) {
    return (struct string) {data, .size = size};
}
void string_clear(struct string* dst) {
    dst->size = 0;
}
void string_cpy(struct string* dst, struct string src) {
    memcpy(dst->data, src.data, src.size);
    dst->size = src.size;
}
void string_cpy_str(struct string* dst, char* src) {
    string_cpy(dst, string_make (src, strlen(src)));
}
void string_cat(struct string* dst, struct string src) {
    memcpy(dst->data + dst->size, src.data, src.size);
    dst->size += src.size;
}
void string_cat_str(struct string* dst, char* src) {
    string_cat(dst, string_make (src, strlen(src)));
}
void string_tostr(struct string* dst) {
    dst->data[dst->size] = '\0';
}
int string_cmp(struct string s1, struct string s2) {
    if(s1.size == s2.size) {
        return memcmp(s1.data, s2.data, s1.size);
    }else {
        return s1.size - s2.size;
    }
}
int string_cmp_str(struct string s1, char* s2) {
    return string_cmp(s1, string_make (s2, strlen(s2)));
}

enum result_type file_read(struct string* dst, const char* path) {
    FILE* fp = fopen(path, "r");
    if (!fp) {
        printf("Error opening file. %s\n", path);
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

const char* http_contenttype(const char* path) {
    const char* ext = strrchr(path, '.');
    if (!ext) {
        return "application/octet-stream";
    }
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) {
        return "text/html";
    } else if (strcmp(ext, ".txt") == 0) {
        return "text/plain";
    } else if (strcmp(ext, ".xml") == 0) {
        return "application/xml";
    } else if (strcmp(ext, ".css") == 0) {
        return "text/css";
    } else if (strcmp(ext, ".js") == 0) {
        return "application/javascript";
    } else if (strcmp(ext, ".png") == 0) {
        return "image/png";
    } else if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) {
        return "image/jpeg";
    } else if (strcmp(ext, ".ico") == 0) {
        return "image/x-icon";
    } else if (strcmp(ext, ".svg") == 0) {
        return "image/svg+xml";
    } else if (strcmp(ext, ".json") == 0) {
        return "application/json";
    } else if (strcmp(ext, ".woff") == 0) {
        return "font/woff";
    } else if (strcmp(ext, ".woff2") == 0) {
        return "font/woff2";
    } else if (strcmp(ext, ".ttf") == 0) {
        return "font/ttf";
    } else if (strcmp(ext, ".otf") == 0) {
        return "font/otf";
    } else if (strcmp(ext, ".mp4") == 0) {
        return "video/mp4";
    } else if (strcmp(ext, ".webm") == 0) {
        return "video/webm";
    } else {
        return "application/octet-stream";
    }
}
void http_response_finalize(struct string* buf_send, struct string* body, const char* content_type) {
    buf_send->size = sprintf(buf_send->data, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %u\r\n\r\n", content_type, body->size);
    memcpy(buf_send->data+buf_send->size,body->data,body->size);
    buf_send->size += body->size;
}
void http_handle_get_readfile(struct string* buf_file, struct string* buf_path, struct string path_string, const char** contenttype) {
    string_cpy_str(buf_path, "./routes");
    string_cat(buf_path, path_string);
    string_tostr(buf_path);
    *contenttype = http_contenttype(buf_path->data);
    if(file_read(buf_file, buf_path->data) == result_type_ok) {
        return;
    }
    string_cat_str(buf_path, ".html");
    string_tostr(buf_path);
    *contenttype = http_contenttype(buf_path->data);
    if(file_read(buf_file, buf_path->data) == result_type_ok) {
        return;
    }
}
void http_handle_get(struct string* buf_file, struct string* buf_path, const char** contenttype, struct string buf_recv) {
    char* path_start = strstr(buf_recv.data, "/");
    char* path_end = strstr(path_start, " ");
    int path_size = path_end - path_start;
    struct string path_string = string_make(path_start, path_size);
    if (string_cmp_str(path_string, "/") == 0) {
        string_cpy_str(buf_path, "/index");
        http_handle_get_readfile(buf_file, buf_path, path_string, contenttype);
    } else if(string_cmp_str(path_string, "/favicon.ico") == 0) {
        string_cpy_str(buf_path, "/favicon.svg");
        http_handle_get_readfile(buf_file, buf_path, path_string, contenttype);
    } else {
        http_handle_get_readfile(buf_file, buf_path, path_string, contenttype);
    }
}

void http_handle_request(struct string* buf_send, struct string buf_recv, struct string* buf_file, struct string* buf_path) {
    const char* contenttype;
    if (strncmp(buf_recv.data, "GET", 3) == 0) {
        http_handle_get(buf_file, buf_path, &contenttype, buf_recv);
        http_response_finalize(buf_send, buf_file, contenttype);
    }else {
        const char* body_text = "<html><body><h1>Method Not Allowed</h1></body></html>";
        struct string body = { .data = (char*)body_text, .size = strlen(body_text) };
        contenttype = "text/html";
        http_response_finalize(buf_send, &body, contenttype);
    }
}
enum result_type http_read(struct string* buf_recv, int http_client) {
    int bytes_read = read(http_client, buf_recv->data, BUFFER_SIZE);
    if (bytes_read <= 0) {
        if (bytes_read == 0) {
            printf("Client disconnected.\n");
        } else {
            perror("Read failed");
        }
        return result_type_err;
    }
    buf_recv->size = bytes_read;
    return result_type_ok;
}

void global_loop(struct global* global) {
    while (1) {
        global->http_client = accept(global->http_server, (struct sockaddr*)&global->http_address, &global->http_addrlen);
        if (global->http_client < 0) {
            perror("Accept failed");
            continue;
        }
        if (http_read(&global->buf_recv, global->http_client) == result_type_ok) {
            http_handle_request(&global->buf_send, global->buf_recv, &global->buf_file, &global->buf_path);
            send(global->http_client, global->buf_send.data, global->buf_send.size, 0);
        }
        close(global->http_client);
    }
}
enum result_type global_init(struct global* global) {
    memset(global, 0, sizeof(struct global));
    global->buf_recv = string_make( global->buf_recv_data, 0);
    global->buf_send = string_make( global->buf_send_data, 0);
    global->buf_file = string_make( global->buf_file_data, 0);
    global->buf_path = string_make( global->buf_path_data, 0);

    global->http_server = socket(AF_INET, SOCK_STREAM, 0);
    if (global->http_server == 0) {
        perror("Socket creation failed");
        return result_type_err;
    }

    int opt = 1;
    if (setsockopt(global->http_server, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        return result_type_err;
    }

    global->http_address.sin_family = AF_INET;
    global->http_address.sin_addr.s_addr = INADDR_ANY;
    global->http_address.sin_port = htons(PORT);
    global->http_addrlen = sizeof(global->http_address);

    if (bind(global->http_server, (struct sockaddr*)&global->http_address, sizeof(global->http_address)) < 0) {
        perror("Bind failed");
        return result_type_err;
    }

    if (listen(global->http_server, 3) < 0) {
        perror("Listen failed");
        return result_type_err;
    }
    return result_type_ok;
}

int main() {
    static struct global global;
    if(global_init(&global) == result_type_ok) {
        printf("Server listening on port %d\n", PORT);
    } else {
        return 0;
    }
    global_loop(&global);
    printf("Server stopped.");
    return 0;
}