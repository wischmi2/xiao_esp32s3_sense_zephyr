/*
 * Copyright (c) 2026 Brian Wischmeyer
 * SPDX-License-Identifier: Apache-2.0
 *
 * OV3660 video sensor driver for Zephyr (16-bit SCCB, DVP).
 * Init tables from esp32-camera; API modeled on drivers/video/ov5640.c.
 */

#define DT_DRV_COMPAT ovti_ov3660

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/video.h>
#include <zephyr/drivers/video-controls.h>
#include <zephyr/dt-bindings/video/video-interfaces.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include "ov3660_init_regs.h"

LOG_MODULE_REGISTER(video_ov3660, CONFIG_VIDEO_LOG_LEVEL);

#define CHIP_ID_REG 0x300a
#define CHIP_ID_VAL 0x3660

#define OV3660_VIDEO_FORMAT_CAP(fourcc, w, h)                                                      \
	{                                                                                          \
		.pixelformat = (fourcc), .width_min = (w), .width_max = (w),                       \
		.width_step = 0, .height_min = (h), .height_max = (h), .height_step = 0,           \
	}

struct ov3660_config {
	struct i2c_dt_spec i2c;
};

struct ov3660_data {
	struct video_format fmt;
	bool vflip;
	bool hmirror;
	bool binning;
	bool scale;
	uint8_t jpeg_quality;
};

static const struct video_format_cap ov3660_fmts[] = {
	OV3660_VIDEO_FORMAT_CAP(VIDEO_PIX_FMT_RGB565, 320, 240),
	OV3660_VIDEO_FORMAT_CAP(VIDEO_PIX_FMT_JPEG, 320, 240),
	OV3660_VIDEO_FORMAT_CAP(VIDEO_PIX_FMT_RGB565, OV3660_MAX_WIDTH, OV3660_MAX_HEIGHT),
	OV3660_VIDEO_FORMAT_CAP(VIDEO_PIX_FMT_JPEG, OV3660_MAX_WIDTH, OV3660_MAX_HEIGHT),
	{0},
};

static int ov3660_read_reg(const struct i2c_dt_spec *spec, uint16_t addr, uint8_t *val)
{
	uint8_t addr_be[2] = { addr >> 8, addr & 0xff };

	return i2c_write_read_dt(spec, addr_be, sizeof(addr_be), val, 1);
}

static int ov3660_write_reg(const struct i2c_dt_spec *spec, uint16_t addr, uint8_t val)
{
	uint8_t buf[3] = { addr >> 8, addr & 0xff, val };

	return i2c_write_dt(spec, buf, sizeof(buf));
}

static int ov3660_modify_reg(const struct i2c_dt_spec *spec, uint16_t addr, uint8_t mask,
			     uint8_t val)
{
	uint8_t reg;
	int ret = ov3660_read_reg(spec, addr, &reg);

	if (ret) {
		return ret;
	}

	return ov3660_write_reg(spec, addr, (reg & ~mask) | (val & mask));
}

static int ov3660_set_reg_bits(const struct i2c_dt_spec *spec, uint16_t addr, uint8_t mask,
			       bool enable)
{
	uint8_t reg;
	int ret = ov3660_read_reg(spec, addr, &reg);

	if (ret) {
		return ret;
	}

	if (enable) {
		reg |= mask;
	} else {
		reg &= ~mask;
	}

	return ov3660_write_reg(spec, addr, reg);
}

static int ov3660_write_reg16(const struct i2c_dt_spec *spec, uint16_t addr, uint16_t value)
{
	int ret = ov3660_write_reg(spec, addr, value >> 8);

	if (ret) {
		return ret;
	}

	return ov3660_write_reg(spec, addr + 1, value & 0xff);
}

static int ov3660_write_addr_reg(const struct i2c_dt_spec *spec, uint16_t addr, uint16_t x,
				 uint16_t y)
{
	int ret = ov3660_write_reg16(spec, addr, x);

	if (ret) {
		return ret;
	}

	return ov3660_write_reg16(spec, addr + 2, y);
}

static int ov3660_set_pll(const struct i2c_dt_spec *spec, bool bypass, uint8_t multiplier,
			  uint8_t sys_div, uint8_t pre_div, bool root_2x, uint8_t seld5,
			  bool pclk_manual, uint8_t pclk_div)
{
	int ret;

	ret = ov3660_write_reg(spec, 0x303a, bypass ? 0x80 : 0x00);
	if (ret) {
		return ret;
	}

	ret = ov3660_write_reg(spec, 0x303b, multiplier & 0x1f);
	if (ret) {
		return ret;
	}

	ret = ov3660_write_reg(spec, 0x303c, 0x10 | (sys_div & 0x0f));
	if (ret) {
		return ret;
	}

	ret = ov3660_write_reg(spec, 0x303d, ((pre_div & 0x3) << 4) | seld5 | (root_2x ? 0x40 : 0));
	if (ret) {
		return ret;
	}

	ret = ov3660_write_reg(spec, 0x3824, pclk_div & 0x1f);
	if (ret) {
		return ret;
	}

	return ov3660_write_reg(spec, 0x460c, pclk_manual ? 0x22 : 0x20);
}

static int ov3660_write_reg_list(const struct i2c_dt_spec *spec,
				 const uint16_t (*regs)[2])
{
	for (int i = 0; regs[i][0] != OV3660_REGLIST_TAIL; i++) {
		if (regs[i][0] == OV3660_REG_DLY) {
			k_msleep(regs[i][1]);
			continue;
		}

		int ret = ov3660_write_reg(spec, regs[i][0], (uint8_t)regs[i][1]);

		if (ret) {
			LOG_ERR("write 0x%04x failed: %d", regs[i][0], ret);
			return ret;
		}
	}

	return 0;
}

static int ov3660_set_stream(const struct device *dev, bool enable)
{
	const struct ov3660_config *cfg = dev->config;

	return ov3660_write_reg(&cfg->i2c, OV3660_SYSTEM_CTRL0, enable ? 0x02 : 0x42);
}

static int ov3660_get_caps(const struct device *dev, enum video_endpoint_id ep,
			   struct video_caps *caps)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(ep);

	caps->format_caps = ov3660_fmts;
	return 0;
}

static int ov3660_get_fmt(const struct device *dev, enum video_endpoint_id ep,
			  struct video_format *fmt)
{
	struct ov3660_data *data = dev->data;

	*fmt = data->fmt;
	return 0;
}

static int ov3660_apply_image_options(const struct ov3660_config *cfg, struct ov3660_data *data)
{
	uint8_t reg20 = 0;
	uint8_t reg21 = 0;
	uint8_t reg4514;
	uint8_t reg4514_test = 0;
	bool jpeg = data->fmt.pixelformat == VIDEO_PIX_FMT_JPEG;
	bool binning = data->binning;
	int ret;

	if (jpeg) {
		reg21 |= 0x20;
	}

	if (binning) {
		reg20 |= 0x01;
		reg21 |= 0x01;
		reg4514_test |= 4;
	} else {
		reg20 |= 0x40;
	}

	if (data->vflip) {
		reg20 |= 0x06;
		reg4514_test |= 1;
	}

	if (data->hmirror) {
		reg21 |= 0x06;
		reg4514_test |= 2;
	}

	switch (reg4514_test) {
	case 0:
		reg4514 = 0x88;
		break;
	case 1:
		reg4514 = 0x88;
		break;
	case 2:
		reg4514 = 0xbb;
		break;
	case 3:
		reg4514 = 0xbb;
		break;
	case 4:
		reg4514 = 0xaa;
		break;
	case 5:
		reg4514 = 0xbb;
		break;
	case 6:
		reg4514 = 0xbb;
		break;
	default:
		reg4514 = 0xaa;
		break;
	}

	ret = ov3660_write_reg(&cfg->i2c, 0x3820, reg20);
	if (ret) {
		return ret;
	}

	ret = ov3660_write_reg(&cfg->i2c, 0x3821, reg21);
	if (ret) {
		return ret;
	}

	ret = ov3660_write_reg(&cfg->i2c, 0x4514, reg4514);
	if (ret) {
		return ret;
	}

	if (binning) {
		ret = ov3660_write_reg(&cfg->i2c, 0x4520, 0x0b);
		if (ret) {
			return ret;
		}
		ret = ov3660_write_reg(&cfg->i2c, 0x3814, 0x31);
		if (ret) {
			return ret;
		}
		ret = ov3660_write_reg(&cfg->i2c, 0x3815, 0x31);
	} else {
		ret = ov3660_write_reg(&cfg->i2c, 0x4520, 0xb0);
		if (ret) {
			return ret;
		}
		ret = ov3660_write_reg(&cfg->i2c, 0x3814, 0x11);
		if (ret) {
			return ret;
		}
		ret = ov3660_write_reg(&cfg->i2c, 0x3815, 0x11);
	}

	return ret;
}

static int ov3660_set_framesize(const struct device *dev, uint16_t w, uint16_t h)
{
	const struct ov3660_config *cfg = dev->config;
	struct ov3660_data *data = dev->data;
	const struct ov3660_ratio *ratio = &ov3660_ratio_4x3;
	int ret;

	if (w > OV3660_MAX_WIDTH || h > OV3660_MAX_HEIGHT) {
		LOG_ERR("Resolution %ux%u exceeds sensor max %ux%u", w, h, OV3660_MAX_WIDTH,
			OV3660_MAX_HEIGHT);
		return -ENOTSUP;
	}

	data->binning = (w <= (ratio->max_w / 2U)) && (h <= (ratio->max_h / 2U));
	data->scale = !((w == ratio->max_w && h == ratio->max_h) ||
			(w == (ratio->max_w / 2U) && h == (ratio->max_h / 2U)));

	ret = ov3660_write_addr_reg(&cfg->i2c, 0x3800, ratio->start_x, ratio->start_y);
	if (ret) {
		return ret;
	}

	ret = ov3660_write_addr_reg(&cfg->i2c, 0x3804, ratio->end_x, ratio->end_y);
	if (ret) {
		return ret;
	}

	ret = ov3660_write_addr_reg(&cfg->i2c, 0x3808, w, h);
	if (ret) {
		return ret;
	}

	if (data->binning) {
		ret = ov3660_write_addr_reg(&cfg->i2c, 0x380c, ratio->total_x, (ratio->total_y / 2U) + 1);
		if (ret) {
			return ret;
		}
		ret = ov3660_write_addr_reg(&cfg->i2c, 0x3810, 8, 2);
	} else {
		ret = ov3660_write_addr_reg(&cfg->i2c, 0x380c, ratio->total_x, ratio->total_y);
		if (ret) {
			return ret;
		}
		ret = ov3660_write_addr_reg(&cfg->i2c, 0x3810, 16, 6);
	}

	if (ret) {
		return ret;
	}

	ret = ov3660_set_reg_bits(&cfg->i2c, OV3660_ISP_CTRL01, 0x20, data->scale);
	if (ret) {
		return ret;
	}

	ret = ov3660_apply_image_options(cfg, data);
	if (ret) {
		return ret;
	}

	if (data->fmt.pixelformat == VIDEO_PIX_FMT_JPEG) {
		if (w == OV3660_MAX_WIDTH && h == OV3660_MAX_HEIGHT) {
			/* QXGA JPEG: 40 MHz SYSCLK, 10 MHz PCLK (esp32-camera) */
			ret = ov3660_set_pll(&cfg->i2c, false, 24, 1, 3, false, 0, true, 8);
		} else {
			/* Sub-QXGA JPEG: 50 MHz SYSCLK, 10 MHz PCLK */
			ret = ov3660_set_pll(&cfg->i2c, false, 30, 1, 3, false, 0, true, 10);
		}
	} else if (h > 240) {
		ret = ov3660_set_pll(&cfg->i2c, false, 4, 1, 0, false, 2, true, 2);
	} else if (w >= 320) {
		ret = ov3660_set_pll(&cfg->i2c, false, 8, 1, 0, false, 2, true, 4);
	} else {
		ret = ov3660_set_pll(&cfg->i2c, false, 8, 1, 0, false, 0, true, 8);
	}

	if (ret == 0) {
		LOG_INF("Framesize %ux%u (binning=%d scale=%d)", w, h, data->binning, data->scale);
	}

	return ret;
}

static int ov3660_set_fmt(const struct device *dev, enum video_endpoint_id ep,
			  struct video_format *fmt)
{
	struct ov3660_data *data = dev->data;
	const struct ov3660_config *cfg = dev->config;
	int ret;

	if (fmt->pixelformat != VIDEO_PIX_FMT_RGB565 &&
	    fmt->pixelformat != VIDEO_PIX_FMT_JPEG) {
		LOG_ERR("Only RGB565 and JPEG supported");
		return -ENOTSUP;
	}

	if (!memcmp(&data->fmt, fmt, sizeof(data->fmt))) {
		return 0;
	}

	if (fmt->pixelformat == VIDEO_PIX_FMT_JPEG) {
		ret = ov3660_write_reg_list(&cfg->i2c, ov3660_fmt_jpeg);
	} else {
		ret = ov3660_write_reg_list(&cfg->i2c, ov3660_fmt_rgb565);
	}

	if (ret) {
		return ret;
	}

	ret = ov3660_set_framesize(dev, fmt->width, fmt->height);
	if (ret) {
		return ret;
	}

	if (fmt->pixelformat == VIDEO_PIX_FMT_JPEG) {
		ret = ov3660_write_reg(&cfg->i2c, 0x4407, data->jpeg_quality & 0x3f);
		if (ret) {
			return ret;
		}
	}

	data->fmt = *fmt;
	if (fmt->pixelformat == VIDEO_PIX_FMT_JPEG) {
		/* ESP32 DMA buffer: up to width*height bytes for JPEG bitstream */
		data->fmt.pitch = fmt->width;
	} else {
		data->fmt.pitch = fmt->width * 2;
	}

	fmt->pitch = data->fmt.pitch;

	return 0;
}

static int ov3660_set_brightness_level(const struct i2c_dt_spec *spec, int level)
{
	uint8_t value = 0;
	bool negative = false;
	int ret;

	switch (level) {
	case 3:
		value = 0x30;
		break;
	case 2:
		value = 0x20;
		break;
	case 1:
		value = 0x10;
		break;
	case -1:
		value = 0x10;
		negative = true;
		break;
	case -2:
		value = 0x20;
		negative = true;
		break;
	case -3:
		value = 0x30;
		negative = true;
		break;
	default:
		value = 0;
		break;
	}

	ret = ov3660_write_reg(spec, 0x5587, value);
	if (ret) {
		return ret;
	}

	return ov3660_modify_reg(spec, 0x5588, 0x08, negative ? 0x08 : 0x00);
}

static int ov3660_set_saturation_level(const struct i2c_dt_spec *spec, int level)
{
	int ret;

	if (level > 4 || level < -4) {
		return -EINVAL;
	}

	const uint8_t *regs = ov3660_saturation_levels[level + 4];

	for (int i = 0; i < 11; i++) {
		ret = ov3660_write_reg(spec, 0x5381 + i, regs[i]);
		if (ret) {
			return ret;
		}
	}

	return 0;
}

static int ov3660_set_ctrl(const struct device *dev, uint32_t cid, void *value)
{
	const struct ov3660_config *cfg = dev->config;
	struct ov3660_data *data = dev->data;
	int val = (int)(intptr_t)value;

	switch (cid) {
	case VIDEO_CID_VFLIP:
		data->vflip = (val != 0);
		return ov3660_apply_image_options(cfg, data);
	case VIDEO_CID_HFLIP:
		data->hmirror = (val != 0);
		return ov3660_apply_image_options(cfg, data);
	case VIDEO_CID_BRIGHTNESS:
		return ov3660_set_brightness_level(&cfg->i2c, val);
	case VIDEO_CID_SATURATION:
		return ov3660_set_saturation_level(&cfg->i2c, val);
	case VIDEO_CID_JPEG_COMPRESSION_QUALITY:
		if (val < 0 || val > 63) {
			return -EINVAL;
		}
		data->jpeg_quality = (uint8_t)val;
		if (data->fmt.pixelformat == VIDEO_PIX_FMT_JPEG) {
			return ov3660_write_reg(&cfg->i2c, 0x4407, data->jpeg_quality & 0x3f);
		}
		return 0;
	default:
		return -ENOTSUP;
	}
}

static DEVICE_API(video, ov3660_driver_api) = {
	.set_format = ov3660_set_fmt,
	.get_format = ov3660_get_fmt,
	.get_caps = ov3660_get_caps,
	.set_stream = ov3660_set_stream,
	.set_ctrl = ov3660_set_ctrl,
};

static int ov3660_init(const struct device *dev)
{
	const struct ov3660_config *cfg = dev->config;
	struct ov3660_data *data = dev->data;
	struct video_format fmt;
	uint8_t id_h;
	uint8_t id_l;
	uint16_t chip_id;
	int ret;

	if (!i2c_is_ready_dt(&cfg->i2c)) {
		LOG_ERR("I2C bus not ready");
		return -ENODEV;
	}

	ret = ov3660_write_reg(&cfg->i2c, OV3660_SYSTEM_CTRL0, 0x82);
	if (ret) {
		return ret;
	}

	k_msleep(10);

	ret = ov3660_write_reg_list(&cfg->i2c, ov3660_default_regs);
	if (ret) {
		LOG_ERR("Default register load failed");
		return -EIO;
	}

	k_msleep(100);

	ret = ov3660_read_reg(&cfg->i2c, CHIP_ID_REG, &id_h);
	if (ret) {
		LOG_ERR("Chip ID read failed: %d", ret);
		return -ENODEV;
	}

	ret = ov3660_read_reg(&cfg->i2c, CHIP_ID_REG + 1, &id_l);
	if (ret) {
		return -ENODEV;
	}

	chip_id = ((uint16_t)id_h << 8) | id_l;
	if (chip_id != CHIP_ID_VAL) {
		LOG_ERR("Wrong chip ID 0x%04x (expected 0x%04x)", chip_id, CHIP_ID_VAL);
		return -ENODEV;
	}

	LOG_INF("OV3660 detected (PID 0x%04x)", chip_id);

	data->vflip = false;
	data->hmirror = false;
	data->binning = false;
	data->scale = false;
	data->jpeg_quality = 12;

	fmt.pixelformat = VIDEO_PIX_FMT_JPEG;
	fmt.width = OV3660_MAX_WIDTH;
	fmt.height = OV3660_MAX_HEIGHT;
	fmt.pitch = fmt.width;

	ret = ov3660_set_fmt(dev, VIDEO_EP_OUT, &fmt);
	if (ret) {
		LOG_ERR("Default QXGA JPEG setup failed");
		return ret;
	}

	return 0;
}

#define OV3660_INIT(n)                                                                             \
	static struct ov3660_data ov3660_data_##n;                                                 \
                                                                                                   \
	static const struct ov3660_config ov3660_cfg_##n = {                                       \
		.i2c = I2C_DT_SPEC_INST_GET(n),                                                    \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(n, ov3660_init, NULL, &ov3660_data_##n, &ov3660_cfg_##n,             \
			      POST_KERNEL, CONFIG_VIDEO_INIT_PRIORITY, &ov3660_driver_api);

DT_INST_FOREACH_STATUS_OKAY(OV3660_INIT)
