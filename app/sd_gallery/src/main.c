/*
 * Phase 5: Browse and download JPEG files from microSD over Wi-Fi.
 *
 * Open http://<device-ip>/ in a browser on the same LAN.
 *   GET /              — HTML gallery listing *.JPG on /SD:
 *   GET /img/NAME.JPG  — download a file (supports multi-MB QXGA shots)
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/fs/fs.h>
#include <zephyr/storage/disk_access.h>

#if defined(CONFIG_FAT_FILESYSTEM_ELM)
#include <ff.h>
#endif

LOG_MODULE_REGISTER(main);

#define DISK_NAME      "SD"
#define MOUNT_POINT    "/SD:"
#define HTTP_PORT      80
#define SEND_CHUNK     4096
#define REQ_BUF_SIZE   512

#if IS_ENABLED(CONFIG_WIFI_CREDENTIALS_STATIC)
#define WIFI_SSID CONFIG_WIFI_CREDENTIALS_STATIC_SSID
#define WIFI_PSK  CONFIG_WIFI_CREDENTIALS_STATIC_PASSWORD
#else
#define WIFI_SSID ""
#define WIFI_PSK  ""
#endif

static FATFS fat_fs;
static struct fs_mount_t mp = {
	.type = FS_FATFS,
	.fs_data = &fat_fs,
	.mnt_point = MOUNT_POINT,
};

static K_SEM_DEFINE(net_ready, 0, 1);
static char device_ip[NET_IPV4_ADDR_LEN];

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
		ssize_t n = recv(client, buf + total, 1, 0);

		if (n <= 0) {
			return n < 0 ? -errno : -ECONNRESET;
		}

		total += (size_t)n;
		buf[total] = '\0';

		if (total >= 4 &&
		    buf[total - 4] == '\r' && buf[total - 3] == '\n' &&
		    buf[total - 2] == '\r' && buf[total - 1] == '\n') {
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
	ret = fs_opendir(&dir, MOUNT_POINT);
	if (ret) {
		return send_http_response(client, 500, "Internal Server Error",
					  "text/plain", "SD read error\r\n");
	}

	off += snprintk(body + off, sizeof(body) - off,
			"<!DOCTYPE html><html><head>"
			"<meta charset=\"utf-8\">"
			"<title>XIAO SD Gallery</title>"
			"<style>body{font-family:sans-serif;margin:2em}"
			"a{display:block;margin:0.4em 0}</style></head><body>"
			"<h1>SD card images</h1><ul>");

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

	off += snprintk(body + off, sizeof(body) - off, "</ul></body></html>");

	return send_http_response(client, 200, "OK", "text/html; charset=utf-8", body);
}

static int send_jpeg_file(int client, const char *filename)
{
	char path[32];
	struct fs_file_t file;
	struct fs_dirent entry;
	uint8_t chunk[SEND_CHUNK];
	char hdr[192];
	int ret;

	snprintk(path, sizeof(path), "%s/%s", MOUNT_POINT, filename);

	ret = fs_stat(path, &entry);
	if (ret || entry.type != FS_DIR_ENTRY_FILE) {
		return send_http_response(client, 404, "Not Found", "text/plain",
					  "File not found\r\n");
	}

	fs_file_t_init(&file);
	ret = fs_open(&file, path, FS_O_READ);
	if (ret) {
		return send_http_response(client, 500, "Internal Server Error",
					  "text/plain", "Open failed\r\n");
	}

	ret = snprintk(hdr, sizeof(hdr),
		       "HTTP/1.1 200 OK\r\n"
		       "Connection: close\r\n"
		       "Content-Type: image/jpeg\r\n"
		       "Content-Length: %zu\r\n"
		       "\r\n",
		       entry.size);
	if (ret < 0 || send_all(client, hdr, (size_t)ret) != 0) {
		fs_close(&file);
		return -EIO;
	}

	while (true) {
		ssize_t n = fs_read(&file, chunk, sizeof(chunk));

		if (n < 0) {
			fs_close(&file);
			return (int)n;
		}
		if (n == 0) {
			break;
		}
		if (send_all(client, chunk, (size_t)n) != 0) {
			fs_close(&file);
			return -EIO;
		}
	}

	fs_close(&file);
	return 0;
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

static void http_server_loop(void)
{
	int serv;
	struct sockaddr_in bind_addr;

	serv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (serv < 0) {
		LOG_ERR("socket failed: %d", errno);
		return;
	}

	bind_addr.sin_family = AF_INET;
	bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind_addr.sin_port = htons(HTTP_PORT);

	if (bind(serv, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
		LOG_ERR("bind failed: %d", errno);
		(void)close(serv);
		return;
	}

	if (listen(serv, 2) < 0) {
		LOG_ERR("listen failed: %d", errno);
		(void)close(serv);
		return;
	}

	printk("\n>>> SD gallery at http://%s/ <<<\n\n", device_ip);
	LOG_INF("HTTP server listening on port %d", HTTP_PORT);

	while (true) {
		struct sockaddr_in client_addr;
		socklen_t client_len = sizeof(client_addr);
		int client = accept(serv, (struct sockaddr *)&client_addr, &client_len);

		if (client < 0) {
			if (errno == EAGAIN || errno == EINTR) {
				k_msleep(100);
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

int main(void)
{
	struct net_mgmt_event_callback wifi_cb;
	struct net_mgmt_event_callback ipv4_cb;

	printk("Phase 5: SD gallery over Wi-Fi\n");

	if (mount_sd() != 0) {
		printk("SD mount failed — insert card and reset\n");
		return 0;
	}

	net_mgmt_init_event_callback(&wifi_cb, wifi_event_handler,
				     NET_EVENT_WIFI_CONNECT_RESULT);
	net_mgmt_add_event_callback(&wifi_cb);

	net_mgmt_init_event_callback(&ipv4_cb, ipv4_event_handler,
				     NET_EVENT_IPV4_ADDR_ADD);
	net_mgmt_add_event_callback(&ipv4_cb);

	k_sleep(K_SECONDS(1));

	if (wifi_connect_sta() != 0) {
		printk("Wi-Fi setup failed — check wifi-credentials.conf\n");
		return 0;
	}

	if (k_sem_take(&net_ready, K_SECONDS(30)) != 0) {
		printk("Timed out waiting for DHCP\n");
		return 0;
	}

	http_server_loop();
	return 0;
}
