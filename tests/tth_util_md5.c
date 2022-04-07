#include "util.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

static char const * const buf = "qwertyuiop[asdfghjkl;'zxcvbnm,";

int main(int argc, char **argv)
{
        if(argc != 2) {
                printf("%s takes one argument.\n", argv[0]);
                return 1;
        }
        
        if (calc_md5_hex_repr(NULL) != NULL) {
                return 2;
        }

        FILE *f = fopen("md5file", "wb+");
        if (f == NULL) {
                printf("Could not open file for writing\n");
                return 77;
        }

        size_t const buf_len = strlen(buf);
        for (int i = 0; i < 1000; i++) {
                fwrite(&buf[i%buf_len], 1, buf_len - (i%buf_len), f);
        }

        fclose(f);

        int ret = system("md5sum -b md5file | cut -d ' ' -f 1 > md5sum");
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
        
        int fd = open("md5sum", O_RDONLY);
        off_t sum_len = lseek(fd, 0, SEEK_END);
        char *sum = (char *) mmap(0, sum_len, PROT_READ, MAP_PRIVATE, fd, 0);
        if (sum == MAP_FAILED) {
                printf("Could not load file into memory:\n\t%s\n",
                       strerror(errno));
                close(fd);
                return 77;
        }
        close(fd);

        fd = open("md5file", O_RDONLY);
        off_t data_len = lseek(fd, 0, SEEK_END);
        char *data = (char *) mmap(0, data_len, PROT_READ, MAP_PRIVATE, fd, 0);
        if (data == MAP_FAILED) {
                printf("Could not load file into memory:\n\t%s\n",
                       strerror(errno));
                close(fd);
                return 77;
        }
        close(fd);

        char *data_str = calloc(1, data_len + 1);
        memcpy(data_str, data, data_len);
        char *md5_str = calc_md5_hex_repr(data_str);
        free(data_str);
        munmap(data, data_len);

        ret = 0;
        if (memcmp(md5_str, sum, 32) != 0) {
                printf("md5 does not match:\n\t%s\n\t%s\n", md5_str, sum);
                ret = 2;
        }

        free(md5_str);

        remove("md5sum");
        remove("md5file");

        return ret;
}
