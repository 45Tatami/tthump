
#include <tthump.h>
#include <stdio.h>

int main(int argc, char **argv)
{
        (void) argc; (void) argv;

        int err;

        struct tth *h;
        tth_create(&h, 4);
        if (h == NULL) {
                fprintf(stderr, "Allocation error\n");
                return 1;
        }

        //for (int i = 0; i < 1024; i++) {
        for (int i = 0; i < 1; i++) {
                err = tth_get_thumbnail_async(h, "large.png", NULL, NULL);
                if (err < 0) {
                        fprintf(stderr, tth_get_error(err));
                        return 2;
                }
        }

        tth_job_wait(h, err);

        tth_destroy(h);

        return 0;
}
