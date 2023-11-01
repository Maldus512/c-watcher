#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "watcher.h"

static char      var1 = 0;
static int       var2 = 0;
static long long var3 = 0;
static struct {
    char      a;
    int       b;
    long      c;
    long long d;
} var4                    = {0};
static uint64_t array[10] = {0};

static int   cbtest       = 0;
static void *entries_arg  = (void *)0x1234;
static void *user_pointer = (void *)0xDEADBEEF;

static void callback(void *old_value, const void *new_value, uint16_t size, void *user_ptr, void *arg) {
    (void)old_value;
    (void)new_value;
    (void)size;
    assert_ptr_equal(arg, entries_arg);
    assert_ptr_equal(user_pointer, user_ptr);
    cbtest++;
}


static void null_arg_callback(void *old_value, const void *new_value, uint16_t size, void *user_ptr, void *arg) {
    (void)old_value;
    (void)new_value;
    (void)size;
    assert_ptr_equal(arg, NULL);
    assert_ptr_equal(user_pointer, user_ptr);
    cbtest++;
}


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


static void watcher_test(void **state) {
    (void)state;
    cbtest = 0;

    watcher_t watcher;
    WATCHER_INIT_STD(&watcher, user_pointer);

    assert_true(WATCHER_ADD_ENTRY(&watcher, &var1, callback, entries_arg) >= 0);
    assert_true(WATCHER_ADD_ENTRY(&watcher, &var2, callback, entries_arg) >= 0);
    assert_true(WATCHER_ADD_ENTRY(&watcher, &var3, callback, entries_arg) >= 0);
    assert_true(WATCHER_ADD_ENTRY(&watcher, &var4, callback, entries_arg) >= 0);
    assert_true(WATCHER_ADD_ENTRY(&watcher, &array, null_arg_callback, NULL) >= 0);

    assert_false(watcher_watch(&watcher, 0));
    var2++;
    assert_true(watcher_watch(&watcher, 0));
    assert_int_equal(1, cbtest);

    assert_false(watcher_watch(&watcher, 0));
    array[5]++;
    assert_true(watcher_watch(&watcher, 0));
    assert_int_equal(2, cbtest);

    watcher_destroy(&watcher);
}


void watcher_delayed_test(void **state) {
    (void)state;
    cbtest = 0;

    watcher_t watcher;
    WATCHER_INIT_STD(&watcher, user_pointer);

    assert_true(WATCHER_ADD_ENTRY_DELAYED(&watcher, &var1, callback, entries_arg, 5000) >= 0);

    var1++;
    assert_false(watcher_watch(&watcher, 0));
    assert_int_equal(0, cbtest);
    assert_false(watcher_watch(&watcher, 1000));
    assert_int_equal(0, cbtest);
    assert_false(watcher_watch(&watcher, 4000));
    assert_int_equal(0, cbtest);
    assert_true(watcher_watch(&watcher, 6000));
    assert_int_equal(1, cbtest);
    assert_false(watcher_watch(&watcher, 11000));
    assert_int_equal(1, cbtest);

    watcher_destroy(&watcher);
}



int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(watcher_test),
        cmocka_unit_test(watcher_delayed_test),
    };

    /* If setup and teardown functions are not
       needed, then NULL may be passed instead */

    int count_fail_tests = cmocka_run_group_tests(tests, setup, teardown);

    return count_fail_tests;
}
