#ifndef __LIST_H_
#define __LIST_H_


#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
	struct list_head name = LIST_HEAD_INIT(name)

#define LIST_POISON1  ((void *) 0x100)
#define LIST_POISON2  ((void *) 0x200)


#define offsetof(type, f) ((u32) \
 ((char *)&((type *)0)->f - (char *)(type *)0))


#define container_of(ptr, type, member) ({				\
	void *__mptr = (void *)(ptr);					\
	((type *)(__mptr - offsetof(type, member))); })


#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)

struct list_head {
    struct list_head *next, *prev;
};

static inline int list_is_head(const struct list_head *list, const struct list_head *head)
{
	return list == head;
}

#define list_for_each(pos, head) \
	for (pos = (head)->next; !list_is_head(pos, (head)); pos = pos->next)

static inline void init_list_head(struct list_head *list)
{
	list->next =  list;
	list->prev = list;
}
extern struct list_head *checkpoint_stat_list;
extern void list_add(struct list_head *new, struct list_head *head);
extern void list_add_tail(struct list_head *new, struct list_head *head);
#endif
