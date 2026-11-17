#pragma once

#include <obs-module.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

struct mah_mobile_cam;

#define MAH_DEFAULT_WEB_PORT 28889

/* Phone opens http://LAN:web_port/?p=PAIR — browser asks camera → WebSocket ingest. */
bool mah_web_start(struct mah_mobile_cam *ctx, int web_port);
void mah_web_stop(struct mah_mobile_cam *ctx);

/* Implemented in mah-mobile-cam.c — shared with native TCP path */
void mah_ingest_jpeg(struct mah_mobile_cam *ctx, const uint8_t *jpeg, size_t len);
void mah_ingest_pcm(struct mah_mobile_cam *ctx, const uint8_t *pcm, size_t len);
void mah_cam_set_status(struct mah_mobile_cam *ctx, const char *fmt, ...);
bool mah_cam_check_pairing(struct mah_mobile_cam *ctx, const char *pair);
bool mah_cam_is_running(struct mah_mobile_cam *ctx);
void mah_cam_set_connected(struct mah_mobile_cam *ctx, bool on);
void mah_cam_get_lan_ip(struct mah_mobile_cam *ctx, char *out, size_t cap);
void mah_cam_get_pairing(struct mah_mobile_cam *ctx, char *out, size_t cap);
void *mah_cam_web_slot(struct mah_mobile_cam *ctx);
void mah_cam_set_web_slot(struct mah_mobile_cam *ctx, void *web);
