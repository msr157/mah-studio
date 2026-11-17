/*
 * MAH Mobile Camera — OBS source plugin
 *
 * Phone camera over Wi‑Fi or USB tether → first-class OBS source.
 * Protocol: mah/PROTOCOL.md (MAH1 + length-prefixed MJPEG).
 */

#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>
#include <media-io/video-io.h>
#include <media-io/audio-io.h>

#include "mah-h264.h"
#include "mah-web.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <errno.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
typedef SOCKET mah_socket_t;
typedef int mah_socklen_t;
#define MAH_INVALID_SOCKET INVALID_SOCKET
#define MAH_SOCKET_ERROR SOCKET_ERROR
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#include <ifaddrs.h>
typedef int mah_socket_t;
typedef socklen_t mah_socklen_t;
#define MAH_INVALID_SOCKET (-1)
#define MAH_SOCKET_ERROR (-1)
#define closesocket close
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#include "stb_image.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("mah-mobile-cam", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Mayank After Hours mobile camera (Wi-Fi / USB companion)";
}

#define MAH_DEFAULT_PORT 28887
#define MAH_DISCOVER_PORT 28888
#define MAH_MAX_JPEG (8 * 1024 * 1024)
#define MAH_MAX_PCM (512 * 1024)
#define MAH_AUDIO_RATE 48000

enum mah_transport {
	MAH_TRANSPORT_WIFI = 0,
	MAH_TRANSPORT_USB = 1,
};

struct mah_mobile_cam {
	obs_source_t *source;

	enum mah_transport transport;
	int port;
	int web_port;
	char pairing[32];
	uint32_t width;
	uint32_t height;

	mah_socket_t listen_fd;
	mah_socket_t discover_fd;
	volatile bool running;
	volatile bool connected;
	char status[128];
	char lan_ip[64];
	void *web; /* mah_web_state* */

	pthread_t listen_thread;
	pthread_t discover_thread;
	pthread_mutex_t mutex;

	uint64_t frames_ok;
	uint64_t frames_drop;
	uint64_t audio_ok;
	bool want_audio;
	bool want_h264;
	struct mah_h264_decoder *h264;
};

static bool mah_decode_jpeg_to_obs(struct mah_mobile_cam *ctx, const uint8_t *jpeg, size_t jpeg_len);
static void mah_output_pcm(struct mah_mobile_cam *ctx, const uint8_t *pcm, size_t bytes);

/* ---------- sockets helpers ---------- */

static bool mah_recv_exact(mah_socket_t fd, void *buf, size_t len)
{
	uint8_t *p = buf;
	size_t got = 0;
	while (got < len) {
		int n = recv(fd, (char *)(p + got), (int)(len - got), 0);
		if (n <= 0)
			return false;
		got += (size_t)n;
	}
	return true;
}

static bool mah_send_all(mah_socket_t fd, const void *buf, size_t len)
{
	const uint8_t *p = buf;
	size_t sent = 0;
	while (sent < len) {
		int n = send(fd, (const char *)(p + sent), (int)(len - sent), 0);
		if (n <= 0)
			return false;
		sent += (size_t)n;
	}
	return true;
}

static uint32_t mah_read_u32_be(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

void mah_cam_set_status(struct mah_mobile_cam *ctx, const char *fmt, ...)
{
	char tmp[128];
	va_list args;
	va_start(args, fmt);
	vsnprintf(tmp, sizeof(tmp), fmt, args);
	va_end(args);

	pthread_mutex_lock(&ctx->mutex);
	snprintf(ctx->status, sizeof(ctx->status), "%s", tmp);
	pthread_mutex_unlock(&ctx->mutex);
}

#define mah_set_status mah_cam_set_status

void mah_ingest_jpeg(struct mah_mobile_cam *ctx, const uint8_t *jpeg, size_t len)
{
	if (ctx && jpeg && len)
		mah_decode_jpeg_to_obs(ctx, jpeg, len);
}

void mah_ingest_pcm(struct mah_mobile_cam *ctx, const uint8_t *pcm, size_t len)
{
	if (ctx && pcm && len)
		mah_output_pcm(ctx, pcm, len);
}

bool mah_cam_check_pairing(struct mah_mobile_cam *ctx, const char *pair)
{
	if (!ctx || !pair)
		return false;
	pthread_mutex_lock(&ctx->mutex);
	const bool ok = (strcmp(pair, ctx->pairing) == 0);
	pthread_mutex_unlock(&ctx->mutex);
	return ok;
}

bool mah_cam_is_running(struct mah_mobile_cam *ctx)
{
	return ctx && ctx->running;
}

void mah_cam_set_connected(struct mah_mobile_cam *ctx, bool on)
{
	if (ctx)
		ctx->connected = on;
}

void mah_cam_get_lan_ip(struct mah_mobile_cam *ctx, char *out, size_t cap)
{
	if (!out || !cap)
		return;
	if (!ctx) {
		out[0] = 0;
		return;
	}
	pthread_mutex_lock(&ctx->mutex);
	snprintf(out, cap, "%s", ctx->lan_ip);
	pthread_mutex_unlock(&ctx->mutex);
}

void mah_cam_get_pairing(struct mah_mobile_cam *ctx, char *out, size_t cap)
{
	if (!out || !cap)
		return;
	if (!ctx) {
		out[0] = 0;
		return;
	}
	pthread_mutex_lock(&ctx->mutex);
	snprintf(out, cap, "%s", ctx->pairing);
	pthread_mutex_unlock(&ctx->mutex);
}

void *mah_cam_web_slot(struct mah_mobile_cam *ctx)
{
	return ctx ? ctx->web : NULL;
}

void mah_cam_set_web_slot(struct mah_mobile_cam *ctx, void *web)
{
	if (ctx)
		ctx->web = web;
}

static void mah_detect_lan_ip(struct mah_mobile_cam *ctx)
{
	snprintf(ctx->lan_ip, sizeof(ctx->lan_ip), "0.0.0.0");

#ifdef _WIN32
	ULONG size = 0;
	GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, NULL,
			     NULL, &size);
	IP_ADAPTER_ADDRESSES *addrs = (IP_ADAPTER_ADDRESSES *)bzalloc(size);
	if (GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
				 NULL, addrs, &size) == NO_ERROR) {
		for (IP_ADAPTER_ADDRESSES *a = addrs; a; a = a->Next) {
			if (a->OperStatus != IfOperStatusUp)
				continue;
			if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
				continue;
			for (IP_ADAPTER_UNICAST_ADDRESS *u = a->FirstUnicastAddress; u; u = u->Next) {
				struct sockaddr_in *sa = (struct sockaddr_in *)u->Address.lpSockaddr;
				if (!sa || sa->sin_family != AF_INET)
					continue;
				char ip[64];
				inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip));
				if (strncmp(ip, "127.", 4) == 0)
					continue;
				snprintf(ctx->lan_ip, sizeof(ctx->lan_ip), "%s", ip);
				/* Prefer RFC1918 / NetBird-ish over CGNAT when possible */
				if (strncmp(ip, "192.168.", 8) == 0 || strncmp(ip, "10.", 3) == 0 ||
				    strncmp(ip, "172.", 4) == 0) {
					bfree(addrs);
					return;
				}
			}
		}
	}
	bfree(addrs);
#else
	struct ifaddrs *ifaddr = NULL;
	if (getifaddrs(&ifaddr) == 0) {
		for (struct ifaddrs *ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
			if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
				continue;
			struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
			char ip[64];
			inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip));
			if (strncmp(ip, "127.", 4) == 0)
				continue;
			snprintf(ctx->lan_ip, sizeof(ctx->lan_ip), "%s", ip);
			if (strncmp(ip, "192.168.", 8) == 0 || strncmp(ip, "10.", 3) == 0) {
				freeifaddrs(ifaddr);
				return;
			}
		}
		freeifaddrs(ifaddr);
	}
#endif
}

static void mah_output_placeholder(struct mah_mobile_cam *ctx, bool waiting)
{
	const uint32_t w = ctx->width ? ctx->width : 1280;
	const uint32_t h = ctx->height ? ctx->height : 720;
	const size_t bytes = (size_t)w * h * 4;
	uint8_t *bgra = bzalloc(bytes);

	for (uint32_t y = 0; y < h; y++) {
		uint8_t *row = bgra + (size_t)y * w * 4;
		for (uint32_t x = 0; x < w; x++) {
			const bool stripe = waiting && (y > h / 2 - 3) && (y < h / 2 + 3);
			row[x * 4 + 0] = stripe ? 0xE5 : 0x0C;
			row[x * 4 + 1] = stripe ? 0xB8 : 0x10;
			row[x * 4 + 2] = stripe ? 0x75 : 0x14;
			row[x * 4 + 3] = 0xFF;
		}
	}

	struct obs_source_frame frame = {0};
	frame.data[0] = bgra;
	frame.linesize[0] = w * 4;
	frame.width = w;
	frame.height = h;
	frame.format = VIDEO_FORMAT_BGRA;
	frame.full_range = true;
	frame.timestamp = os_gettime_ns();
	obs_source_output_video(ctx->source, &frame);
	bfree(bgra);
}

static void mah_output_pcm(struct mah_mobile_cam *ctx, const uint8_t *pcm, size_t bytes)
{
	if (!pcm || bytes < 2 || (bytes % 2) != 0)
		return;

	struct obs_source_audio audio = {0};
	audio.data[0] = pcm;
	audio.frames = (uint32_t)(bytes / 2); /* s16 mono */
	audio.speakers = SPEAKERS_MONO;
	audio.format = AUDIO_FORMAT_16BIT;
	audio.samples_per_sec = MAH_AUDIO_RATE;
	audio.timestamp = os_gettime_ns();
	obs_source_output_audio(ctx->source, &audio);
	ctx->audio_ok++;
}

static bool mah_decode_jpeg_to_obs(struct mah_mobile_cam *ctx, const uint8_t *jpeg, size_t jpeg_len)
{
	int w = 0, h = 0, comp = 0;
	/* Request 4 channels; stb returns RGBA. Convert to BGRA for OBS. */
	unsigned char *rgba = stbi_load_from_memory(jpeg, (int)jpeg_len, &w, &h, &comp, 4);
	if (!rgba || w <= 0 || h <= 0) {
		ctx->frames_drop++;
		return false;
	}

	const size_t bytes = (size_t)w * (size_t)h * 4;
	uint8_t *bgra = bmalloc(bytes);
	for (size_t i = 0; i < (size_t)w * (size_t)h; i++) {
		bgra[i * 4 + 0] = rgba[i * 4 + 2];
		bgra[i * 4 + 1] = rgba[i * 4 + 1];
		bgra[i * 4 + 2] = rgba[i * 4 + 0];
		bgra[i * 4 + 3] = 0xFF;
	}
	stbi_image_free(rgba);

	pthread_mutex_lock(&ctx->mutex);
	ctx->width = (uint32_t)w;
	ctx->height = (uint32_t)h;
	pthread_mutex_unlock(&ctx->mutex);

	struct obs_source_frame frame = {0};
	frame.data[0] = bgra;
	frame.linesize[0] = (uint32_t)w * 4;
	frame.width = (uint32_t)w;
	frame.height = (uint32_t)h;
	frame.format = VIDEO_FORMAT_BGRA;
	frame.full_range = true;
	frame.timestamp = os_gettime_ns();
	obs_source_output_video(ctx->source, &frame);
	bfree(bgra);

	ctx->frames_ok++;
	return true;
}

static bool mah_readline(mah_socket_t fd, char *buf, size_t cap, int timeout_ms)
{
	size_t n = 0;
	while (n + 1 < cap) {
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		struct timeval tv;
		tv.tv_sec = timeout_ms / 1000;
		tv.tv_usec = (timeout_ms % 1000) * 1000;
		int sel = select((int)fd + 1, &rfds, NULL, NULL, &tv);
		if (sel <= 0)
			return false;
		char c;
		int r = recv(fd, &c, 1, 0);
		if (r <= 0)
			return false;
		if (c == '\n') {
			buf[n] = 0;
			if (n > 0 && buf[n - 1] == '\r')
				buf[n - 1] = 0;
			return true;
		}
		buf[n++] = c;
	}
	return false;
}

static bool mah_handle_client(struct mah_mobile_cam *ctx, mah_socket_t cfd)
{
	char line[256];
	if (!mah_readline(cfd, line, sizeof(line), 5000)) {
		mah_set_status(ctx, "Handshake timeout");
		return false;
	}

	blog(LOG_INFO, "[mah-mobile-cam] hello: %s", line);

	/* MAH1 <pairing> <WxH> <codec> */
	char tag[16] = {0};
	char pair[64] = {0};
	char res[32] = {0};
	char codec[32] = {0};
	if (sscanf(line, "%15s %63s %31s %31s", tag, pair, res, codec) < 4 || strcmp(tag, "MAH1") != 0) {
		const char *err = "MAH1 ERR bad-handshake\n";
		mah_send_all(cfd, err, strlen(err));
		mah_set_status(ctx, "Bad handshake");
		return false;
	}

	pthread_mutex_lock(&ctx->mutex);
	const bool pair_ok = (strcmp(pair, ctx->pairing) == 0);
	pthread_mutex_unlock(&ctx->mutex);
	if (!pair_ok) {
		const char *err = "MAH1 ERR bad-pairing\n";
		mah_send_all(cfd, err, strlen(err));
		mah_set_status(ctx, "Bad pairing code");
		return false;
	}

	const bool codec_mjpeg = (strcmp(codec, "mjpeg") == 0 || strcmp(codec, "MJPEG") == 0);
	const bool codec_av = (strcmp(codec, "mjpeg+pcm") == 0);
	const bool codec_h264 = (strcmp(codec, "h264") == 0);
	const bool codec_h264a = (strcmp(codec, "h264+pcm") == 0);
	if (!codec_mjpeg && !codec_av && !codec_h264 && !codec_h264a) {
		const char *err = "MAH1 ERR codec-unsupported\n";
		mah_send_all(cfd, err, strlen(err));
		mah_set_status(ctx, "Codec: mjpeg[+pcm] or h264[+pcm]");
		return false;
	}
	if ((codec_h264 || codec_h264a) && !mah_h264_available()) {
		const char *err = "MAH1 ERR codec-unsupported\n";
		mah_send_all(cfd, err, strlen(err));
		mah_set_status(ctx, "H.264 not built (need FFmpeg)");
		return false;
	}
	ctx->want_audio = codec_av || codec_h264a;
	ctx->want_h264 = codec_h264 || codec_h264a;
	if (ctx->want_h264) {
		mah_h264_destroy(ctx->h264);
		ctx->h264 = mah_h264_create();
		if (!ctx->h264) {
			const char *err = "MAH1 ERR codec-unsupported\n";
			mah_send_all(cfd, err, strlen(err));
			mah_set_status(ctx, "H.264 decoder init failed");
			return false;
		}
	}

	unsigned rw = 0, rh = 0;
	if (sscanf(res, "%ux%u", &rw, &rh) == 2 && rw > 0 && rh > 0) {
		pthread_mutex_lock(&ctx->mutex);
		ctx->width = rw;
		ctx->height = rh;
		pthread_mutex_unlock(&ctx->mutex);
	}

	const char *ok = "MAH1 OK\n";
	if (!mah_send_all(cfd, ok, strlen(ok)))
		return false;

	ctx->connected = true;
	if (ctx->want_h264)
		mah_set_status(ctx, ctx->want_audio ? "Connected — H.264+mic" : "Connected — H.264");
	else
		mah_set_status(ctx, ctx->want_audio ? "Connected — MJPEG+mic" : "Connected — MJPEG");

	/*
	 * Media frames (v1.1+):
	 *   [uint8 type 'V'|'H'|'A'][uint32_be length][payload]
	 * V=JPEG, H=H.264 Annex-B AU, A=PCM s16le mono 48k
	 * Legacy v1 (length-prefixed JPEG only) still accepted.
	 */
	uint8_t *payload = NULL;
	size_t payload_cap = 0;

	while (ctx->running) {
		uint8_t type = 0;
		if (!mah_recv_exact(cfd, &type, 1))
			break;

		uint32_t plen = 0;
		if (type == 'V' || type == 'H' || type == 'A') {
			uint8_t lenbuf[4];
			if (!mah_recv_exact(cfd, lenbuf, 4))
				break;
			plen = mah_read_u32_be(lenbuf);
		} else {
			/* Legacy: first length byte already read as `type`. */
			uint8_t rest[3];
			if (!mah_recv_exact(cfd, rest, 3))
				break;
			uint8_t lenbuf[4] = {type, rest[0], rest[1], rest[2]};
			plen = mah_read_u32_be(lenbuf);
			type = 'V';
		}

		const uint32_t max_len = (type == 'A') ? MAH_MAX_PCM : MAH_MAX_JPEG;
		if (plen == 0 || plen > max_len) {
			ctx->frames_drop++;
			break;
		}
		if (plen > payload_cap) {
			bfree(payload);
			payload_cap = plen + 64 * 1024;
			payload = bmalloc(payload_cap);
		}
		if (!mah_recv_exact(cfd, payload, plen))
			break;

		if (type == 'A') {
			mah_output_pcm(ctx, payload, plen);
		} else if (type == 'H') {
			if (mah_h264_decode(ctx->h264, payload, plen, ctx->source))
				ctx->frames_ok++;
			else
				ctx->frames_drop++;
		} else {
			mah_decode_jpeg_to_obs(ctx, payload, plen);
		}
	}

	bfree(payload);
	ctx->connected = false;
	mah_set_status(ctx, "Waiting for phone… %s:%d", ctx->lan_ip, ctx->port);
	mah_output_placeholder(ctx, true);
	return true;
}

static void *mah_listen_thread(void *data)
{
	struct mah_mobile_cam *ctx = data;

#ifdef _WIN32
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

	ctx->listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (ctx->listen_fd == MAH_INVALID_SOCKET) {
		mah_set_status(ctx, "socket() failed");
		return NULL;
	}

	int yes = 1;
	setsockopt(ctx->listen_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons((uint16_t)ctx->port);

	if (bind(ctx->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0 ||
	    listen(ctx->listen_fd, 1) != 0) {
		blog(LOG_WARNING, "[mah-mobile-cam] bind/listen failed on %d", ctx->port);
		mah_set_status(ctx, "Bind failed on port %d", ctx->port);
		closesocket(ctx->listen_fd);
		ctx->listen_fd = MAH_INVALID_SOCKET;
		return NULL;
	}

	mah_detect_lan_ip(ctx);
	mah_set_status(ctx, "Listening %s:%d  code %s", ctx->lan_ip, ctx->port, ctx->pairing);
	blog(LOG_INFO, "[mah-mobile-cam] listening TCP %s:%d pairing=%s transport=%s", ctx->lan_ip, ctx->port,
	     ctx->pairing, ctx->transport == MAH_TRANSPORT_USB ? "usb" : "wifi");
	mah_output_placeholder(ctx, true);

	while (ctx->running) {
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(ctx->listen_fd, &rfds);
		struct timeval tv = {.tv_sec = 1, .tv_usec = 0};
		int sel = select((int)ctx->listen_fd + 1, &rfds, NULL, NULL, &tv);
		if (sel <= 0)
			continue;

		struct sockaddr_in client;
		mah_socklen_t clen = sizeof(client);
		mah_socket_t cfd = accept(ctx->listen_fd, (struct sockaddr *)&client, &clen);
		if (cfd == MAH_INVALID_SOCKET)
			continue;

		char cip[64];
		inet_ntop(AF_INET, &client.sin_addr, cip, sizeof(cip));
		blog(LOG_INFO, "[mah-mobile-cam] accept from %s", cip);
		mah_set_status(ctx, "Phone connecting from %s…", cip);

		mah_handle_client(ctx, cfd);
		closesocket(cfd);
	}

	if (ctx->listen_fd != MAH_INVALID_SOCKET) {
		closesocket(ctx->listen_fd);
		ctx->listen_fd = MAH_INVALID_SOCKET;
	}
#ifdef _WIN32
	WSACleanup();
#endif
	return NULL;
}

/*
 * UDP discovery: phone broadcasts "MAH1 DISCOVER\n" to port 28888.
 * Desktop replies: "MAH1 HOST <ip> <port> <pairing>\n"
 */
static void *mah_discover_thread(void *data)
{
	struct mah_mobile_cam *ctx = data;

	ctx->discover_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (ctx->discover_fd == MAH_INVALID_SOCKET)
		return NULL;

	int yes = 1;
	setsockopt(ctx->discover_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(MAH_DISCOVER_PORT);

	if (bind(ctx->discover_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		closesocket(ctx->discover_fd);
		ctx->discover_fd = MAH_INVALID_SOCKET;
		return NULL;
	}

	while (ctx->running) {
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(ctx->discover_fd, &rfds);
		struct timeval tv = {.tv_sec = 1, .tv_usec = 0};
		if (select((int)ctx->discover_fd + 1, &rfds, NULL, NULL, &tv) <= 0)
			continue;

		char buf[128];
		struct sockaddr_in from;
		mah_socklen_t flen = sizeof(from);
		int n = recvfrom(ctx->discover_fd, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&from, &flen);
		if (n <= 0)
			continue;
		buf[n] = 0;
		if (strncmp(buf, "MAH1 DISCOVER", 13) != 0)
			continue;

		mah_detect_lan_ip(ctx);
		char reply[160];
		snprintf(reply, sizeof(reply), "MAH1 HOST %s %d %s\n", ctx->lan_ip, ctx->port, ctx->pairing);
		sendto(ctx->discover_fd, reply, (int)strlen(reply), 0, (struct sockaddr *)&from, flen);
	}

	if (ctx->discover_fd != MAH_INVALID_SOCKET) {
		closesocket(ctx->discover_fd);
		ctx->discover_fd = MAH_INVALID_SOCKET;
	}
	return NULL;
}

static void mah_stop(struct mah_mobile_cam *ctx)
{
	mah_web_stop(ctx);

	if (!ctx->running)
		return;
	ctx->running = false;

	if (ctx->listen_fd != MAH_INVALID_SOCKET) {
		closesocket(ctx->listen_fd);
		ctx->listen_fd = MAH_INVALID_SOCKET;
	}
	if (ctx->discover_fd != MAH_INVALID_SOCKET) {
		closesocket(ctx->discover_fd);
		ctx->discover_fd = MAH_INVALID_SOCKET;
	}

	pthread_join(ctx->listen_thread, NULL);
	pthread_join(ctx->discover_thread, NULL);
}

static void mah_start(struct mah_mobile_cam *ctx)
{
	mah_stop(ctx);
	ctx->running = true;
	mah_detect_lan_ip(ctx);
	pthread_create(&ctx->listen_thread, NULL, mah_listen_thread, ctx);
	pthread_create(&ctx->discover_thread, NULL, mah_discover_thread, ctx);
	mah_web_start(ctx, ctx->web_port);
}

static const char *mah_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("MAHMobileCam");
}

static void mah_random_pairing(char *out, size_t cap)
{
	/* 6-char easy code */
	static const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
	static bool seeded = false;
	if (!seeded) {
		srand((unsigned)(os_gettime_ns() ^ (uintptr_t)out));
		seeded = true;
	}
	size_t n = 6;
	if (n + 1 > cap)
		n = cap ? cap - 1 : 0;
	for (size_t i = 0; i < n; i++)
		out[i] = alphabet[(unsigned)rand() % (sizeof(alphabet) - 1)];
	if (cap)
		out[n] = 0;
}

static void *mah_create(obs_data_t *settings, obs_source_t *source)
{
	struct mah_mobile_cam *ctx = bzalloc(sizeof(*ctx));
	ctx->source = source;
	ctx->listen_fd = MAH_INVALID_SOCKET;
	ctx->discover_fd = MAH_INVALID_SOCKET;
	ctx->width = 1280;
	ctx->height = 720;
	ctx->port = MAH_DEFAULT_PORT;
	ctx->web_port = MAH_DEFAULT_WEB_PORT;
	ctx->web = NULL;
	pthread_mutex_init(&ctx->mutex, NULL);
	snprintf(ctx->status, sizeof(ctx->status), "Starting…");
	snprintf(ctx->pairing, sizeof(ctx->pairing), "MAH1");

	obs_source_update(source, settings);
	return ctx;
}

static void mah_destroy(void *data)
{
	struct mah_mobile_cam *ctx = data;
	mah_stop(ctx);
	mah_h264_destroy(ctx->h264);
	ctx->h264 = NULL;
	obs_source_output_video(ctx->source, NULL);
	pthread_mutex_destroy(&ctx->mutex);
	bfree(ctx);
}

static void mah_update(void *data, obs_data_t *settings)
{
	struct mah_mobile_cam *ctx = data;

	ctx->transport = (enum mah_transport)obs_data_get_int(settings, "transport");
	ctx->port = (int)obs_data_get_int(settings, "port");
	if (ctx->port < 1024 || ctx->port > 65535)
		ctx->port = MAH_DEFAULT_PORT;

	ctx->web_port = (int)obs_data_get_int(settings, "web_port");
	if (ctx->web_port < 1024 || ctx->web_port > 65535)
		ctx->web_port = MAH_DEFAULT_WEB_PORT;

	const char *pair = obs_data_get_string(settings, "pairing");
	if (pair && *pair)
		snprintf(ctx->pairing, sizeof(ctx->pairing), "%s", pair);
	else {
		mah_random_pairing(ctx->pairing, sizeof(ctx->pairing));
		obs_data_set_string(settings, "pairing", ctx->pairing);
	}

	const char *res = obs_data_get_string(settings, "resolution");
	if (res && strcmp(res, "1920x1080") == 0) {
		ctx->width = 1920;
		ctx->height = 1080;
	} else if (res && strcmp(res, "640x480") == 0) {
		ctx->width = 640;
		ctx->height = 480;
	} else {
		ctx->width = 1280;
		ctx->height = 720;
	}

	mah_detect_lan_ip(ctx);
	char phone[192];
	snprintf(phone, sizeof(phone), "http://%s:%d/?p=%s", ctx->lan_ip, ctx->web_port, ctx->pairing);
	obs_data_set_string(settings, "phone_url", phone);

	mah_start(ctx);
}

static uint32_t mah_width(void *data)
{
	struct mah_mobile_cam *ctx = data;
	pthread_mutex_lock(&ctx->mutex);
	uint32_t w = ctx->width;
	pthread_mutex_unlock(&ctx->mutex);
	return w;
}

static uint32_t mah_height(void *data)
{
	struct mah_mobile_cam *ctx = data;
	pthread_mutex_lock(&ctx->mutex);
	uint32_t h = ctx->height;
	pthread_mutex_unlock(&ctx->mutex);
	return h;
}

static void mah_show_status(obs_properties_t *props, obs_data_t *settings, struct mah_mobile_cam *ctx)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(settings);
	if (!ctx)
		return;
	/* Status is shown via a read-only text property updated in get_properties. */
}

static bool mah_refresh_clicked(obs_properties_t *props, obs_property_t *p, void *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(p);
	struct mah_mobile_cam *ctx = data;
	if (!ctx)
		return false;
	mah_detect_lan_ip(ctx);
	obs_data_t *settings = obs_source_get_settings(ctx->source);
	char phone[192];
	snprintf(phone, sizeof(phone), "http://%s:%d/?p=%s", ctx->lan_ip, ctx->web_port, ctx->pairing);
	obs_data_set_string(settings, "phone_url", phone);
	obs_data_release(settings);
	mah_set_status(ctx, "Phone: http://%s:%d/?p=%s", ctx->lan_ip, ctx->web_port, ctx->pairing);
	return true;
}

static bool mah_regen_clicked(obs_properties_t *props, obs_property_t *p, void *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(p);
	struct mah_mobile_cam *ctx = data;
	if (!ctx)
		return false;
	mah_random_pairing(ctx->pairing, sizeof(ctx->pairing));
	obs_data_t *settings = obs_source_get_settings(ctx->source);
	obs_data_set_string(settings, "pairing", ctx->pairing);
	char phone[192];
	snprintf(phone, sizeof(phone), "http://%s:%d/?p=%s", ctx->lan_ip, ctx->web_port, ctx->pairing);
	obs_data_set_string(settings, "phone_url", phone);
	obs_data_release(settings);
	mah_set_status(ctx, "Phone: http://%s:%d/?p=%s", ctx->lan_ip, ctx->web_port, ctx->pairing);
	return true;
}

static obs_properties_t *mah_properties(void *data)
{
	struct mah_mobile_cam *ctx = data;
	obs_properties_t *props = obs_properties_create();

	obs_properties_add_text(props, "phone_url", obs_module_text("PhoneUrl"), OBS_TEXT_INFO);

	obs_property_t *t = obs_properties_add_list(props, "transport", obs_module_text("Transport"),
						    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(t, "Wi-Fi (LAN / hotspot)", MAH_TRANSPORT_WIFI);
	obs_property_list_add_int(t, "USB tether (same TCP over phone network)", MAH_TRANSPORT_USB);

	obs_properties_add_int(props, "web_port", obs_module_text("WebPort"), 1024, 65535, 1);
	obs_properties_add_int(props, "port", obs_module_text("Port"), 1024, 65535, 1);
	obs_properties_add_text(props, "pairing", obs_module_text("PairingCode"), OBS_TEXT_DEFAULT);

	obs_property_t *r = obs_properties_add_list(props, "resolution", obs_module_text("Resolution"),
						    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(r, "640x480", "640x480");
	obs_property_list_add_string(r, "1280x720", "1280x720");
	obs_property_list_add_string(r, "1920x1080", "1920x1080");

	obs_properties_add_text(props, "host_info", obs_module_text("HostInfo"), OBS_TEXT_INFO);
	obs_properties_add_text(props, "status_info", obs_module_text("Status"), OBS_TEXT_INFO);

	obs_properties_add_button(props, "refresh_lan", obs_module_text("RefreshLan"), mah_refresh_clicked);
	obs_properties_add_button(props, "regen_pair", obs_module_text("RegenPairing"), mah_regen_clicked);

	mah_show_status(props, NULL, ctx);
	return props;
}

static void mah_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "transport", MAH_TRANSPORT_WIFI);
	obs_data_set_default_int(settings, "port", MAH_DEFAULT_PORT);
	obs_data_set_default_int(settings, "web_port", MAH_DEFAULT_WEB_PORT);
	obs_data_set_default_string(settings, "pairing", "MAH1");
	obs_data_set_default_string(settings, "resolution", "1280x720");
	obs_data_set_default_string(settings, "phone_url",
				    "Add source → open Phone URL on phone → Allow camera");
	obs_data_set_default_string(settings, "host_info",
				    "Best: phone browser (no app). Native MAH Cam optional (TCP 28887).");
	obs_data_set_default_string(settings, "status_info", "Idle");
}

static struct obs_source_info mah_mobile_cam_info = {
	.id = "mah_mobile_cam",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE,
	.get_name = mah_get_name,
	.create = mah_create,
	.destroy = mah_destroy,
	.update = mah_update,
	.get_defaults = mah_defaults,
	.get_properties = mah_properties,
	.get_width = mah_width,
	.get_height = mah_height,
	.icon_type = OBS_ICON_TYPE_CAMERA,
};

bool obs_module_load(void)
{
	obs_register_source(&mah_mobile_cam_info);
	blog(LOG_INFO, "[mah-mobile-cam] loaded — web companion + MAH1 TCP + UDP discovery");
	return true;
}

void obs_module_unload(void) {}
