#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
        if(argc != 2) {
                printf("%s takes one argument.\n", argv[0]);
                return 1;
        }

        if (get_mtime(NULL) >= 0) {
                return 2;
        }


        if (get_mtime("./doesnotexist") >= 0) {
                return 3;
        }

        int ret = system("touch mtime -d @123456");
        if (ret != 0) {
                if (ret == -1) {
                        printf("Could not run subprocess 'touch':\n\t%s\n",
                               strerror(errno));
                } else if (ret == 127) {
                        printf("Could not run subshell\n");
                } else {
                        printf("touch exited with non-zero: %d\n", ret);
                }
                return 77;
        }

        time_t t = get_mtime("mtime");
        if(t != 123456) {
                printf("Expected mtime: 123456, Gotten: %ld\n",
                       (unsigned long) t);
                remove("mtime");
                return 4;
        }
        remove("mtime");

        return 0;
}
