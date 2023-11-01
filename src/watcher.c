#include <string.h>
#include "watcher.h"
#include "_timecheck.h"
#ifndef C_WATCHER_CUSTOM_MEM_INCLUDE
#define C_WATCHER_CUSTOM_MEM_INCLUDE <stdlib.h>
#endif
#include C_WATCHER_CUSTOM_MEM_INCLUDE

#ifndef C_WATCHER_CUSTOM_MEM_REALLOC
#define C_WATCHER_CUSTOM_MEM_REALLOC realloc
#endif

#ifndef C_WATCHER_CUSTOM_MEM_FREE
#define C_WATCHER_CUSTOM_MEM_FREE free
#endif

#define VECTOR_INIT(name)                                                                                              \
    {                                                                                                                  \
        name.items    = NULL;                                                                                          \
        name.num      = 0;                                                                                             \
        name.capacity = 0;                                                                                             \
    }

#define VECTOR_INIT_STATIC(name, memory, cap)                                                                          \
    {                                                                                                                  \
        name.items    = memory;                                                                                        \
        name.num      = 0;                                                                                             \
        name.capacity = cap;                                                                                           \
    }

#define VECTOR_GROW(name)                                                                                              \
    {                                                                                                                  \
        void *new_entries = C_WATCHER_CUSTOM_MEM_REALLOC(name.items, (name.capacity + 1) * sizeof(name.items[0]));     \
        if (new_entries != NULL) {                                                                                     \
            name.items = new_entries;                                                                                  \
            name.capacity++;                                                                                           \
        }                                                                                                              \
    }

#define VECTOR_APPEND(name, item)                                                                                      \
    {                                                                                                                  \
        if (!VECTOR_FULL(name)) {                                                                                      \
            name.items[name.num] = item;                                                                               \
            name.num++;                                                                                                \
        }                                                                                                              \
    }

#define VECTOR_FULL(name)            (name.num == name.capacity)
#define VECTOR_AT_MAX_CAPACITY(name) (name.capacity == WATCHER_MAX_ENTRIES)

#define NULL_INDEX 0xFFFF


void watcher_init(watcher_t *watcher, void *user_ptr) {
    VECTOR_INIT(watcher->entries);
    VECTOR_INIT(watcher->callbacks);
    VECTOR_INIT(watcher->args);
    VECTOR_INIT(watcher->delays);
    VECTOR_INIT(watcher->timestamps);
    watcher->user_ptr = user_ptr;
    watcher->growable = 1;
}


void watcher_init_static(watcher_t *watcher, watcher_entry_t *entries, uint16_t entries_capacity,
                         watcher_callback_t *callbacks, uint16_t callbacks_capacity, void **args,
                         uint16_t args_capacity, unsigned long *delays, uint16_t delays_capacity,
                         unsigned long *timestamps, uint16_t timestamps_capacity, void *user_ptr) {
    VECTOR_INIT_STATIC(watcher->entries, entries, entries_capacity);
    VECTOR_INIT_STATIC(watcher->callbacks, callbacks, callbacks_capacity);
    VECTOR_INIT_STATIC(watcher->args, args, args_capacity);
    VECTOR_INIT_STATIC(watcher->delays, delays, delays_capacity);
    VECTOR_INIT_STATIC(watcher->timestamps, timestamps, timestamps_capacity);
    watcher->user_ptr = user_ptr;
    watcher->growable = 0;
}


void watcher_destroy(watcher_t *watcher) {
    C_WATCHER_CUSTOM_MEM_FREE(watcher->entries.items);
    C_WATCHER_CUSTOM_MEM_FREE(watcher->callbacks.items);
    C_WATCHER_CUSTOM_MEM_FREE(watcher->args.items);
    C_WATCHER_CUSTOM_MEM_FREE(watcher->delays.items);
    C_WATCHER_CUSTOM_MEM_FREE(watcher->timestamps.items);
}


int16_t watcher_add_entry(watcher_t *watcher, const void *pointer, uint16_t size, watcher_callback_t callback,
                          void *arg) {
    void *old_buffer = C_WATCHER_CUSTOM_MEM_REALLOC(NULL, size);
    if (old_buffer == NULL) {
        return -1;
    } else {
        return watcher_add_entry_static(watcher, pointer, size, callback, arg, old_buffer);
    }
}

int16_t watcher_add_entry_delayed(watcher_t *watcher, const void *pointer, uint16_t size, watcher_callback_t callback,
                                  void *arg, unsigned long delay) {
    void *old_buffer = C_WATCHER_CUSTOM_MEM_REALLOC(NULL, size);
    if (old_buffer == NULL) {
        return -1;
    } else {
        return watcher_add_entry_delayed_static(watcher, pointer, size, callback, arg, delay, old_buffer);
    }
}


int16_t watcher_add_entry_static(watcher_t *watcher, const void *pointer, uint16_t size, watcher_callback_t callback,
                                 void *arg, void *old_buffer) {
#define GROW_OR_FAIL(field)                                                                                            \
    if (VECTOR_FULL(watcher->field)) {                                                                                 \
        if (watcher->growable && !VECTOR_AT_MAX_CAPACITY(watcher->field)) {                                            \
            VECTOR_GROW(watcher->field);                                                                               \
        } else {                                                                                                       \
            return -1;                                                                                                 \
        }                                                                                                              \
    }

    GROW_OR_FAIL(entries);

    uint16_t i              = 0;
    uint8_t  callback_found = 0;
    uint16_t callback_index = 0;
    for (i = 0; i < watcher->callbacks.num; i++) {
        if (watcher->callbacks.items[i] == callback) {
            callback_found = 1;
            callback_index = i;
            break;
        }
    }

    if (!callback_found) {
        GROW_OR_FAIL(callbacks);
        callback_index = watcher->callbacks.num;
        VECTOR_APPEND(watcher->callbacks, callback);
    }

    uint16_t arg_index = NULL_INDEX;
    if (arg != NULL) {
        uint8_t arg_found = 0;
        for (i = 0; i < watcher->args.num; i++) {
            if (watcher->args.items[i] == arg) {
                arg_found = 1;
                arg_index = i;
                break;
            }
        }

        if (!arg_found) {
            GROW_OR_FAIL(args);
            arg_index = watcher->args.num;
            VECTOR_APPEND(watcher->args, arg);
        }
    }

    memcpy(old_buffer, pointer, size);
    watcher_entry_t entry = {
        .memory          = pointer,
        .old_buffer      = old_buffer,
        .size            = size,
        .callback_index  = callback_index,
        .arg_index       = arg_index,
        .delay_index     = NULL_INDEX,
        .timestamp_index = 0,
    };
    GROW_OR_FAIL(entries);
    int16_t result = watcher->entries.num;
    VECTOR_APPEND(watcher->entries, entry);

    return result;

#undef GROW_OR_FAIL
}


int16_t watcher_add_entry_delayed_static(watcher_t *watcher, const void *pointer, uint16_t size,
                                         watcher_callback_t callback, void *arg, unsigned long delay,
                                         void *old_buffer) {
    int16_t entry_index = watcher_add_entry_static(watcher, pointer, size, callback, arg, old_buffer);
    watcher_set_delayed(watcher, entry_index, delay);
    return entry_index;
}


int16_t watcher_set_delayed(watcher_t *watcher, int16_t entry_index, unsigned long delay) {
#define GROW_OR_FAIL(field)                                                                                            \
    if (VECTOR_FULL(watcher->field)) {                                                                                 \
        if (watcher->growable && !VECTOR_AT_MAX_CAPACITY(watcher->field)) {                                            \
            VECTOR_GROW(watcher->field);                                                                               \
        } else {                                                                                                       \
            return -1;                                                                                                 \
        }                                                                                                              \
    }

    // Invalid index
    if (entry_index >= watcher->entries.num || entry_index < 0) {
        return -1;
    }

    // Entry is already delayed
    if (watcher->entries.items[entry_index].delay_index != NULL_INDEX) {
        return entry_index;
    }

    uint16_t i           = 0;
    uint8_t  delay_found = 0;
    uint16_t delay_index = 0;
    for (i = 0; i < watcher->delays.num; i++) {
        if (watcher->delays.items[i] == delay) {
            delay_found = 1;
            delay_index = i;
            break;
        }
    }

    if (!delay_found) {
        if (VECTOR_FULL(watcher->delays)) {
            if (watcher->growable && !VECTOR_AT_MAX_CAPACITY(watcher->delays)) {
                VECTOR_GROW(watcher->delays);
            } else {
                return -1;
            }
        }
        delay_index = watcher->delays.num;
        VECTOR_APPEND(watcher->delays, delay);
    }

    GROW_OR_FAIL(timestamps);
    uint16_t timestamp_index = watcher->timestamps.num;
    VECTOR_APPEND(watcher->timestamps, 0);

    watcher->entries.items[entry_index].delay_index     = delay_index;
    watcher->entries.items[entry_index].timestamp_index = timestamp_index;
    return entry_index;

#undef GROW_OR_FAIL
}


uint16_t watcher_watch(watcher_t *watcher, unsigned long timestamp) {
    uint16_t count = 0;
    uint16_t i     = 0;

    for (i = 0; i < watcher->entries.num; i++) {
        watcher_entry_t *entry = &watcher->entries.items[i];

        if (memcmp(entry->memory, entry->old_buffer, entry->size)) {
            if (entry->delay_index != NULL_INDEX) {
                // Delayed logic
                if (entry->timestamp_triggered) {
                    if (is_expired(watcher->timestamps.items[entry->timestamp_index], timestamp,
                                   watcher->delays.items[entry->delay_index])) {
                        watcher_trigger_entry(watcher, i);
                        count++;
                    }
                } else {
                    entry->timestamp_triggered                        = 1;
                    watcher->timestamps.items[entry->timestamp_index] = timestamp;
                }
            } else {
                // Immediate logic
                watcher_trigger_entry(watcher, i);
                count++;
            }
        } else {
            entry->timestamp_triggered = 0;
        }
    }

    return count;
}


void watcher_trigger_entry(watcher_t *watcher, int16_t entry_index) {
    // Invalid index
    if (entry_index >= watcher->entries.num || entry_index < 0) {
        return;
    }
    watcher_entry_t *entry = &watcher->entries.items[entry_index];

    void *arg = NULL;

    if (entry->arg_index != NULL_INDEX) {
        arg = watcher->args.items[entry->arg_index];
    }

    watcher->callbacks.items[entry->callback_index](entry->old_buffer, entry->memory, entry->size, watcher->user_ptr,
                                                    arg);
    memcpy(entry->old_buffer, entry->memory, entry->size);
    entry->timestamp_triggered = 0;
}


void watcher_trigger_all(watcher_t *watcher) {
    uint16_t i = 0;
    for (i = 0; i < watcher->entries.num; i++) {
        watcher_trigger_entry(watcher, i);
    }
}
