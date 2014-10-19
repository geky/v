/* 
 *  Error handling
 */

#ifndef V_ERR
#define V_ERR

#include "var.h"


// Error type as table
// lowest bit set if error
typedef void *err_t;


// Error detection functions
#define err_select(e, err, a, b)                                            \
    __builtin_choose_expr(__builtin_types_compatible_p(typeof(err), var_t), \
    ({ var_t e = __builtin_choose_expr(__builtin_types_compatible_p(        \
                                       typeof(err), var_t), err, vnil);     \
       (a); }),                                                             \
    ({ void *e = __builtin_choose_expr(__builtin_types_compatible_p(        \
                                       typeof(err), var_t), 0, err);        \
       (b); }))

#define iserr(err)                                                  \
    err_select(_e, err, __builtin_expect(var_iserr(_e), 0),       \
                        __builtin_expect(1 & (uint32_t)_e, 0)) 

#define onerr(e, err, block) {  \
    typeof(err) e = (err);      \
                                \
    if (iserr(e)) {             \
        {block};                \
    }                           \
}


// Errors used in core V implementation
__attribute__((const)) err_t err_nomem(void);
__attribute__((const)) err_t err_len(void);
__attribute__((const)) err_t err_ro(void);
__attribute__((const)) err_t err_parse(void);

#endif
