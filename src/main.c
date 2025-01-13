#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/resource.h>

#define PORT 8080
#define BUFFER_SIZE (1024 * 1024 * 16)
#define RLIMIT_SIZE (1024 * 1024 * 128)

enum result {
    result_ok,
    result_err,
};
struct string {
    const char* data;
    const uint32_t size;
};
struct vec {
    char* data;
    uint32_t size;
};

void vec_cpy(struct vec* dst, struct string src) {
    memcpy(dst->data, src.data, src.size);
    dst->size = src.size;    
}
void vec_cat(struct vec* dst, struct string src) {
    memcpy(dst->data + dst->size, src.data, src.size);
    dst->size += src.size;    
}
void vec_tostr(struct vec* dst) {
    dst->data[dst->size] = '\0';
}
void vec_cpy_str(struct vec* dst, const char* src) {
    vec_cpy(dst, (struct string){.data = src, .size = strlen(src)});
}
void vec_cat_str(struct vec* dst, const char* src) {
    vec_cat(dst, (struct string){.data = src, .size = strlen(src)});
}


enum result file_read(struct vec* dst, struct vec* path) {
    vec_tostr(path);
    FILE* fp = fopen(path->data, "r");
    if (!fp) {
        dst->size = 0;
        return result_err;
    }
    fseek(fp, 0, SEEK_END);
    dst->size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    fread(dst->data, 1, dst->size, fp);
    fclose(fp);
    return result_ok;
}
enum result handle(int client_socket) {
    char send_data[BUFFER_SIZE];
    char recv_data[BUFFER_SIZE];
    char file_data[BUFFER_SIZE];
    char path_data[BUFFER_SIZE];
    struct vec send_vec = (struct vec){.data = send_data, .size = 0};
    struct vec recv_vec = (struct vec){.data = recv_data, .size = 0};
    struct vec file_vec = (struct vec){.data = file_data, .size = 0};
    struct vec path_vec = (struct vec){.data = path_data, .size = 0};
    const char* content_type = "text/html";
    int bytes_received = recv(client_socket, recv_vec.data, BUFFER_SIZE, 0);
    if (bytes_received == -1) {
        printf("recv\n");
        return result_err;
    }
    recv_vec.size = bytes_received;
    if(memcmp(recv_vec.data, "GET ", 4) == 0) {
        char* path_start = strstr(recv_vec.data, "/");
        char* path_end = strstr(path_start, " ");
        char* path_ext;
        uint32_t path_size = path_end - path_start;
        vec_cpy_str(&path_vec, "./routes");
        if(memcmp(path_start, "/", 1) == 0 && path_size == 1) {
            vec_cat_str(&path_vec, "/index.html");
        } else if(memcmp(path_start, "/favicon.ico", 12) == 0 && path_size == 12) {
            vec_cat_str(&path_vec, "/favicon.svg");
        } else {
            vec_cat(&path_vec, (struct string){.data = path_start, .size = path_size});
        }
        vec_tostr(&path_vec);
        char* ext = strrchr(path_vec.data, '.');
        if(ext[1] == '/') {
            vec_cat_str(&path_vec, ".html");
        }
        if(file_read(&file_vec, &path_vec) == result_err) {
            printf("file_read %s\n", path_vec.data);
            return result_ok;
        }
        send_vec.size = sprintf(send_vec.data, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %u\r\n\r\n", content_type, file_vec.size);
        memcpy(send_vec.data + send_vec.size, file_vec.data, file_vec.size);
        send_vec.size += file_vec.size;
        send(client_socket, send_vec.data, send_vec.size, 0);
    }
    return result_ok;
}
enum result loop(int server_socket, struct sockaddr_in* address) {
    int address_length = sizeof(address);
    int client_socket;
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)address, &address_length);
        if (client_socket < 0) {
            printf("accept\n");
            continue;
        }
        if(handle(client_socket) == result_err) {
            printf("handle\n");
            close(client_socket);
            return result_err;
        }
        close(client_socket);
    }
}
enum result deinit(int server_socket) {
    close(server_socket);
    return result_ok;
}
enum result init(int* server_socket, struct sockaddr_in* address) {
    const rlim_t kstacksize = RLIMIT_SIZE;
    struct rlimit rl;
    int result;
    int option = 1;
    result = getrlimit(RLIMIT_STACK, &rl);
    if (result != 0) {
        printf("getrlimit\n");
        return result_err;
    }
    if (rl.rlim_cur >= kstacksize) {
        return result_ok;
    }
    rl.rlim_cur = kstacksize;
    result = setrlimit(RLIMIT_STACK, &rl);
    if (result != 0) {
        printf("setrlimit\n");
        return result_err;
    }
    *server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (*server_socket == 0) {
        printf("socket\n");
        return result_err;
    }
    if (setsockopt(*server_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &option, sizeof(option))) {
        printf("setsockopt\n");
        return result_err;
    }
    address->sin_family = AF_INET;
    address->sin_addr.s_addr = INADDR_ANY;
    address->sin_port = htons(PORT);
    if (bind(*server_socket, (struct sockaddr*)address, sizeof(*address)) < 0) {
        printf("bind\n");
        return result_err;
    }
    if (listen(*server_socket, SOMAXCONN) < 0) {
        printf("listen\n");
        return result_err;
    }
    return result_ok;
}

int main() {
    int server_socket;
    struct sockaddr_in address;
    if(init(&server_socket, &address) == result_err) {
        printf("init\n");
        return 0;
    }
    if(loop(server_socket, &address) == result_err) {
        printf("loop\n");
    }
    if(deinit(server_socket) == result_err) {
        printf("deinit\n");
    }
    return 0;
}