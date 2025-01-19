#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/resource.h>

#define PORT 8080
#define BUFFER_SIZE (1024 * 16 * 1)
#define RLIMIT_SIZE (1024 * 1024 * 128)

enum result {
    result_ok,
    result_err,
};
struct string {
    const char* data;
    uint32_t size;
};
struct vec {
    char* data;
    uint32_t size;
};
struct json {
    struct string string;
    uint64_t hash;
    uint64_t random;
    struct json* val;
    struct json* lhs;
    struct json* rhs;
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
uint64_t string_hash(struct string src) {
    uint64_t x = 0;
    for(int i=0;i<src.size;i++) {
        x <<= 8;
        x |= src.data[i];
    }
    return x;
}
int json_isspace(char ch) {
    return (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r');
}
int json_issign(char ch) {
    return (ch == '{' || ch == '}' || ch == '[' || ch == ']' || ch == ':' || ch == ',');
}
uint64_t json_random(uint64_t x) {
	x ^= x << 13;
	x ^= x >> 7;
	x ^= x << 17;
    return x;
}
struct json* json_newnode(struct json** dst_end, uint64_t* random, struct string string) {
    struct json* x = *dst_end;
    *random = json_random(*random);
    *x = (struct json){
        .string = string,
        .hash = string_hash(string),
        .random = *random, 
        .val = NULL, 
        .lhs = NULL, 
        .rhs = NULL
    };
    *dst_end += 1;
    return x;
}
struct json* json_treap_rightrotate(struct json* y) {
    struct json* x = y->lhs;
    struct json* T2 = x->rhs;
    x->rhs = y;
    y->lhs = T2;
    return x;
}
struct json* json_treap_leftrotate(struct json* x) {
    struct json* y = x->rhs;
    struct json* T2 = y->lhs;
    y->lhs = x;
    x->rhs = T2;
    return y;
}
struct json* json_treap_insert(struct json* root, struct json* x) {
    if (root == NULL) {
        return x;
    }
    if (x->random > root->random) {
        if (x->hash < root->hash) {
            root->lhs = json_treap_insert(root->lhs, x);
            return json_treap_rightrotate(root);
        } else {
            root->rhs = json_treap_insert(root->rhs, x);
            return json_treap_leftrotate(root);
        }
    } else {
        if (x->hash < root->hash) {
            root->lhs = json_treap_insert(root->lhs, x);
        } else {
            root->rhs = json_treap_insert(root->rhs, x);
        }
        return root;
    }
}
void json_tokenize(struct string* dst, struct string src) {
    struct string* dst_end = dst;
    const char* current = src.data;
    const char* end = src.data + src.size;

    while (current < end) {
        if (json_isspace(*current)) {
            current++;
            continue;
        }
        if (*current == '"') {
            const char* start = current + 1;
            const char* end_quote = NULL;
            current++;
            while (current < end) {
                if (*current == '"') {
                    end_quote = current;
                    break;
                }
                if (*current == '\\' && current + 1 < end) {
                    current += 2;
                } else {
                    current++;
                }
            }
            if (end_quote != NULL) {
                *dst_end = (struct string){.data = start, .size = end_quote - start};
                dst_end++;
                current++;
                continue;
            } else {
                fprintf(stderr, "Error: Unterminated string literal.\n");
                return;
            }
        }
        if (json_issign(*current)) {
            *dst_end = (struct string){.data = current, .size = 1};
            dst_end++;
            current++;
            continue;
        }
        const char* token_start = current;
        while (current < end && !json_isspace(*current) && !json_issign(*current)) {
            current++;
        }
        if (current > token_start) {
            *dst_end = (struct string){.data = token_start, .size = current - token_start};
            dst_end++;
        }
    }
    *dst_end = (struct string){.data = NULL, .size = 0};
}

struct json* json_parse(struct json* dst, struct string src) {
    struct string token[BUFFER_SIZE];
    struct json* stack_json[BUFFER_SIZE];
    int stack_iskey[BUFFER_SIZE];
    struct json* dst_end = dst;
    struct string* token_itr = token;
    struct json* root = NULL;
    int nest = 0;
    uint64_t random = 1;

    json_tokenize(token, src); // Tokenize the input JSON string.
    token_itr = token;

    stack_json[0] = NULL;
    stack_iskey[0] = 1;

    while (token_itr->data != NULL) {
        if ((token_itr->size == 1 && token_itr->data[0] == '{') || (token_itr->size == 1 && token_itr->data[0] == '[')) {
            nest++;
        } else if ((token_itr->size == 1 && token_itr->data[0] == '}') || (token_itr->size == 1 && token_itr->data[0] == ']')) {
            nest--;
        } else if (token_itr->size == 1 && token_itr->data[0] == ':') {

        } else if (token_itr->size == 1 && token_itr->data[0] == ',') {

        } else {
            struct json* newnode = json_newnode(&dst_end, &random, *token_itr);
        }
        token_itr++;
    }

    return root;
}

struct json* json_get(struct json* root, struct string* path) {
}

void json_test() {
    struct json example_json[BUFFER_SIZE];
    const char *example_str = "{\"name\": \"John Doe\", \"age\": 30, \"city\": \"Tokyo\", \"items\": [\"apple\", \"orange\"]}";
    struct json* example_root = json_parse(example_json, (struct string){.data=example_str, .size = strlen(example_str)});
    return;
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
enum result handle_get(struct vec* send_vec, struct vec* recv_vec) {
    char file_data[BUFFER_SIZE];
    char path_data[BUFFER_SIZE];
    struct vec file_vec = (struct vec){.data = file_data, .size = 0};
    struct vec path_vec = (struct vec){.data = path_data, .size = 0};
    const char* content_type;
    char* path_start = strstr(recv_vec->data, "/");
    char* path_end = strstr(path_start, " ");
    char* path_ext;
    uint32_t path_size = path_end - path_start;
    if(memcmp(path_start, "/", 1) == 0 && path_size == 1) {
        vec_cpy_str(&path_vec, "./routes/index.html");
    } else if(memcmp(path_start, "/favicon.ico", 12) == 0 && path_size == 12) {
        vec_cpy_str(&path_vec, "./routes/favicon.svg");
    } else {
        vec_cpy_str(&path_vec, "./routes");
        vec_cat(&path_vec, (struct string){.data = path_start, .size = path_size});
    }
    vec_tostr(&path_vec);
    path_ext = strrchr(path_vec.data, '.');
    if(path_ext[1] == '/') {
        vec_cat_str(&path_vec, ".html");
    }
    vec_tostr(&path_vec);
    path_ext = strrchr(path_vec.data, '.');
    if (!path_ext) {
        content_type = "application/octet-stream";
    } else if (strcmp(path_ext, ".html") == 0 || strcmp(path_ext, ".htm") == 0) {
        content_type = "text/html";
    } else if (strcmp(path_ext, ".txt") == 0) {
        content_type = "text/plain";
    } else if (strcmp(path_ext, ".xml") == 0) {
        content_type = "application/xml";
    } else if (strcmp(path_ext, ".css") == 0) {
        content_type = "text/css";
    } else if (strcmp(path_ext, ".js") == 0) {
        content_type = "application/javascript";
    } else if (strcmp(path_ext, ".png") == 0) {
        content_type = "image/png";
    } else if (strcmp(path_ext, ".jpg") == 0 || strcmp(path_ext, ".jpeg") == 0) {
        content_type = "image/jpeg";
    } else if (strcmp(path_ext, ".ico") == 0) {
        content_type = "image/x-icon";
    } else if (strcmp(path_ext, ".svg") == 0) {
        content_type = "image/svg+xml";
    } else if (strcmp(path_ext, ".webp") == 0) {
        content_type = "image/webp";
    } else if (strcmp(path_ext, ".json") == 0) {
        content_type = "application/json";
    } else if (strcmp(path_ext, ".woff") == 0) {
        content_type = "font/woff";
    } else if (strcmp(path_ext, ".woff2") == 0) {
        content_type = "font/woff2";
    } else if (strcmp(path_ext, ".ttf") == 0) {
        content_type = "font/ttf";
    } else if (strcmp(path_ext, ".otf") == 0) {
        content_type = "font/otf";
    } else if (strcmp(path_ext, ".mp4") == 0) {
        content_type = "video/mp4";
    } else if (strcmp(path_ext, ".webm") == 0) {
        content_type = "video/webm";
    } else if (strcmp(path_ext, ".mov") == 0) {
        content_type = "video/quicktime";
    } else {
        content_type = "application/octet-stream";
    }
    if(file_read(&file_vec, &path_vec) == result_err) {
        printf("file_read %s\n", path_vec.data);
        return result_err;
    }
    send_vec->size = sprintf(send_vec->data, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %u\r\n\r\n", content_type, file_vec.size);
    memcpy(send_vec->data + send_vec->size, file_vec.data, file_vec.size);
    send_vec->size += file_vec.size;
}
enum result handle(int client_socket) {
    char send_data[BUFFER_SIZE];
    char recv_data[BUFFER_SIZE];
    struct vec send_vec = (struct vec){.data = send_data, .size = 0};
    struct vec recv_vec = (struct vec){.data = recv_data, .size = 0};
    int bytes_received = recv(client_socket, recv_vec.data, BUFFER_SIZE, 0);
    if (bytes_received == -1) {
        printf("recv\n");
        return result_ok;
    }
    recv_vec.size = bytes_received;
    if(memcmp(recv_vec.data, "GET ", 4) == 0) {
        if(handle_get(&send_vec, &recv_vec) == result_err) {
            printf("handle_get\n\n");
            return result_ok;
        }
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
    rl.rlim_max = kstacksize;
    result = setrlimit(RLIMIT_STACK, &rl);
    if (result != 0) {
        printf("setrlimit\n");
        return result_err;
    }
    // *server_socket = socket(AF_INET, SOCK_STREAM, 0);
    // if (*server_socket == 0) {
    //     printf("socket\n");
    //     return result_err;
    // }
    // if (setsockopt(*server_socket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option))) {
    //     printf("setsockopt\n");
    //     return result_err;
    // }
    // address->sin_family = AF_INET;
    // address->sin_addr.s_addr = INADDR_ANY;
    // address->sin_port = htons(PORT);
    // if (bind(*server_socket, (struct sockaddr*)address, sizeof(*address)) < 0) {
    //     printf("bind\n");
    //     return result_err;
    // }
    // if (listen(*server_socket, SOMAXCONN) < 0) {
    //     printf("listen\n");
    //     return result_err;
    // }
    return result_ok;
}

int main() {
    int server_socket;
    struct sockaddr_in address;
    if(init(&server_socket, &address) == result_err) {
        printf("init\n\n");
        return 0;
    }
    json_test();
    if(loop(server_socket, &address) == result_err) {
        printf("loop\n\n");
    }
    if(deinit(server_socket) == result_err) {
        printf("deinit\n\n");
    }
    return 0;
}
