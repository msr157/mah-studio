#pragma once

#include <obs-module.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct mah_h264_decoder;

struct mah_h264_decoder *mah_h264_create(void);
void mah_h264_destroy(struct mah_h264_decoder *d);

/* Feed one Annex-B access unit. On success, outputs an OBS video frame (I420). */
bool mah_h264_decode(struct mah_h264_decoder *d, const uint8_t *data, size_t size, obs_source_t *source);

bool mah_h264_available(void);

#ifdef __cplusplus
}
#endif
