#ifndef C_WATCHER_H_INCLUDED
#define C_WATCHER_H_INCLUDED

#include <stdint.h>
#include <stdlib.h>

// Type of entry indexes
// TODO: use extensive checks to allow for even smaller indexes
#ifndef C_WATCHER_SIZE_TYPE
#define C_WATCHER_SIZE_TYPE uint16_t
#endif

// The maximumum number of entries is (realistically) limited to use smaller data types and save RAM
#ifndef C_WATCHER_MAX_ENTRIES
#define C_WATCHER_MAX_ENTRIES (0xFFFF)
#endif

#ifndef C_WATCHER_STRUCT_ATTRIBUTES
#define C_WATCHER_STRUCT_ATTRIBUTES
#endif

#define WATCHER_INIT_STD(Watcher, User_ptr) watcher_init(Watcher, User_ptr, realloc, free)


/**
 * @brief Add a new entry (in the form of an array) to the watcher
 *
 * @param watcher
 * @param pointer pointer to observe
 * @param num number of items
 * @param callback function to be called on memory change
 * @param arg additional argument to be passed to the function
 * @return int16_t entry index if successful, -1 on failure
 */
#define WATCHER_ADD_ARRAY_ENTRY(watcher, ptr, num, cb, arg)                                                            \
    watcher_add_entry(watcher, ptr, sizeof(*(ptr)) * (num), cb, ((void *)(arg)))

/**
 * @brief Add a new entry to the watcher
 *
 * @param watcher
 * @param pointer pointer to observe
 * @param callback function to be called on memory change
 * @param arg additional argument to be passed to the function
 * @return int16_t entry index if successful, -1 on failure
 */
#define WATCHER_ADD_ENTRY(watcher, ptr, cb, arg) watcher_add_entry(watcher, ptr, sizeof(*(ptr)), cb, ((void *)(arg)))

/**
 * @brief Add a new entry (in the form of an array, with delayed reaction) to the watcher
 *
 * @param watcher
 * @param pointer pointer to observe
 * @param num number of items
 * @param callback function to be called on memory change
 * @param arg additional argument to be passed to the function
 * @param delay number of ticks to wait after a change before invoking the callback
 * @return int16_t entry index if successful, -1 on failure
 */
#define WATCHER_ADD_ARRAY_ENTRY_DELAYED(watcher, ptr, num, cb, arg, delay)                                             \
    watcher_add_entry_delayed(watcher, ptr, sizeof(*(ptr)) * (num), cb, arg, delay)

/**
 * @brief Add a new entry (with delayed reaction) to the watcher
 *
 * @param watcher
 * @param pointer pointer to observe
 * @param callback function to be called on memory change
 * @param arg additional argument to be passed to the function
 * @param delay number of ticks to wait after a change before invoking the callback
 * @return int16_t entry index if successful, -1 on failure
 */
#define WATCHER_ADD_ENTRY_DELAYED(watcher, ptr, cb, arg, delay)                                                        \
    watcher_add_entry_delayed(watcher, ptr, sizeof(*(ptr)), cb, arg, delay)


// Private utility, defines a vector struct
#define VECTOR_DEFINE(type, name)                                                                                      \
    struct {                                                                                                           \
        type          *items;                                                                                          \
        watcher_size_t num;                                                                                            \
        watcher_size_t capacity;                                                                                       \
    } name

typedef C_WATCHER_SIZE_TYPE watcher_size_t;

/**
 * @brief Callback typedef
 *
 * @param old_value old value
 * @param new_value new value
 * @param size size of the data type
 * @param user_ptr user specified context
 * @param arg extra argument
 */
typedef void (*watcher_callback_t)(void *old_value, const void *new_value, watcher_size_t size, void *user_ptr,
                                   void *arg);


typedef enum {
    WATCHER_RESULT_OK = 0,
    WATCHER_RESULT_INVALID_ARGS,
    WATCHER_RESULT_ALLOC_ERROR,
    WATCHER_RESULT_STATIC_OVERFLOW,
} watcher_result_t;


// TODO: consider whether the vector index optimization is appropriate for the callback's argument
typedef struct C_WATCHER_STRUCT_ATTRIBUTES {
    const void    *watched;        // Memory pointer
    void          *old_buffer;     // Old value buffer
    watcher_size_t size;           // Memory size

    watcher_size_t callback_index;     // Index for the callback vector
    watcher_size_t arg_index;          // Index for the argument vector
} watcher_entry_t;


typedef struct C_WATCHER_STRUCT_ATTRIBUTES {
    unsigned long  timestamp;
    watcher_size_t delay_index;
    watcher_size_t callback_index;
    watcher_size_t arg_index;
    uint8_t        triggered;
} watcher_debouncer_t;


// Watcher data
typedef struct {
    VECTOR_DEFINE(watcher_entry_t, entries);
    VECTOR_DEFINE(unsigned long, delays);
    VECTOR_DEFINE(watcher_callback_t, callbacks);
    VECTOR_DEFINE(void *, args);
    VECTOR_DEFINE(watcher_debouncer_t, debouncers);

    void *user_ptr;

    // Allocator
    void *(*fn_realloc)(void *, size_t);
    void (*fn_free)(void *);

    uint8_t changed;
} watcher_t;


/**
 * @brief Initialize a watcher structure, dynamically allocating the required memory
 *
 * @param watcher
 * @param user_ptr user pointer that will be provided to the entries' callbacks
 */
watcher_result_t watcher_init(watcher_t *watcher, void *user_ptr, void *(*fn_realloc)(void *, size_t),
                              void (*fn_free)(void *));

/**
 * @brief Initialize a watcher structure, providing static memory for allocation.
 * Every parameter is relative to a different vector
 *
 * @param watcher
 * @param entries
 * @param entries_capacity
 * @param callbacks
 * @param callbacks_capacity
 * @param args
 * @param args_capacity
 * @param delays
 * @param delays_capacity
 * @param timestamps
 * @param timestamps_capacity
 * @param user_ptr user pointer that will be provided to the entries' callbacks
 */
// TODO: collect all the required parameters in a struct
// TODO: add a function that returns that struct (with allocated memory information) for a watcher that has been
// dynamically allocated
void watcher_init_static(watcher_t *watcher, watcher_entry_t *entries, watcher_size_t entries_capacity,
                         watcher_callback_t *callbacks, watcher_size_t callbacks_capacity, void **args,
                         watcher_size_t args_capacity, unsigned long *delays, watcher_size_t delays_capacity,
                         watcher_debouncer_t *debouncers, watcher_size_t debouncers_capacity, void *user_ptr);

/**
 * @brief Frees the allocated memory for a buffer (if it was not statically allocated)
 *
 * @param watcher
 */
// TODO: add a "clear" function that doesn't deallocate memory
void watcher_destroy(watcher_t *watcher);

/**
 * @brief Adds a new entry to the watched vector, allocating the memory dinamically.
 *
 * @param watcher
 * @param pointer pointer to observe
 * @param size size of the associated type
 * @param callback function to be called on memory change
 * @param arg additional argument to be passed to the function
 * @return int16_t entry index if successful, -1 on failure
 */
watcher_result_t watcher_add_entry(watcher_t *watcher, const void *pointer, watcher_size_t size,
                                   watcher_callback_t callback, void *arg);

/**
 * @brief Adds a new (delayed) entry to the watched vector, allocating the memory dinamically.
 *
 * @param watcher
 * @param pointer pointer to observe
 * @param size size of the associated type
 * @param callback function to be called on memory change
 * @param arg additional argument to be passed to the function
 * @param delay delay in ticks
 * @return int16_t entry index if successful, -1 on failure
 */
watcher_result_t watcher_add_entry_delayed(watcher_t *watcher, const void *pointer, watcher_size_t size,
                                           watcher_callback_t callback, void *arg, unsigned long delay);

/**
 * @brief Adds a new entry to the watched vector, with pre allocated memory for the old value buffer
 *
 * @param watcher
 * @param pointer pointer to observe
 * @param size size of the associated type
 * @param callback function to be called on memory change
 * @param arg additional argument to be passed to the function
 * @param old_buffer pre allocated memory (of corresponding size)
 * @return int16_t entry index if successful, -1 on failure
 */
watcher_result_t watcher_add_entry_static(watcher_t *watcher, const void *pointer, watcher_size_t size,
                                          watcher_callback_t callback, void *arg, void *old_buffer);

/**
 * @brief Adds a new (delayed) entry to the watched vector, with pre allocated memory for the old value buffer
 *
 * @param watcher
 * @param pointer pointer to observe
 * @param size size of the associated type
 * @param callback function to be called on memory change
 * @param arg additional argument to be passed to the function
 * @param delays delay in ticks
 * @param old_buffer pre allocated memory (of corresponding size)
 * @return int16_t entry index if successful, -1 on failure
 */
watcher_result_t watcher_add_entry_delayed_static(watcher_t *watcher, const void *pointer, watcher_size_t size,
                                                  watcher_callback_t callback, void *arg, unsigned long delay,
                                                  void *old_buffer);


/**
 * @brief Run the observer engine
 *
 * @param watcher
 * @param timestamp current time
 * @return watcher_size_t number of entries which had their callback invoked
 */
watcher_size_t watcher_watch(watcher_t *watcher, unsigned long timestamp);

/**
 * @brief Trigger an entry, invoking its callback
 *
 * @param watcher
 * @param entry_index
 */
void watcher_trigger_entry(watcher_t *watcher, int16_t entry_index);

/**
 * @brief Trigger all entries
 *
 * @param watcher
 */
void watcher_trigger_all(watcher_t *watcher);


void watcher_reset_all(watcher_t *watcher);


#endif
