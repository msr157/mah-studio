/*
 * Software H.264 Annex-B decode via FFmpeg (optional at build time).
 */

#include "mah-h264.h"

#ifdef MAH_HAVE_FFMPEG

#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <media-io/video-io.h>
#include <util/platform.h>

struct mah_h264_decoder {
	const AVCodec *codec;
	AVCodecContext *ctx;
	AVPacket *pkt;
	AVFrame *frame;
};

struct mah_h264_decoder *mah_h264_create(void)
{
	struct mah_h264_decoder *d = bzalloc(sizeof(*d));
	d->codec = avcodec_find_decoder(AV_CODEC_ID_H264);
	if (!d->codec)
		goto fail;
	d->ctx = avcodec_alloc_context3(d->codec);
	if (!d->ctx)
		goto fail;
	d->ctx->flags2 |= AV_CODEC_FLAG2_FAST;
	d->pkt = av_packet_alloc();
	d->frame = av_frame_alloc();
	if (!d->pkt || !d->frame)
		goto fail;
	if (avcodec_open2(d->ctx, d->codec, NULL) < 0)
		goto fail;
	return d;

fail:
	mah_h264_destroy(d);
	return NULL;
}

void mah_h264_destroy(struct mah_h264_decoder *d)
{
	if (!d)
		return;
	if (d->frame)
		av_frame_free(&d->frame);
	if (d->pkt)
		av_packet_free(&d->pkt);
	if (d->ctx)
		avcodec_free_context(&d->ctx);
	bfree(d);
}

static enum video_format mah_pix_fmt(enum AVPixelFormat f)
{
	switch (f) {
	case AV_PIX_FMT_YUV420P:
	case AV_PIX_FMT_YUVJ420P:
		return VIDEO_FORMAT_I420;
	case AV_PIX_FMT_YUV422P:
	case AV_PIX_FMT_YUVJ422P:
		return VIDEO_FORMAT_I422;
	case AV_PIX_FMT_YUV444P:
	case AV_PIX_FMT_YUVJ444P:
		return VIDEO_FORMAT_I444;
	case AV_PIX_FMT_NV12:
		return VIDEO_FORMAT_NV12;
	default:
		return VIDEO_FORMAT_NONE;
	}
}

bool mah_h264_decode(struct mah_h264_decoder *d, const uint8_t *data, size_t size, obs_source_t *source)
{
	if (!d || !d->ctx || !data || size == 0 || !source)
		return false;

	d->pkt->data = (uint8_t *)data;
	d->pkt->size = (int)size;

	if (avcodec_send_packet(d->ctx, d->pkt) < 0)
		return false;

	bool any = false;
	while (avcodec_receive_frame(d->ctx, d->frame) == 0) {
		enum video_format fmt = mah_pix_fmt(d->ctx->pix_fmt);
		if (fmt == VIDEO_FORMAT_NONE)
			continue;

		struct obs_source_frame out = {0};
		for (int i = 0; i < MAX_AV_PLANES; i++) {
			out.data[i] = d->frame->data[i];
			out.linesize[i] = (uint32_t)d->frame->linesize[i];
		}
		out.width = (uint32_t)d->frame->width;
		out.height = (uint32_t)d->frame->height;
		out.format = fmt;
		out.full_range = (d->ctx->color_range == AVCOL_RANGE_JPEG);
		out.timestamp = os_gettime_ns();
		obs_source_output_video(source, &out);
		any = true;
	}
	return any;
}

bool mah_h264_available(void)
{
	return avcodec_find_decoder(AV_CODEC_ID_H264) != NULL;
}

#else /* !MAH_HAVE_FFMPEG */

struct mah_h264_decoder {
	int unused;
};

struct mah_h264_decoder *mah_h264_create(void)
{
	return NULL;
}

void mah_h264_destroy(struct mah_h264_decoder *d)
{
	UNUSED_PARAMETER(d);
}

bool mah_h264_decode(struct mah_h264_decoder *d, const uint8_t *data, size_t size, obs_source_t *source)
{
	UNUSED_PARAMETER(d);
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(size);
	UNUSED_PARAMETER(source);
	return false;
}

bool mah_h264_available(void)
{
	return false;
}

#endif
