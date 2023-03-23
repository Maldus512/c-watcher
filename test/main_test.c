#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "watcher.h"


/* These functions will be used to initialize
   and clean resources up after each test run */
int setup(void **state) {
    (void)state;
    return 0;
}

int teardown(void **state) {
    (void)state;
    return 0;
}


typedef struct __attribute__((packed)) {
    void   *old;
    void   *current;
    uint8_t size;
    void (*cb)(void *, void *);
    void         *data;
    unsigned long timestamp;
    uint16_t      delay;
    uint8_t       moved;
} old_watcher_entry_t;

static void watcher_test(void **state) {
    (void)state;

    printf("%lu - %lu\n", sizeof(watcher_entry_t), sizeof(old_watcher_entry_t));
}


int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(watcher_test),
    };

    /* If setup and teardown functions are not
       needed, then NULL may be passed instead */

    int count_fail_tests = cmocka_run_group_tests(tests, setup, teardown);

    return count_fail_tests;
}
