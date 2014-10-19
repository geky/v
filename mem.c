#include "mem.h"
#include "var.h"
#include "err.h"

#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

// Manual memory management
// Currently just a wrapper over malloc and free
// Garuntees 8 byte alignment
void *valloc(size_t size) {
    void *m;
    
    if (size == 0) return 0;

    m = (void *)malloc(size);

    if (m == 0)
        return err_nomem();

    assert(sizeof m == sizeof(uint32_t)); // garuntee address width
    assert((0x7 & (uint32_t)m) == 0); // garuntee alignment

    return m;
}

void *vrealloc(void *m, size_t prev, size_t size) {
    m = realloc(m, size);

    if (m == 0) // TODO clean both?
        return err_nomem();

    assert(sizeof m == sizeof(uint32_t)); // garuntee address width
    assert((0x7 & (uint32_t)m) == 0); // garuntee alignment

    return m;
}

void vdealloc(void *m, size_t size) {
//    free(m);
}


// Garbage collected memory based on reference counting
// Each block of memory prefixed with ref_t reference
// count. Deallocated immediately when ref hits zero.
// It is up to the user to avoid cyclic dependencies.
void *vref_alloc(size_t size) {
    ref_t *m = valloc(sizeof(ref_t) + size);

    if (iserr(m))
        return m;

    // start with a count of 1
    *m = 1;
    m += 1;

    return m;
}

void vref_dealloc(void *m, size_t size) {
    vdealloc(m, sizeof(ref_t) + size);
}
