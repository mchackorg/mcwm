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
    if (NULL == item || NULL == mainlist || NULL == *mainlist)
    {
        return;
    }
    
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

    /* Remember the new head. */
    *mainlist = item;
}

void movetonext(struct item **mainlist, struct item *item, struct item *next)
{
    if (NULL == item || NULL == next || NULL == mainlist || NULL == *mainlist)
    {
        return;
    }

    if (item->next == next || item == next)
    {
        /* Already in position. Do nothing. */
        return;
    }

    /* Braid together the list where next used to be. */

    if (NULL != next->next)
    {
        next->next->prev = next->prev;
    }
    
    if (NULL != next->prev)
    {
        next->prev->next = next->next;
    }
    else
    {
        /* next is currently head. move head one step further along. */
        if (NULL != next->next)
        {
            *mainlist = next->next;
            next->next->prev = NULL;
        }
    }

    /* Position next after item and braid together list again. */
    next->prev = item;
    next->next = item->next;

    if (NULL != next->next)
    {
        next->next->prev = next;
    }

    /* Remember where next is now. */
    item->next = next;
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
    
    if (NULL == mainlist || NULL == *mainlist || NULL == item)
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
        printf("item #%d (stored at %p).\n", i, (void *)item);
    }
}

#if 0

void listall(struct item *mainlist)
{
    struct item *item;
    int i;

    printf("Listing all:\n");
    
    for (item = mainlist, i = 1; item != NULL; item = item->next, i ++)
    {
        printf("%d at %p: %s.\n", i, (void *)item, (char *)item->data);
        printf("  prev: %p\n", item->prev);
        printf("  next: %p\n", item->next);
    }
}

int main(void)
{
    struct item *mainlist = NULL;
    struct item *item1;
    struct item *item2;
    struct item *item3;
    struct item *item4;
    struct item *item;
    struct item *nextitem;
    int i;
    char *foo1 = "1";
    char *foo2 = "2";
    char *foo3 = "3";
    char *foo4 = "4";
    
    item1 = additem(&mainlist);
    if (NULL == item1)
    {
        printf("Couldn't allocate.\n");
        exit(1);
    }
    item1->data = foo1;
    printf("Current elements:\n");
    listall(mainlist);
    
    item2 = additem(&mainlist);
    if (NULL == item2)
    {
        printf("Couldn't allocate.\n");
        exit(1);
    }
    item2->data = foo2;
    printf("Current elements:\n");    
    listall(mainlist);
    
    item3 = additem(&mainlist);
    if (NULL == item3)
    {
        printf("Couldn't allocate.\n");
        exit(1);
    }
    item3->data = foo3;
    printf("Current elements:\n");    
    listall(mainlist);

    item4 = additem(&mainlist);
    if (NULL == item4)
    {
        printf("Couldn't allocate.\n");
        exit(1);
    }
    item4->data = foo4;
    printf("Current elements:\n");    
    listall(mainlist);

    printf("----------------------------------------------------------------------\n");

    printf("Moving item3 to be after item2\n");
    movetonext(&mainlist, item2, item3);
    printf("Current elements:\n");    
    listall(mainlist);

    printf("----------------------------------------------------------------------\n");

    printf("Moving head! item4 to be after item2\n");
    movetonext(&mainlist, item2, item4);
    printf("Current elements:\n");    
    listall(mainlist);

    printf("----------------------------------------------------------------------\n");
    
    printf("Moving tail! item1 to be after item2\n");
    movetonext(&mainlist, item2, item1);
    printf("Current elements:\n");
    listall(mainlist);

    printf("----------------------------------------------------------------------\n");

    printf("Moving head to be after tail.\n");
    movetonext(&mainlist, item3, item2);
    printf("Current elements:\n");
    listall(mainlist);
    
    printf("Moving all the items after each other.\n");
    /* item3 is tail. work backwards. */
    for (item = mainlist, i = 1;
         item != NULL;
         item = item->next, i ++)
    {
        for (nextitem = item2; nextitem != NULL; nextitem = nextitem->prev)
        {
            movetonext(&mainlist, nextitem, item);
            printf("Current elements:\n");
            listall(mainlist);
        }
    }
    
    printf("----------------------------------------------------------------------\n");

#if 0    
    movetohead(&mainlist, item2);
    printf("Current elements:\n");    
    listall(mainlist);
    
    printf("----------------------------------------------------------------------\n");
#endif
    
    printf("Deleting item stored at %p\n", item3);
    delitem(&mainlist, item3);
    printf("Current elements:\n");    
    listall(mainlist);

    puts("");
    
    delitem(&mainlist, item2);
    printf("Current elements:\n");    
    listall(mainlist);

    puts("");
    
    delitem(&mainlist, item1);
    printf("Current elements:\n");    
    listall(mainlist);

    puts("");

    printf("----------------------------------------------------------------------\n");
               
    exit(0);
}
#endif
