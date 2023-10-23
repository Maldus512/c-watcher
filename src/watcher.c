#include <string.h>
#include "watcher.h"
#include "_timecheck.h"


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
        typeof(name.items) new_entries = watcher->fn_realloc(name.items, (name.capacity + 1) * sizeof(name.items[0])); \
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

#define GROW_OR_FAIL(field)                                                                                            \
    if (VECTOR_FULL(watcher->field)) {                                                                                 \
        if (watcher->fn_realloc != NULL && !VECTOR_AT_MAX_CAPACITY(watcher->field)) {                                  \
            VECTOR_GROW(watcher->field);                                                                               \
        } else {                                                                                                       \
            return WATCHER_RESULT_STATIC_OVERFLOW;                                                                     \
        }                                                                                                              \
    }

#define VECTOR_FULL(name)               (name.num == name.capacity)
#define VECTOR_AT_MAX_CAPACITY(name)    (name.capacity == C_WATCHER_MAX_ENTRIES)
#define FITS_IN_POINTER(size)           ((size) < sizeof(void *))
#define ENTRY_GET_OLD_BUFFER_POINTER(e) (FITS_IN_POINTER((e).size) ? &(e).old_buffer : (e).old_buffer)

// TODO: there is no need for the null index
#define NULL_INDEX ((watcher_size_t)-1)


static watcher_result_t add_callback(watcher_t *watcher, watcher_callback_t callback, watcher_size_t *callback_index);
static watcher_result_t add_arg(watcher_t *watcher, void *arg, watcher_size_t *arg_index);
static watcher_result_t _add_entry_static(watcher_t *watcher, const void *pointer, watcher_size_t size,
                                          watcher_callback_t callback, void *arg, void *old_buffer,
                                          unsigned long delay);


watcher_result_t watcher_init(watcher_t *watcher, void *user_ptr, void *(*fn_realloc)(void *, size_t),
                              void (*fn_free)(void *)) {
    if (fn_realloc == NULL || fn_free == NULL) {
        return WATCHER_RESULT_INVALID_ARGS;
    }

    watcher->fn_realloc = fn_realloc;
    watcher->fn_free    = fn_free;

    VECTOR_INIT(watcher->entries);
    VECTOR_INIT(watcher->callbacks);
    VECTOR_INIT(watcher->args);
    VECTOR_INIT(watcher->delays);
    VECTOR_INIT(watcher->debounces);

    watcher->user_ptr = user_ptr;

    return WATCHER_RESULT_OK;
}


void watcher_init_static(watcher_t *watcher, watcher_entry_t *entries, watcher_size_t entries_capacity,
                         watcher_callback_t *callbacks, watcher_size_t callbacks_capacity, void **args,
                         watcher_size_t args_capacity, unsigned long *delays, watcher_size_t delays_capacity,
                         watcher_debounce_data_t *debounces, watcher_size_t debounces_capacity, void *user_ptr) {
    VECTOR_INIT_STATIC(watcher->entries, entries, entries_capacity);
    VECTOR_INIT_STATIC(watcher->callbacks, callbacks, callbacks_capacity);
    VECTOR_INIT_STATIC(watcher->args, args, args_capacity);
    VECTOR_INIT_STATIC(watcher->delays, delays, delays_capacity);
    VECTOR_INIT_STATIC(watcher->debounces, debounces, debounces_capacity);

    watcher->user_ptr   = user_ptr;
    watcher->fn_realloc = NULL;
    watcher->fn_free    = NULL;
}


void watcher_destroy(watcher_t *watcher) {
    if (watcher->fn_free != NULL) {
        watcher->fn_free(watcher->entries.items);
        watcher->fn_free(watcher->callbacks.items);
        watcher->fn_free(watcher->args.items);
        watcher->fn_free(watcher->delays.items);
        watcher->fn_free(watcher->debounces.items);
    }
}


watcher_result_t watcher_add_entry(watcher_t *watcher, const void *pointer, watcher_size_t size,
                                   watcher_callback_t callback, void *arg) {
    void *old_buffer = NULL;
    if (FITS_IN_POINTER(size)) {
        // If the watched region fits in a pointer we can use the old_buffer field itself
    } else {
        old_buffer = watcher->fn_realloc(NULL, size);
        if (old_buffer == NULL) {
            return WATCHER_RESULT_ALLOC_ERROR;
        }
    }
    return _add_entry_static(watcher, pointer, size, callback, arg, old_buffer, 0);
}

watcher_result_t watcher_add_entry_delayed(watcher_t *watcher, const void *pointer, watcher_size_t size,
                                           watcher_callback_t callback, void *arg, unsigned long delay) {
    void *old_buffer = NULL;
    if (FITS_IN_POINTER(size)) {
        // If the watched region fits in a pointer we can use the old_buffer field itself
    } else {
        old_buffer = watcher->fn_realloc(NULL, size);
        if (old_buffer == NULL) {
            return WATCHER_RESULT_ALLOC_ERROR;
        }
    }
    return watcher_add_entry_delayed_static(watcher, pointer, size, callback, arg, delay, old_buffer);
}


watcher_result_t watcher_add_entry_delayed_static(watcher_t *watcher, const void *pointer, watcher_size_t size,
                                                  watcher_callback_t callback, void *arg, unsigned long delay,
                                                  void *old_buffer) {
    return _add_entry_static(watcher, pointer, size, callback, arg, old_buffer, delay);
}


watcher_size_t watcher_watch(watcher_t *watcher, unsigned long timestamp) {
    watcher_size_t count = 0;
    watcher_size_t i     = 0;

    for (i = 0; i < watcher->entries.num; i++) {
        watcher_entry_t *entry      = &watcher->entries.items[i];
        void            *old_buffer = ENTRY_GET_OLD_BUFFER_POINTER(*entry);

        if (entry->debounce_index != NULL_INDEX) {
            // Delayed logic
            watcher_debounce_data_t *pdebounce = &watcher->debounces.items[entry->debounce_index];

            if (pdebounce->triggered) {
                // Debounce was triggered but there is a new change: reset the counter
                if (memcmp(entry->watched, old_buffer, entry->size)) {
                    pdebounce->timestamp = timestamp;
                    memcpy(old_buffer, entry->watched, entry->size);
                }

                if (is_expired(pdebounce->timestamp, timestamp, watcher->delays.items[pdebounce->delay_index])) {
                    watcher_trigger_entry(watcher, i);
                    count++;
                }
            } else if (memcmp(entry->watched, old_buffer, entry->size)) {
                pdebounce->triggered = 1;
                pdebounce->timestamp = timestamp;
                memcpy(old_buffer, entry->watched, entry->size);
            }
        } else if (memcmp(entry->watched, old_buffer, entry->size)) {
            // Immediate logic
            watcher_trigger_entry(watcher, i);
            count++;
        }
    }

    return count;
}


void watcher_trigger_entry(watcher_t *watcher, int16_t entry_index) {
    // Invalid index
    if (entry_index >= watcher->entries.num || entry_index < 0) {
        return;
    }
    watcher_entry_t *entry      = &watcher->entries.items[entry_index];
    void            *old_buffer = ENTRY_GET_OLD_BUFFER_POINTER(*entry);

    void *arg = NULL;

    if (entry->arg_index != NULL_INDEX) {
        arg = watcher->args.items[entry->arg_index];
    }

    watcher->callbacks.items[entry->callback_index](old_buffer, entry->watched, entry->size, watcher->user_ptr, arg);
    memcpy(old_buffer, entry->watched, entry->size);

    if (entry->debounce_index != NULL_INDEX) {
        watcher_debounce_data_t *pdebounce = &watcher->debounces.items[entry->debounce_index];
        pdebounce->triggered               = 0;
    }
}


void watcher_trigger_all(watcher_t *watcher) {
    watcher_size_t i = 0;
    for (i = 0; i < watcher->entries.num; i++) {
        watcher_trigger_entry(watcher, i);
    }
}


static watcher_result_t add_callback(watcher_t *watcher, watcher_callback_t callback, watcher_size_t *callback_index) {
    watcher_size_t i              = 0;
    uint8_t        callback_found = 0;
    for (i = 0; i < watcher->callbacks.num; i++) {
        if (watcher->callbacks.items[i] == callback) {
            callback_found  = 1;
            *callback_index = i;
            break;
        }
    }

    if (!callback_found) {
        GROW_OR_FAIL(callbacks);
        *callback_index = watcher->callbacks.num;
        VECTOR_APPEND(watcher->callbacks, callback);
    }

    return WATCHER_RESULT_OK;
}


static watcher_result_t add_arg(watcher_t *watcher, void *arg, watcher_size_t *arg_index) {
    watcher_size_t i = 0;
    *arg_index       = NULL_INDEX;

    if (arg != NULL) {
        uint8_t arg_found = 0;
        for (i = 0; i < watcher->args.num; i++) {
            if (watcher->args.items[i] == arg) {
                arg_found  = 1;
                *arg_index = i;
                break;
            }
        }

        if (!arg_found) {
            GROW_OR_FAIL(args);
            *arg_index = watcher->args.num;
            VECTOR_APPEND(watcher->args, arg);
        }
    }

    return WATCHER_RESULT_OK;
}


void watcher_trigger_entry_silently(watcher_t *watcher, int16_t entry_index) {
    // Invalid index
    if (entry_index >= watcher->entries.num || entry_index < 0) {
        return;
    }
    watcher_entry_t *entry      = &watcher->entries.items[entry_index];
    void            *old_buffer = ENTRY_GET_OLD_BUFFER_POINTER(*entry);

    memcpy(old_buffer, entry->watched, entry->size);

    if (entry->debounce_index != NULL_INDEX) {
        watcher_debounce_data_t *pdebounce = &watcher->debounces.items[entry->debounce_index];
        pdebounce->triggered               = 0;
    }
}


static watcher_result_t _add_entry_static(watcher_t *watcher, const void *pointer, watcher_size_t size,
                                          watcher_callback_t callback, void *arg, void *old_buffer,
                                          unsigned long delay) {
    GROW_OR_FAIL(entries);

    watcher_size_t   callback_index = NULL_INDEX;
    watcher_result_t result         = WATCHER_RESULT_OK;
    if ((result = add_callback(watcher, callback, &callback_index)) != WATCHER_RESULT_OK) {
        return result;
    }

    watcher_size_t arg_index = NULL_INDEX;
    if ((result = add_arg(watcher, arg, &arg_index)) != WATCHER_RESULT_OK) {
        return result;
    }

    watcher_entry_t entry = {
        .watched        = pointer,
        .old_buffer     = old_buffer,
        .size           = size,
        .callback_index = callback_index,
        .arg_index      = arg_index,
        .debounce_index = NULL_INDEX,
    };
    // If the watched region fits in a pointer just use the corresponding field
    old_buffer = ENTRY_GET_OLD_BUFFER_POINTER(entry);
    if (old_buffer == NULL) {
        return WATCHER_RESULT_ALLOC_ERROR;
    }
    memcpy(old_buffer, pointer, size);

    GROW_OR_FAIL(entries);
    int16_t entry_index = watcher->entries.num;
    VECTOR_APPEND(watcher->entries, entry);

    if (delay > 0) {
        // Entry is already delayed
        if (watcher->entries.items[entry_index].debounce_index != NULL_INDEX) {
            return entry_index;
        }

        // Look for or add a new delay tracker
        watcher_size_t i           = 0;
        uint8_t        delay_found = 0;
        int16_t        delay_index = 0;

        for (i = 0; i < watcher->delays.num; i++) {
            if (watcher->delays.items[i] == delay) {
                delay_found = 1;
                delay_index = i;
                break;
            }
        }

        if (!delay_found) {
            if (VECTOR_FULL(watcher->delays)) {
                if (watcher->fn_realloc != NULL && !VECTOR_AT_MAX_CAPACITY(watcher->delays)) {
                    VECTOR_GROW(watcher->delays);
                } else {
                    return WATCHER_RESULT_STATIC_OVERFLOW;
                }
            }
            delay_index = watcher->delays.num;
            VECTOR_APPEND(watcher->delays, delay);
        }

        GROW_OR_FAIL(debounces);
        int16_t                 debounce_index = watcher->debounces.num;
        watcher_debounce_data_t debounce_data  = {
             .timestamp   = 0,
             .delay_index = delay_index,
             .triggered   = 0,
        };
        VECTOR_APPEND(watcher->debounces, debounce_data);

        watcher->entries.items[entry_index].debounce_index = debounce_index;
    }

    return entry_index;
}
