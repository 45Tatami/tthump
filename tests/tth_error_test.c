#include <tthump.h>
#include <stdio.h>

int main(int argc, char **argv)
{
        if(argc != 1) {
                printf("%s takes no arguments.\n", argv[0]);
                return 1;
        }

        tth_create(NULL, 0);
        tth_destroy(NULL);

        int err;

        if ((err = tth_get_thumbnail_async(NULL, NULL, NULL, NULL)) >= 0) {
                printf("async call did not return error on NULL handle\n");
                return 2;
        }
        tth_get_error(err);

        struct tth *h = NULL;

        tth_create(&h, 2);
        if (h == NULL) {
                printf("Allocation error\n");
                return 3;
        }

        if ((err = tth_get_thumbnail_async(h, NULL, NULL, NULL)) >= 0) {
                printf("async call did not return error on NULL path");
                return 4;
        }
        tth_get_error(err);

        tth_destroy(h);

        return 0;
}
