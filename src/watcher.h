#ifndef C_WATCHER_H_INCLUDED
#define C_WATCHER_H_INCLUDED

#include <stdint.h>

// The maximumum number of entries is (realistically) limited to use smaller data types and save RAM
#define WATCHER_MAX_ENTRIES (0x7FFF)

// Define a vector struct
#define WATCHER_VECTOR_DEFINE(type, name)                                                                              \
    struct {                                                                                                           \
        type    *items;                                                                                                \
        uint16_t num;                                                                                                  \
        uint16_t capacity;                                                                                             \
    } name

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
#define WATCHER_ADD_ENTRY(watcher, ptr, cb, arg) WATCHER_ADD_ARRAY_ENTRY(watcher, ptr, 1, cb, arg)

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
    WATCHER_ADD_ARRAY_ENTRY_DELAYED(watcher, ptr, 1, cb, arg, delay)

/**
 * @brief Callback typedef
 *
 * @param old_value old value of the memory
 * @param memory current pointer
 * @param size size of the data type
 * @param user_ptr user specified context
 * @param arg extra argument
 */
typedef void (*watcher_callback_t)(void *old_value, const void *memory, uint16_t size, void *user_ptr, void *arg);

typedef struct __attribute__((packed)) {
    const void *memory;         // Memory pointer
    void       *old_buffer;     // Old value buffer
    uint16_t    size;           // Memory size

    uint16_t callback_index;     // Index for the callback vector
    uint16_t arg_index;          // Index for the argument vector

    // TODO: reduce the number of indexes here (use the timestamp index as main index and add an indirection there for
    // the delay index) This will not save RAM when using delays but will spare 2 bytes from each non-delayed watcher
    uint16_t delay_index;                 // Index for the delay vector
    uint16_t timestamp_index : 15;        // Index for the timestamp vector
    uint16_t timestamp_triggered : 1;     // Whether a change was triggered and the timer is counting
} watcher_entry_t;

// Watcher data
typedef struct {
    WATCHER_VECTOR_DEFINE(watcher_entry_t, entries);
    WATCHER_VECTOR_DEFINE(unsigned long, delays);
    WATCHER_VECTOR_DEFINE(unsigned long, timestamps);
    WATCHER_VECTOR_DEFINE(watcher_callback_t, callbacks);
    WATCHER_VECTOR_DEFINE(void *, args);

    void   *user_ptr;
    uint8_t growable;
} watcher_t;


/**
 * @brief Initialize a watcher structure, dynamically allocating the required memory
 *
 * @param watcher
 * @param user_ptr user pointer that will be provided to the entries' callbacks
 */
void watcher_init(watcher_t *watcher, void *user_ptr);

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
void watcher_init_static(watcher_t *watcher, watcher_entry_t *entries, uint16_t entries_capacity,
                         watcher_callback_t *callbacks, uint16_t callbacks_capacity, void **args,
                         uint16_t args_capacity, unsigned long *delays, uint16_t delays_capacity,
                         unsigned long *timestamps, uint16_t timestamps_capacity, void *user_ptr);

/**
 * @brief Frees the allocated memory for a buffer (if it was not statically allocated)
 *
 * @param watcher
 */
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
int16_t watcher_add_entry(watcher_t *watcher, const void *pointer, uint16_t size, watcher_callback_t callback,
                          void *arg);

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
int16_t watcher_add_entry_delayed(watcher_t *watcher, const void *pointer, uint16_t size, watcher_callback_t callback,
                                  void *arg, unsigned long delay);

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
int16_t watcher_add_entry_static(watcher_t *watcher, const void *pointer, uint16_t size, watcher_callback_t callback,
                                 void *arg, void *old_buffer);

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
int16_t watcher_add_entry_delayed_static(watcher_t *watcher, const void *pointer, uint16_t size,
                                         watcher_callback_t callback, void *arg, unsigned long delay, void *old_buffer);

/**
 * @brief Mark an entry as delayed
 *
 * @param watcher
 * @param entry_index
 * @param delay
 * @return int16_t entry index if successful, -1 on failure
 */
int16_t watcher_set_delayed(watcher_t *watcher, int16_t entry_index, unsigned long delay);

/**
 * @brief Run the observer engine
 *
 * @param watcher
 * @param timestamp current time
 * @return uint16_t number of entries which had their callback invoked
 */
uint16_t watcher_watch(watcher_t *watcher, unsigned long timestamp);

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

#endif
