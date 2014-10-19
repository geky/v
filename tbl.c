#include "tbl.h"

#include "mem.h"
#include "str.h"

#include <assert.h>
#include <string.h>

// TODO check lengths appropriately

// finds capactiy based on load factor of 1.5
static inline hash_t tbl_ncap(hash_t s) {
    return s + (s >> 1);
}

// finds next power of two
static inline hash_t tbl_npw2(hash_t s) {
    return s ? 1 << (32-__builtin_clz(s - 1)) : 0;
}

// iterates through hash entries using
// i = i*5 + 1 to avoid degenerative cases
static inline hash_t tbl_next(hash_t i) {
    return (i<<2) + i + 1;
}


// Functions for managing tables
// Each table is preceeded with a reference count
// which is used as its handle in a var
tbl_t *tbl_create(len_t size) {
    tbl_t *tbl = vref_alloc(sizeof(tbl_t));
    if (iserr(tbl))
        return tbl;

    // Support an additional element due to 
    // early resizing in tbl_insert
    tbl->mask = tbl_npw2(tbl_ncap(size+1)) - 1;

    tbl->tail = 0;
    tbl->nils = 0;
    tbl->len = 0;

    tbl->offset = 0;
    tbl->stride = 0;

    return tbl;
}



// Called by garbage collector to clean up
void tbl_destroy(void *m) {
    tbl_t *tbl = m;

    if (tbl->stride > 0) {
        int i, cap, entries;

        if (tbl->stride < 2) {
            cap = tbl->mask + 1;
            entries = tbl->len;
        } else {
            cap = 2 * (tbl->mask + 1);
            entries = cap;
        }

        for (i=0; i < entries; i++)
            var_dec(tbl->array[i]);

        vdealloc(tbl->array, cap * sizeof(var_t));
    }

    if (tbl->tail)
        tbl_dec(tbl->tail);

    vref_dealloc(m, sizeof(tbl_t));
}


// Recursively looks up a key in the table
// returns either that value or nil
var_t tbl_lookup(tbl_t *tbl, var_t key) {
    if (var_isnil(key))
        return vnil;

    hash_t i, hash = var_hash(key);

    for (tbl = tbl_readp(tbl); tbl; tbl = tbl_readp(tbl->tail)) {
        if (tbl->stride < 2) {
            if (num_ishash(key, hash) && hash < tbl->len) {
                if (tbl->stride == 0)
                    return vnum(hash + tbl->offset);
                else
                    return tbl->array[hash];
            }
        } else {
            for (i = hash;; i = tbl_next(i)) {
                hash_t mi = i & tbl->mask;
                var_t *v = &tbl->array[2*mi];

                if (var_isnil(v[0]))
                    break;

                if (var_equals(key, v[0]) && !var_isnil(v[1]))
                    return v[1];
            }
        }
    }

    return vnil;
}


// Recursively looks up either a key or index
// if key is not found
var_t tbl_lookdn(tbl_t *tbl, var_t key, len_t i) {
    tbl = tbl_readp(tbl);

    if (!tbl->tail && tbl->stride < 2) {
        if (i < tbl->len) {
            if (tbl->stride == 0)
                return vnum(i + tbl->offset);
            else
                return tbl->array[i];
        }

        return vnil;
    } else {
        var_t val = tbl_lookup(tbl, key);

        if (!var_isnil(val))
            return val;

        return tbl_lookup(tbl, vnum(i));
    }
}


// converts implicit range to actual array of nums on heap
static err_t tbl_realizevars(tbl_t *tbl) {
    hash_t cap = tbl->mask + 1;
    int i;

    var_t *w = valloc(cap * sizeof(var_t));
    if (iserr(w))
        return w;

    for (i=0; i < tbl->len; i++) {
        w[i] = vnum(i + tbl->offset);
    }

    tbl->array = w;
    tbl->stride = 1;

    return 0;
}

static err_t tbl_realizekeys(tbl_t *tbl) {
    hash_t cap = tbl->mask + 1;
    int i;

    var_t *w = valloc(2*cap * sizeof(var_t));
    if (iserr(w))
        return w;

    if (tbl->stride == 0) {
        for (i=0; i < tbl->len; i++) {
            w[2*i  ] = vnum(i);
            w[2*i+1] = vnum(i + tbl->offset);
        }
    } else {
        for (i=0; i < tbl->len; i++) {
            w[2*i  ] = vnum(i);
            w[2*i+1] = tbl->array[i];
        }

        vdealloc(tbl->array, cap * sizeof(var_t));
    }

    memset(w + 2*tbl->len, 0, 2*(cap - tbl->len) * sizeof(var_t));
    tbl->array = w;
    tbl->stride = 2;

    return 0;
}


// reallocates and rehashes a table
static err_t tbl_resize(tbl_t * tbl, len_t size) {
    hash_t cap = tbl_npw2(tbl_ncap(size));
    hash_t mask = cap - 1;

    if (tbl->stride < 2) {
        if (tbl->stride == 0) {
            tbl->mask = mask;
            return 0;
        }

        var_t *w = valloc(cap * sizeof(var_t));
        if (iserr(w))
            return w;

        memcpy(w, tbl->array, tbl->len * sizeof(var_t));

        vdealloc(tbl->array, (tbl->mask+1) * sizeof(var_t));
        tbl->array = w;
        tbl->mask = mask;
    } else {
        var_t *w = valloc(2*cap * sizeof(var_t));
        if (iserr(w))
            return w;

        memset(w, 0, 2*cap * sizeof(var_t));

        hash_t i, j;

        for (j=0; j <= tbl->mask; j++) {
            var_t *u = &tbl->array[2*j];

            if (var_isnil(u[0]) || var_isnil(u[1]))
                continue;

            for (i = var_hash(u[0]);; i = tbl_next(i)) {
                hash_t mi = i & mask;
                var_t *v = &w[2*mi];

                if (var_isnil(v[0])) {
                    v[0] = u[0];
                    v[1] = u[1];
                    break;
                }
            }
        }

        vdealloc(tbl->array, 2*(tbl->mask+1) * sizeof(var_t));
        tbl->array = w;
        tbl->nils = 0;
        tbl->mask = mask;
    }

    return 0;
}


// Preallocates memory, using the lazy tbl_create function
// is preferred, however this can be used to avoid checking
// for errors during insertions.
tbl_t *tbl_createlist(len_t size) {
    tbl_t *tbl = tbl_create(size);
    if (iserr(tbl))
        return tbl;

    onerr(err, tbl_realizevars(tbl), {
        return err;
    })

    return tbl;
}

tbl_t *tbl_createhash(len_t size) {
    tbl_t *tbl = tbl_create(size);
    if (iserr(tbl))
        return tbl;

    onerr(err, tbl_realizekeys(tbl), {
        return err;
    })

    return tbl;
}
    

// Inserts a value in the table with the given key
// without decending down the tail chain
static err_t tbl_insertnil(tbl_t *tbl, var_t key, var_t val) {
    hash_t i, hash = var_hash(key);

    if (tbl->stride < 2) {
        if (!num_ishash(key, hash) || hash >= tbl->len)
            return 0;

        if (hash == tbl->len - 1) {
            if (tbl->stride != 0)
                var_dec(tbl->array[hash]);

            tbl->len--;
            return 0;
        }

        onerr(err, tbl_realizekeys(tbl), {
            return err;
        })
    }

    for (i = hash;; i = tbl_next(i)) {
        hash_t mi = i & tbl->mask;
        var_t *v = &tbl->array[2*mi];

        if (var_isnil(v[0]))
            return 0;

        if (var_equals(key, v[0])) {
            if (!var_isnil(v[1])) {
                var_dec(v[1]);
                v[1] = vnil;
                tbl->nils++;
                tbl->len--;
            }

            return 0;
        }
    }
}


static err_t tbl_insertval(tbl_t *tbl, var_t key, var_t val) {
    hash_t i, hash = var_hash(key);

    if (tbl_ncap(tbl->nils+tbl->len + 1) > tbl->mask + 1) {
        onerr(err, tbl_resize(tbl, tbl->len + 1), {
            return err;
        })
    }

    if (tbl->stride < 2) {
        if (num_ishash(key, hash)) {
            if (hash == tbl->len) {
                if (tbl->stride == 0) {
                    if (var_isnum(val)) {
                        if (tbl->len == 0)
                            tbl->offset = num_hash(val);

                        if (num_ishash(val, hash + tbl->offset)) {
                            tbl->len++;
                            return 0;
                        }
                    }

                    onerr(err, tbl_realizevars(tbl), {
                        return err;
                    })
                }

                tbl->array[hash] = val;
                tbl->len++;
                return 0;
            } else if (hash < tbl->len) {
                if (tbl->stride == 0) {
                    if (num_ishash(val, hash + tbl->offset))
                        return 0;

                    onerr(err, tbl_realizevars(tbl), {
                        return err;
                    })
                }

                var_dec(tbl->array[hash]);
                tbl->array[hash] = val;
                return 0;
            }
        }

        onerr(err, tbl_realizekeys(tbl), {
            return err;
        })
    }

    for (i = hash;; i = tbl_next(i)) {
        hash_t mi = i & tbl->mask;
        var_t *v = &tbl->array[2*mi];

        if (var_isnil(v[0])) {
            v[0] = key;
            v[1] = val;
            tbl->len++;
            return 0;
        }

        if (var_equals(key, v[0])) {
            if (var_isnil(v[1])) {
                v[1] = val;
                tbl->nils--;
                tbl->len++;
            } else {
                var_dec(v[1]);
                v[1] = val;
            }

            return 0;
        }
    }
}
    

err_t tbl_insert(tbl_t *tbl, var_t key, var_t val) {
    tbl = tbl_writep(tbl);
    if (iserr(tbl))
        return tbl;

    if (var_isnil(key))
        return 0;

    if (var_isnil(val))
        return tbl_insertnil(tbl, key, val);
    else
        return tbl_insertval(tbl, key, val);
}


// Sets the next index in the table with the value
err_t tbl_add(tbl_t *tbl, var_t val) {
    return tbl_insert(tbl, vnum(tbl->len), val);
}


// Recursively assigns a value in the table with the given key
// decends down the tail chain until its found
static err_t tbl_assignnil(tbl_t *tbl, var_t key, var_t val) {
    hash_t i, hash = var_hash(key);

    for (; tbl; tbl = tbl->tail) {
        if (tbl_isro(tbl))
            break;

        if (tbl->stride < 2) {
            if (!num_ishash(key, hash) || hash >= tbl->len)
                continue;

            if (hash == tbl->len - 1) {
                if (tbl->stride != 0)
                    var_dec(tbl->array[hash]);

                tbl->len--;
                return 0;
            }

            onerr(err, tbl_realizekeys(tbl), {
                return err;
            })
        }

        for (i = hash;; i = tbl_next(i)) {
            hash_t mi = i & tbl->mask;
            var_t *v = &tbl->array[2*mi];

            if (var_isnil(v[0])) 
                break;

            if (var_equals(key, v[0])) {
                if (var_isnil(v[1]))
                    break;

                var_dec(v[1]);
                v[1] = vnil;
                tbl->nils++;
                tbl->len--;
                return 0;
            }
        }
    }

    return 0;
}


static err_t tbl_assignval(tbl_t *head, var_t key, var_t val) {
    hash_t i, hash = var_hash(key);
    tbl_t *tbl = head;

    for (; tbl; tbl = tbl->tail) {
        if (tbl_isro(tbl))
            break;

        if (tbl->stride < 2) {
            if (!num_ishash(key, hash) || hash >= tbl->len)
                continue;

            if (tbl->stride == 0) {
                if (num_ishash(val, hash + tbl->offset))
                    return 0;

                onerr(err, tbl_realizevars(tbl), {
                    return err;
                })
            }

            var_dec(tbl->array[hash]);
            tbl->array[hash] = val;
            return 0;
        }

        for (i = hash;; i = tbl_next(i)) {
            hash_t mi = i & tbl->mask;
            var_t *v = &tbl->array[2*mi];

            if (var_isnil(v[0]))
                break;

            if (var_equals(key, v[0])) {
                if (var_isnil(v[1]))
                    break;

                var_dec(v[1]);
                v[1] = val;
                return 0;
            }
        }
    }


    tbl = tbl_writep(head);
    if (iserr(tbl))
        return tbl;

    if (tbl_ncap(tbl->len+tbl->nils + 1) > tbl->mask + 1) {
        onerr(err, tbl_resize(tbl, tbl->len+1), {
            return err;
        })
    }

    if (tbl->stride < 2) {
        if (num_ishash(key, hash) && hash == tbl->len) {
            if (tbl->stride == 0) {
                if (var_isnum(val)) {
                    if (tbl->len == 0)
                        tbl->offset = num_hash(val);

                    if (num_ishash(val, hash + tbl->offset)) {
                        tbl->len++;
                        return 0;
                    }
                }

                onerr(err, tbl_realizevars(tbl), {
                    return err;
                })
            }

            tbl->array[hash] = val;
            tbl->len++;
            return 0;
        }

        onerr(err, tbl_realizekeys(tbl), {
            return err;
        })
    }

    for (i = hash;; i = tbl_next(i)) {
        hash_t mi = i & tbl->mask;
        var_t *v = &tbl->array[2*mi];

        if (var_isnil(v[0])) {
            v[0] = key;
            v[1] = val;
            tbl->len++;
            return 0;
        }

        if (var_equals(key, v[0]) && var_isnil(v[1])) {
            v[1] = val;
            tbl->nils--;
            tbl->len++;
            return 0;
        }
    }
}


err_t tbl_assign(tbl_t *tbl, var_t key, var_t val) {
    if (var_isnil(key))
        return 0;

    if (var_isnil(val))
        return tbl_assignnil(tbl, key, val);
    else
        return tbl_assignval(tbl, key, val);
}



// Performs iteration on a table
v_fn var_t tbl_0_iteration(tbl_t *args, tbl_t *scope) {
    tbl_t *tbl = tbl_lookup(scope, vnum(0)).tbl;
    tbl_t *ret = tbl_lookup(scope, vnum(1)).tbl;
    int i = tbl_lookup(scope, vnum(2)).data;

    if (i >= tbl->len)
        return vnil;

    tbl_insert(ret, vnum(0), vnum(tbl->offset + i));
    tbl_insert(ret, vnum(1), vnum(i));
    tbl_insert(ret, vnum(2), vnum(i));

    i += 1;
    tbl_insert(scope, vnum(2), vraw(i));

    return vtbl(ret);
}

v_fn var_t tbl_1_iteration(tbl_t *args, tbl_t *scope) {
    tbl_t *tbl = tbl_lookup(scope, vnum(0)).tbl;
    tbl_t *ret = tbl_lookup(scope, vnum(1)).tbl;
    int i = tbl_lookup(scope, vnum(2)).data;

    if (i >= tbl->len)
        return vnil;

    tbl_insert(ret, vnum(0), tbl->array[i]);
    tbl_insert(ret, vnum(1), vnum(i));
    tbl_insert(ret, vnum(2), vnum(i));

    i += 1;
    tbl_insert(scope, vnum(2), vraw(i));

    return vtbl(ret);
}

v_fn var_t tbl_2_iteration(tbl_t *args, tbl_t *scope) {
    tbl_t *tbl = tbl_lookup(scope, vnum(0)).tbl;
    tbl_t *ret = tbl_lookup(scope, vnum(1)).tbl;
    int i = tbl_lookup(scope, vnum(2)).data;
    int j = tbl_lookup(scope, vnum(3)).data;
    var_t k, v;

    if (i >= tbl->len)
        return vnil;

    do {
        k = tbl->array[2*j  ];
        v = tbl->array[2*j+1];

        j += 1;
    } while (var_isnil(k) || var_isnil(v));

    tbl_insert(ret, vnum(0), v);
    tbl_insert(ret, vnum(1), k);
    tbl_insert(ret, vnum(2), vnum(i));

    i += 1;
    tbl_insert(scope, vnum(2), vraw(i));
    tbl_insert(scope, vnum(3), vraw(j));

    return vtbl(ret);
}

var_t tbl_iter(var_t v) {
    static bfn_t * const tbl_iters[3] = {
        tbl_0_iteration,
        tbl_1_iteration,
        tbl_2_iteration
    };

    tbl_t *tbl = tbl_readp(v.tbl);

    tbl_t *scope = tbl_createlist(3);
    if (iserr(scope))
        return verr(scope);

    tbl_t *ret = tbl_createlist((tbl->stride < 2) ? 3 : 4);
    if (iserr(ret))
        return verr(ret);

    tbl_insert(scope, vnum(0), vtbl(tbl));
    tbl_insert(scope, vnum(1), vtbl(ret));

    return vbfn(tbl_iters[tbl->stride], scope);
}


// Returns a string representation of the table
static var_t tbl_reprcleanup(var_t *rs, int count, int cap, var_t err) {
    int i;

    for (i = 0; i < count; i++)
        var_dec(rs[i]);

    vdealloc(rs, cap);

    return err;
}

var_t tbl_repr(var_t v) {
    tbl_t *tbl = tbl_readp(v.tbl);
    int cap = 2*tbl->len * sizeof(var_t);

    var_t *rs = valloc(cap);
    if (iserr(rs))
        return verr(rs);

    int size = 2;
    int i = 0;

    tbl_for (k, v, tbl, {
        var_t n = var_repr(k);
        if (iserr(n))
            return tbl_reprcleanup(rs, 2*i, cap, n);

        rs[2*i] = n;
        size += rs[2*i].len;

        n = var_repr(v);
        if (iserr(n))
            return tbl_reprcleanup(rs, 2*i+1, cap, n);

        rs[2*i+1] = n;
        size += rs[2*i+1].len;

        size += (i == tbl->len-1) ? 2 : 4;
        i += 1;
    })

    if (size > VMAXLEN)
        return tbl_reprcleanup(rs, 2*tbl->len, cap, verr(err_len()));

    str_t *out = str_create(size);
    str_t *res = out;
    if (iserr(out))
        return tbl_reprcleanup(rs, 2*tbl->len, cap, verr(out));


    *res++ = '[';

    for (i=0; i < tbl->len; i++) {
        memcpy(res, var_str(rs[2*i]), rs[2*i].len);
        res += rs[2*i].len;
        var_dec(rs[2*i]);

        *res++ = ':';
        *res++ = ' ';

        memcpy(res, var_str(rs[2*i+1]), rs[2*i+1].len);
        res += rs[2*i+1].len;
        var_dec(rs[2*i+1]);

        if (i != tbl->len-1) {
            *res++ = ',';
            *res++ = ' ';
        }
    }

    *res++ = ']';

    vdealloc(rs, cap);


    return vstr(out, 0, size);
}
