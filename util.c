#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if defined(__STD_NO_THREADS__) || defined(NO_C11_THREADS)
#include "c11threads.h"
#else
#include <threads.h>
#endif

#include "libavutil/error.h"
#include "libavutil/md5.h"

time_t get_mtime(char const *path)
{
        struct stat buf;

        if (stat(path, &buf) != 0) {
                int err = errno;
                fprintf(stderr,
                        "[ERR] Getting mtime for image via stat() failed:\n\t%s\n",
                        strerror(err));
                return -1;
        }

        return buf.st_mtime;

}

char *calc_md5_hex_repr(char *data)
{
        if (data == NULL)
                return NULL;

        uint8_t md5_buf[16] = {0};

        av_md5_sum(md5_buf, (uint8_t *) data,  strlen(data));

        char *md5_str = malloc(33);
        for (int i = 0; i < 16; i++) {
                snprintf(&md5_str[i*2], 3, "%.2x", md5_buf[i]);
        }

        return md5_str;
}

char const *strerror_av(int errnum) {
        static thread_local char buf[128];
        if (av_strerror(errnum, buf, 128) == 0) {
                return buf;
        } else {
                return "";
        }
}
