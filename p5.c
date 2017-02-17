#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>

#define MISSING() do { \
    fprintf(stderr,"missing code at %s:%d\n",__FILE__,__LINE__); \
    error("missing"); \
} while (0)

enum token_type {
    IF_KWD,
    ELSE_KWD,
    WHILE_KWD,
    FUN_KWD,
    RETURN_KWD,
    PRINT_KWD,
    EQ, 
    EQ_EQ,
    LT,
    GT,
    LT_GT,
    SEMI,
    COMMA,
    LEFT,
    RIGHT,
    LEFT_BLOCK,
    RIGHT_BLOCK,
    PLUS,
    MUL,
    ID,
    INTEGER,
    END,
};

union token_value {
    char *id;
    uint64_t integer;
};

struct token {
    enum token_type type;
    union token_value value;
};

struct trie_node {
    struct trie_node *children[36];
    struct trie_node *parent;

    //for the global namespace, var_num is 0 if unassigned and 1 if assigned
    //for a local namespace, var_num is 0 if unassigned and the parameter number (1 indexed) if assigned
    int var_num;
    //for purposes of printing
    char ch;
};

static jmp_buf escape;

static char *id_buffer;
static unsigned int id_length;
static unsigned int id_buffer_size;

static struct token *tokens;
static unsigned int token_count;
static unsigned int token_buffer_size;

static unsigned int token_index;

static struct trie_node *global_root_ptr;

static unsigned int if_count = 0;
static unsigned int while_count = 0;

static void error(char *message) {
    fprintf(stderr,"error: %s\n", message);
    longjmp(escape, 1);
}

/* append a character to the id buffer */
void appendChar(char ch) {
    if (id_length == id_buffer_size) {
        id_buffer_size *= 2;
        id_buffer = realloc(id_buffer, id_buffer_size);
    }
    id_buffer[id_length] = ch;
    id_length++;
}

/* returns true if the given character can be part of an id, false otherwise */
int isIdChar(char ch) {
    return islower(ch) || isdigit(ch);
}

/* append a token to the token array */
void appendToken(struct token token) {
    if (token_count == token_buffer_size) {
        token_buffer_size *= 2;
        tokens = realloc(tokens, token_buffer_size);
    }
    tokens[token_count] = token;
    token_count++;
}

/* read a token from standard in */
struct token getToken(void) {
    struct token next_token;

    static char next_char = ' ';

    while (isspace(next_char)) {
        next_char = getchar();
    }

    if (next_char == -1) {
        next_token.type = END;
    } else if (next_char == '=') {
        next_char = getchar();
        if (next_char == '=') {
            next_char = getchar();
            next_token.type = EQ_EQ;
        } else {
            next_token.type = EQ;
        }
    } else if (next_char == '<') {
        next_char = getchar();
        if (next_char == '>') {
            next_char = getchar();
            next_token.type = LT_GT;
        } else {
            next_token.type = LT;
        }
    } else if (next_char == '>') {
        next_char = getchar();
        next_token.type = GT;
    } else if (next_char == ';') {
        next_char = getchar();
        next_token.type = SEMI;
    } else if (next_char == ',') {
        next_char = getchar();
        next_token.type = COMMA;
    } else if (next_char == '(') {
        next_char = getchar();
        next_token.type = LEFT;
    } else if (next_char == ')') {
        next_char = getchar();
        next_token.type = RIGHT;
    } else if (next_char == '{') {
        next_char = getchar();
        next_token.type = LEFT_BLOCK;
    } else if (next_char == '}') {
        next_char = getchar();
        next_token.type = RIGHT_BLOCK;
    } else if (next_char == '+') {
        next_char = getchar();
        next_token.type = PLUS;
    } else if (next_char == '*') {
        next_char = getchar();
        next_token.type = MUL;
    } else if (isdigit(next_char)) {
        next_token.type = INTEGER;
        next_token.value.integer = 0;
        while (isdigit(next_char)) {
            next_token.value.integer = next_token.value.integer * 10 + (next_char - '0');
            next_char = getchar();
            while (next_char == '_') {
                next_char = getchar();
            }
        }
    } else if (islower(next_char)) {
        id_length = 0;
        while (isIdChar(next_char)) {
            appendChar(next_char);
            next_char = getchar();
        }
        appendChar('\0');

        if (strcmp(id_buffer, "if") == 0) {
            next_token.type = IF_KWD;
        } else if (strcmp(id_buffer, "else") == 0) {
            next_token.type = ELSE_KWD;
        } else if (strcmp(id_buffer, "while") == 0) {
            next_token.type = WHILE_KWD;
        } else if (strcmp(id_buffer, "fun") == 0) {
            next_token.type = FUN_KWD;
        } else if (strcmp(id_buffer, "return") == 0) {
            next_token.type = RETURN_KWD;
        } else if (strcmp(id_buffer, "print") == 0) {
            next_token.type = PRINT_KWD;
        } else {
            next_token.type = ID;
            next_token.value.id = strcpy(malloc(id_length), id_buffer);
        }
    } else {
        error("invalid character");
    }

    return next_token;
}

/* proceed to the next token */
void consume() {
    token_index++;
}

int isWhile() {
    return tokens[token_index].type == WHILE_KWD;
}

int isIf() {
    return tokens[token_index].type == IF_KWD;
}

int isElse() {
    return tokens[token_index].type == ELSE_KWD;
}

int isFun() {
    return tokens[token_index].type == FUN_KWD;
}

int isReturn() {
    return tokens[token_index].type == RETURN_KWD;
}

int isPrint() {
    return tokens[token_index].type == PRINT_KWD;
}

int isSemi() {
    return tokens[token_index].type == SEMI;
}

int isComma() {
    return tokens[token_index].type == COMMA;
}

int isLeftBlock() {
    return tokens[token_index].type == LEFT_BLOCK;
}

int isRightBlock() {
    return tokens[token_index].type == RIGHT_BLOCK;
}

int isEq() {
    return tokens[token_index].type == EQ;
}

int isEqEq() {
    return tokens[token_index].type == EQ_EQ;
}

int isLt() {
    return tokens[token_index].type == LT;
}

int isGt() {
    return tokens[token_index].type == GT;
}

int isLtGt() {
    return tokens[token_index].type == LT_GT;
}

int isLeft() {
    return tokens[token_index].type == LEFT;
}

int isRight() {
    return tokens[token_index].type == RIGHT;
}

int isEnd() {
    return tokens[token_index].type == END;
}

int isId() {
    return tokens[token_index].type == ID;
}

int isMul() {
    return tokens[token_index].type == MUL;
}

int isPlus() {
    return tokens[token_index].type == PLUS;
}

int isInt() {
    return tokens[token_index].type == INTEGER;
}

char *getId() {
    return tokens[token_index].value.id;
}

uint64_t getInt() {
    return tokens[token_index].value.integer;
}

void freeTrie(struct trie_node *node_ptr) {
    if (node_ptr == 0) {
        return;
    }
    for (int child_num = 0; child_num < 36; child_num++) {
        freeTrie(node_ptr->children[child_num]);
    }
    free(node_ptr);
}

int getVarNum(char *id, struct trie_node *node_ptr) {
    for (char* ch_ptr = id; *ch_ptr != 0; ch_ptr++) {
        int child_num;
        if (isdigit(*ch_ptr)) {
            child_num = *ch_ptr - '0';
        } else {
            child_num = *ch_ptr - 'a' + 10;
        }
        if (node_ptr->children[child_num] == 0) {
            return 0;
        }
        node_ptr = node_ptr->children[child_num];
    }
    return node_ptr->var_num;
}

void setVarNum(char *id, struct trie_node *node_ptr, int var_num) {
    for (char* ch_ptr = id; *ch_ptr != 0; ch_ptr++) {
        int child_num;
        if (isdigit(*ch_ptr)) {
            child_num = *ch_ptr - '0';
        } else {
            child_num = *ch_ptr - 'a' + 10;
        }
        if (node_ptr->children[child_num] == 0) {
            struct trie_node *new_node_ptr = calloc(1, sizeof(struct trie_node));
            new_node_ptr->parent = node_ptr;
            new_node_ptr->ch = *ch_ptr;
            node_ptr->children[child_num] = new_node_ptr;
        }
        node_ptr = node_ptr->children[child_num];
    }
    node_ptr->var_num = var_num;
}

/* prints instructions to set the value of %rax to the value of the variable */
void get(char *id, struct trie_node *local_root_ptr) {
    int var_num = getVarNum(id, local_root_ptr);
    switch (var_num) {
        case 0:
            setVarNum(id, global_root_ptr, 1);
            printf("    mov %s_var,%%rax\n", id);
            break;
        case 1:
            printf("    mov %%rdi,%%rax\n");
            break;
        case 2:
            printf("    mov %%rsi,%%rax\n");
            break;
        case 3:
            printf("    mov %%rdx,%%rax\n");
            break;
        case 4:
            printf("    mov %%rcx,%%rax\n");
            break;
        case 5:
            printf("    mov %%r8,%%rax\n");
            break;
        case 6:
            printf("    mov %%r9,%%rax\n");
            break;
        default:
            printf("    mov %d(%%rbp),%%rax\n", 8 * (var_num - 7));
    }
}

/* prints instructions to set the value of the variable to the value of %rax */
void set(char *id, struct trie_node *local_root_ptr) {
    int var_num = getVarNum(id, local_root_ptr);
    switch (var_num) {
        case 0:
            setVarNum(id, global_root_ptr, 1);
            printf("    mov %%rax,%s_var\n", id);
            break;
        case 1:
            printf("    mov %%rax,%%rdi\n");
            break;
        case 2:
            printf("    mov %%rax,%%rsi\n");
            break;
        case 3:
            printf("    mov %%rax,%%rdx\n");
            break;
        case 4:
            printf("    mov %%rax,%%rcx\n");
            break;
        case 5:
            printf("    mov %%rax,%%r8\n");
            break;
        case 6:
            printf("    mov %%rax,%%r9\n");
            break;
        default:
            printf("    mov %%rax,%d(%%rbp)\n", 8 * (var_num - 7));
    }
}

/* prints the name of the variable that the given node represents */
void printId(struct trie_node *node_ptr) {
    if (node_ptr->ch == '\0') {
        return;
    }
    printId(node_ptr->parent);
    printf("%c", node_ptr->ch);
}

/* generates labels for global variables and initializes their values to 0 */
void initVars(struct trie_node *node_ptr) {
    if (node_ptr == 0) {
        return;
    }
    if (node_ptr->var_num) {
        printId(node_ptr);
        printf("_var:\n");
        printf("    .quad 0\n");
    }
    for (int child_num = 0; child_num < 36; child_num++) {
        initVars(node_ptr->children[child_num]);
    }
}

void expression(struct trie_node *);
void seq(struct trie_node *);

/* handle id, literals, and (...) */
void e1(struct trie_node *local_root_ptr) {
    if (isLeft()) {
        consume();
        expression(local_root_ptr);
        printf("    mov %%rax,%%r12\n");
        if (!isRight()) {
            error("unclosed parenthesis expression");
        }
        consume();
    } else if (isInt()) {
        uint64_t v = getInt();
        consume();
        printf("    mov $%" PRIu64 ",%%r12\n", v);
    } else if (isId()) {
        char *id = getId();
        consume();
        if (isLeft()) {
            consume();
            printf("    push %%rdi\n");
            printf("    push %%rsi\n");
            printf("    push %%rdx\n");
            printf("    push %%rcx\n");
            printf("    push %%r8\n");
            printf("    push %%r9\n");
            int params = 0;
            while (!isRight()) {
                expression(local_root_ptr);
                if (isComma()) {
                    consume();
                }
                params++;
                if (params % 2 == 0) {
                    printf("    mov %%rax,(%%rsp)\n");
                } else {
                    printf("    push %%rax\n");
                    printf("    sub $8,%%rsp\n");
                }
            }
            consume();
            if (params % 2 != 0) {
                params++;
            }
            for (int index = 0; index < params; index++) {
                printf("    pushq %d(%%rsp)\n", 16 * index);
            }
            for (int index = 0; index < params; index++) {
                printf("    popq %d(%%rsp)\n", 8 * (params - 1));
            }
            for (int param_num = 1; param_num <= 6 && param_num <= params; param_num++) {
                switch (param_num) {
                    case 1:
                        printf("    pop %%rdi\n");
                        break;
                    case 2:
                        printf("    pop %%rsi\n");
                        break;
                    case 3:
                        printf("    pop %%rdx\n");
                        break;
                    case 4:
                        printf("    pop %%rcx\n");
                        break;
                    case 5:
                        printf("    pop %%r8\n");
                        break;
                    case 6:
                        printf("    pop %%r9\n");
                        break;
                }
            }
            printf("    call %s_fun\n", id);
            if (params > 6) {
                printf("    add $%d,%%rsp\n", 8 * (params - 6));
            }
            printf("    pop %%r9\n");
            printf("    pop %%r8\n");
            printf("    pop %%rcx\n");
            printf("    pop %%rdx\n");
            printf("    pop %%rsi\n");
            printf("    pop %%rdi\n");
        } else {
            get(id, local_root_ptr);
        }
        printf("    mov %%rax,%%r12\n");
    } else {
        error("expected expression");
    }
}

/* handle '*' */
void e2(struct trie_node *local_root_ptr) {
    e1(local_root_ptr);
    printf("    mov %%r12,%%r13\n");
    while (isMul()) {
        consume();
        e1(local_root_ptr);
        printf("    imul %%r12,%%r13\n");
    }
}

/* handle '+' */
void e3(struct trie_node *local_root_ptr) {
    e2(local_root_ptr);
    printf("    mov %%r13,%%r14\n");
    while (isPlus()) {
        consume();
        e2(local_root_ptr);
        printf("    add %%r13,%%r14\n");
    }
}

/* handle '==' */
void e4(struct trie_node *local_root_ptr) {
    e3(local_root_ptr);
    printf("    mov %%r14,%%r15\n");
    while (1) {
        if (isEqEq()) {
            consume();
            e3(local_root_ptr);
            printf("    cmp %%r14,%%r15\n");
            printf("    sete %%r15b\n");
            printf("    movzbq %%r15b,%%r15\n");
        } else if (isLt()) {
            consume();
            e3(local_root_ptr);
            printf("    cmp %%r14,%%r15\n");
            printf("    setb %%r15b\n");
            printf("    movzbq %%r15b,%%r15\n");
        } else if (isGt()) {
            consume();
            e3(local_root_ptr);
            printf("    cmp %%r14,%%r15\n");
            printf("    seta %%r15b\n");
            printf("    movzbq %%r15b,%%r15\n");
        } else if (isLtGt()) {
            consume();
            e3(local_root_ptr);
            printf("    cmp %%r14,%%r15\n");
            printf("    setne %%r15b\n");
            printf("    movzbq %%r15b,%%r15\n");
        } else {
            break;
        }
    }
}

void expression(struct trie_node *local_root_ptr) {
    printf("    push %%r12\n");
    printf("    push %%r13\n");
    printf("    push %%r14\n");
    printf("    push %%r15\n");
    e4(local_root_ptr);
    printf("    mov %%r15,%%rax\n");
    printf("    pop %%r15\n");
    printf("    pop %%r14\n");
    printf("    pop %%r13\n");
    printf("    pop %%r12\n");
}

int statement(struct trie_node *local_root_ptr) {
    if (isId()) {
        char *id = getId();
        consume();
        if (!isEq()) {
            error("expected =");
        }
        consume();
        expression(local_root_ptr);
        set(id, local_root_ptr);
        if (isSemi()) {
            consume();
        }
        return 1;
    } else if (isLeftBlock()) {
        consume();
        seq(local_root_ptr);
        if (!isRightBlock())
            error("unclosed statement block");
        consume();
        return 1;
    } else if (isIf()) {
        unsigned int if_num = if_count++;
        consume();
        expression(local_root_ptr);
        printf("    test %%rax,%%rax\n");
        printf("    jz if_end_%u\n", if_num);
        statement(local_root_ptr);
        printf("    jmp else_end_%u\n", if_num);
        printf("if_end_%u:\n", if_num);
        if (isElse()) {
            consume();
            statement(local_root_ptr);
        }
        printf("else_end_%u:\n", if_num);
        return 1;
    } else if (isWhile()) {
        unsigned int while_num = while_count++;
        consume();
        printf("while_begin_%u:\n", while_num);
        expression(local_root_ptr);
        printf("    test %%rax,%%rax\n");
        printf("    jz while_end_%u\n", while_num);
        statement(local_root_ptr);
        printf("    jmp while_begin_%u\n", while_num);
        printf("while_end_%u:\n", while_num);
        return 1;
    } else if (isSemi()) {
        consume();
        return 1;
    } else if (isReturn()) {
        consume();
        expression(local_root_ptr);
        printf("    pop %%rbp\n");
        printf("    ret\n");
        if (isSemi()) {
            consume();
        }
        return 1;
    } else if (isPrint()) {
        consume();
        expression(local_root_ptr);
        printf("    push %%rdi\n");
        printf("    push %%rsi\n");
        printf("    push %%rdx\n");
        printf("    push %%rcx\n");
        printf("    push %%r8\n");
        printf("    push %%r9\n");
        printf("    mov $output_format,%%rdi\n");
        printf("    mov %%rax,%%rsi\n");
        printf("    call printf\n");
        printf("    pop %%r9\n");
        printf("    pop %%r8\n");
        printf("    pop %%rcx\n");
        printf("    pop %%rdx\n");
        printf("    pop %%rsi\n");
        printf("    pop %%rdi\n");
        if (isSemi()) {
            consume();
        }
        return 1;
    } else {
        return 0;
    }
}

void seq(struct trie_node *local_root_ptr) {
    while (statement(local_root_ptr)) { fflush(stdout); }
}

void function(void) {
    if (!isFun()) {
        error("expected fun");
    }
    consume();
    if (!isId()) {
        error("invalid function name");
    }
    char *id = getId();
    consume();
    printf("%s_fun:\n", id);
    printf("    push %%rbp\n");
    printf("    lea 16(%%rsp),%%rbp\n");
    if (!isLeft()) {
        error("expected function parameter declaration");
    }
    consume();
    struct trie_node *local_root_ptr = calloc(1, sizeof(struct trie_node));
    int var_num = 1;
    while (!isRight()) {
        if (!isId()) {
            error("invalid parameter name");
        }
        char *param_id = getId();
        consume();
        setVarNum(param_id, local_root_ptr, var_num++);
        free(param_id);
        if (isComma()) {
            consume();
        }
    }
    consume();
    statement(local_root_ptr);
    freeTrie(local_root_ptr);
    printf("    pop %%rbp\n");
    printf("    ret\n");
}

void program(void) {
    while (isFun()) {
        function();
    }
    if (!isEnd())
        error("expected end of file");
}

void compile(void) {
    printf("    .text\n");
    printf("    .global main\n");
    printf("main:\n");
    printf("    sub $8,%%rsp\n");
    printf("    call main_fun\n");
    printf("    mov $0,%%rax\n");
    printf("    add $8,%%rsp\n");
    printf("    ret\n");

    id_buffer = malloc(10);
    id_buffer_size = 10;
    tokens = malloc(sizeof(struct token) * 100);
    token_buffer_size = 100;
    do {
        appendToken(getToken());
    } while (tokens[token_count - 1].type != END);

    global_root_ptr = calloc(1, sizeof(struct trie_node));
    int x = setjmp(escape);
    if (x == 0) {
        program();
    }
    printf("    .data\n");
    printf("output_format:\n");
    printf("    .string \"%%" PRIu64 "\\n\"\n");
    initVars(global_root_ptr);

    free(id_buffer);
    freeTrie(global_root_ptr);
}

int main(int argc, char *argv[]) {
    compile();
    return 0;
}