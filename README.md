# C-Watcher

A C implementation of the observer design pattern with focus on lightweight RAM usage through the [Array of Structs](https://en.wikipedia.org/wiki/AoS_and_SoA) approach.

## Building

The project includes both `scons` and `CMake` ([ESP-IDF](https://github.com/espressif/esp-idf.git)) flavor) for compilation, but it is overall composed of 1 source and two header files; it can be easily included in any C application.

## Testing

`scons test`

## Example

```c
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include "watcher.h"


static void callback(void *old_value, const void *new_value, watcher_size_t size, void *user_ptr, void *arg) {
    int old = *(int *)old_value;
    int new = *(int *)new_value;
    printf("%i to %i, size %i, user pointer %p and arg %p\n", old, new, size, user_ptr, arg);
}


int main(void) {
    watcher_t watcher          = {0};
    int       variable         = 0;
    int       delayed_variable = 0;

    WATCHER_INIT_STD(&watcher, (void *)(uintptr_t)1);
    WATCHER_ADD_ENTRY(&watcher, &variable, callback, (void *)(uintptr_t)2);
    WATCHER_ADD_ENTRY_DELAYED(&watcher, &delayed_variable, callback, (void *)(uintptr_t)3, 2);

    watcher_watch(&watcher, time(NULL));
    variable = 4;
    delayed_variable = 5;
    watcher_watch(&watcher, time(NULL));
    sleep(3);
    watcher_watch(&watcher, time(NULL));
}
```

## TODO

 - Return indexes and expose trigger/defuse api
