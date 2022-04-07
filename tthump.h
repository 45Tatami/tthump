#pragma once
#if defined _WIN32 || defined __CYGWIN__
  #ifdef BUILDING_TTHUMP
    #define TTH_PUBLIC __declspec(dllexport)
  #else
    #define TTH_PUBLIC __declspec(dllimport)
  #endif
#else
  #ifdef BUILDING_TTHUMP
      #define TTH_PUBLIC __attribute__ ((visibility ("default")))
  #else
      #define TTH_PUBLIC
  #endif
#endif


#include <stdbool.h>
#include <stdint.h>

#define TTH_ERR_COMMON -1 // You can assume an error message has been printed
#define TTH_ERR_INV_PARA -2
#define TTH_ERR_NO_DECODER -3 // No decoder found for requested file

struct tth;

/** References an async thumbnailing job */
typedef int64_t tth_job;

/** Callback signature on async job **/
typedef void (tth_callback)(tth_job, char const *, void*);


/** Creates a new tth handle for async operation with num `workers` */
TTH_PUBLIC void tth_create(struct tth **hndl, uint8_t workers);
/** Shuts down and frees a previously allocated tth handle */
TTH_PUBLIC void tth_destroy(struct tth *hndl);

/**
 * Fetches thumbnail for path without blocking
 *
 * `path` must contain the filesystem path to the image to be thumbnailed.
 * `callback` will be called with the corresponding job id, thumbnail image
 * path and `user` parameter. Image path in callback owned by caller and must
 * not be stored or modified..
 * 
 * Returns job id or <0 for error, errno contains additional information. See
 * tth_job_* functions and `tth_get_error`.
 */
TTH_PUBLIC tth_job tth_get_thumbnail_async(struct tth *hndl, char const *path,
                                           tth_callback callback, void *user);

// TODO tth_job_* tests

/** 0 -> not done, >0 -> done, <0 -> unknown job id */
TTH_PUBLIC int tth_job_state(struct tth *hndl, tth_job const job);

/** If valid job, cancels it, if invalid, does nothing. */
// TTH_PUBLIC void tth_job_cancel(struct tth *hndl, tth_job job);

/** If valid job, waits until finish, if invalid, does nothing. */
TTH_PUBLIC int tth_job_wait(struct tth *hndl, tth_job const job);

/**
 * Fetches thumbnail for path while blocking
 *
 * Details from tth_get_thumbnail_async apply
 */
// TTH_PUBLIC char *tth_get_thumbnail(char const *path);

/** Checks if a thumnail already exists */
// TTH_PUBLIC bool tth_has_thumbnail(struct tth *hndl, char const *path);

/** 
 * Returns an argb buffer for the given thumbnail path
 *
 * Returns NULL with errno set on error. Buffer is owned by caller
 */
// TTH_PUBLIC char *tth_get_thumbnail_buf(char const *path);

/** Returns thread-safe error message owned by callee */
TTH_PUBLIC char const *tth_get_error(int error);
