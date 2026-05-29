/*
 * Phase 2: OV3660 JPEG capture to microSD — press BOOT to shoot.
 */

#include <stdio.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/video.h>
#include <zephyr/drivers/video-controls.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/disk_access.h>

#if defined(CONFIG_FAT_FILESYSTEM_ELM)
#include <ff.h>
#endif

LOG_MODULE_REGISTER(main);

#define DISK_NAME   "SD"
#define MOUNT_POINT "/SD:"

#define CAM_WIDTH   2048
#define CAM_HEIGHT  1536

#define BOOT_NODE   DT_ALIAS(sw0)

static FATFS fat_fs;
static struct fs_mount_t mp = {
	.type = FS_FATFS,
	.fs_data = &fat_fs,
	.mnt_point = MOUNT_POINT,
};

#if DT_NODE_HAS_STATUS(BOOT_NODE, okay)
static const struct gpio_dt_spec boot_btn = GPIO_DT_SPEC_GET(BOOT_NODE, gpios);
#else
#error "Board overlay must define sw0 alias (BOOT button on GPIO0)"
#endif

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
	ret = fs_open(&file, path, FS_O_CREATE | FS_O_WRITE);
	if (ret) {
		LOG_ERR("fs_open %s failed: %d", path, ret);
		return ret;
	}

	ret = fs_write(&file, (uint8_t *)vbuf->buffer + start, jlen);
	if (ret < 0) {
		fs_close(&file);
		return ret;
	}

	LOG_INF("Wrote %s (%zd bytes JPEG from %u byte buffer)", path, jlen, vbuf->bytesused);
	fs_close(&file);
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
			LOG_ERR("capture pool alloc failed (%u bytes)", (unsigned int)bsize);
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
	LOG_INF("Capture pool: %u bytes x %d (PSRAM)", (unsigned int)bsize, NUM_CAPTURE_BUFS);
	return 0;
}

/* Re-queue both capture buffers after stream_stop for the next shot */
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
		/* discard spurious frames after stop */
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

	video_set_ctrl(video_dev, VIDEO_CID_VFLIP, (void *)1);
	video_set_ctrl(video_dev, VIDEO_CID_BRIGHTNESS, (void *)1);
	video_set_ctrl(video_dev, VIDEO_CID_SATURATION, (void *)0);
	video_set_ctrl(video_dev, VIDEO_CID_JPEG_COMPRESSION_QUALITY, (void *)12);

	LOG_INF("Camera: %ux%u JPEG", CAM_WIDTH, CAM_HEIGHT);
	return 0;
}

static void capture_status(const char *msg)
{
	printk("cam: %s\n", msg);
	LOG_INF("%s", msg);
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
		LOG_ERR("stream_start failed");
		capture_pool_restore_all(video_dev);
		return -EIO;
	}

	capture_status("camera streaming — waiting for warmup frame...");

	ret = video_dequeue(video_dev, VIDEO_EP_OUT, &vbuf, K_SECONDS(30));
	if (ret) {
		LOG_ERR("dequeue (warmup) failed: %d", ret);
		capture_pool_restore_all(video_dev);
		return ret;
	}
	capture_status("warmup done — waiting for capture frame...");
	video_enqueue(video_dev, VIDEO_EP_OUT, vbuf);

	ret = video_dequeue(video_dev, VIDEO_EP_OUT, &vbuf, K_SECONDS(30));
	if (ret) {
		LOG_ERR("dequeue failed: %d", ret);
		capture_pool_restore_all(video_dev);
		return ret;
	}

	LOG_INF("Frame captured: %u bytes", vbuf->bytesused);

	/* Stop DMA before SD write — else lcd_cam starts a 3rd frame and errors */
	video_stream_stop(video_dev);

	capture_status("writing JPEG to SD (may take a few seconds)...");

	ret = write_jpeg(path, vbuf);
	capture_pool_restore(video_dev, vbuf);

	if (ret == 0) {
		capture_status("capture complete");
	}

	return ret;
}

int main(void)
{
	const struct device *video_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_camera));
	static unsigned int shot_num;
	char path[32];
	int ret;

	printk("cam_capture_sd: boot\n");
	LOG_INF("OV3660 QXGA JPEG — press BOOT to capture");

	if (!device_is_ready(video_dev)) {
		LOG_ERR("Camera not ready");
		return 0;
	}

	ret = mount_sd();
	if (ret) {
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

	/* Auto-capture at boot so flash/monitor verifies the pipeline without BOOT */
	LOG_INF("Boot test capture → /SD:/ZEPHR000.JPG");
	ret = capture_and_save(video_dev, MOUNT_POINT "/ZEPHR000.JPG");
	if (ret) {
		LOG_ERR("Boot capture FAILED (%d) — check serial log before using BOOT", ret);
	} else {
		LOG_INF("Boot capture OK");
		shot_num = 1;
	}

	LOG_INF("Ready — press BOOT for ZEPHR%03u.JPG and up", shot_num);
	printk("\n>>> READY — press BOOT to capture (QXGA ~5 s per shot) <<<\n\n");

	while (1) {
		wait_for_button_press();
		snprintf(path, sizeof(path), MOUNT_POINT "/ZEPHR%03u.JPG", shot_num++);

		printk("\n>>> BOOT pressed — capturing %s <<<\n", path + strlen(MOUNT_POINT));
		ret = capture_and_save(video_dev, path);
		if (ret) {
			printk(">>> CAPTURE FAILED (%d) <<<\n", ret);
			LOG_ERR("Capture failed: %d", ret);
		} else {
			printk(">>> SAVED %s — press BOOT for next <<<\n\n", path + strlen(MOUNT_POINT));
		}
	}

	return 0;
}
