/*
 *  Parser for Mu
 */

#ifdef MU_DEF
#ifndef MU_PARSE_DEF
#define MU_PARSE_DEF

#include "mu.h"
#include "var.h"
#include "tbl.h"
#include "err.h"
#include "lex.h"


// Specific state structures
struct opparse {
    len_t ins;
    uint8_t lprec;
    uint8_t rprec;
};

struct jparse {
    tbl_t *ctbl;
    tbl_t *btbl;
};

struct fnparse {
    mstr_t *bcode;

    len_t stack;
    len_t len;
    len_t ins;

    tbl_t *fns;
    tbl_t *vars;
};

// State of a parse
typedef struct parse {
    struct fnparse *fn;
    struct jparse j;
    struct opparse op;
    tbl_t *args;

    tbl_t *keys;

    uint8_t indirect;
    uint8_t stmt;
    uint8_t left;
    uint8_t key;
    uint8_t paren;

    uint8_t jsize;
    uint8_t jtsize;
    uint8_t jfsize;

    tok_t tok;
    var_t val;

    ref_t *ref;
    str_t *str;
    str_t *pos;
    str_t *end;

    eh_t *eh;
} parse_t;


#endif
#else
#ifndef MU_PARSE_H
#define MU_PARSE_H
#define MU_DEF
#include "parse.h"
#undef MU_DEF

// Entry points into parsing Mu source into bytecode
parse_t *parse_create(var_t code, eh_t *eh);
void parse_destroy(parse_t *p);

void parse_args(parse_t *p, tbl_t *args);
void parse_stmts(parse_t *p);
void parse_stmt(parse_t *p);
void parse_expr(parse_t *p);
void parse_end(parse_t *p);

#endif
#endif
