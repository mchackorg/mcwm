#include <stdlib.h>
#include <stdio.h>
#include "list.h"

#ifdef DEBUG
#define PDEBUG(Args...) \
  do { fprintf(stderr, "mcwm: "); fprintf(stderr, ##Args); } while(0)
#define D(x) x
#else
#define PDEBUG(Args...)
#define D(x)
#endif

void movetohead(struct item **mainlist, struct item *item)
{
    if (*mainlist == item)
    {
        /* Already at head. Do nothing. */
        return;
    }

    /* Braid together the list where we are now. */
    if (NULL != item->prev)
    {
        item->prev->next = item->next;
    }

    if (NULL != item->next)
    {
        item->next->prev = item->prev;
    }

    /* Now at head. */
    item->prev = NULL;
    item->next = *mainlist;
}

/*
 */
struct item *additem(struct item **mainlist)
{
    struct item *item;
    
    if (NULL == (item = (struct item *) malloc(sizeof (struct item))))
    {
        return NULL;
    }
  
    if (NULL == *mainlist)
    {
        /* First in the list. */

        PDEBUG("First in list.\n");
        item->prev = NULL;
        item->next = NULL;
    }
    else
    {
        /* Add to beginning of list. */

        item->next = *mainlist;
        item->next->prev = item;
        item->prev = NULL;
    }

    *mainlist = item;
        
    return item;
}

void delitem(struct item **mainlist, struct item *item)
{
    struct item *ml = *mainlist;
    
    if (NULL == mainlist)
    {
        return;
    }

    if (item == *mainlist)
    {
        /* First entry was removed. Remember the next one instead. */
        *mainlist = ml->next;
    }
    else
    {
        item->prev->next = item->next;

        if (NULL != item->next)
        {
            /* This is not the last item in the list. */
            item->next->prev = item->prev;
        }
    }

    free(item);
}

void listitems(struct item *mainlist)
{
    struct item *item;
    int i;
    
    for (item = mainlist, i = 1; item != NULL; item = item->next, i ++)
    {
        printf("item #%d (stored at %p).\n", i, item);
    }
}

#if 0
int main(void)
{
    struct item *mainlist = NULL;
    struct item *item1;
    struct item *item2;
    struct item *item3;
    char *foo1 = "1";
    char *foo2 = "2";
    char *foo3 = "3";

    item1 = addhead(&mainlist);
    if (NULL == item1)
    {
        printf("Couldn't allocate.\n");
        exit(1);
    }
    item1->data = foo1;
    listitems(mainlist);
    
    item2 = addhead(&mainlist);
    if (NULL == item2)
    {
        printf("Couldn't allocate.\n");
        exit(1);
    }
    item2->data = foo2;
    listitems(mainlist);
    
    item3 = addhead(&mainlist);
    if (NULL == item3)
    {
        printf("Couldn't allocate.\n");
        exit(1);
    }
    item3->data = foo3;
    listitems(mainlist);

    printf("DELETING.\n");

/*
        delclient: entry removed. item = 0x28201040, *mainlist = 0x28201050
        in listitems: mainlist = 0x28201050
        item #1 (stored at 0x28201050): 3
        item #2 (stored at 0x28201030): 1

        Removing last item...

        next == NULL
        prev == *mainlist
        
        delclient: entry removed. item = 0x28201030, *mainlist = 0x28201050
        in listitems: mainlist = 0x28201050
        item #1 (stored at 0x28201050): 3
        item #2 (stored at 0x28201030): 1

        delclient: first entry removed.
        in listitems: mainlist = 0x28201030
        item #1 (stored at 0x28201030): 1
*/
        
    delitem(&mainlist, item2);
    listitems(mainlist);

    puts("");
    
    delitem(&mainlist, item1);
    listitems(mainlist);

    puts("");
    
    delitem(&mainlist, item3);
    listitems(mainlist);

    puts("");
    
    exit(0);
}
#endif
