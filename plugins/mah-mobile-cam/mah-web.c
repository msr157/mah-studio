/*
 * Browser companion: HTTP page + WebSocket MAH1 ingest.
 * Phone: open URL → Allow camera → frames to OBS. No app store.
 */

#include "mah-web.h"

#include <util/platform.h>
#include <util/threading.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
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
typedef int mah_socket_t;
typedef socklen_t mah_socklen_t;
#define MAH_INVALID_SOCKET (-1)
#define MAH_SOCKET_ERROR (-1)
#define closesocket close
#endif

#define MAH_WEB_MAX_HDR (16 * 1024)
#define MAH_WEB_MAX_FRAME (8 * 1024 * 1024)

struct mah_web_state {
	struct mah_mobile_cam *cam;
	int port;
	mah_socket_t fd;
	pthread_t thread;
	volatile bool running;
};

/* ---------- tiny SHA-1 (WebSocket Accept) ---------- */

struct mah_sha1 {
	uint32_t h[5];
	uint64_t bits;
	uint8_t buf[64];
	size_t nbuf;
};

static uint32_t mah_rol(uint32_t v, int n)
{
	return (v << n) | (v >> (32 - n));
}

static void mah_sha1_init(struct mah_sha1 *s)
{
	s->h[0] = 0x67452301;
	s->h[1] = 0xEFCDAB89;
	s->h[2] = 0x98BADCFE;
	s->h[3] = 0x10325476;
	s->h[4] = 0xC3D2E1F0;
	s->bits = 0;
	s->nbuf = 0;
}

static void mah_sha1_block(struct mah_sha1 *s, const uint8_t *p)
{
	uint32_t w[80];
	for (int i = 0; i < 16; i++)
		w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
		       ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
	for (int i = 16; i < 80; i++)
		w[i] = mah_rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

	uint32_t a = s->h[0], b = s->h[1], c = s->h[2], d = s->h[3], e = s->h[4];
	for (int i = 0; i < 80; i++) {
		uint32_t f, k;
		if (i < 20) {
			f = (b & c) | ((~b) & d);
			k = 0x5A827999;
		} else if (i < 40) {
			f = b ^ c ^ d;
			k = 0x6ED9EBA1;
		} else if (i < 60) {
			f = (b & c) | (b & d) | (c & d);
			k = 0x8F1BBCDC;
		} else {
			f = b ^ c ^ d;
			k = 0xCA62C1D6;
		}
		uint32_t t = mah_rol(a, 5) + f + e + k + w[i];
		e = d;
		d = c;
		c = mah_rol(b, 30);
		b = a;
		a = t;
	}
	s->h[0] += a;
	s->h[1] += b;
	s->h[2] += c;
	s->h[3] += d;
	s->h[4] += e;
}

static void mah_sha1_update(struct mah_sha1 *s, const void *data, size_t len)
{
	const uint8_t *p = data;
	s->bits += (uint64_t)len * 8;
	while (len) {
		size_t n = 64 - s->nbuf;
		if (n > len)
			n = len;
		memcpy(s->buf + s->nbuf, p, n);
		s->nbuf += n;
		p += n;
		len -= n;
		if (s->nbuf == 64) {
			mah_sha1_block(s, s->buf);
			s->nbuf = 0;
		}
	}
}

static void mah_sha1_final(struct mah_sha1 *s, uint8_t out[20])
{
	uint8_t pad[64];
	size_t n = s->nbuf;
	memcpy(pad, s->buf, n);
	pad[n++] = 0x80;
	if (n > 56) {
		while (n < 64)
			pad[n++] = 0;
		mah_sha1_block(s, pad);
		n = 0;
	}
	while (n < 56)
		pad[n++] = 0;
	uint64_t bits = s->bits;
	for (int i = 0; i < 8; i++)
		pad[56 + i] = (uint8_t)(bits >> (56 - 8 * i));
	mah_sha1_block(s, pad);
	for (int i = 0; i < 5; i++) {
		out[i * 4] = (uint8_t)(s->h[i] >> 24);
		out[i * 4 + 1] = (uint8_t)(s->h[i] >> 16);
		out[i * 4 + 2] = (uint8_t)(s->h[i] >> 8);
		out[i * 4 + 3] = (uint8_t)(s->h[i]);
	}
}

static void mah_b64(const uint8_t *in, size_t len, char *out, size_t cap)
{
	static const char *t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	size_t o = 0;
	for (size_t i = 0; i < len && o + 4 < cap; i += 3) {
		uint32_t v = ((uint32_t)in[i]) << 16;
		if (i + 1 < len)
			v |= ((uint32_t)in[i + 1]) << 8;
		if (i + 2 < len)
			v |= (uint32_t)in[i + 2];
		out[o++] = t[(v >> 18) & 63];
		out[o++] = t[(v >> 12) & 63];
		out[o++] = (i + 1 < len) ? t[(v >> 6) & 63] : '=';
		out[o++] = (i + 2 < len) ? t[v & 63] : '=';
	}
	if (o < cap)
		out[o] = 0;
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

static char *mah_load_index_html(size_t *out_len)
{
	char *path = obs_module_file("web/index.html");
	if (path) {
		FILE *f = fopen(path, "rb");
		bfree(path);
		if (f) {
			fseek(f, 0, SEEK_END);
			long sz = ftell(f);
			fseek(f, 0, SEEK_SET);
			if (sz > 0 && sz < 512 * 1024) {
				char *buf = bmalloc((size_t)sz + 1);
				if (fread(buf, 1, (size_t)sz, f) == (size_t)sz) {
					buf[sz] = 0;
					fclose(f);
					*out_len = (size_t)sz;
					return buf;
				}
				bfree(buf);
			}
			fclose(f);
		}
	}

	/* Minimal fallback if data/ not installed yet */
	static const char *fallback =
		"<!DOCTYPE html><html><head><meta charset=utf-8>"
		"<meta name=viewport content=\"width=device-width,initial-scale=1\">"
		"<title>MAH Cam</title></head><body style=\"font-family:system-ui;background:#0a0d14;color:#f8fafc;"
		"padding:24px\">"
		"<h1 style=\"color:#e5b875\">MAH CAM</h1>"
		"<p>Install plugin data (web/index.html) for the full companion. "
		"Or open the built page from the MAH Mobile Camera source.</p>"
		"<button id=b style=\"padding:14px 18px;font-size:16px\">Allow camera &amp; start</button>"
		"<pre id=s></pre><video id=v playsinline muted autoplay style=\"width:100%;max-width:480px\"></video>"
		"<canvas id=c style=\"display:none\"></canvas>"
		"<script>"
		"const p=new URLSearchParams(location.search).get('p')||'MAH1';"
		"const s=document.getElementById('s');"
		"document.getElementById('b').onclick=async()=>{"
		"try{const stream=await navigator.mediaDevices.getUserMedia({video:{facingMode:'user'},audio:false});"
		"v.srcObject=stream;await v.play();"
		"const ws=new WebSocket((location.protocol==='https:'?'wss:':'ws:')+'//'+location.host+'/ws?p='+encodeURIComponent(p));"
		"ws.binaryType='arraybuffer';"
		"await new Promise((res,rej)=>{ws.onopen=()=>{ws.send('MAH1 '+p+' 1280x720 mjpeg\\n');res();};ws.onerror=()=>rej(new Error('ws'));});"
		"s.textContent='Live';"
		"const c=document.getElementById('c'),x=c.getContext('2d');"
		"setInterval(()=>{c.width=v.videoWidth||1280;c.height=v.videoHeight||720;x.drawImage(v,0,0);"
		"c.toBlob(b=>{if(!b||ws.readyState!==1)return;b.arrayBuffer().then(buf=>{const u=new Uint8Array(buf);"
		"const o=new Uint8Array(5+u.length);o[0]=0x56;o[1]=(u.length>>>24)&255;o[2]=(u.length>>>16)&255;"
		"o[3]=(u.length>>>8)&255;o[4]=u.length&255;o.set(u,5);ws.send(o);});},'image/jpeg',0.7);},80);"
		"}catch(e){s.textContent=e.message;}};"
		"</script></body></html>";
	*out_len = strlen(fallback);
	char *copy = bmalloc(*out_len + 1);
	memcpy(copy, fallback, *out_len + 1);
	return copy;
}

static const char *mah_hdr_get(const char *hdrs, const char *key)
{
	size_t klen = strlen(key);
	const char *p = hdrs;
	while (*p) {
		bool match = true;
		for (size_t i = 0; i < klen; i++) {
			char a = (char)tolower((unsigned char)p[i]);
			char b = (char)tolower((unsigned char)key[i]);
			if (a != b) {
				match = false;
				break;
			}
		}
		if (match && p[klen] == ':') {
			p += klen + 1;
			while (*p == ' ' || *p == '\t')
				p++;
			return p;
		}
		const char *nl = strstr(p, "\r\n");
		if (!nl)
			break;
		p = nl + 2;
	}
	return NULL;
}

static void mah_hdr_value(const char *line_start, char *out, size_t cap)
{
	size_t i = 0;
	while (line_start[i] && line_start[i] != '\r' && line_start[i] != '\n' && i + 1 < cap) {
		out[i] = line_start[i];
		i++;
	}
	out[i] = 0;
}

static bool mah_ws_accept_key(const char *client_key, char *out, size_t cap)
{
	char key[128];
	mah_hdr_value(client_key, key, sizeof(key));
	/* trim */
	size_t n = strlen(key);
	while (n && (key[n - 1] == ' ' || key[n - 1] == '\t'))
		key[--n] = 0;

	char concat[160];
	snprintf(concat, sizeof(concat), "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", key);

	struct mah_sha1 s;
	mah_sha1_init(&s);
	mah_sha1_update(&s, concat, strlen(concat));
	uint8_t dig[20];
	mah_sha1_final(&s, dig);
	mah_b64(dig, 20, out, cap);
	return out[0] != 0;
}

static bool mah_ws_read_frame(mah_socket_t fd, uint8_t *opcode_out, uint8_t **payload_out, size_t *len_out)
{
	uint8_t hdr[2];
	if (!mah_recv_exact(fd, hdr, 2))
		return false;
	uint8_t opcode = hdr[0] & 0x0f;
	bool masked = (hdr[1] & 0x80) != 0;
	uint64_t len = hdr[1] & 0x7f;
	if (len == 126) {
		uint8_t ext[2];
		if (!mah_recv_exact(fd, ext, 2))
			return false;
		len = ((uint64_t)ext[0] << 8) | ext[1];
	} else if (len == 127) {
		uint8_t ext[8];
		if (!mah_recv_exact(fd, ext, 8))
			return false;
		len = 0;
		for (int i = 0; i < 8; i++)
			len = (len << 8) | ext[i];
	}
	if (len > MAH_WEB_MAX_FRAME)
		return false;

	uint8_t mask[4] = {0};
	if (masked && !mah_recv_exact(fd, mask, 4))
		return false;

	uint8_t *payload = NULL;
	if (len) {
		payload = bmalloc((size_t)len + 1);
		if (!mah_recv_exact(fd, payload, (size_t)len)) {
			bfree(payload);
			return false;
		}
		if (masked) {
			for (uint64_t i = 0; i < len; i++)
				payload[i] ^= mask[i % 4];
		}
		payload[len] = 0;
	}

	*opcode_out = opcode;
	*payload_out = payload;
	*len_out = (size_t)len;
	return true;
}

static bool mah_ws_send_text(mah_socket_t fd, const char *text)
{
	size_t len = strlen(text);
	uint8_t hdr[4];
	size_t hlen = 2;
	hdr[0] = 0x81; /* FIN + text */
	if (len < 126) {
		hdr[1] = (uint8_t)len;
	} else if (len < 65536) {
		hdr[1] = 126;
		hdr[2] = (uint8_t)(len >> 8);
		hdr[3] = (uint8_t)len;
		hlen = 4;
	} else {
		return false;
	}
	return mah_send_all(fd, hdr, hlen) && mah_send_all(fd, text, len);
}

static void mah_handle_ws(struct mah_web_state *w, mah_socket_t cfd)
{
	struct mah_mobile_cam *ctx = w->cam;
	bool hello_ok = false;

	mah_cam_set_status(ctx, "Phone browser connecting…");

	while (w->running && mah_cam_is_running(ctx)) {
		uint8_t opcode = 0;
		uint8_t *payload = NULL;
		size_t len = 0;
		if (!mah_ws_read_frame(cfd, &opcode, &payload, &len))
			break;

		if (opcode == 0x8) { /* close */
			bfree(payload);
			break;
		}
		if (opcode == 0x9) { /* ping → pong */
			uint8_t hdr[2] = {0x8a, (uint8_t)(len < 126 ? len : 0)};
			if (len < 126) {
				mah_send_all(cfd, hdr, 2);
				if (len)
					mah_send_all(cfd, payload, len);
			}
			bfree(payload);
			continue;
		}

		if (!hello_ok && (opcode == 0x1 || opcode == 0x2)) {
			/* MAH1 <pair> <WxH> <codec> */
			char line[256];
			size_t copy = len < sizeof(line) - 1 ? len : sizeof(line) - 1;
			memcpy(line, payload, copy);
			line[copy] = 0;
			for (size_t i = 0; i < copy; i++) {
				if (line[i] == '\r' || line[i] == '\n') {
					line[i] = 0;
					break;
				}
			}

			char tag[16] = {0}, pair[64] = {0}, res[32] = {0}, codec[32] = {0};
			if (sscanf(line, "%15s %63s %31s %31s", tag, pair, res, codec) < 2 ||
			    strcmp(tag, "MAH1") != 0 || !mah_cam_check_pairing(ctx, pair)) {
				mah_ws_send_text(cfd, "MAH1 ERR bad-pairing\n");
				bfree(payload);
				mah_cam_set_status(ctx, "Web: bad pairing");
				break;
			}
			mah_ws_send_text(cfd, "MAH1 OK\n");
			hello_ok = true;
			mah_cam_set_connected(ctx, true);
			mah_cam_set_status(ctx, "Connected — phone browser");
			bfree(payload);
			continue;
		}

		if (hello_ok && opcode == 0x2 && payload && len >= 5) {
			uint8_t type = payload[0];
			uint32_t plen = ((uint32_t)payload[1] << 24) | ((uint32_t)payload[2] << 16) |
					((uint32_t)payload[3] << 8) | (uint32_t)payload[4];
			if (5 + plen <= len) {
				if (type == 'V')
					mah_ingest_jpeg(ctx, payload + 5, plen);
				else if (type == 'A')
					mah_ingest_pcm(ctx, payload + 5, plen);
			}
		}
		bfree(payload);
	}

	mah_cam_set_connected(ctx, false);
	mah_cam_set_status(ctx, "Waiting for phone… open the Phone URL");
}

static void mah_handle_http(struct mah_web_state *w, mah_socket_t cfd)
{
	char *req = bmalloc(MAH_WEB_MAX_HDR);
	size_t got = 0;
	while (got + 1 < MAH_WEB_MAX_HDR) {
		int n = recv(cfd, req + got, (int)(MAH_WEB_MAX_HDR - 1 - got), 0);
		if (n <= 0) {
			bfree(req);
			return;
		}
		got += (size_t)n;
		req[got] = 0;
		if (strstr(req, "\r\n\r\n"))
			break;
	}

	char method[16] = {0}, path[256] = {0};
	sscanf(req, "%15s %255s", method, path);

	const char *hdrs = strstr(req, "\r\n");
	hdrs = hdrs ? hdrs + 2 : req;

	bool is_ws = false;
	const char *upgrade = mah_hdr_get(hdrs, "Upgrade");
	const char *ws_key = mah_hdr_get(hdrs, "Sec-WebSocket-Key");
	if (upgrade && ws_key) {
		char u[32];
		mah_hdr_value(upgrade, u, sizeof(u));
		for (char *p = u; *p; p++)
			*p = (char)tolower((unsigned char)*p);
		if (strncmp(u, "websocket", 9) == 0)
			is_ws = true;
	}

	bool path_ws = (strncmp(path, "/ws", 3) == 0);

	if (is_ws && path_ws && ws_key) {
		char accept[64];
		if (!mah_ws_accept_key(ws_key, accept, sizeof(accept))) {
			bfree(req);
			return;
		}
		char resp[512];
		snprintf(resp, sizeof(resp),
			 "HTTP/1.1 101 Switching Protocols\r\n"
			 "Upgrade: websocket\r\n"
			 "Connection: Upgrade\r\n"
			 "Sec-WebSocket-Accept: %s\r\n"
			 "\r\n",
			 accept);
		if (mah_send_all(cfd, resp, strlen(resp)))
			mah_handle_ws(w, cfd);
		bfree(req);
		return;
	}

	/* GET / or /index.html → companion page */
	if (strcmp(method, "GET") == 0) {
		size_t html_len = 0;
		char *html = mah_load_index_html(&html_len);
		char hdr[256];
		snprintf(hdr, sizeof(hdr),
			 "HTTP/1.1 200 OK\r\n"
			 "Content-Type: text/html; charset=utf-8\r\n"
			 "Content-Length: %zu\r\n"
			 "Cache-Control: no-store\r\n"
			 "Connection: close\r\n"
			 "\r\n",
			 html_len);
		mah_send_all(cfd, hdr, strlen(hdr));
		mah_send_all(cfd, html, html_len);
		bfree(html);
	} else {
		const char *resp = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
		mah_send_all(cfd, resp, strlen(resp));
	}
	bfree(req);
}

static void *mah_web_thread(void *data)
{
	struct mah_web_state *w = data;

#ifdef _WIN32
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

	w->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (w->fd == MAH_INVALID_SOCKET) {
		mah_cam_set_status(w->cam, "Web socket() failed");
		return NULL;
	}

	int yes = 1;
	setsockopt(w->fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons((uint16_t)w->port);

	if (bind(w->fd, (struct sockaddr *)&addr, sizeof(addr)) != 0 || listen(w->fd, 2) != 0) {
		blog(LOG_WARNING, "[mah-mobile-cam] web bind failed on %d", w->port);
		mah_cam_set_status(w->cam, "Web bind failed on %d", w->port);
		closesocket(w->fd);
		w->fd = MAH_INVALID_SOCKET;
		return NULL;
	}

	char ip[64], pair[32];
	mah_cam_get_lan_ip(w->cam, ip, sizeof(ip));
	mah_cam_get_pairing(w->cam, pair, sizeof(pair));
	blog(LOG_INFO, "[mah-mobile-cam] web companion http://%s:%d/?p=%s", ip, w->port, pair);
	mah_cam_set_status(w->cam, "Phone: http://%s:%d/?p=%s", ip, w->port, pair);

	while (w->running) {
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(w->fd, &rfds);
		struct timeval tv = {.tv_sec = 1, .tv_usec = 0};
		int sel = select((int)w->fd + 1, &rfds, NULL, NULL, &tv);
		if (sel <= 0)
			continue;

		struct sockaddr_in client;
		mah_socklen_t clen = sizeof(client);
		mah_socket_t cfd = accept(w->fd, (struct sockaddr *)&client, &clen);
		if (cfd == MAH_INVALID_SOCKET)
			continue;

		mah_handle_http(w, cfd);
		closesocket(cfd);
	}

	if (w->fd != MAH_INVALID_SOCKET) {
		closesocket(w->fd);
		w->fd = MAH_INVALID_SOCKET;
	}
#ifdef _WIN32
	WSACleanup();
#endif
	return NULL;
}

bool mah_web_start(struct mah_mobile_cam *ctx, int web_port)
{
	mah_web_stop(ctx);
	if (web_port < 1024 || web_port > 65535)
		web_port = MAH_DEFAULT_WEB_PORT;

	struct mah_web_state *w = bzalloc(sizeof(*w));
	w->cam = ctx;
	w->port = web_port;
	w->fd = MAH_INVALID_SOCKET;
	w->running = true;
	mah_cam_set_web_slot(ctx, w);
	pthread_create(&w->thread, NULL, mah_web_thread, w);
	return true;
}

void mah_web_stop(struct mah_mobile_cam *ctx)
{
	struct mah_web_state *w = mah_cam_web_slot(ctx);
	if (!w)
		return;
	w->running = false;
	if (w->fd != MAH_INVALID_SOCKET) {
		closesocket(w->fd);
		w->fd = MAH_INVALID_SOCKET;
	}
	pthread_join(w->thread, NULL);
	mah_cam_set_web_slot(ctx, NULL);
	bfree(w);
}
