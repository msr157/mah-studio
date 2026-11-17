/*
 * MAH Mobile Camera — OBS source plugin
 *
 * Goal: Iriun/DroidCam-class phone camera over Wi‑Fi or USB, as a first-class
 * OBS source (no third-party virtual webcam driver required long-term).
 *
 * v0 status (honest):
 *  - Registers source + properties (transport, port, pairing, resolution)
 *  - Opens a TCP listen socket for the companion app handshake
 *  - Renders a placeholder frame until a phone session is negotiated
 *  - Full H.264/JPEG frame ingest + USB accessory path = roadmap (see mah/ROADMAP.md)
 */

#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>
#include <graphics/graphics.h>

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET mah_socket_t;
typedef int mah_socklen_t;
#define MAH_INVALID_SOCKET INVALID_SOCKET
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int mah_socket_t;
typedef socklen_t mah_socklen_t;
#define MAH_INVALID_SOCKET (-1)
#define closesocket close
#endif

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("mah-mobile-cam", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Mayank After Hours mobile camera (Wi-Fi / USB companion)";
}

enum mah_transport {
	MAH_TRANSPORT_WIFI = 0,
	MAH_TRANSPORT_USB = 1,
};

struct mah_mobile_cam {
	obs_source_t *source;

	enum mah_transport transport;
	int port;
	char pairing[16];
	uint32_t width;
	uint32_t height;

	mah_socket_t listen_fd;
	volatile bool listening;
	volatile bool connected;
	pthread_t listen_thread;

	gs_texture_t *tex;
	uint8_t *frame;
	size_t frame_bytes;
};

static const char *mah_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "MAH Mobile Camera";
}

static void mah_fill_placeholder(struct mah_mobile_cam *ctx)
{
	if (!ctx->frame || ctx->frame_bytes == 0)
		return;

	/* Simple dark charcoal + amber stripe so the source is visibly alive. */
	for (uint32_t y = 0; y < ctx->height; y++) {
		uint8_t *row = ctx->frame + (size_t)y * ctx->width * 4;
		for (uint32_t x = 0; x < ctx->width; x++) {
			const bool stripe = (y > ctx->height / 2 - 4) && (y < ctx->height / 2 + 4);
			row[x * 4 + 0] = stripe ? 0xE5 : 0x0C; /* B */
			row[x * 4 + 1] = stripe ? 0xB8 : 0x10; /* G */
			row[x * 4 + 2] = stripe ? 0x75 : 0x14; /* R */
			row[x * 4 + 3] = 0xFF;
		}
	}
}

static void mah_rebuild_frame(struct mah_mobile_cam *ctx)
{
	obs_enter_graphics();
	if (ctx->tex) {
		gs_texture_destroy(ctx->tex);
		ctx->tex = NULL;
	}
	obs_leave_graphics();

	bfree(ctx->frame);
	ctx->frame_bytes = (size_t)ctx->width * ctx->height * 4;
	ctx->frame = bzalloc(ctx->frame_bytes);
	mah_fill_placeholder(ctx);

	obs_enter_graphics();
	ctx->tex = gs_texture_create(ctx->width, ctx->height, GS_BGRA, 1, (const uint8_t **)&ctx->frame, GS_DYNAMIC);
	obs_leave_graphics();
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
		blog(LOG_WARNING, "[mah-mobile-cam] socket() failed");
		ctx->listening = false;
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
		blog(LOG_WARNING, "[mah-mobile-cam] bind/listen failed on port %d", ctx->port);
		closesocket(ctx->listen_fd);
		ctx->listen_fd = MAH_INVALID_SOCKET;
		ctx->listening = false;
		return NULL;
	}

	blog(LOG_INFO, "[mah-mobile-cam] listening on TCP %d (pairing %s, transport %s)", ctx->port, ctx->pairing,
	     ctx->transport == MAH_TRANSPORT_USB ? "usb" : "wifi");

	while (ctx->listening) {
		struct sockaddr_in client;
		mah_socklen_t clen = sizeof(client);
		mah_socket_t cfd = accept(ctx->listen_fd, (struct sockaddr *)&client, &clen);
		if (cfd == MAH_INVALID_SOCKET)
			continue;

		/* Handshake placeholder: companion will send MAH1 + pairing + caps. */
		char hello[64];
		int n = recv(cfd, hello, sizeof(hello) - 1, 0);
		if (n > 0) {
			hello[n] = 0;
			blog(LOG_INFO, "[mah-mobile-cam] companion hello: %s", hello);
			ctx->connected = true;
		}
		closesocket(cfd);
		/* Frame ingest loop lands in a later milestone. */
		ctx->connected = false;
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

static void mah_stop_listen(struct mah_mobile_cam *ctx)
{
	if (!ctx->listening)
		return;
	ctx->listening = false;
	if (ctx->listen_fd != MAH_INVALID_SOCKET) {
		closesocket(ctx->listen_fd);
		ctx->listen_fd = MAH_INVALID_SOCKET;
	}
	pthread_join(ctx->listen_thread, NULL);
}

static void mah_start_listen(struct mah_mobile_cam *ctx)
{
	mah_stop_listen(ctx);
	ctx->listening = true;
	pthread_create(&ctx->listen_thread, NULL, mah_listen_thread, ctx);
}

static void *mah_create(obs_data_t *settings, obs_source_t *source)
{
	struct mah_mobile_cam *ctx = bzalloc(sizeof(*ctx));
	ctx->source = source;
	ctx->listen_fd = MAH_INVALID_SOCKET;
	ctx->width = 1280;
	ctx->height = 720;
	ctx->port = 28887;
	snprintf(ctx->pairing, sizeof(ctx->pairing), "MAH1");

	obs_source_update(source, settings);
	return ctx;
}

static void mah_destroy(void *data)
{
	struct mah_mobile_cam *ctx = data;
	mah_stop_listen(ctx);

	obs_enter_graphics();
	if (ctx->tex)
		gs_texture_destroy(ctx->tex);
	obs_leave_graphics();

	bfree(ctx->frame);
	bfree(ctx);
}

static void mah_update(void *data, obs_data_t *settings)
{
	struct mah_mobile_cam *ctx = data;

	ctx->transport = (enum mah_transport)obs_data_get_int(settings, "transport");
	ctx->port = (int)obs_data_get_int(settings, "port");
	const char *pair = obs_data_get_string(settings, "pairing");
	if (pair && *pair)
		snprintf(ctx->pairing, sizeof(ctx->pairing), "%s", pair);

	const char *res = obs_data_get_string(settings, "resolution");
	if (res && strcmp(res, "1920x1080") == 0) {
		ctx->width = 1920;
		ctx->height = 1080;
	} else {
		ctx->width = 1280;
		ctx->height = 720;
	}

	mah_rebuild_frame(ctx);
	mah_start_listen(ctx);
}

static uint32_t mah_width(void *data)
{
	return ((struct mah_mobile_cam *)data)->width;
}

static uint32_t mah_height(void *data)
{
	return ((struct mah_mobile_cam *)data)->height;
}

static void mah_video_render(void *data, gs_effect_t *effect)
{
	struct mah_mobile_cam *ctx = data;
	if (!ctx->tex)
		return;

	obs_enter_graphics();
	gs_texture_set_image(ctx->tex, ctx->frame, ctx->width * 4, false);
	obs_leave_graphics();

	const bool prev = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(false);
	gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), ctx->tex);
	gs_draw_sprite(ctx->tex, 0, ctx->width, ctx->height);
	gs_enable_framebuffer_srgb(prev);
}

static obs_properties_t *mah_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *props = obs_properties_create();

	obs_property_t *t = obs_properties_add_list(props, "transport", "Connection", OBS_COMBO_TYPE_LIST,
						    OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(t, "Wi-Fi", MAH_TRANSPORT_WIFI);
	obs_property_list_add_int(t, "USB", MAH_TRANSPORT_USB);

	obs_properties_add_int(props, "port", "Listen port", 1024, 65535, 1);
	obs_properties_add_text(props, "pairing", "Pairing code", OBS_TEXT_DEFAULT);

	obs_property_t *r = obs_properties_add_list(props, "resolution", "Resolution", OBS_COMBO_TYPE_LIST,
						    OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(r, "1280x720", "1280x720");
	obs_property_list_add_string(r, "1920x1080", "1920x1080");

	return props;
}

static void mah_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "transport", MAH_TRANSPORT_WIFI);
	obs_data_set_default_int(settings, "port", 28887);
	obs_data_set_default_string(settings, "pairing", "MAH1");
	obs_data_set_default_string(settings, "resolution", "1280x720");
}

static struct obs_source_info mah_mobile_cam_info = {
	.id = "mah_mobile_cam",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_DO_NOT_DUPLICATE,
	.get_name = mah_get_name,
	.create = mah_create,
	.destroy = mah_destroy,
	.update = mah_update,
	.get_defaults = mah_defaults,
	.get_properties = mah_properties,
	.get_width = mah_width,
	.get_height = mah_height,
	.video_render = mah_video_render,
	.icon_type = OBS_ICON_TYPE_CAMERA,
};

bool obs_module_load(void)
{
	obs_register_source(&mah_mobile_cam_info);
	blog(LOG_INFO, "[mah-mobile-cam] loaded (placeholder ingest; companion protocol pending)");
	return true;
}

void obs_module_unload(void) {}
