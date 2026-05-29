/*
 * Phase 2 step 1: probe OV3660 on Sense board I2C (camera SCL/SDA on GPIO 39/40).
 * OV3660 SCCB address 0x3c, chip ID at 16-bit registers 0x300A/0x300B (expect PID 0x3660).
 */

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define REG_PID_H 0x300a
#define REG_PID_L 0x300b

static int sccb_read16_reg(const struct i2c_dt_spec *spec, uint16_t reg, uint8_t *val)
{
	uint8_t addr_be[2] = { reg >> 8, reg & 0xff };

	return i2c_write_read_dt(spec, addr_be, sizeof(addr_be), val, 1);
}

static void probe_addr(const struct device *i2c, uint8_t addr)
{
	struct i2c_dt_spec spec = {
		.bus = i2c,
		.addr = addr,
	};
	uint8_t pid_h;
	uint8_t pid_l;
	int ret_h;
	int ret_l;

	ret_h = sccb_read16_reg(&spec, REG_PID_H, &pid_h);
	ret_l = sccb_read16_reg(&spec, REG_PID_L, &pid_l);

	if (ret_h == 0 && ret_l == 0) {
		uint16_t pid = ((uint16_t)pid_h << 8) | pid_l;

		printk("  0x%02x: PID 0x%04x", addr, pid);
		if (pid == 0x3660) {
			printk(" (OV3660)");
		} else if (pid == 0x2640) {
			printk(" (OV2640)");
		} else if (pid == 0x5640) {
			printk(" (OV5640)");
		}
		printk("\n");
	} else {
		printk("  0x%02x: no 16-bit SCCB response (h=%d l=%d)\n", addr, ret_h, ret_l);
	}
}

int main(void)
{
	const struct device *i2c = DEVICE_DT_GET(DT_NODELABEL(i2c1));
	uint8_t scan_addr;

	if (!device_is_ready(i2c)) {
		printk("i2c1 not ready\n");
		return 0;
	}

	printk("Camera I2C probe (i2c1 / GPIO 39 SCL, 40 SDA)\n");

#if DT_HAS_CHOSEN(zephyr_camera)
	const struct device *cam = DEVICE_DT_GET(DT_CHOSEN(zephyr_camera));

	if (device_is_ready(cam)) {
		printk("lcd_cam (XMCLK) ready: %s\n", cam->name);
	} else {
		printk("lcd_cam not ready — SCCB may fail without MCLK\n");
	}
	/* Allow master clock to stabilize after PRE_KERNEL init */
	k_msleep(100);
#else
	printk("WARNING: no zephyr,camera chosen — XMCLK may be off\n");
#endif

	printk("Known OmniVision SCCB addresses:\n");
	probe_addr(i2c, 0x30);
	probe_addr(i2c, 0x3c);

	printk("\nI2C bus scan (7-bit addresses with ACK):\n");
	for (scan_addr = 0x08; scan_addr < 0x78; scan_addr++) {
		struct i2c_msg msg = {
			.buf = &scan_addr,
			.len = 0,
			.flags = I2C_MSG_WRITE | I2C_MSG_STOP,
		};
		struct i2c_dt_spec spec = { .bus = i2c, .addr = scan_addr };

		if (i2c_transfer_dt(&spec, &msg, 1) == 0) {
			printk("  found 0x%02x\n", scan_addr);
		}
	}

	printk("\nDone.\n");
	return 0;
}
