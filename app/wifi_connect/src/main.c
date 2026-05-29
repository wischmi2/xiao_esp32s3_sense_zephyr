/*
 * Phase 4: Join Wi-Fi AP, obtain DHCP address, print IP on serial.
 *
 * Credentials: copy config/wifi-credentials.conf.example to
 * config/wifi-credentials.conf and set SSID/password before building.
 *
 * Fallback: use the shell — wifi scan, wifi connect "SSID" <sec> password
 */

#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>

LOG_MODULE_REGISTER(main);

#if IS_ENABLED(CONFIG_WIFI_CREDENTIALS_STATIC)
#define WIFI_SSID CONFIG_WIFI_CREDENTIALS_STATIC_SSID
#define WIFI_PSK  CONFIG_WIFI_CREDENTIALS_STATIC_PASSWORD
#else
#define WIFI_SSID ""
#define WIFI_PSK  ""
#endif

static K_SEM_DEFINE(addr_sem, 0, 1);
static char ip_str[NET_IPV4_ADDR_LEN];

static void wifi_event_handler(struct net_mgmt_event_callback *cb,
			       uint32_t mgmt_event, struct net_if *iface)
{
	if (mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
		const struct wifi_status *status = (const struct wifi_status *)cb->info;

		if (status->status) {
			LOG_ERR("Wi-Fi connect failed: %d", status->status);
		} else {
			LOG_INF("Wi-Fi connected to %s", WIFI_SSID);
		}
	} else if (mgmt_event == NET_EVENT_WIFI_DISCONNECT_RESULT) {
		LOG_WRN("Wi-Fi disconnected");
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
			      ip_str, sizeof(ip_str));
		LOG_INF("DHCP address: %s", ip_str);
		printk("\n>>> Wi-Fi ready — ping %s <<<\n\n", ip_str);
		k_sem_give(&addr_sem);
		return;
	}
}

static int wifi_connect_sta(void)
{
	struct net_if *iface = net_if_get_wifi_sta();
	struct wifi_connect_req_params params = {0};
	int ret;

	if (!iface) {
		LOG_ERR("No Wi-Fi STA interface");
		return -ENODEV;
	}

	if (strlen(WIFI_SSID) == 0 || strcmp(WIFI_SSID, "YOUR_WIFI_SSID") == 0) {
		LOG_WRN("No credentials — set config/wifi-credentials.conf");
		printk("\n>>> Set wifi-credentials.conf, or use: wifi scan / wifi connect <<<\n\n");
		return -EINVAL;
	}

	ret = net_if_up(iface);
	if (ret && ret != -EALREADY) {
		LOG_ERR("net_if_up failed: %d", ret);
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
	ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params));
	if (ret) {
		LOG_ERR("NET_REQUEST_WIFI_CONNECT failed: %d", ret);
		return ret;
	}

	return 0;
}

int main(void)
{
	struct net_mgmt_event_callback wifi_cb;
	struct net_mgmt_event_callback ipv4_cb;

	printk("Phase 4: Wi-Fi connectivity\n");

	net_mgmt_init_event_callback(&wifi_cb, wifi_event_handler,
				     NET_EVENT_WIFI_CONNECT_RESULT |
					     NET_EVENT_WIFI_DISCONNECT_RESULT);
	net_mgmt_add_event_callback(&wifi_cb);

	net_mgmt_init_event_callback(&ipv4_cb, ipv4_event_handler,
				     NET_EVENT_IPV4_ADDR_ADD);
	net_mgmt_add_event_callback(&ipv4_cb);

	k_sleep(K_SECONDS(1));

	if (wifi_connect_sta() == 0) {
		if (k_sem_take(&addr_sem, K_SECONDS(30)) != 0) {
			LOG_ERR("Timed out waiting for DHCP");
		}
	}

	while (true) {
		k_sleep(K_SECONDS(60));
	}

	return 0;
}
