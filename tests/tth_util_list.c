#include <util.h>
#include <stdio.h>

struct t {
        struct list_head list;
        int useless;
};

int main(int argc, char **argv)
{
        if(argc != 1) {
                printf("%s takes no arguments.\n", argv[0]);
                return 1;
        }
        
        struct t a = { 0 };
        struct t b[20] = { 0 };

        list_init(&a.list);

        for (int i = 0; i < 20; i++) {
                b[i].useless = i;
                list_add_tail(&b[i].list, &a.list);
        }

        if ((struct t *) a.list.next != &b[0]) {
                return 1;
        }

        if ((struct t *) a.list.next->next != &b[1]) {
                return 2;
        }

        if ((struct t *) a.list.prev != &b[19]) {
                // printf("%d %d\n", ((struct t *) a.list.prev)->useless, b[19].useless);
                return 3;
        }

        list_del(a.list.next);
        if ((struct t *) a.list.next != &b[1]) {
                return 4;
        }
        if ((struct t *) a.list.next->prev != &a) {
                return 5;
        }

        list_del(a.list.next->next);
        if ((struct t *) a.list.next != &b[1]) {
                return 6;
        }
        if ((struct t *) a.list.next->next != &b[3]) {
                return 7;
        }
        if ((struct t *) a.list.next->next->prev != &b[1]) {
                return 8;
        }

        list_del(&a.list);
        if ((struct t *) b[19].list.next != &b[1]) {
                return 9;
        }
        if ((struct t *) b[3].list.prev->prev != &b[19]) {
                return 9;
        }

        return 0;
}
