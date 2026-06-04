/*
 * Combined Phase 2 + 5: OV3660 JPEG capture to SD and Wi-Fi gallery.
 *
 *   BOOT button — save /SD:/ZEPHRnnn.JPG (1024x768 JPEG in this combined build)
 *   Browser     — http://<device-ip>/ lists and downloads *.JPG
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/video.h>
#include <zephyr/drivers/video-controls.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/storage/disk_access.h>

#include "ov3660.h"

#if defined(CONFIG_FAT_FILESYSTEM_ELM)
#include <ff.h>
#endif

LOG_MODULE_REGISTER(main);

#define DISK_NAME      "SD"
#define MOUNT_POINT    "/SD:"
#define HTTP_PORT      80
#define SEND_CHUNK     4096
#define REQ_BUF_SIZE   512

/* 1024x768 = native 1/2 QXGA path (sharper than arbitrary SVGA scale); QXGA needs cam_capture_sd */
#define CAM_WIDTH      1024
#define CAM_HEIGHT     768
#define BOOT_NODE      DT_ALIAS(sw0)

#if IS_ENABLED(CONFIG_WIFI_CREDENTIALS_STATIC)
#define WIFI_SSID CONFIG_WIFI_CREDENTIALS_STATIC_SSID
#define WIFI_PSK  CONFIG_WIFI_CREDENTIALS_STATIC_PASSWORD
#else
#define WIFI_SSID ""
#define WIFI_PSK  ""
#endif

#define HTTP_STACK_SIZE  10240
#define HTTP_YIELD_EVERY 16

K_MUTEX_DEFINE(sd_fs_mutex);
static int http_listen_fd = -1;
K_THREAD_STACK_DEFINE(http_stack, HTTP_STACK_SIZE);
static struct k_thread http_thread_data;

static FATFS fat_fs;
static struct fs_mount_t mp = {
	.type = FS_FATFS,
	.fs_data = &fat_fs,
	.mnt_point = MOUNT_POINT,
};

static K_SEM_DEFINE(net_ready, 0, 1);
static char device_ip[NET_IPV4_ADDR_LEN];

#if DT_NODE_HAS_STATUS(BOOT_NODE, okay)
static const struct gpio_dt_spec boot_btn = GPIO_DT_SPEC_GET(BOOT_NODE, gpios);
#else
#error "Board overlay must define sw0 alias (BOOT button on GPIO0)"
#endif

/* --- SD mount --- */

static int mount_sd(void)
{
	const char *disk = DISK_NAME;
	int ret;

	ret = disk_access_ioctl(disk, DISK_IOCTL_CTRL_INIT, NULL);
	if (ret) {
		LOG_ERR("SD init failed: %d", ret);
		return ret;
	}

	ret = fs_mount(&mp);
	if (ret) {
		LOG_ERR("SD mount failed: %d", ret);
		return ret;
	}

	LOG_INF("SD mounted at %s", MOUNT_POINT);
	return 0;
}

/* --- Wi-Fi --- */

static void wifi_event_handler(struct net_mgmt_event_callback *cb,
			       uint32_t mgmt_event, struct net_if *iface)
{
	if (mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
		const struct wifi_status *status = (const struct wifi_status *)cb->info;

		if (status->status) {
			LOG_ERR("Wi-Fi connect failed: %d", status->status);
		} else {
			LOG_INF("Wi-Fi connected");
		}
	}
}

static void ipv4_event_handler(struct net_mgmt_event_callback *cb,
			       uint32_t mgmt_event, struct net_if *iface)
{
	ARG_UNUSED(cb);

	if (mgmt_event != NET_EVENT_IPV4_ADDR_ADD) {
		return;
	}

	for (int i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
		if (iface->config.ip.ipv4->unicast[i].ipv4.addr_type != NET_ADDR_DHCP) {
			continue;
		}

		net_addr_ntop(AF_INET,
			      &iface->config.ip.ipv4->unicast[i].ipv4.address.in_addr,
			      device_ip, sizeof(device_ip));
		LOG_INF("DHCP address: %s", device_ip);
		k_sem_give(&net_ready);
		return;
	}
}

static int wifi_connect_sta(void)
{
	struct net_if *iface = net_if_get_wifi_sta();
	struct wifi_connect_req_params params = {0};
	int ret;

	if (!iface) {
		return -ENODEV;
	}

	if (strlen(WIFI_SSID) == 0 || strcmp(WIFI_SSID, "YOUR_WIFI_SSID") == 0) {
		LOG_ERR("Set config/wifi-credentials.conf before building");
		return -EINVAL;
	}

	ret = net_if_up(iface);
	if (ret && ret != -EALREADY) {
		return ret;
	}

	params.ssid = (const uint8_t *)WIFI_SSID;
	params.ssid_length = strlen(WIFI_SSID);
	params.psk = (const uint8_t *)WIFI_PSK;
	params.psk_length = strlen(WIFI_PSK);
	params.security = strlen(WIFI_PSK) > 0 ? WIFI_SECURITY_TYPE_PSK
					       : WIFI_SECURITY_TYPE_NONE;
	params.channel = WIFI_CHANNEL_ANY;
	params.band = WIFI_FREQ_BAND_2_4_GHZ;

	LOG_INF("Connecting to \"%s\"...", WIFI_SSID);
	return net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params));
}

/* --- HTTP gallery (Phase 5) --- */

static bool ends_with_jpg(const char *name)
{
	size_t len = strlen(name);

	if (len < 5) {
		return false;
	}

	return name[len - 4] == '.' &&
	       (toupper((unsigned char)name[len - 3]) == 'J') &&
	       (toupper((unsigned char)name[len - 2]) == 'P') &&
	       (toupper((unsigned char)name[len - 1]) == 'G');
}

static bool safe_filename(const char *name)
{
	size_t len = strlen(name);

	if (len == 0 || len > 12) {
		return false;
	}

	for (size_t i = 0; i < len; i++) {
		char c = name[i];

		if (c == '.' || c == '-' || c == '_') {
			continue;
		}
		if (!isalnum((unsigned char)c)) {
			return false;
		}
	}

	return ends_with_jpg(name);
}

static int recv_until_headers_done(int client, char *buf, size_t buf_len)
{
	size_t total = 0;

	while (total + 1 < buf_len) {
		size_t space = buf_len - total - 1;
		ssize_t n = recv(client, buf + total, space, 0);

		if (n <= 0) {
			return n < 0 ? -errno : -ECONNRESET;
		}

		total += (size_t)n;
		buf[total] = '\0';

		if (strstr(buf, "\r\n\r\n") != NULL) {
			return (int)total;
		}
	}

	return -EMSGSIZE;
}

static int send_all(int sock, const void *data, size_t len)
{
	const uint8_t *p = data;

	while (len > 0) {
		ssize_t sent = send(sock, p, len, 0);

		if (sent <= 0) {
			return sent < 0 ? -errno : -ECONNRESET;
		}
		p += sent;
		len -= (size_t)sent;
	}

	return 0;
}

static int send_http_response(int client, int code, const char *status,
			      const char *content_type, const char *body)
{
	char hdr[256];
	int body_len = body ? (int)strlen(body) : 0;
	int hlen;

	hlen = snprintk(hdr, sizeof(hdr),
			"HTTP/1.1 %d %s\r\n"
			"Connection: close\r\n"
			"Content-Type: %s\r\n"
			"Content-Length: %d\r\n"
			"\r\n",
			code, status, content_type, body_len);
	if (hlen < 0 || hlen >= (int)sizeof(hdr)) {
		return -ENOMEM;
	}

	if (send_all(client, hdr, (size_t)hlen) != 0) {
		return -EIO;
	}
	if (body && body_len > 0) {
		return send_all(client, body, (size_t)body_len);
	}
	return 0;
}

static int send_gallery_page(int client)
{
	struct fs_dir_t dir;
	struct fs_dirent entry;
	char body[4096];
	size_t off = 0;
	int ret;

	fs_dir_t_init(&dir);
	k_mutex_lock(&sd_fs_mutex, K_FOREVER);
	ret = fs_opendir(&dir, MOUNT_POINT);
	if (ret) {
		k_mutex_unlock(&sd_fs_mutex);
		return send_http_response(client, 500, "Internal Server Error",
					  "text/plain", "SD read error\r\n");
	}

	off += snprintk(body + off, sizeof(body) - off,
			"<!DOCTYPE html><html><head>"
			"<meta charset=\"utf-8\">"
			"<title>XIAO Cam Gallery</title>"
			"<style>body{font-family:sans-serif;margin:2em}"
			"a{display:block;margin:0.4em 0}</style></head><body>"
			"<h1>SD card images</h1>"
			"<p>Press BOOT on the XIAO to capture ZEPHRnnn.JPG</p><ul>");

	while (off + 128 < sizeof(body)) {
		ret = fs_readdir(&dir, &entry);
		if (ret || entry.name[0] == '\0') {
			break;
		}
		if (entry.type != FS_DIR_ENTRY_FILE || !ends_with_jpg(entry.name)) {
			continue;
		}

		off += snprintk(body + off, sizeof(body) - off,
				"<li><a href=\"/img/%s\">%s</a> (%zu bytes)</li>",
				entry.name, entry.name, entry.size);
	}

	fs_closedir(&dir);
	k_mutex_unlock(&sd_fs_mutex);

	off += snprintk(body + off, sizeof(body) - off, "</ul></body></html>");

	return send_http_response(client, 200, "OK", "text/html; charset=utf-8", body);
}

static int send_jpeg_file(int client, const char *filename)
{
	char path[32];
	struct fs_file_t file;
	struct fs_dirent entry;
	static uint8_t chunk[SEND_CHUNK];
	char hdr[256];
	int ret;
	unsigned int chunks = 0;

	snprintk(path, sizeof(path), "%s/%s", MOUNT_POINT, filename);

	k_mutex_lock(&sd_fs_mutex, K_FOREVER);
	ret = fs_stat(path, &entry);
	if (ret || entry.type != FS_DIR_ENTRY_FILE) {
		k_mutex_unlock(&sd_fs_mutex);
		return send_http_response(client, 404, "Not Found", "text/plain",
					  "File not found\r\n");
	}

	fs_file_t_init(&file);
	ret = fs_open(&file, path, FS_O_READ);
	if (ret) {
		k_mutex_unlock(&sd_fs_mutex);
		return send_http_response(client, 500, "Internal Server Error",
					  "text/plain", "Open failed\r\n");
	}
	k_mutex_unlock(&sd_fs_mutex);

	ret = snprintk(hdr, sizeof(hdr),
		       "HTTP/1.1 200 OK\r\n"
		       "Connection: close\r\n"
		       "Content-Type: image/jpeg\r\n"
		       "Content-Disposition: attachment; filename=\"%s\"\r\n"
		       "Content-Length: %zu\r\n"
		       "\r\n",
		       filename, entry.size);
	if (ret < 0 || ret >= (int)sizeof(hdr)) {
		k_mutex_lock(&sd_fs_mutex, K_FOREVER);
		fs_close(&file);
		k_mutex_unlock(&sd_fs_mutex);
		return -ENOMEM;
	}

	if (send_all(client, hdr, (size_t)ret) != 0) {
		k_mutex_lock(&sd_fs_mutex, K_FOREVER);
		fs_close(&file);
		k_mutex_unlock(&sd_fs_mutex);
		return -EIO;
	}

	while (true) {
		ssize_t n;

		k_mutex_lock(&sd_fs_mutex, K_FOREVER);
		n = fs_read(&file, chunk, sizeof(chunk));
		if (n < 0) {
			fs_close(&file);
			k_mutex_unlock(&sd_fs_mutex);
			LOG_ERR("SD read failed for %s: %d", filename, (int)n);
			return (int)n;
		}
		if (n == 0) {
			fs_close(&file);
			k_mutex_unlock(&sd_fs_mutex);
			return 0;
		}
		k_mutex_unlock(&sd_fs_mutex);

		if (send_all(client, chunk, (size_t)n) != 0) {
			LOG_ERR("TCP send failed for %s", filename);
			k_mutex_lock(&sd_fs_mutex, K_FOREVER);
			fs_close(&file);
			k_mutex_unlock(&sd_fs_mutex);
			return -EIO;
		}

		if ((++chunks % HTTP_YIELD_EVERY) == 0) {
			k_yield();
		}
	}
}

static void handle_client(int client)
{
	char req[REQ_BUF_SIZE];
	char method[8];
	char url[64];
	int ret;

	ret = recv_until_headers_done(client, req, sizeof(req));
	if (ret <= 0) {
		return;
	}

	if (sscanf(req, "%7s %63s", method, url) != 2) {
		send_http_response(client, 400, "Bad Request", "text/plain",
				   "Bad request\r\n");
		return;
	}

	if (strcmp(method, "GET") != 0) {
		send_http_response(client, 405, "Method Not Allowed", "text/plain",
				   "GET only\r\n");
		return;
	}

	if (strcmp(url, "/") == 0) {
		send_gallery_page(client);
		return;
	}

	if (strncmp(url, "/img/", 5) == 0) {
		const char *name = url + 5;

		if (!safe_filename(name)) {
			send_http_response(client, 400, "Bad Request", "text/plain",
					   "Invalid filename\r\n");
			return;
		}
		send_jpeg_file(client, name);
		return;
	}

	send_http_response(client, 404, "Not Found", "text/plain", "Not found\r\n");
}

static int http_listen_socket_create(void)
{
	struct sockaddr_in bind_addr;

	http_listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (http_listen_fd < 0) {
		LOG_ERR("socket failed: %d", errno);
		return -errno;
	}

	bind_addr.sin_family = AF_INET;
	bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind_addr.sin_port = htons(HTTP_PORT);

	if (bind(http_listen_fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
		LOG_ERR("bind failed: %d", errno);
		close(http_listen_fd);
		http_listen_fd = -1;
		return -errno;
	}

	if (listen(http_listen_fd, 2) < 0) {
		LOG_ERR("listen failed: %d", errno);
		close(http_listen_fd);
		http_listen_fd = -1;
		return -errno;
	}

	return 0;
}

static void http_server_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	if (http_listen_socket_create() != 0) {
		return;
	}

	printk("\n>>> Gallery: http://%s/  |  BOOT = capture <<<\n\n", device_ip);
	LOG_INF("HTTP server on port %d", HTTP_PORT);

	while (true) {
		struct sockaddr_in client_addr;
		socklen_t client_len = sizeof(client_addr);
		int client = accept(http_listen_fd, (struct sockaddr *)&client_addr, &client_len);

		if (client < 0) {
			if (errno == EINTR) {
				continue;
			}
			LOG_ERR("accept failed: %d", errno);
			k_msleep(500);
			continue;
		}

		handle_client(client);
		(void)close(client);
	}
}

/* --- Camera capture (Phase 2) --- */

static int boot_button_init(void)
{
	if (!gpio_is_ready_dt(&boot_btn)) {
		LOG_ERR("BOOT button GPIO not ready");
		return -ENODEV;
	}

	return gpio_pin_configure_dt(&boot_btn, GPIO_INPUT);
}

static bool boot_button_pressed(void)
{
	return gpio_pin_get_dt(&boot_btn) == 0;
}

static void wait_for_button_release(void)
{
	while (boot_button_pressed()) {
		k_msleep(10);
	}
	k_msleep(50);
}

static void wait_for_button_press(void)
{
	while (!boot_button_pressed()) {
		k_msleep(50);
	}
	k_msleep(30);
	wait_for_button_release();
}

static ssize_t jpeg_find(const uint8_t *buf, size_t len, size_t *start, size_t *end)
{
	size_t soi = (size_t)-1;
	size_t eoi = (size_t)-1;

	for (size_t i = 0; i + 1 < len; i++) {
		if (buf[i] == 0xff && buf[i + 1] == 0xd8) {
			soi = i;
			break;
		}
	}

	if (soi == (size_t)-1) {
		return -ENOENT;
	}

	for (size_t i = len - 2; i > soi; i--) {
		if (buf[i] == 0xff && buf[i + 1] == 0xd9) {
			eoi = i + 2;
			break;
		}
	}

	if (eoi == (size_t)-1) {
		return -ENOENT;
	}

	*start = soi;
	*end = eoi;
	return (ssize_t)(eoi - soi);
}

static int write_jpeg(const char *path, const struct video_buffer *vbuf)
{
	struct fs_file_t file;
	size_t start;
	size_t end;
	ssize_t jlen;
	int ret;

	jlen = jpeg_find(vbuf->buffer, vbuf->bytesused, &start, &end);
	if (jlen < 0) {
		LOG_ERR("JPEG SOI/EOI not found in %u byte buffer", vbuf->bytesused);
		return (int)jlen;
	}

	fs_file_t_init(&file);
	k_mutex_lock(&sd_fs_mutex, K_FOREVER);
	ret = fs_open(&file, path, FS_O_CREATE | FS_O_WRITE);
	if (ret) {
		k_mutex_unlock(&sd_fs_mutex);
		LOG_ERR("fs_open %s failed: %d", path, ret);
		return ret;
	}

	ret = fs_write(&file, (uint8_t *)vbuf->buffer + start, jlen);
	if (ret < 0) {
		fs_close(&file);
		k_mutex_unlock(&sd_fs_mutex);
		return ret;
	}

	fs_close(&file);
	k_mutex_unlock(&sd_fs_mutex);

	LOG_INF("Wrote %s (%zd bytes JPEG)", path, jlen);
	return 0;
}

#define NUM_CAPTURE_BUFS 2

static struct video_buffer *cap_bufs[NUM_CAPTURE_BUFS];
static bool cap_bufs_ready;

static int capture_pool_init(const struct device *video_dev, size_t bsize)
{
	if (cap_bufs_ready) {
		return 0;
	}

	for (int i = 0; i < NUM_CAPTURE_BUFS; i++) {
		cap_bufs[i] = video_buffer_aligned_alloc(bsize, 32, K_FOREVER);
		if (cap_bufs[i] == NULL) {
			for (int j = 0; j < i; j++) {
				video_buffer_release(cap_bufs[j]);
				cap_bufs[j] = NULL;
			}
			return -ENOMEM;
		}
	}

	for (int i = 0; i < NUM_CAPTURE_BUFS; i++) {
		video_enqueue(video_dev, VIDEO_EP_OUT, cap_bufs[i]);
	}

	cap_bufs_ready = true;
	LOG_INF("Capture pool: %u bytes x %d", (unsigned int)bsize, NUM_CAPTURE_BUFS);
	return 0;
}

static void capture_pool_restore_all(const struct device *video_dev)
{
	video_stream_stop(video_dev);

	for (int i = 0; i < NUM_CAPTURE_BUFS; i++) {
		video_enqueue(video_dev, VIDEO_EP_OUT, cap_bufs[i]);
	}
}

static void capture_pool_restore(const struct device *video_dev, struct video_buffer *last)
{
	struct video_buffer *other = (last == cap_bufs[0]) ? cap_bufs[1] : cap_bufs[0];
	struct video_buffer *stray;

	video_stream_stop(video_dev);

	while (video_dequeue(video_dev, VIDEO_EP_OUT, &stray, K_NO_WAIT) == 0) {
		/* discard */
	}

	video_enqueue(video_dev, VIDEO_EP_OUT, last);
	video_enqueue(video_dev, VIDEO_EP_OUT, other);
}

static int camera_configure(const struct device *video_dev)
{
	struct video_format fmt;

	if (video_get_format(video_dev, VIDEO_EP_OUT, &fmt)) {
		return -EIO;
	}

	fmt.width = CAM_WIDTH;
	fmt.height = CAM_HEIGHT;
	fmt.pixelformat = VIDEO_PIX_FMT_JPEG;
	fmt.pitch = CAM_WIDTH;

	if (video_set_format(video_dev, VIDEO_EP_OUT, &fmt)) {
		return -EIO;
	}

	/* Seeed Arduino OV3660 tuning + indoor exposure (bright room) */
	video_set_ctrl(video_dev, VIDEO_CID_VFLIP, (void *)1);
	video_set_ctrl(video_dev, VIDEO_CID_BRIGHTNESS, (void *)0);
	video_set_ctrl(video_dev, VIDEO_CID_SATURATION, (void *)-3);
	video_set_ctrl(video_dev, VIDEO_CID_CONTRAST, (void *)1);
	video_set_ctrl(video_dev, OV3660_CID_AE_LEVEL, (void *)-3);
	video_set_ctrl(video_dev, OV3660_CID_SHARPNESS, (void *)2);
	video_set_ctrl(video_dev, VIDEO_CID_JPEG_COMPRESSION_QUALITY, (void *)10);

	LOG_INF("Camera: %ux%u JPEG", CAM_WIDTH, CAM_HEIGHT);
	return 0;
}

static int capture_and_save(const struct device *video_dev, const char *path)
{
	struct video_buffer *vbuf;
	struct video_format fmt;
	size_t bsize;
	int ret;

	if (video_get_format(video_dev, VIDEO_EP_OUT, &fmt)) {
		return -EIO;
	}

	bsize = fmt.pitch * fmt.height;

	ret = capture_pool_init(video_dev, bsize);
	if (ret) {
		return ret;
	}

	video_stream_stop(video_dev);

	if (video_stream_start(video_dev)) {
		capture_pool_restore_all(video_dev);
		return -EIO;
	}

	/* Discard two warmup frames (AWB/AEC settle) */
	for (int i = 0; i < 2; i++) {
		ret = video_dequeue(video_dev, VIDEO_EP_OUT, &vbuf, K_SECONDS(30));
		if (ret) {
			capture_pool_restore_all(video_dev);
			return ret;
		}
		video_enqueue(video_dev, VIDEO_EP_OUT, vbuf);
	}

	ret = video_dequeue(video_dev, VIDEO_EP_OUT, &vbuf, K_SECONDS(30));
	if (ret) {
		capture_pool_restore_all(video_dev);
		return ret;
	}

	LOG_INF("Frame: %u bytes", vbuf->bytesused);
	video_stream_stop(video_dev);

	ret = write_jpeg(path, vbuf);
	capture_pool_restore(video_dev, vbuf);

	return ret;
}

static unsigned int scan_next_shot_num(void)
{
	struct fs_dir_t dir;
	struct fs_dirent entry;
	unsigned int max = 0;
	int ret;

	fs_dir_t_init(&dir);
	k_mutex_lock(&sd_fs_mutex, K_FOREVER);
	ret = fs_opendir(&dir, MOUNT_POINT);
	if (ret) {
		k_mutex_unlock(&sd_fs_mutex);
		return 0;
	}

	while (fs_readdir(&dir, &entry) == 0 && entry.name[0] != '\0') {
		unsigned int n;

		if (strncmp(entry.name, "ZEPHR", 5) != 0) {
			continue;
		}
		if (sscanf(entry.name, "ZEPHR%u", &n) == 1 && n >= max) {
			max = n + 1;
		}
	}

	fs_closedir(&dir);
	k_mutex_unlock(&sd_fs_mutex);
	return max;
}

/* --- Main --- */

int main(void)
{
	const struct device *video_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_camera));
	struct net_mgmt_event_callback wifi_cb;
	struct net_mgmt_event_callback ipv4_cb;
	unsigned int shot_num;
	char path[32];
	int ret;

	printk("cam_gallery: camera + Wi-Fi SD browser\n");

	if (mount_sd() != 0) {
		printk("SD mount failed — insert card and reset\n");
		return 0;
	}

	shot_num = scan_next_shot_num();

	net_mgmt_init_event_callback(&wifi_cb, wifi_event_handler,
				     NET_EVENT_WIFI_CONNECT_RESULT);
	net_mgmt_add_event_callback(&wifi_cb);

	net_mgmt_init_event_callback(&ipv4_cb, ipv4_event_handler,
				     NET_EVENT_IPV4_ADDR_ADD);
	net_mgmt_add_event_callback(&ipv4_cb);

	k_sleep(K_SECONDS(1));

	if (wifi_connect_sta() != 0) {
		printk("Wi-Fi failed — check wifi-credentials.conf\n");
		return 0;
	}

	if (k_sem_take(&net_ready, K_SECONDS(30)) != 0) {
		printk("DHCP timeout\n");
		return 0;
	}

	ret = device_init(video_dev);
	if (ret) {
		LOG_ERR("Camera device_init failed: %d", ret);
		return 0;
	}
	if (!device_is_ready(video_dev)) {
		LOG_ERR("Camera not ready after init");
		return 0;
	}

	ret = boot_button_init();
	if (ret) {
		return 0;
	}

	ret = camera_configure(video_dev);
	if (ret) {
		LOG_ERR("Camera configure failed: %d", ret);
		return 0;
	}

	k_thread_create(&http_thread_data, http_stack,
			K_THREAD_STACK_SIZEOF(http_stack),
			http_server_thread, NULL, NULL, NULL,
			K_PRIO_COOP(7), 0, K_NO_WAIT);
	k_thread_name_set(&http_thread_data, "http");

	LOG_INF("Next capture: ZEPHR%03u.JPG", shot_num);
	printk("\n>>> READY: BOOT=capture ZEPHR%03u+, gallery http://%s/ <<<\n\n",
	       shot_num, device_ip);

	while (true) {
		if (!boot_button_pressed()) {
			k_msleep(50);
			continue;
		}

		wait_for_button_release();
		snprintf(path, sizeof(path), MOUNT_POINT "/ZEPHR%03u.JPG", shot_num++);

		printk("\n>>> Capturing %s (~2 s) <<<\n", path + strlen(MOUNT_POINT));
		ret = capture_and_save(video_dev, path);
		if (ret) {
			printk(">>> CAPTURE FAILED (%d) <<<\n", ret);
			shot_num--;
		} else {
			printk(">>> SAVED — refresh browser to see %s <<<\n\n",
			       path + strlen(MOUNT_POINT));
		}
	}

	return 0;
}
