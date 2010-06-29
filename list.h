struct item
{
    void *data;
    struct item *prev;
    struct item *next;
};

void movetohead(struct item **mainlist, struct item *item);
struct item *additem(struct item **mainlist);
void delitem(struct item **mainlist, struct item *item);
void listitems(struct item *mainlist);
