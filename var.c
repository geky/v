#include "var.h"

#include "num.h"
#include "str.h"
#include "tbl.h"
#include "fn.h"

#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>


// Returns true if both variables are the
// same type and equivalent.
// all nils are equal
static bool nil_equals(var_t a, var_t b) { return true; }
// compare raw bits by default
static bool bit_equals(var_t a, var_t b) { return a.bits == b.bits; }

static bool (* const var_equalss[8])(var_t, var_t) = {
    nil_equals, bit_equals, num_equals, bit_equals,
    bit_equals, bit_equals, str_equals, bit_equals
};

bool var_equals(var_t a, var_t b) {
    if (a.type != b.type)
        return false;

    return var_equalss[a.type](a, b);
}


// Returns a hash value of the given variable. 
// nils should never be hashed
// use raw bits by default
static hash_t bit_hash(var_t v) { return v.meta ^ v.data; }

hash_t var_hash(var_t v) {
    static hash_t (* const var_hashs[8])(var_t) = {
        bit_hash, bit_hash, num_hash, bit_hash,
        bit_hash, bit_hash, str_hash, bit_hash
    };

    return var_hashs[v.type](v);
}

// Performs iteration on variables
static var_t nil_iter(var_t v) { assert(false); } // TODO error

var_t var_iter(var_t v) {
    static var_t (* const var_iters[8])(var_t) = {
        nil_iter, nil_iter, nil_iter, nil_iter,
        tbl_iter, nil_iter, nil_iter, nil_iter
    };

    return var_iters[v.type](v);
}
    


// Returns a string representation of the variable
static var_t nil_repr(var_t v) { return vcstr("nil"); }
static var_t err_repr(var_t v) { return vcstr("error!"); }

var_t var_repr(var_t v) {
    static var_t (* const var_reprs[8])(var_t) = {
        nil_repr, err_repr, num_repr, fn_repr,
        tbl_repr,        0, str_repr, fn_repr
    };

    return var_reprs[v.type](v);
}

// Prints variable to stdout for debugging
void var_print(var_t v) {
    var_t repr = var_repr(v);
    printf("%.*s", repr.len, var_str(repr));
}


// Table related functions performed on variables
static var_t nil_lookup(var_t t, var_t k) { assert(false); } // TODO error
static var_t vtbl_lookup(var_t t, var_t k) { return tbl_lookup(t.tbl, k); }

var_t var_lookup(var_t v, var_t key) {
    static var_t (* const var_lookups[8])(var_t, var_t) = {
        nil_lookup, nil_lookup, nil_lookup, nil_lookup,
        vtbl_lookup, nil_lookup, nil_lookup, nil_lookup
    };

    return var_lookups[v.type](v, key);
}

static var_t nil_lookdn(var_t t, var_t k, len_t i) { assert(false); } // TODO errors
static var_t vtbl_lookdn(var_t t, var_t k, len_t i) { return tbl_lookdn(t.tbl, k, i); }

var_t var_lookdn(var_t v, var_t key, len_t i) {
    static var_t (* const var_lookdns[8])(var_t, var_t, len_t) = {
        nil_lookdn, nil_lookdn, nil_lookdn, nil_lookdn,
        vtbl_lookdn, nil_lookdn, nil_lookdn, nil_lookdn
    };

    return var_lookdns[v.type](v, key, i);
}


static void nil_insert(var_t t, var_t k, var_t v) { assert(false); } // TODO error
static void vtbl_insert(var_t t, var_t k, var_t v) { tbl_insert(t.tbl, k, v); }

void var_insert(var_t v, var_t key, var_t val) {
    static void (* const var_inserts[8])(var_t, var_t, var_t) = {
        nil_insert, nil_insert, nil_insert, nil_insert,
        vtbl_insert, nil_insert, nil_insert, nil_insert
    };

    var_inserts[v.type](v, key, val);
}


static void nil_assign(var_t t, var_t k, var_t v) { assert(false); } // TODO error
static void vtbl_assign(var_t t, var_t k, var_t v) { tbl_assign(t.tbl, k, v); }

void var_assign(var_t v, var_t key, var_t val) {
    static void (* const var_assigns[8])(var_t, var_t, var_t) = {
        nil_assign, nil_assign, nil_assign, nil_assign,
        vtbl_assign, nil_assign, nil_assign, nil_assign
    };

    var_assigns[v.type](v, key, val);
}


static void nil_add(var_t t, var_t v) { assert(false); } // TODO error
static void vtbl_add(var_t t, var_t v) { tbl_add(t.tbl, v); }

void var_add(var_t v, var_t val) {
    static void (* const var_adds[8])(var_t, var_t) = {
        nil_add, nil_add, nil_add, nil_add,
        vtbl_add, nil_add, nil_add, nil_add
    };

    var_adds[v.type](v, val);
}


// Function calls performed on variables
static var_t nil_call(var_t f, tbl_t *a)  { assert(false); } // TODO error
static var_t vfn_call(var_t f, tbl_t *a)  { f.meta &= ~3; return fn_call(f.fn, a, f.tbl); }
static var_t vbfn_call(var_t f, tbl_t *a) { f.meta &= ~3; return f.bfn(a, f.tbl); }

var_t var_call(var_t v, tbl_t *args) {
    static var_t (* const var_calls[8])(var_t, tbl_t *) = {
        nil_call, nil_call, nil_call, vbfn_call, 
        nil_call, nil_call, nil_call, vfn_call
    };

    return var_calls[v.type](v, args);
}

