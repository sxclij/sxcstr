#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/resource.h>

#define PORT 8080
#define BUFFER_SIZE (1024 * 1024 * 16)
#define RLIMIT_SIZE (1024 * 1024 * 1024)

enum result {
    result_ok,
    result_err,
};
enum jsontype {
    json_type_key,
    json_type_val,
    json_type_arr,
};
struct vec {
    char* data;
    uint32_t size;
};
struct string {
    const char* data;
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

struct string str_tostring(const char* src) {
    return (struct string){.data = src, .size = strlen(src)};
}
struct string itr_tostring(const char* begin, const char* end) {
    return (struct string){.data = begin, .size = end - begin};
}
struct string prim_tostring(const char* begin, uint32_t size) {
    return (struct string){.data = begin, .size = size};
}
struct string vec_tostring(struct vec* src) {
    return (struct string){.data = src->data, .size = src->size};
}
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
    vec_cpy(dst, str_tostring(src));
}
void vec_cat_str(struct vec* dst, const char* src) {
    vec_cat(dst, str_tostring(src));
}
uint64_t string_hash(struct string src) {
    uint64_t x = 5381;
    for (int i = 0; i < src.size; i++) {
        x = ((x << 5) + x) + src.data[i];
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
        .rhs = NULL};
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
                *dst_end = itr_tostring(start, end_quote);
                dst_end++;
                current++;
                continue;
            } else {
                fprintf(stderr, "Error: Unterminated string literal.\n");
                return;
            }
        }
        if (json_issign(*current)) {
            *dst_end = prim_tostring(current, 1);
            dst_end++;
            current++;
            continue;
        }
        const char* token_start = current;
        while (current < end && !json_isspace(*current) && !json_issign(*current)) {
            current++;
        }
        if (current > token_start) {
            *dst_end = itr_tostring(token_start, token_start);
            dst_end++;
        }
    }
    *dst_end = prim_tostring(NULL, 0);
}
struct json* json_find(struct json* root, uint64_t hash) {
    struct json* current = root;
    while (current != NULL) {
        if (hash == current->hash) {
            return current;
        } else if (hash < current->hash) {
            current = current->lhs;
        } else {
            current = current->rhs;
        }
    }
    return NULL;
}
struct json* json_parse(struct json* dst, struct string src) {
    struct string token[BUFFER_SIZE];
    struct json* stack_json[BUFFER_SIZE];
    struct json* stack_back[BUFFER_SIZE];
    enum jsontype stack_type[BUFFER_SIZE];
    struct json* dst_end = dst;
    struct string* token_itr = token;
    int nest = 0;
    uint64_t random = 1;

    json_tokenize(token, src);
    token_itr = token;

    stack_json[0] = NULL;
    stack_back[0] = NULL;
    stack_type[0] = json_type_key;

    while (token_itr->data != NULL) {
        if (token_itr->size == 1 && token_itr->data[0] == '{') {
            nest++;
            stack_json[nest] = NULL;
            stack_back[nest] = NULL;
            stack_type[nest] = json_type_key;
        } else if (token_itr->size == 1 && token_itr->data[0] == '[') {
            nest++;
            stack_json[nest] = NULL;
            stack_back[nest] = NULL;
            stack_type[nest] = json_type_arr;
        } else if ((token_itr->size == 1 && token_itr->data[0] == '}') || (token_itr->size == 1 && token_itr->data[0] == ']')) {
            struct json* currentnode = stack_json[nest];
            nest--;
            if (nest >= 0) {
                if (stack_type[nest] == json_type_key || stack_type[nest] == json_type_arr) {
                    stack_json[nest] = json_treap_insert(stack_json[nest], currentnode);
                } else if (stack_type[nest] == json_type_val && stack_back[nest] != NULL) {
                    stack_back[nest]->val = json_treap_insert(stack_back[nest]->val, currentnode);
                }
            }
        } else if (token_itr->size == 1 && token_itr->data[0] == ':') {
            stack_type[nest] = json_type_val;
        } else if (token_itr->size == 1 && token_itr->data[0] == ',') {
            if (stack_type[nest] == json_type_val) {
                stack_type[nest] = json_type_key;
            } else if (nest > 0 && stack_type[nest - 1] == json_type_arr) {
                stack_type[nest] = json_type_arr;
            }
        } else {
            struct json* newnode = json_newnode(&dst_end, &random, *token_itr);
            if (nest >= 0) {
                if (stack_type[nest] == json_type_key || stack_type[nest] == json_type_arr) {
                    stack_json[nest] = json_treap_insert(stack_json[nest], newnode);
                    stack_back[nest] = newnode;
                } else if (stack_type[nest] == json_type_val && stack_back[nest] != NULL) {
                    stack_back[nest]->val = json_treap_insert(stack_back[nest]->val, newnode);
                }
            }
        }
        token_itr++;
    }

    return stack_json[0];
}
struct json* json_get(struct json* root, struct string path) {
    const char* start = path.data;
    const char* end = path.data + path.size;
    const char* current = start;
    struct json* node = root;
    while (current < end && node != NULL) {
        if (*current == '.') {
            current++;
            continue;
        }
        const char* key_start = current;
        while (current < end && *current != '.') {
            current++;
        }
        struct string key = {.data = key_start, .size = current - key_start};
        uint64_t key_hash = string_hash(key);
        if (node) {
            node = json_find(node, key_hash);
            if (node && node->val) {
                node = node->val;
            } else if (!node) {
                return NULL;
            }
        }
    }
    return node;
}
void json_escape_string(struct vec* dst, struct string str) {
    for (uint32_t i = 0; i < str.size; i++) {
        switch (str.data[i]) {
            case '"':
                vec_cat_str(dst, "\\\"");
                break;
            case '\\':
                vec_cat_str(dst, "\\\\");
                break;
            case '/':
                vec_cat_str(dst, "\\/");
                break;
            case '\b':
                vec_cat_str(dst, "\\b");
                break;
            case '\f':
                vec_cat_str(dst, "\\f");
                break;
            case '\n':
                vec_cat_str(dst, "\\n");
                break;
            case '\r':
                vec_cat_str(dst, "\\r");
                break;
            case '\t':
                vec_cat_str(dst, "\\t");
                break;
            default:
                {
                    char ch = str.data[i];
                    vec_cat(dst, prim_tostring(&ch, 1));
                }
                break;
        }
    }
}
void json_tovec_no_recursion(struct vec* dst, struct json* src) {
    dst->size = 0;
    if (!src) return;
    struct json* stack[BUFFER_SIZE];
    int top = -1;
    stack[++top] = src;
    int is_first[BUFFER_SIZE];
    is_first[top] = 1;
    while (top >= 0) {
        struct json* current = stack[top];
        if (current->lhs || current->rhs) { 
            if (is_first[top]) {
                vec_cat_str(dst, "{");
                is_first[top] = 0;
                struct json* child = current;
                while (child) {
                    stack[++top] = child->lhs;
                    is_first[top] = (child == current);
                    child = child->rhs;
                }
                continue;
            } else {
                if (current->lhs) {
                    vec_cat_str(dst, ",");
                    vec_cat_str(dst, "\"");
                    json_escape_string(dst, current->string);
                    vec_cat_str(dst, "\":");
                    stack[top] = current->val;
                    is_first[top] = 1;
                    continue;
                }
            }
            vec_cat_str(dst, "}");
            top--;
        } else if (current->val) { 
            if (is_first[top]) {
                vec_cat_str(dst, "[");
                is_first[top] = 0;
                struct json* child = current->val;
                while (child) {
                    stack[++top] = child;
                    is_first[top] = (child == current->val);
                    child = child->rhs;
                }
                continue;
            } else {
                if (current) {
                    vec_cat_str(dst, ",");
                    json_tovec_no_recursion(dst, current); 
                }
            }
            vec_cat_str(dst, "]");
            top--;
        } else { 
            vec_cat_str(dst, "\"");
            json_escape_string(dst, current->string);
            vec_cat_str(dst, "\"");
            top--;
        }
    }
    vec_tostr(dst);
}
void json_tovec(struct vec* dst, struct json* src) {
    dst->size = 0;
    json_tovec_no_recursion(dst, src);
    vec_tostr(dst);
}
enum result file_read_str(struct vec* dst, const char* path) {
    FILE* fp = fopen(path, "r");
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
enum result file_read_vec(struct vec* dst, struct vec* path) {
    vec_tostr(path);
    return file_read_str(dst, path->data);
}
enum result handle_get(struct vec* send_vec, struct vec* recv_vec, struct json* setting_root) {
    char file_data[BUFFER_SIZE];
    char path_data[BUFFER_SIZE];
    char contenttype_data[BUFFER_SIZE];
    struct vec file_vec = (struct vec){.data = file_data, .size = 0};
    struct vec path_vec = (struct vec){.data = path_data, .size = 0};
    struct vec contenttype_vec = (struct vec){.data = contenttype_data, .size = 0};
    char* path_start = strstr(recv_vec->data, "/");
    char* path_end = strstr(path_start, " ");
    char* path_ext;
    uint32_t path_size = path_end - path_start;
    if(strstr(recv_vec->data, ".json")) {
        handle_json();
    }
    if(memcmp(path_start, "/", 1) == 0 && path_size == 1) {
        vec_cpy_str(&path_vec, "./routes/index.html");
    } else if(memcmp(path_start, "/favicon.ico", 12) == 0 && path_size == 12) {
        vec_cpy_str(&path_vec, "./routes/favicon.svg");
    } else {
        vec_cpy_str(&path_vec, "./routes");
        vec_cat(&path_vec, itr_tostring(path_start, path_end));
    }
    vec_tostr(&path_vec);
    path_ext = strrchr(path_vec.data, '.');
    if(path_ext[1] == '/') {
        vec_cat_str(&path_vec, ".html");
    }
    vec_tostr(&path_vec);
    path_ext = strrchr(path_vec.data, '.') + 1;
    struct json* contenttype_node = json_get(setting_root, itr_tostring(path_ext, path_end));
    json_tovec(&contenttype_vec, contenttype_node);
    if(file_read_vec(&file_vec, &path_vec) == result_err) {
        printf("file_read %s\n", path_vec.data);
        return result_err;
    }
    send_vec->size = sprintf(send_vec->data, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %u\r\n\r\n", contenttype_vec.data, file_vec.size);
    memcpy(send_vec->data + send_vec->size, file_vec.data, file_vec.size);
    send_vec->size += file_vec.size;
}
enum result handle(int client_socket, struct json* setting_root) {
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
        if(handle_get(&send_vec, &recv_vec, setting_root) == result_err) {
            printf("handle_get\n\n");
            return result_ok;
        }
        send(client_socket, send_vec.data, send_vec.size, 0);
    }
    return result_ok;
}
enum result loop(int server_socket, struct sockaddr_in* address, struct json* setting_root) {
    int address_length = sizeof(address);
    int client_socket;
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)address, &address_length);
        if (client_socket < 0) {
            printf("accept\n\n");
            continue;
        }
        if(handle(client_socket, setting_root) == result_err) {
            printf("handle\n");
            close(client_socket);
            return result_err;
        }
        close(client_socket);
    }
}
enum result init_limit() {
    const rlim_t kstacksize = RLIMIT_SIZE;
    struct rlimit rl;
    if (getrlimit(RLIMIT_STACK, &rl) != 0) {
        printf("getrlimit\n");
        return result_err;
    }
    if (rl.rlim_cur >= kstacksize) {
        return result_ok;
    }
    rl.rlim_cur = kstacksize;
    rl.rlim_max = kstacksize;
    if (setrlimit(RLIMIT_STACK, &rl) != 0) {
        printf("setrlimit\n");
        return result_err;
    }
}
enum result init_socket(int* server_socket, struct sockaddr_in* address) {
    int option = 1;
    *server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (*server_socket == 0) {
        printf("socket\n");
        return result_err;
    }
    if (setsockopt(*server_socket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option))) {
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
enum result init_setting(struct vec* setting_vec, struct json* setting_json, struct json** setting_root) {
    if(file_read_str(setting_vec, "./routes/setting.json") == result_err) {
        printf("read setting.json\n");
        return result_err;
    }
    *setting_root = json_parse(setting_json, vec_tostring(setting_vec));
    return result_ok;
}
enum result main2() {
    char setting_data[BUFFER_SIZE];
    struct json setting_json[BUFFER_SIZE / sizeof(struct json)];
    struct vec setting_vec = (struct vec){.data = setting_data, .size = 0};
    struct json* setting_root;
    int server_socket;
    struct sockaddr_in address;
    if(init_setting(&setting_vec, setting_json, &setting_root) == result_err) {
        printf("init_setting\n");
        return result_err;
    }
    if(init_socket(&server_socket, &address) == result_err) {
        printf("init_socket\n");
        return result_err;
    }
    loop(server_socket, &address, setting_root);
}
int main() {
    if(init_limit() == result_err) {
        printf("init\n\n");
        return 0;
    }
    if(main2() == result_err) {
        printf("loop\n\n");
    }
    return 0;
}
