struct item
{
    void *data;
    struct item *prev;
    struct item *next;
};

/*
 * Move element in item to the head of list mainlist.
 */
void movetohead(struct item **mainlist, struct item *item);

/*
 * Create space for a new item and add it to the head of mainlist.
 *
 * Returns item or NULL if out of memory.
 */
struct item *additem(struct item **mainlist);

/*
 *
 */ 
void delitem(struct item **mainlist, struct item *item);

/*
 *
 */ 
void listitems(struct item *mainlist);
