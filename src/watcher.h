#ifndef C_WATCHER_H_INCLUDED
#define C_WATCHER_H_INCLUDED

#include <stdint.h>

#define C_WATCHER_MAX_ENTRIES (0x7FFF)

#define C_WATCHER_VECTOR(type, name)                                                                                   \
    struct {                                                                                                           \
        type    *items;                                                                                                \
        uint16_t num;                                                                                                  \
        uint16_t capacity;                                                                                             \
    } name

typedef void (*watcher_callback_t)(void *old_value, void *memory, void *user_ptr, void *arg);

typedef struct __attribute__((packed)){
    void    *memory;
    void    *old_buffer;
    uint16_t size;     // Assume we don't need to watch anything bigger than 65KB

    uint16_t callback_index;
    uint16_t arg_index;

    uint16_t delay_index;
    uint16_t timestamp_index : 15;
    uint16_t timestamp_triggered : 1;
} watcher_entry_t;

typedef struct {
    C_WATCHER_VECTOR(watcher_entry_t, entries);
    C_WATCHER_VECTOR(unsigned long, delays);
    C_WATCHER_VECTOR(unsigned long, timestamps);
    C_WATCHER_VECTOR(watcher_callback_t, callbacks);
    C_WATCHER_VECTOR(void *, args);

    void   *user_ptr;
    uint8_t growable;
} watcher_t;


void     watcher_init(watcher_t *watcher, void *user_ptr);
void     watcher_init_static(watcher_t *watcher, watcher_entry_t *entries, uint16_t entries_capacity,
                             watcher_callback_t *callbacks, uint16_t callbacks_capacity, unsigned long *delays,
                             uint16_t delays_capacity, unsigned long *timestamps, uint16_t timestamps_capacity,
                             void *user_ptr);
void     watcher_destroy(watcher_t *watcher);
int16_t  watcher_add_entry(watcher_t *watcher, void *pointer, uint16_t size, watcher_callback_t callback, void *arg);
int16_t  watcher_add_entry_static(watcher_t *watcher, void *pointer, uint16_t size, watcher_callback_t callback,
                                  void *arg, void *old_buffer);
int16_t  watcher_set_delayed(watcher_t *watcher, int16_t entry_index, unsigned long delay);
uint16_t watcher_watch(watcher_t *watcher, unsigned long timestamp);
void     watcher_trigger_entry(watcher_t *watcher, int16_t entry_index);
void     watcher_trigger_all(watcher_t *watcher);

#endif