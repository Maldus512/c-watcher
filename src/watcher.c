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
        void *new_entries = watcher->fn_realloc(name.items, (name.capacity + 1) * sizeof(name.items[0]));              \
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


typedef enum {
    TRIGGER_STATE_INACTIVE = 0,
    TRIGGER_STATE_ACTIVE,
    TRIGGER_STATE_RESET,
} trigger_state_t;


static watcher_result_t add_callback(watcher_t *watcher, watcher_callback_t callback, watcher_size_t *callback_index);
static watcher_result_t add_arg(watcher_t *watcher, void *arg, watcher_size_t *arg_index);
static watcher_result_t add_entry_static(watcher_t *watcher, const void *pointer, watcher_size_t size,
                                         watcher_callback_t callback, void *arg, void *old_buffer, unsigned long delay);
static void debouncer_callback(void *old_value, const void *new_value, watcher_size_t size, void *user_ptr, void *arg);
static uint8_t is_debounced(watcher_t *watcher, watcher_size_t entry_index);
static void    trigger_debouncer_entry(watcher_t *watcher, watcher_size_t entry_index);


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
    VECTOR_INIT(watcher->debouncers);

    watcher->user_ptr = user_ptr;

    return WATCHER_RESULT_OK;
}


void watcher_init_static(watcher_t *watcher, watcher_entry_t *entries, watcher_size_t entries_capacity,
                         watcher_callback_t *callbacks, watcher_size_t callbacks_capacity, void **args,
                         watcher_size_t args_capacity, unsigned long *delays, watcher_size_t delays_capacity,
                         watcher_debouncer_t *debouncers, watcher_size_t debouncers_capacity, void *user_ptr) {
    VECTOR_INIT_STATIC(watcher->entries, entries, entries_capacity);
    VECTOR_INIT_STATIC(watcher->callbacks, callbacks, callbacks_capacity);
    VECTOR_INIT_STATIC(watcher->args, args, args_capacity);
    VECTOR_INIT_STATIC(watcher->delays, delays, delays_capacity);
    VECTOR_INIT_STATIC(watcher->debouncers, debouncers, debouncers_capacity);

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
        watcher->fn_free(watcher->debouncers.items);
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
    return add_entry_static(watcher, pointer, size, callback, arg, old_buffer, 0);
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
    return add_entry_static(watcher, pointer, size, callback, arg, old_buffer, delay);
}


watcher_size_t watcher_watch(watcher_t *watcher, unsigned long timestamp) {
    watcher_size_t count = 0;
    watcher_size_t i     = 0;

    for (i = 0; i < watcher->entries.num; i++) {
        watcher_entry_t *pentry     = &watcher->entries.items[i];
        void            *old_buffer = ENTRY_GET_OLD_BUFFER_POINTER(*pentry);

        if (memcmp(pentry->watched, old_buffer, pentry->size)) {
            // Immediate logic
            watcher_trigger_entry(watcher, i);

            // A debounced entry is considered triggered after the delay
            if (!is_debounced(watcher, i)) {
                count++;
            }
        }
    }

    for (i = 0; i < watcher->debouncers.num; i++) {
        // Debounced logic
        watcher_debouncer_t *pdebouncer = &watcher->debouncers.items[i];

        if (pdebouncer->triggered == TRIGGER_STATE_RESET) {
            pdebouncer->triggered = TRIGGER_STATE_ACTIVE;
            pdebouncer->timestamp = timestamp;
        }

        if (pdebouncer->triggered == TRIGGER_STATE_ACTIVE &&
            is_expired(pdebouncer->timestamp, timestamp, watcher->delays.items[pdebouncer->delay_index])) {
            trigger_debouncer_entry(watcher, i);
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

    void *arg = watcher->args.items[entry->arg_index];

    watcher->callbacks.items[entry->callback_index](old_buffer, entry->watched, entry->size, watcher->user_ptr, arg);
    memcpy(old_buffer, entry->watched, entry->size);
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

    return WATCHER_RESULT_OK;
}


static watcher_result_t add_entry_static(watcher_t *watcher, const void *pointer, watcher_size_t size,
                                         watcher_callback_t callback, void *arg, void *old_buffer,
                                         unsigned long delay) {
    GROW_OR_FAIL(entries);

    watcher_size_t   callback_index = 0;
    watcher_result_t result         = WATCHER_RESULT_OK;
    if ((result = add_callback(watcher, callback, &callback_index)) != WATCHER_RESULT_OK) {
        return result;
    }

    watcher_size_t arg_index = 0;
    if ((result = add_arg(watcher, arg, &arg_index)) != WATCHER_RESULT_OK) {
        return result;
    }

    watcher_entry_t entry = {
        .watched        = pointer,
        .old_buffer     = old_buffer,
        .size           = size,
        .callback_index = callback_index,
        .arg_index      = arg_index,
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
        watcher_entry_t *pentry = &watcher->entries.items[entry_index];

        // Entry is already debounced
        if (is_debounced(watcher, entry_index)) {
            return entry_index;
        }

        // Look for or add a new debouncer
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

        GROW_OR_FAIL(debouncers);
        int16_t debouncer_index = watcher->debouncers.num;

        watcher_size_t debouncer_callback_index = 0;
        result                                  = WATCHER_RESULT_OK;
        if ((result = add_callback(watcher, debouncer_callback, &debouncer_callback_index)) != WATCHER_RESULT_OK) {
            return result;
        }

        watcher_size_t debouncer_arg_index = 0;
        if ((result = add_arg(watcher, &watcher->debouncers.items[debouncer_index], &debouncer_arg_index)) !=
            WATCHER_RESULT_OK) {
            return result;
        }

        watcher_debouncer_t debounce_data = {
            .timestamp      = 0,
            .delay_index    = delay_index,
            .callback_index = pentry->callback_index,
            .arg_index      = pentry->arg_index,
            .triggered      = TRIGGER_STATE_INACTIVE,
        };

        // Fix the callback index
        pentry->callback_index = debouncer_callback_index;
        pentry->arg_index      = debouncer_arg_index;

        VECTOR_APPEND(watcher->debouncers, debounce_data);
    }

    return entry_index;
}


static void debouncer_callback(void *old_value, const void *new_value, watcher_size_t size, void *user_ptr, void *arg) {
    (void)old_value;
    (void)new_value;
    (void)size;
    (void)user_ptr;

    watcher_debouncer_t *pdebouncer = (watcher_debouncer_t *)arg;
    // Debounce was triggered
    pdebouncer->triggered = TRIGGER_STATE_RESET;
}


static uint8_t is_debounced(watcher_t *watcher, watcher_size_t entry_index) {
    return watcher->callbacks.items[watcher->entries.items[entry_index].callback_index] == debouncer_callback;
}


static void trigger_debouncer_entry(watcher_t *watcher, watcher_size_t entry_index) {
    watcher_entry_t     *pentry     = &watcher->entries.items[entry_index];
    watcher_debouncer_t *pdebouncer = watcher->args.items[pentry->arg_index];
    void                *arg        = watcher->args.items[pdebouncer->arg_index];

    void *old_buffer = ENTRY_GET_OLD_BUFFER_POINTER(*pentry);

    watcher->callbacks.items[pdebouncer->callback_index](old_buffer, pentry->watched, pentry->size, watcher->user_ptr,
                                                         arg);
    pdebouncer->triggered = TRIGGER_STATE_INACTIVE;
}
