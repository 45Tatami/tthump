#include <stddef.h>
#include <time.h>

// Simple double-linked list
struct list_head {
        struct list_head *next;
        struct list_head *prev;
};

static inline void list_add_in(struct list_head *new,
                               struct list_head *prev,
                               struct list_head *next)
{
        prev->next = new;
        new->prev = prev;
        new->next = next;
        next->prev = new;
}

static inline void list_init(struct list_head *new)
{
        new->next = new->prev = new;
}

static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
        list_add_in(new, head->prev, head);
}

static inline void list_del(struct list_head *el)
{
        el->prev->next = el->next;
        el->next->prev = el->prev;
        el->prev = el->next = NULL; // sanity
}

time_t get_mtime(char const *path);
char *calc_md5_hex_repr(char *data);
