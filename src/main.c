#include <assert.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

// Memory pool at 30MB -> Some programs need it ?
// static constexpr size_t BF_MEMORY_SIZE = 30'000'000;

// Traditional memory pool size
static constexpr size_t BF_MEMORY_SIZE = 30'000;

// Cell size to use (keep it unsigned else it will go to shit)
typedef uint32_t CellType;

typedef enum {
    TK_PLUS,
    TK_MINUS,
    TK_SQUARE_B,
    TK_SQUARE_E,
    TK_SHIFT_L,
    TK_SHIFT_R,
    TK_DOT,
    TK_COMMA,
    TK_INVALID,
} TokenType;

typedef struct {
    TokenType type;
    size_t count;
    size_t jump; // Only used with TK_SQUARE_E
} Token;

typedef struct {
    Token *data;
    size_t length;
    size_t capacity;
} TokenList;

typedef struct {
    CellType mem[BF_MEMORY_SIZE];
    TokenList prgm;
    size_t mp;
} Context;

static int append_token(TokenList *tl, Token t)
{
    if (tl->length == tl->capacity) {
        if (tl->capacity == 0)
            tl->capacity = 8;
        else
            tl->capacity *= 2;
        Token *new_buf = realloc(tl->data, tl->capacity * sizeof(Token));
        if (unlikely(new_buf == nullptr)) {
            perror("realloc");
            return -1;
        }
        tl->data = new_buf;
    }
    tl->data[tl->length++] = t;
    return 0;
}

static int add_token(TokenList *tl, TokenType tt)
{
    if (unlikely(tl->length == 0))
        return append_token(tl, (Token) { .type = tt, .count = 1 });

    Token *last_tk = &tl->data[tl->length - 1];
    if (last_tk->type != tt || last_tk->type == TK_SQUARE_B)
        return append_token(tl, (Token) { .type = tt, .count = 1 });
    last_tk->count++;
    return 0;
}

static TokenType get_token_type(char c)
{
    switch (c) {
    case '+':
        return TK_PLUS;
    case '-':
        return TK_MINUS;
    case '[':
        return TK_SQUARE_B;
    case ']':
        return TK_SQUARE_E;
    case '<':
        return TK_SHIFT_L;
    case '>':
        return TK_SHIFT_R;
    case '.':
        return TK_DOT;
    case ',':
        return TK_COMMA;
    default:
        return TK_INVALID;
    }
}

static int cache_b_jump(TokenList *tl, size_t idx)
{
    assert(tl->data[idx].type == TK_SQUARE_E);
    size_t seen_se = 0;
    for (ssize_t i = (ssize_t) idx - 1; i >= 0; i--) {
        if (tl->data[i].type == TK_SQUARE_E) {
            seen_se += tl->data[i].count;
            continue;
        }
        if (tl->data[i].type == TK_SQUARE_B) {
            if (seen_se < tl->data[i].count) {
                tl->data[idx].jump = (size_t) i;
                tl->data[i].jump = idx;
                return 0;
            }
            seen_se -= tl->data[i].count;
        }
    }
    fprintf(stderr, "Loop mismatch, fix your code >:(\n");
    return -1;
}

static int cache_b_jumps(TokenList *tl)
{
    for (size_t i = 0; i < tl->length; i++)
        if (tl->data[i].type == TK_SQUARE_E && cache_b_jump(tl, i) != 0)
            return -1;
    return 0;
}

static int load_program(Context *ctx, const char *filepath)
{
    int fd = open(filepath, O_RDONLY);
    if (fd == -1) {
        perror("open");
        return -1;
    }

    struct stat s;
    int status = fstat(fd, &s);
    if (status != 0) {
        perror("fstat");
        return -1;
    }
    ssize_t size = s.st_size;
    assert(size >= 0);
    char *f = mmap(nullptr, (size_t) size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (f == nullptr) {
        perror("mmap");
        return -1;
    }

    // Start load program

    TokenList *tl = &ctx->prgm;
    for (size_t i = 0; i < (size_t) size; i++) {
        const TokenType tt = get_token_type(f[i]);
        if (tt == TK_INVALID)
            continue;
        if (add_token(tl, tt) != 0)
            return -1;
    }
    if (cache_b_jumps(tl) != 0)
        return -1;

    // End load program

    if (munmap(f, (size_t) size) != 0) {
        perror("munmap");
        return -1;
    }
    if (close(fd) != 0) {
        perror("close");
        return -1;
    }
    return 0;
}

void execute_program(Context *ctx)
{
    for (size_t i = 0; i < ctx->prgm.length; i++) {
        const Token tk = ctx->prgm.data[i];
        // printf("%lu: %d %lu\n", i, tk.type, tk.count);
        switch (tk.type) {
        case TK_DOT:
            const char val = (char) (ctx->mem[ctx->mp] % 256);
            for (size_t a = 0; a < tk.count; a++)
                write(1, &val, 1);
            break;
        case TK_SHIFT_L:
            size_t to_shift = tk.count;
            if (to_shift >= BF_MEMORY_SIZE)
                to_shift %= BF_MEMORY_SIZE;
            if (ctx->mp < to_shift) {
                to_shift -= ctx->mp;
                ctx->mp = BF_MEMORY_SIZE - to_shift;
            } else
                ctx->mp -= tk.count;
            break;
        case TK_SHIFT_R:
            ctx->mp += tk.count;
            if (unlikely(ctx->mp >= BF_MEMORY_SIZE))
                ctx->mp %= BF_MEMORY_SIZE;
            break;
        case TK_SQUARE_E:
            if (ctx->mem[ctx->mp] != 0)
                i = tk.jump;
            break;
        case TK_SQUARE_B:
            if (ctx->mem[ctx->mp] == 0)
                i = tk.jump;
            break;
        case TK_MINUS:
            ctx->mem[ctx->mp] -= tk.count;
            break;
        case TK_PLUS:
            ctx->mem[ctx->mp] += tk.count;
            break;
        case TK_COMMA:
            for (size_t a = 0; a < tk.count; a++) {
                char h = (char) getchar();
                if (h == EOF)
                    ctx->mem[ctx->mp] = 0;
                else
                    ctx->mem[ctx->mp] = (CellType) * (uint8_t *) &h;
            }
            break;
        case TK_INVALID:
        default:
            break;
        }
    }
}

#ifdef UNIT_TESTS
#define MAIN definitely_not_main
#else
#define MAIN main
#endif

int MAIN(int argc, const char **argv)
{
    if (argc != 2)
        return 1;

    Context *ctx = calloc(1, sizeof *ctx);
    if (load_program(ctx, argv[1]) != 0) {
        if (ctx->prgm.data != nullptr)
            free(ctx->prgm.data);
        free(ctx);
        return 1;
    }
    execute_program(ctx);

    if (ctx->prgm.data != nullptr)
        free(ctx->prgm.data);
    free(ctx);
    return 0;
}
