#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE (1024 * 1024 * 16)

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

enum result http(struct global* global) {
    
    char recv_data[BUFFER_SIZE];
    char send_data[BUFFER_SIZE];
    struct vec recv_vec = (struct vec){.data = recv_data, .size = 0};
    struct vec send_vec = (struct vec){.data = send_data, .size = 0};

    int server_socket;
    int client_socket;
    struct sockaddr_in server_address;
    int server_address_length;
    int option = 1;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == 0) {
        printf("socket\n");
        return result_err;
    }

    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &option, sizeof(option))) {
        printf("setsockopt\n");
        return result_err;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);
    server_address_length = sizeof(server_address);

    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        printf("bind\n");
        return result_err;
    }
    if (listen(server_socket, 3) < 0) {
        printf("listen\n");
        return result_err;
    }

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)&server_address, &server_address_length);
        if (client_socket < 0) {
            continue;
        }
        if(http_handle(&send_vec, &recv_vec, client_socket) == result_err) {
            printf("http_handle\n");
            return result_err;
        }
        close(client_socket);
    }
}
enum result init() {

    const rlim_t kstacksize = RLIMIT_SIZE;
    struct rlimit rl;
    int result;

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
    return result_ok;
}

int main() {
    if(init() == result_err) {
        printf("init\n");
        return 0;
    }
    if(http() == result_err) {
        printf("http\n");
        return 0;
    }
    return 0;
}