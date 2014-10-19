#include "err.h"
#include "tbl.h"

// Errors used in core V implementation
// TODO preallocate these so that Bad Things don't happen
__attribute__((const)) err_t err_nomem(void) {
    static tbl_t *err = 0;

    if (err) return err;

    err = tbl_create(0);
    tbl_insert(err, vcstr("type"), vcstr("memory error"));
    tbl_insert(err, vcstr("reason"), vcstr("out of memory"));

    return err;
}

__attribute__((const)) err_t err_len(void) {
    static tbl_t *err = 0;

    if (err) return err;

    err = tbl_create(0);
    tbl_insert(err, vcstr("type"), vcstr("length error"));
    tbl_insert(err, vcstr("reason"), vcstr("exceeded max length"));

    return err;
}

__attribute__((const)) err_t err_ro(void) {
    static tbl_t *err = 0;

    if (err) return err;

    err = tbl_create(0);
    tbl_insert(err, vcstr("type"), vcstr("readonly error"));
    tbl_insert(err, vcstr("reason"), vcstr("assigning to readonly table"));

    return err;
}

__attribute__((const)) err_t err_parse(void) {
    static tbl_t *err = 0;

    if (err) return err;

    err = tbl_create(0);
    tbl_insert(err, vcstr("type"), vcstr("parse error"));
    tbl_insert(err, vcstr("reason"), vcstr("expression could not be parsed"));

    return err;
}
