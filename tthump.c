#include "tthump.h"

#include "util.h"

#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if defined(__STD_NO_THREADS__) || defined(NO_C11_THREADS)
#include "c11threads.h"
#else
#include <threads.h>
#endif

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>

#define ERR_BUF_SIZE 256


#define for_each(head, it) \
        it = head.next; \
        for(; it != &head; it = it->next)


static thread_local AVFrame *src_frame = NULL;
static thread_local AVFrame *dst_frame = NULL;
static thread_local AVCodec const *enc = NULL; // Not free'd
static thread_local AVCodecContext *enc_ctx = NULL;
static thread_local AVPacket *pkt = NULL;

// TODO On new job scheduled, check cur id and reuse from done after certain

// For done list
struct job_stub {
        struct list_head list; // DO NOT REORDER!
        tth_job id;
};

struct job {
        struct list_head list; // DO NOT REORDER!
        tth_job id;

        char *path;
        tth_callback *cllbck;
        void *user;

        uint32_t u_waiting; // If user(s) waiting, free in user wait thread
        bool fnsh; // If exists, signals done to user wait thread(s)
        cnd_t fnsh_cnd;
};

struct tth {
        mtx_t mut;

        tth_job next_id;

        struct list_head jobs; // struct job
        cnd_t      jobs_cnd;

        struct list_head jobs_done; // struct job_stub

        uint8_t num_workers;
        thrd_t *workers;

        bool shutdown;

        char const *thumb_dir;
};



static void free_job(struct job *j)
{
        cnd_destroy(&j->fnsh_cnd);
        free(j->path);
        memset(j, 0, sizeof(*j));
        free(j);
}

static int load_png(FILE *file)
{

        static size_t const header_r = 8;
        unsigned char *header = malloc(8);
        if (fread(header, 1, header_r, file) != header_r) {
                fprintf(stderr, "[ERROR] (Almost) empty file\n");
                goto cleanup_1;
        }

        // TODO

cleanup_1:
        free(header);

        return 0;
}

static int write_frame(AVFormatContext *fmt, AVFrame *frame) {
        int ret = 0;

        AVPacket *pkt = av_packet_alloc();

        avcodec_send_frame(enc_ctx, frame);

        while (ret >= 0) {
                ret = avcodec_receive_packet(enc_ctx, pkt);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                        // TODO
                        break;
                } else if (ret < 0) {
                        fprintf(stderr, "[ERR] Error encoding a frame:\n\t%s\n",
                                av_err2str(ret));
                        break;
                }
                //TODO write
                av_write_frame(fmt, pkt); //TODO flush
        }

        return ret;
}

static int create_thumb(char const *img_path, char const *thmb_path)
{
        // FILE *thmb_f = NULL;
        // uint8_t *dst_data = NULL;
        // size_t dst_buf_size = 0;
        AVCodec const *dec = NULL;
        int const dst_w = 128, dst_h = 128; 
        int ret; 

        if (enc == NULL) {
                if ((enc = avcodec_find_encoder_by_name("png")) == NULL) {
                        fprintf(stderr,
                                "[ERR] ffmpeg not compiled with png encoder\n");
                        return TTH_ERR_COMMON;
                }
        }

        if (enc_ctx == NULL) {
                if((enc_ctx = avcodec_alloc_context3(enc)) == NULL) {
                        fprintf(stderr,
                                "[ERR] Could not allocate encoder context\n");
                        return TTH_ERR_COMMON;
                }

                enc_ctx->time_base = (AVRational){1, 25};
                enc_ctx->pix_fmt = AV_PIX_FMT_RGBA;
                enc_ctx->width = dst_w;
                enc_ctx->height = dst_h;

                if (avcodec_open2(enc_ctx, NULL, NULL) < 0) {
                        fprintf(stderr,
                                "[ERR] Could not configure encoder context\n");
                        return TTH_ERR_COMMON;
                }
        }

        if (src_frame == NULL) {
                src_frame = av_frame_alloc();
        }
        if (dst_frame == NULL) {
                dst_frame = av_frame_alloc();
        }

        if (pkt == NULL) {
                if ((pkt = av_packet_alloc()) == NULL) {
                        fprintf(stderr, "[ERR] Could not allocate packet\n");
                        return TTH_ERR_COMMON;
                }
        }

        fprintf(stderr, "[NFO] Creating %s -> %s\n", img_path, thmb_path);

        // Decode
        AVFormatContext *c = NULL;
        if ((ret = avformat_open_input(&c, img_path, NULL, NULL)) < 0) {
                fprintf(stderr, "[ERR] Error allocating lavf context:\n\t%s\n",
                        av_err2str(ret));
                return TTH_ERR_COMMON;
        }

        if ((ret = avformat_find_stream_info(c, NULL)) < 0) {
                fprintf(stderr, "[WRN] Possibly image format unknown:\n\t%s\n",
                        av_err2str(ret));
        }

        if (c->nb_streams < 1) {
                fprintf(stderr, "[ERR] No stream detected in file:\n");
                avformat_close_input(&c);
                return TTH_ERR_COMMON;
        }

        if (c->nb_streams > 1) {
                fprintf(stderr, "[WRN] Multiple streams detected; FIXME\n");
        }

        AVStream *stream = c->streams[0];
        enum AVCodecID const codec_dec_id = stream->codecpar->codec_id;
        if ((dec = avcodec_find_decoder(codec_dec_id)) == NULL) {
                avformat_close_input(&c);
                return TTH_ERR_NO_DECODER;
        }

        AVCodecContext *dec_ctx;
        if ((dec_ctx = avcodec_alloc_context3(dec)) == NULL) {
                fprintf(stderr,
                        "[ERR] Error allocating lavf decoder context\n");
                avformat_close_input(&c);
                return TTH_ERR_COMMON;
        }

        ret = avcodec_parameters_to_context(dec_ctx, stream->codecpar);
        if (ret < 0) {
                fprintf(stderr, "[ERR] Error copying input codec params\n");
                avcodec_free_context(&dec_ctx);
                avformat_close_input(&c);
                return TTH_ERR_COMMON;
        }

        if ((ret = avcodec_open2(dec_ctx, dec, NULL)) < 0) {
                fprintf(stderr, "[ERR] Could not open decoder:\n\t%s\n",
                        av_err2str(ret));
                avcodec_free_context(&dec_ctx);
                avformat_close_input(&c);
                return TTH_ERR_COMMON;
        }

        if ((ret = av_read_frame(c, pkt)) < 0) {
                fprintf(stderr, "[ERR] Could not read frame\n");
                av_packet_free(&pkt);
                avcodec_free_context(&dec_ctx);
                avformat_close_input(&c);
                return TTH_ERR_COMMON;

        }

        if ((ret = avcodec_send_packet(dec_ctx, pkt)) < 0) {
                fprintf(stderr,
                        "[ERR] Error sending enc frame to decoder:\n\t%s\n",
                        av_err2str(ret));
                av_packet_unref(pkt);
                av_packet_free(&pkt);
                avcodec_free_context(&dec_ctx);
                avformat_close_input(&c);
                return TTH_ERR_COMMON;
        }

        // TODO maybe need to send multiple packets

        if ((ret = avcodec_receive_frame(dec_ctx, src_frame)) != 0) {
                fprintf(stderr,
                        "[ERR] Error sending enc frame to decoder:\n\t%s\n",
                        av_err2str(ret));
                av_packet_unref(pkt);
                av_packet_free(&pkt);
                avcodec_free_context(&dec_ctx);
                avformat_close_input(&c);
                return TTH_ERR_COMMON;
        }

        //av_packet_unref(pkt);
        //avcodec_free_context(&dec_ctx);
        //avformat_close_input(&c);
        
        // Scale
        dst_frame = av_frame_alloc();

        int64_t src_w = src_frame->width,
                src_h = src_frame->height,
                src_fmt = src_frame->format,
                dst_fmt = enc_ctx->pix_fmt;

        struct SwsContext *scale_ctx = sws_getContext(src_w, src_h, src_fmt,
                                                      dst_w, dst_h, dst_fmt,
                                                      SWS_FAST_BILINEAR,
                                                      NULL, NULL, NULL);

        if (scale_ctx == NULL) {
                fprintf(stderr, "[ERR] Error allocating scale context:\n\t%s\n",
                        av_err2str(ret));
                av_frame_unref(src_frame);
                return TTH_ERR_COMMON;
        }

        if ((ret = sws_scale_frame(scale_ctx, dst_frame, src_frame)) < 0) {
                fprintf(stderr, "[ERR] Error scaling:\n\t%s\n",
                        av_err2str(ret));
                sws_freeContext(scale_ctx);
                av_frame_unref(src_frame);
                av_frame_unref(dst_frame);
                return TTH_ERR_COMMON;
        }

        sws_freeContext(scale_ctx);

        av_frame_unref(src_frame);

        // Encode
        (void) enc;
        (void) thmb_path;

        AVFormatContext *ofmt_ctx = NULL;
        if ((ret = avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, thmb_path)) < 0) {
                fprintf(stderr, "[ERR] Error creating output context:\n\t%s\n",
                        av_err2str(ret));
                // TODO
                return TTH_ERR_COMMON;
        }

        if ((ret = avformat_write_header(ofmt_ctx, NULL)) < 0) {
                fprintf(stderr, "[ERR] Error writing thumb header:\n\t%s\n",
                        av_err2str(ret));
                avformat_free_context(ofmt_ctx);
                av_frame_unref(dst_frame);
                return TTH_ERR_COMMON;
        }

        write_frame(ofmt_ctx, dst_frame);
        av_frame_unref(dst_frame);

        if ((ret = av_write_trailer(ofmt_ctx)) != 0) {
                fprintf(stderr, "[ERR] Error writing thumb header:\n\t%s\n",
                        av_err2str(ret));
                avformat_free_context(ofmt_ctx);
                return TTH_ERR_COMMON;
        }

        avformat_free_context(ofmt_ctx);

        /*
        // Open dst path and write
        if ((thmb_f = fopen(thmb_path, "wb+")) == NULL) {
                int err = errno;
                fprintf(stderr,
                        "[ERR] Could not open thumbnail target '%s':\n\t%s\n",
                        thmb_path, strerror(err));
        } else {
                fwrite(dst_data, 1, dst_buf_size, thmb_f);
                fseek(thmb_f, 0, SEEK_SET);
        }

        return thmb_f;
        */

        return 0;
}

static char *create_or_get_thumb_path(char const *path)
{
        int ret;
        char *result = NULL;
        char const *thumb_dir = "/home/joshua/.cache/thumbnails/normal"; // FIXME

        // TODO non file://
        char *abs_path = malloc(PATH_MAX + 7);
        strcpy(abs_path, "file://");
        realpath(path, abs_path+7);
        if (abs_path == NULL) {
                fprintf(stderr, "[ERROR] Could not create abspath for thumb\n");
                goto cleanup_1;
        }

        char *md5_str = calc_md5_hex_repr(abs_path);

        static char const *thmb_format = "%s/%x.png%s";
        int thmb_path_len = snprintf(NULL, 0,
                                     thmb_format, thumb_dir, md5_str) + 1;
        char *thmb_path = malloc(thmb_path_len);
        snprintf(thmb_path, thmb_path_len, thmb_format, thumb_dir, md5_str, "");


        // 1. Check if the thumbnail already exists and if it's valid.
        FILE *thmb_f;
check_again:
        if ((thmb_f = fopen(thmb_path, "rb")) == NULL) {
                if (errno == ENOENT) {
                        // Doesn't exist -> Create

                        // Create temporary thumbnail path
                        char *thmb_path_tmp;
                        char const *thmb_path_tmp_ext = ".XXXXXXX"; // See mkstemp below

                        int buf_len = snprintf(NULL, 0, thmb_format,
                                               thumb_dir, md5_str,
                                               thmb_path_tmp_ext) + 1;
                        thmb_path_tmp = malloc(buf_len);
                        snprintf(thmb_path_tmp, buf_len, thmb_format,
                                 thumb_dir, md5_str, thmb_path_tmp_ext);

                        ret = create_thumb(path, thmb_path_tmp);
                        free(thmb_path_tmp);

                        if (ret != 0) {
                                // TODO
                                goto cleanup_2;
                        }

                        // Move tmp file to real target
                        if(rename(thmb_path_tmp, thmb_path) != 0) {
                                int err = errno;
                                fprintf(stderr, "[ERR] Could not move temporary thumbnail to final location '%s':\n\t%s\n",
                                        thmb_path, strerror(err));
                                remove(thmb_path_tmp);
                                goto cleanup_2;
                        }

                        goto check_again;

                } else {
                        fprintf(stderr, "[ERROR] Could not open file\n");
                        goto cleanup_2;
                }
        }

        // Check if thumbnail
        load_png(thmb_f);
        // TODO
        fclose(thmb_f);

        (void) get_mtime;
        // Load comments
        /*
        png_textp png_text;
        int png_numtext = 0;
        png_get_text(png, png_info, &png_text, &png_numtext);

        char const *thmb_uri = NULL;
        time_t thmb_mtime = 0;
        for (int i = 0; i < png_numtext; i++) {
                char const * const tk = png_text[i].key;
                char const * const txt = png_text[i].text;
                int const txt_cmpr = png_text[i].compression;


                // TODO optional fields
                // TODO rewrite more elegant
                if(!strcmp(tk, "Thumb::URI")) {
                        if (thmb_uri != NULL)
                                fprintf(stderr,
                                        "[WARN] Multiple URI fields in png. Overwriting\n");

                        if (txt_cmpr != PNG_TEXT_COMPRESSION_NONE) {
                                fprintf(stderr,
                                        "[ERR] URI field found in thumbnail but compressed. Unsupported so ignoring FIXME\n");
                                continue;
                        }

                        thmb_uri = txt;

                } else if (!strcmp(tk, "Thumb::MTime")) {
                        if (thmb_mtime != 0)
                                fprintf(stderr,
                                        "[WARN] Multiple MTime fields in png. Overwriting\n");

                        if (txt_cmpr != PNG_TEXT_COMPRESSION_NONE) {
                                fprintf(stderr,
                                        "[ERR] MTime field found in thumbnail but compressed. Unsupported so ignoring FIXME\n");
                                continue;
                        }

                        thmb_mtime = atoll(txt);
                        if (thmb_mtime <= 0) {
                                thmb_mtime = -1;
                        }
                }
        }

        // If we did not find the required metadata or the thumbnail is
        // outdated/has collision, delete existing thumbnail and recursively
        // call this function to create it 
        int thmb_collision = strcmp(thmb_uri, abs_path);
        time_t img_mtime = get_image_mtime(path);
        if (thmb_uri == NULL || thmb_mtime <= 0 || thmb_collision || thmb_mtime != img_mtime) {
                if (thmb_uri == NULL) {
                        fprintf(stderr,
                                "[WARN] Missing uri metadata in thumbnail. Can not confirm thumb corresponds to image and will be recreated\n");
                } else if (thmb_collision) {
                        fprintf(stderr,
                                "[INFO] Existing thumbnail has different source img uri in metadata. Possible hash collision. Recreating\n");
                }
                if (thmb_mtime == 0) {
                        fprintf(stderr,
                                "[WARN] Missing MTime metadata in thumbnail. Can not confirm thumb corresponds to image and will be recreated\n");
                } else if (thmb_mtime < 0) {
                        fprintf(stderr,
                                "[ERR] Invalid MTime metadata in thumbnail. Wil be recreated\n");
                } else {
                        fprintf(stderr,
                                "[INFO] Existing thumbnail found but is out of date. Will be recreated\n");
                }

                remove(thmb_path);
                result = create_or_get_thumb_path(path);
                goto cleanup_4;

        } else {
                result = strdup(thmb_path);
        }

cleanup_4:
        png_destroy_read_struct(&png, &png_info, NULL);
        */
cleanup_2:
        free(thmb_path);
        free(md5_str);
cleanup_1:
        free(abs_path);

        return result;
}

static int worker_thread_fn(void *p)
{
        struct tth *hndl = (struct tth *) p;
        struct list_head *jobs = &hndl->jobs;

        mtx_lock(&hndl->mut);
        while (true) {
                while (!hndl->shutdown
                       && jobs->next == jobs) {
                        cnd_wait(&hndl->jobs_cnd, &hndl->mut);
                }

                if (hndl->shutdown)
                        break;

                struct job *j = (struct job *) jobs->next;
                list_del(&j->list);

                mtx_unlock(&hndl->mut);

                char *thmb_path = create_or_get_thumb_path(j->path);

                if(j->cllbck != NULL)
                        j->cllbck(j->id, thmb_path, j->user);
                free(thmb_path);

                mtx_lock(&hndl->mut);

                struct job_stub *jtmp = calloc(1, sizeof(*jtmp));
                list_init(&jtmp->list);
                jtmp->id = j->id;
                list_add_tail(&hndl->jobs_done, (struct list_head *) jtmp);

                // We can only free if currently nobody is waiting
                if (j->u_waiting == 0) {
                        free_job(j);
                } else {
                        j->fnsh = true;
                        cnd_broadcast(&j->fnsh_cnd);
                }
        }
        mtx_unlock(&hndl->mut);

        if (src_frame != NULL) {
                av_frame_free(&src_frame);
        }
        if (dst_frame != NULL) {
                av_frame_free(&dst_frame);
        }
        if (enc_ctx != NULL) {
                avcodec_free_context(&enc_ctx);
        }
        if (pkt != NULL) {
                av_packet_free(&pkt);
        }

        return 0;
}

void tth_create(struct tth **hndl, uint8_t workers)
{
        if (hndl == NULL)
                return;

        struct tth *nh = calloc(sizeof(**hndl), 1);

        mtx_init(&nh->mut, mtx_plain);

        list_init(&nh->jobs);
        cnd_init(&nh->jobs_cnd);

        nh->num_workers = workers;
        nh->workers = calloc(sizeof(thrd_t), workers);
        for (int i = 0; i < workers; i++) {
                thrd_create(nh->workers + i, &worker_thread_fn, nh);
        }

        list_init(&nh->jobs_done);

        *hndl = nh;
}

void tth_destroy(struct tth *hndl)
{
        if (hndl == NULL)
                return;

        mtx_lock(&hndl->mut);
        hndl->shutdown = true;
        cnd_broadcast(&hndl->jobs_cnd);
        mtx_unlock(&hndl->mut);

        for (int i = 0; i < hndl->num_workers; i++) {
                if (thrd_join(*(hndl->workers + i), NULL) != thrd_success) {
                        fprintf(stderr, "Error joining thread\n");
                }
        }
        free(hndl->workers);


        struct list_head *cur = hndl->jobs.next;
        while (cur != &hndl->jobs) {
                cur = cur->next;
                free_job((struct job *) cur->prev);
        }
        if (cur->prev != &hndl->jobs)
                free_job((struct job *) cur->prev);

        cur = hndl->jobs_done.next;
        while (cur != &hndl->jobs_done) {
                cur = cur->next;
                free(cur->prev);
        }


        mtx_destroy(&hndl->mut);

        memset(hndl, 0, sizeof(*hndl));

        free(hndl);
}

tth_job tth_get_thumbnail_async(struct tth *hndl, char const *path,
                                tth_callback *cllbck, void *user)
{
        if (hndl == NULL || path == NULL) {
                if (hndl == NULL)
                        errno = 1;
                else
                        errno = 2;
                return TTH_ERR_INV_PARA;
        }
        mtx_lock(&hndl->mut);

        struct job *njob = calloc(sizeof(*njob), 1);
        njob->id = hndl->next_id++;

        njob->path = strdup(path);
        njob->cllbck = cllbck;
        njob->user = user;

        list_add_tail(&njob->list, &hndl->jobs);

        cnd_signal(&hndl->jobs_cnd);

        mtx_unlock(&hndl->mut);

        return njob->id;
}

int tth_job_state(struct tth *hndl, tth_job const job_id)
{
        if (hndl == NULL) {
                return -1;
        }

        int ret = -1;

        mtx_lock(&hndl->mut);

        struct list_head *it;

        for_each(hndl->jobs, it) {
                if (((struct job *) it)->id == job_id) {
                        ret = 0;
                        goto exit;
                }
        }

        for_each(hndl->jobs_done, it) {
                if (((struct job *) it)->id == job_id) {
                        ret = 1;
                        goto exit;
                }
        }

exit:
        mtx_unlock(&hndl->mut);
        return ret;
}

int tth_job_wait(struct tth *hndl, tth_job const job_id)
{
        if (hndl == NULL) {
                errno = 1;
                return TTH_ERR_INV_PARA;
        }

        mtx_lock(&hndl->mut);

        struct list_head *it;
        struct job *j = NULL;

        for_each(hndl->jobs, it) {
                if (((struct job *) it)->id == job_id) {
                        j = (struct job *) it;
                        break;
                }
        }

        if (j == NULL) {
                goto exit;
        }

        j->u_waiting++;
        while (!j->fnsh)
                cnd_wait(&j->fnsh_cnd, &hndl->mut);
        if (--j->u_waiting == 0)
                free_job(j);

exit:
        mtx_unlock(&hndl->mut);
        return 1;
}

char const *tth_get_error(int error) {
        static thread_local char buf[ERR_BUF_SIZE];
        int err2 = errno;
        switch (error) {
        case TTH_ERR_INV_PARA:
                snprintf(buf, ERR_BUF_SIZE,
                        "Invalid parameter in argument %d\n", err2);
                break;
        case 0:
                strncpy(buf, "No error\n", ERR_BUF_SIZE);
                break;
        default:
                strncpy(buf, "Unknown error\n", ERR_BUF_SIZE);
        }
        return (char *) &buf;
}
