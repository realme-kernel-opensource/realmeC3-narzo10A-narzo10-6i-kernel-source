/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": %s: " fmt, __func__

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/pinctrl/consumer.h>

#include "flashlight-core.h"
#include "flashlight-dt.h"
/* device tree should be defined in flashlight-dt.h */
#ifndef MONET_DTNAME
#define MONET_DTNAME "mediatek,flashlights_monet"
#endif
#ifndef MONET_DTNAME_I2C
#define MONET_DTNAME_I2C   "mediatek,strobe_main"
#endif

#define MONET_NAME "flashlights_monet"

/* define registers */
#define MONET_REG_ENABLE           (0x01)
#define MONET_MASK_ENABLE_LED1     (0x01)
#define MONET_MASK_ENABLE_LED2     (0x02)
#define MONET_DISABLE              (0x00)
#define MONET_TORCH_MODE           (0x08)
#define MONET_FLASH_MODE           (0x0C)
#define MONET_ENABLE_LED1          (0x01)
#define MONET_ENABLE_LED1_TORCH    (0x09)
#define MONET_ENABLE_LED1_FLASH    (0x0D)
#define MONET_ENABLE_LED2          (0x02)
#define MONET_ENABLE_LED2_TORCH    (0x0A)
#define MONET_ENABLE_LED2_FLASH    (0x0E)

#define MONET_REG_TORCH_LEVEL_LED1 (0x05)
#define MONET_REG_FLASH_LEVEL_LED1 (0x03)
#define MONET_REG_TORCH_LEVEL_LED2 (0x06)
#define MONET_REG_FLASH_LEVEL_LED2 (0x04)

#define MONET_REG_TIMING_CONF      (0x08)
#define MONET_TORCH_RAMP_TIME      (0x10)
#define MONET_FLASH_TIMEOUT        (0x0F)



/* define channel, level */
#define MONET_CHANNEL_NUM          2
#define MONET_CHANNEL_CH1          0
#define MONET_CHANNEL_CH2          1
/* define level */
#define MONET_LEVEL_NUM 26
#define MONET_LEVEL_TORCH 7

#define MONET_HW_TIMEOUT 400 /* ms */

/* define mutex and work queue */
static DEFINE_MUTEX(monet_mutex);
static struct work_struct monet_work_ch1;
static struct work_struct monet_work_ch2;

/* define pinctrl */
#define MONET_PINCTRL_PIN_HWEN 0
#define MONET_PINCTRL_PINSTATE_LOW 0
#define MONET_PINCTRL_PINSTATE_HIGH 1
#define MONET_PINCTRL_STATE_HWEN_HIGH "hwen_high"
#define MONET_PINCTRL_STATE_HWEN_LOW  "hwen_low"


//////////////////////////////////////////////////////////////////////////////////////////////////////////////
static ssize_t monet_get_reg(struct device* cd,struct device_attribute *attr, char* buf);
static ssize_t monet_set_reg(struct device* cd, struct device_attribute *attr,const char* buf, size_t len);
static ssize_t monet_set_hwen(struct device* cd, struct device_attribute *attr,const char* buf, size_t len);
static ssize_t monet_get_hwen(struct device* cd, struct device_attribute *attr, char* buf);

static DEVICE_ATTR(reg, 0660, monet_get_reg,  monet_set_reg);
static DEVICE_ATTR(hwen, 0660, monet_get_hwen,  monet_set_hwen);

struct i2c_client *monet_flashlight_client;

static struct pinctrl *monet_pinctrl;
static struct pinctrl_state *monet_hwen_high;
static struct pinctrl_state *monet_hwen_low;

/* define usage count */
static int use_count;

/* define i2c */
static struct i2c_client *monet_i2c_client;

/* platform data */
struct monet_platform_data {
	int channel_num;
	struct flashlight_device_id *dev_id;
};

/* monet chip data */
struct monet_chip_data {
	struct i2c_client *client;
	struct monet_platform_data *pdata;
	struct mutex lock;
	u8 last_flag;
	u8 no_pdata;
};


/******************************************************************************
 * Pinctrl configuration
 *****************************************************************************/
static int monet_pinctrl_init(struct platform_device *pdev)
{
	int ret = 0;

	/* get pinctrl */
	monet_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(monet_pinctrl)) {
		pr_err("Failed to get flashlight pinctrl.\n");
		ret = PTR_ERR(monet_pinctrl);
	}

	/* Flashlight HWEN pin initialization */
	monet_hwen_high = pinctrl_lookup_state(monet_pinctrl, MONET_PINCTRL_STATE_HWEN_HIGH);
	if (IS_ERR(monet_hwen_high)) {
		pr_err("Failed to init (%s)\n", MONET_PINCTRL_STATE_HWEN_HIGH);
		ret = PTR_ERR(monet_hwen_high);
	}
	monet_hwen_low = pinctrl_lookup_state(monet_pinctrl, MONET_PINCTRL_STATE_HWEN_LOW);
	if (IS_ERR(monet_hwen_low)) {
		pr_err("Failed to init (%s)\n", MONET_PINCTRL_STATE_HWEN_LOW);
		ret = PTR_ERR(monet_hwen_low);
	}

	return ret;
}

static int monet_pinctrl_set(int pin, int state)
{
	int ret = 0;

	if (IS_ERR(monet_pinctrl)) {
		pr_err("pinctrl is not available\n");
		return -1;
	}

	switch (pin) {
	case MONET_PINCTRL_PIN_HWEN:
		if (state == MONET_PINCTRL_PINSTATE_LOW && !IS_ERR(monet_hwen_low))
			pinctrl_select_state(monet_pinctrl, monet_hwen_low);
		else if (state == MONET_PINCTRL_PINSTATE_HIGH && !IS_ERR(monet_hwen_high))
			pinctrl_select_state(monet_pinctrl, monet_hwen_high);
		else
			pr_err("set err, pin(%d) state(%d)\n", pin, state);
		break;
	default:
		pr_err("set err, pin(%d) state(%d)\n", pin, state);
		break;
	}
	pr_info("pin(%d) state(%d)\n", pin, state);

	return ret;
}
/******************************************************************************
 * monet operations
 *****************************************************************************/
static const int *monet_current;
static const unsigned char *monet_torch_level;
static const unsigned char *monet_flash_level;
static const int aw3643_current[MONET_LEVEL_NUM] = {
	22,  46,  70,  100,  116,  140, 163, 198, 245, 304,
	351, 398, 445, 503, 550,  597, 656, 703, 750, 796,
	855, 902, 949, 996, 1054, 1101
};
static const unsigned char aw3643_torch_level[MONET_LEVEL_NUM] = {
	0x06, 0x0F, 0x17, 0x1F, 0x27, 0x2F, 0x37, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const unsigned char aw3643_flash_level[MONET_LEVEL_NUM] = {
	0x01, 0x03, 0x05, 0x07, 0x09, 0x0B, 0x0D, 0x10, 0x14, 0x19,
	0x1D, 0x21, 0x25, 0x2A, 0x2E, 0x32, 0x37, 0x3B, 0x3F, 0x43,
	0x48, 0x4C, 0x50, 0x54, 0x59, 0x5D};

static const int ocp81373_current[MONET_LEVEL_NUM] = {
	24,  46,  70,  100,  116,  140, 164, 199, 246, 304,
	351, 398, 445, 504, 551,  598, 656, 703, 750, 797,
	856, 903, 949, 996, 1055, 1102
};

static const unsigned char ocp81373_torch_level[MONET_LEVEL_NUM] = {
	0x08, 0x10, 0x18, 0x21, 0x29, 0x31, 0x3A, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const unsigned char ocp81373_flash_level[MONET_LEVEL_NUM] = {
	0x01, 0x03, 0x05, 0x07, 0x09, 0x0B, 0x0D, 0x10, 0x14, 0x19,
	0x1D, 0x21, 0x25, 0x2A, 0x2E, 0x32, 0x37, 0x3B, 0x3F, 0x43,
	0x48, 0x4C, 0x50, 0x54, 0x59, 0x5D};
static volatile unsigned char monet_reg_enable;
static volatile int monet_level_ch1 = -1;
static volatile int monet_level_ch2 = -1;

static int monet_is_torch(int level)
{
	if (level >= MONET_LEVEL_TORCH)
		return -1;

	return 0;
}

static int monet_verify_level(int level)
{
	if (level < 0)
		level = 0;
	else if (level >= MONET_LEVEL_NUM)
		level = MONET_LEVEL_NUM - 1;

	return level;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// i2c write and read
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int monet_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	int ret;
	struct monet_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	ret = i2c_smbus_write_byte_data(client, reg, val);
	mutex_unlock(&chip->lock);

	if (ret < 0)
		pr_err("failed writing at 0x%02x\n", reg);

	return ret;
}

/* i2c wrapper function */
static int monet_read_reg(struct i2c_client *client, u8 reg)
{
	int val;
	struct monet_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	val = i2c_smbus_read_byte_data(client, reg);
	mutex_unlock(&chip->lock);

	if (val < 0)
		pr_err("failed read at 0x%02x\n", reg);

	return val;
}

/* flashlight enable function */
static int monet_enable_ch1(void)
{
	unsigned char reg, val;

	reg = MONET_REG_ENABLE;
	if (!monet_is_torch(monet_level_ch1)) {
		/* torch mode */
		monet_reg_enable |= MONET_ENABLE_LED1_TORCH;
	} else {
		/* flash mode */
		monet_reg_enable |= MONET_ENABLE_LED1_FLASH;
	}
	val = monet_reg_enable;

	return monet_write_reg(monet_i2c_client, reg, val);
}

static int monet_enable_ch2(void)
{
	unsigned char reg, val;

	reg = MONET_REG_ENABLE;
	if (!monet_is_torch(monet_level_ch2)) {
		/* torch mode */
		monet_reg_enable |= MONET_ENABLE_LED2_TORCH;
	} else {
		/* flash mode */
		monet_reg_enable |= MONET_ENABLE_LED2_FLASH;
	}
	val = monet_reg_enable;

	return monet_write_reg(monet_i2c_client, reg, val);
}

static int monet_enable(int channel)
{
	if (channel == MONET_CHANNEL_CH1)
		monet_enable_ch1();
	else if (channel == MONET_CHANNEL_CH2)
		monet_enable_ch2();
	else {
		pr_err("Error channel\n");
		return -1;
	}

	return 0;
}

/* flashlight disable function */
static int monet_disable_ch1(void)
{
	unsigned char reg, val;

	reg = MONET_REG_ENABLE;
	if (monet_reg_enable & MONET_MASK_ENABLE_LED2) {
		/* if LED 2 is enable, disable LED 1 */
		monet_reg_enable &= (~MONET_ENABLE_LED1);
	} else {
		/* if LED 2 is enable, disable LED 1 and clear mode */
		monet_reg_enable &= (~MONET_ENABLE_LED1_FLASH);
	}
	val = monet_reg_enable;

	return monet_write_reg(monet_i2c_client, reg, val);
}

static int monet_disable_ch2(void)
{
	unsigned char reg, val;

	reg = MONET_REG_ENABLE;
	if (monet_reg_enable & MONET_MASK_ENABLE_LED1) {
		/* if LED 1 is enable, disable LED 2 */
		monet_reg_enable &= (~MONET_ENABLE_LED2);
	} else {
		/* if LED 1 is enable, disable LED 2 and clear mode */
		monet_reg_enable &= (~MONET_ENABLE_LED2_FLASH);
	}
	val = monet_reg_enable;

	return monet_write_reg(monet_i2c_client, reg, val);
}

static int monet_disable(int channel)
{
	if (channel == MONET_CHANNEL_CH1)
		monet_disable_ch1();
	else if (channel == MONET_CHANNEL_CH2)
		monet_disable_ch2();
	else {
		pr_err("Error channel\n");
		return -1;
	}

	return 0;
}

/* set flashlight level */
static int monet_set_level_ch1(int level)
{
	int ret;
	unsigned char reg, val;

	level = monet_verify_level(level);

	/* set torch brightness level */
	reg = MONET_REG_TORCH_LEVEL_LED1;
	val = monet_torch_level[level];
	ret = monet_write_reg(monet_i2c_client, reg, val);

	monet_level_ch1 = level;

	/* set flash brightness level */
	reg = MONET_REG_FLASH_LEVEL_LED1;
	val = monet_flash_level[level];
	ret = monet_write_reg(monet_i2c_client, reg, val);

	return ret;
}

int monet_set_level_ch2(int level)
{
	int ret;
	unsigned char reg, val;

	level = monet_verify_level(level);

	/* set torch brightness level */
	reg = MONET_REG_TORCH_LEVEL_LED2;
	val = monet_torch_level[level];
	ret = monet_write_reg(monet_i2c_client, reg, val);

	monet_level_ch2 = level;

	/* set flash brightness level */
	reg = MONET_REG_FLASH_LEVEL_LED2;
	val = monet_flash_level[level];
	ret = monet_write_reg(monet_i2c_client, reg, val);

	return ret;
}

static int monet_set_level(int channel, int level)
{
	if (channel == MONET_CHANNEL_CH1)
		monet_set_level_ch1(level);
	else if (channel == MONET_CHANNEL_CH2)
		monet_set_level_ch2(level);
	else {
		pr_err("Error channel\n");
		return -1;
	}

	return 0;
}

/* flashlight init */
int monet_init(void)
{
	int ret;
	unsigned char reg, val;

	monet_pinctrl_set(MONET_PINCTRL_PIN_HWEN, MONET_PINCTRL_PINSTATE_HIGH);
    msleep(2);

	/* clear enable register */
	reg = MONET_REG_ENABLE;
	val = MONET_DISABLE;
	ret = monet_write_reg(monet_i2c_client, reg, val);

	monet_reg_enable = val;

	/* set torch current ramp time and flash timeout */
	reg = MONET_REG_TIMING_CONF;
	val = MONET_TORCH_RAMP_TIME | MONET_FLASH_TIMEOUT;
	ret = monet_write_reg(monet_i2c_client, reg, val);

	return ret;
}

/* flashlight uninit */
int monet_uninit(void)
{
	monet_disable(MONET_CHANNEL_CH1);
	monet_disable(MONET_CHANNEL_CH2);
	monet_pinctrl_set(MONET_PINCTRL_PIN_HWEN, MONET_PINCTRL_PINSTATE_LOW);

	return 0;
}


/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static struct hrtimer monet_timer_ch1;
static struct hrtimer monet_timer_ch2;
static unsigned int monet_timeout_ms[MONET_CHANNEL_NUM];

static void monet_work_disable_ch1(struct work_struct *data)
{
	pr_info("ht work queue callback\n");
	monet_disable_ch1();
}

static void monet_work_disable_ch2(struct work_struct *data)
{
	pr_info("lt work queue callback\n");
	monet_disable_ch2();
}

static enum hrtimer_restart monet_timer_func_ch1(struct hrtimer *timer)
{
	schedule_work(&monet_work_ch1);
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart monet_timer_func_ch2(struct hrtimer *timer)
{
	schedule_work(&monet_work_ch2);
	return HRTIMER_NORESTART;
}

int monet_timer_start(int channel, ktime_t ktime)
{
	if (channel == MONET_CHANNEL_CH1)
		hrtimer_start(&monet_timer_ch1, ktime, HRTIMER_MODE_REL);
	else if (channel == MONET_CHANNEL_CH2)
		hrtimer_start(&monet_timer_ch2, ktime, HRTIMER_MODE_REL);
	else {
		pr_err("Error channel\n");
		return -1;
	}

	return 0;
}

int monet_timer_cancel(int channel)
{
	if (channel == MONET_CHANNEL_CH1)
		hrtimer_cancel(&monet_timer_ch1);
	else if (channel == MONET_CHANNEL_CH2)
		hrtimer_cancel(&monet_timer_ch2);
	else {
		pr_err("Error channel\n");
		return -1;
	}

	return 0;
}


/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
static int monet_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;
	ktime_t ktime;

	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	/* verify channel */
	if (channel < 0 || channel >= MONET_CHANNEL_NUM) {
		pr_err("Failed with error channel\n");
		return -EINVAL;
	}
	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		pr_info("FLASH_IOC_SET_TIME_OUT_TIME_MS(%d): %d\n",
				channel, (int)fl_arg->arg);
		monet_timeout_ms[channel] = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		pr_info("FLASH_IOC_SET_DUTY(%d): %d\n",
				channel, (int)fl_arg->arg);
		monet_set_level(channel, fl_arg->arg);
		break;

	case FLASH_IOC_SET_ONOFF:
		pr_info("FLASH_IOC_SET_ONOFF(%d): %d\n",
				channel, (int)fl_arg->arg);
		if (fl_arg->arg == 1) {
			if (monet_timeout_ms[channel]) {
				ktime = ktime_set(monet_timeout_ms[channel] / 1000,
						(monet_timeout_ms[channel] % 1000) * 1000000);
				monet_timer_start(channel, ktime);
			}
			monet_enable(channel);
		} else {
			monet_disable(channel);
			monet_timer_cancel(channel);
		}
		break;

	case FLASH_IOC_GET_DUTY_NUMBER:
		pr_info("FLASH_IOC_GET_DUTY_NUMBER(%d)\n", channel);
		fl_arg->arg = MONET_LEVEL_NUM;
		break;

	case FLASH_IOC_GET_MAX_TORCH_DUTY:
		pr_info("FLASH_IOC_GET_MAX_TORCH_DUTY(%d)\n", channel);
		fl_arg->arg = MONET_LEVEL_TORCH - 1;
		break;

	case FLASH_IOC_GET_DUTY_CURRENT:
		fl_arg->arg = monet_verify_level(fl_arg->arg);
		pr_info("FLASH_IOC_GET_DUTY_CURRENT(%d): %d\n",
				channel, (int)fl_arg->arg);
		fl_arg->arg = monet_current[fl_arg->arg];
		break;

	case FLASH_IOC_GET_HW_TIMEOUT:
		pr_info("FLASH_IOC_GET_HW_TIMEOUT(%d)\n", channel);
		fl_arg->arg = MONET_HW_TIMEOUT;
		break;

	default:
		pr_info("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int monet_open(void)
{
	/* Actual behavior move to set driver function since power saving issue */
	return 0;
}

static int monet_release(void)
{
	/* uninit chip and clear usage count */
/*
	mutex_lock(&monet_mutex);
	use_count--;
	if (!use_count)
		monet_uninit();
	if (use_count < 0)
		use_count = 0;
	mutex_unlock(&monet_mutex);

	pr_info("Release: %d\n", use_count);
*/
	return 0;
}

static int monet_set_driver(int set)
{
	int ret = 0;

	/* set chip and usage count */
	mutex_lock(&monet_mutex);
	if (set) {
		if (!use_count)
			ret = monet_init();
		use_count++;
		pr_info("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			ret = monet_uninit();
		if (use_count < 0)
			use_count = 0;
		pr_info("Unset driver: %d\n", use_count);
	}
	mutex_unlock(&monet_mutex);

	return ret;
}

static ssize_t monet_strobe_store(struct flashlight_arg arg)
{
	monet_set_driver(1);
	monet_set_level(arg.channel, arg.level);
	monet_timeout_ms[arg.channel] = 0;
	monet_enable(arg.channel);
	msleep(arg.dur);
	monet_disable(arg.channel);
	//monet_release(NULL);
	monet_set_driver(0);
	return 0;
}

static struct flashlight_operations monet_ops = {
	monet_open,
	monet_release,
	monet_ioctl,
	monet_strobe_store,
	monet_set_driver
};


/******************************************************************************
 * I2C device and driver
 *****************************************************************************/
static int monet_chip_init(struct monet_chip_data *chip)
{
	/* NOTE: Chip initialication move to "set driver" operation for power saving issue.
	 * monet_init();
	 */

	return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//MONET Debug file
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static ssize_t monet_get_reg(struct device* cd,struct device_attribute *attr, char* buf)
{
	unsigned char reg_val;
	unsigned char i;
	ssize_t len = 0;
	for(i=0;i<0x0E;i++)
	{
		reg_val = monet_read_reg(monet_i2c_client,i);
		len += snprintf(buf+len, PAGE_SIZE-len, "reg%2X = 0x%2X \n, ", i,reg_val);
	}
	len += snprintf(buf+len, PAGE_SIZE-len, "\r\n");
	return len;
}

static ssize_t monet_set_reg(struct device* cd, struct device_attribute *attr, const char* buf, size_t len)
{
	unsigned int databuf[2];
	if(2 == sscanf(buf,"%x %x",&databuf[0], &databuf[1]))
	{
		//i2c_write_reg(databuf[0],databuf[1]);
		monet_write_reg(monet_i2c_client,databuf[0],databuf[1]);
	}
	return len;
}

static ssize_t monet_get_hwen(struct device* cd,struct device_attribute *attr, char* buf)
{
	ssize_t len = 0;
	len += snprintf(buf+len, PAGE_SIZE-len, "//monet_hwen_on(void)\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "echo 1 > hwen\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "//monet_hwen_off(void)\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "echo 0 > hwen\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "\n");

	return len;
}

static ssize_t monet_set_hwen(struct device* cd, struct device_attribute *attr, const char* buf, size_t len)
{
	unsigned char databuf[16];

	sscanf(buf,"%c",&databuf[0]);
#if 1
	if(databuf[0] == 0) {	// OFF
		//monet_hwen_low();
	} else {				// ON
		//monet_hwen_high();
	}
#endif
	return len;
}

static int monet_create_sysfs(struct i2c_client *client)
{
	int err;
	struct device *dev = &(client->dev);

	err = device_create_file(dev, &dev_attr_reg);
	err = device_create_file(dev, &dev_attr_hwen);

	return err;
}

static int monet_parse_dt(struct device *dev,
		struct monet_platform_data *pdata)
{
	struct device_node *np, *cnp;
	u32 decouple = 0;
	int i = 0;

	if (!dev || !dev->of_node || !pdata)
		return -ENODEV;

	np = dev->of_node;

	pdata->channel_num = of_get_child_count(np);
	if (!pdata->channel_num) {
		pr_info("Parse no dt, node.\n");
		return 0;
	}
	pr_info("Channel number(%d).\n", pdata->channel_num);

	if (of_property_read_u32(np, "decouple", &decouple))
		pr_info("Parse no dt, decouple.\n");

	pdata->dev_id = devm_kzalloc(dev,
			pdata->channel_num *
			sizeof(struct flashlight_device_id),
			GFP_KERNEL);
	if (!pdata->dev_id)
		return -ENOMEM;

	for_each_child_of_node(np, cnp) {
		if (of_property_read_u32(cnp, "type", &pdata->dev_id[i].type))
			goto err_node_put;
		if (of_property_read_u32(cnp, "ct", &pdata->dev_id[i].ct))
			goto err_node_put;
		if (of_property_read_u32(cnp, "part", &pdata->dev_id[i].part))
			goto err_node_put;
		snprintf(pdata->dev_id[i].name, FLASHLIGHT_NAME_SIZE,
				MONET_NAME);
		pdata->dev_id[i].channel = i;
		pdata->dev_id[i].decouple = decouple;

		pr_info("Parse dt (type,ct,part,name,channel,decouple)=(%d,%d,%d,%s,%d,%d).\n",
				pdata->dev_id[i].type, pdata->dev_id[i].ct,
				pdata->dev_id[i].part, pdata->dev_id[i].name,
				pdata->dev_id[i].channel,
				pdata->dev_id[i].decouple);
		i++;
	}

	return 0;

err_node_put:
	of_node_put(cnp);
	return -EINVAL;
}

#ifdef ODM_WT_EDIT
#ifdef OPPO_FLASHLIGHT_TEST
/* Xingyu.Liu@Camera.Driver, 2019/10/10, add for [wingtech ATO factory app camera] */
int high_cct_led_strobe_enable_part1(void)
{
    monet_pinctrl_set(MONET_PINCTRL_PIN_HWEN, MONET_PINCTRL_PINSTATE_HIGH);

    mdelay(1);

    pr_info("!!!\n");
    return 0;
}

int high_cct_led_strobe_setduty_part1(int duty)
{
    int ret = 0;
    /* set torch brightness level */
    ret = monet_write_reg(monet_i2c_client, MONET_REG_TORCH_LEVEL_LED1, duty);

    return 0;

}

int high_cct_led_strobe_on_part1(int onoff)
{
    unsigned int temp;

    monet_write_reg(monet_i2c_client, MONET_REG_TIMING_CONF, 0x1A);

    temp = monet_read_reg(monet_i2c_client, MONET_REG_ENABLE);
    temp = onoff ? ((temp & 0x83) | (MONET_TORCH_MODE | MONET_ENABLE_LED1)) : (((temp & 0x83) | MONET_TORCH_MODE) & (~MONET_ENABLE_LED1));
    monet_write_reg(monet_i2c_client, MONET_REG_ENABLE, temp);

    return 0;
}

int low_cct_led_strobe_enable_part1(void)
{
    monet_pinctrl_set(MONET_PINCTRL_PIN_HWEN, MONET_PINCTRL_PINSTATE_HIGH);

    mdelay(1);

    pr_info("!!!\n");
    return 0;
}

int low_cct_led_strobe_setduty_part1(int duty)
{
  int ret = 0;
  /* set torch brightness level */
  ret = monet_write_reg(monet_i2c_client, MONET_REG_TORCH_LEVEL_LED2, duty);

  return 0;

}

int low_cct_led_strobe_on_part1(int onoff)
{
    unsigned int temp;

    monet_write_reg(monet_i2c_client, MONET_REG_TIMING_CONF, 0x1A);

    temp = monet_read_reg(monet_i2c_client, MONET_REG_ENABLE);
    temp = onoff ? ((temp & 0x83) | (MONET_TORCH_MODE | MONET_ENABLE_LED2)) : (((temp & 0x83) | MONET_TORCH_MODE) & (~MONET_ENABLE_LED2));
    monet_write_reg(monet_i2c_client, MONET_REG_ENABLE, temp);

    return 0;
}
#endif
#endif
enum FLASHLIGHT_DEVICE {
	AW3643_SM = 0x12,
	OCP81373_SM = 0x3A,
};

#define USE_AW3643_IC	0x0001
#define USE_OCP81373_IC	0x0011
#define USE_NOT_PRO		0x1111

static int monet_chip_id(void)
{
	char chip_id;

	monet_pinctrl_set(MONET_PINCTRL_PIN_HWEN, MONET_PINCTRL_PINSTATE_HIGH);
	chip_id = monet_read_reg(monet_i2c_client, 0x0c);
	pr_info("flashlight chip id: reg:0x0c, data:0x%x", chip_id);
	monet_pinctrl_set(MONET_PINCTRL_PIN_HWEN, MONET_PINCTRL_PINSTATE_LOW);

	if (chip_id == AW3643_SM) {
		pr_info(" the device's flashlight driver IC is AW3643\n");
		return USE_AW3643_IC;
	} else if (chip_id == OCP81373_SM){
		pr_info(" the device's flashlight driver IC is OCP81373\n");
		return USE_OCP81373_IC;
	} else {
		pr_err(" the device's flashlight driver IC is not used in our project!\n");
		return USE_NOT_PRO;
	}
}
static int monet_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct monet_chip_data *chip;
	struct monet_platform_data *pdata = client->dev.platform_data;
	int err;
	int i;
	int chip_id;

	pr_info("Probe start.\n");

	/* check i2c */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("Failed to check i2c functionality.\n");
		err = -ENODEV;
		goto err_out;
	}

	/* init chip private data */
	chip = kzalloc(sizeof(struct monet_chip_data), GFP_KERNEL);
	if (!chip) {
		err = -ENOMEM;
		goto err_out;
	}
	chip->client = client;

	/* init platform data */
	if (!pdata) {
		pr_err("Platform data does not exist\n");
		pdata = kzalloc(sizeof(struct monet_platform_data), GFP_KERNEL);
		if (!pdata) {
			err = -ENOMEM;
			goto err_free;
		}
		client->dev.platform_data = pdata;
		err = monet_parse_dt(&client->dev, pdata);
		if (err)
			goto err_free;
	}
	chip->pdata = pdata;
	i2c_set_clientdata(client, chip);
	monet_i2c_client = client;

	/* init mutex and spinlock */
	mutex_init(&chip->lock);

	/* init work queue */
	INIT_WORK(&monet_work_ch1, monet_work_disable_ch1);
	INIT_WORK(&monet_work_ch2, monet_work_disable_ch2);

	/* init timer */
	hrtimer_init(&monet_timer_ch1, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	monet_timer_ch1.function = monet_timer_func_ch1;
	hrtimer_init(&monet_timer_ch2, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	monet_timer_ch2.function = monet_timer_func_ch2;
	monet_timeout_ms[MONET_CHANNEL_CH1] = 100;
	monet_timeout_ms[MONET_CHANNEL_CH2] = 100;

	/* init chip hw */
	monet_chip_init(chip);

	chip_id = monet_chip_id();
	if(chip_id == USE_AW3643_IC){
		monet_current = aw3643_current;
		monet_torch_level = aw3643_torch_level;
		monet_flash_level = aw3643_flash_level;
	}
	else if (chip_id == USE_OCP81373_IC){
		monet_current = ocp81373_current;
		monet_torch_level = ocp81373_torch_level;
		monet_flash_level = ocp81373_flash_level;
	} else if (chip_id == USE_NOT_PRO){
		monet_current = aw3643_current;
		monet_torch_level = aw3643_torch_level;
		monet_flash_level = aw3643_flash_level;
	}
	/* register flashlight operations */
	if (pdata->channel_num) {
		for (i = 0; i < pdata->channel_num; i++)
			if (flashlight_dev_register_by_device_id(
						&pdata->dev_id[i],
						&monet_ops)) {
                pr_err("Failed to register flashlight device.\n");
				err = -EFAULT;
				goto err_free;
			}
	} else {
		if (flashlight_dev_register(MONET_NAME, &monet_ops)) {
			pr_err("Failed to register flashlight device.\n");
			err = -EFAULT;
			goto err_free;
		}
	}

    monet_create_sysfs(client);

	pr_info("Probe done.\n");

	return 0;

err_free:
	i2c_set_clientdata(client, NULL);
	kfree(chip);
err_out:
	return err;
}

static int monet_i2c_remove(struct i2c_client *client)
{
	struct monet_platform_data *pdata = dev_get_platdata(&client->dev);
	struct monet_chip_data *chip = i2c_get_clientdata(client);
	int i;

	pr_info("Remove start.\n");

	client->dev.platform_data = NULL;

	/* unregister flashlight device */
	if (pdata && pdata->channel_num)
		for (i = 0; i < pdata->channel_num; i++)
			flashlight_dev_unregister_by_device_id(
					&pdata->dev_id[i]);
	else
		flashlight_dev_unregister(MONET_NAME);
	/* flush work queue */
	flush_work(&monet_work_ch1);
	flush_work(&monet_work_ch2);

	/* unregister flashlight operations */
	flashlight_dev_unregister(MONET_NAME);

	/* free resource */
	if (chip->no_pdata)
		kfree(chip->pdata);
	kfree(chip);

	pr_info("Remove done.\n");

	return 0;
}

static const struct i2c_device_id monet_i2c_id[] = {
	{MONET_NAME, 0},
	{}
};

#ifdef CONFIG_OF
static const struct of_device_id monet_i2c_of_match[] = {
	{.compatible = MONET_DTNAME_I2C},
	{},
};
#endif

static struct i2c_driver monet_i2c_driver = {
	.driver = {
		   .name = MONET_NAME,
#ifdef CONFIG_OF
		   .of_match_table = monet_i2c_of_match,
#endif
		   },
	.probe = monet_i2c_probe,
	.remove = monet_i2c_remove,
	.id_table = monet_i2c_id,
};


/******************************************************************************
 * Platform device and driver
 *****************************************************************************/
static int monet_probe(struct platform_device *dev)
{
	pr_info("Probe start.\n");

	/* init pinctrl */
	if (monet_pinctrl_init(dev)) {
		pr_err("Failed to init pinctrl.\n");
		return -1;
	}

	if (i2c_add_driver(&monet_i2c_driver)) {
		pr_err("Failed to add i2c driver.\n");
		return -1;
	}

	pr_info("Probe done.\n");

	return 0;
}

static int monet_remove(struct platform_device *dev)
{
	pr_info("Remove start.\n");

	i2c_del_driver(&monet_i2c_driver);

	pr_info("Remove done.\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id monet_of_match[] = {
	{.compatible = MONET_DTNAME},
	{},
};
MODULE_DEVICE_TABLE(of, monet_of_match);
#else
static struct platform_device monet_platform_device[] = {
	{
		.name = MONET_NAME,
		.id = 0,
		.dev = {}
	},
	{}
};
MODULE_DEVICE_TABLE(platform, monet_platform_device);
#endif

static struct platform_driver monet_platform_driver = {
	.probe = monet_probe,
	.remove = monet_remove,
	.driver = {
		.name = MONET_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = monet_of_match,
#endif
	},
};

static int __init flashlight_monet_init(void)
{
	int ret;

	pr_info("flashlight_monet-Init start.\n");

#ifndef CONFIG_OF
	ret = platform_device_register(&monet_platform_device);
	if (ret) {
		pr_err("Failed to register platform device\n");
		return ret;
	}
#endif

	ret = platform_driver_register(&monet_platform_driver);
	if (ret) {
		pr_err("Failed to register platform driver\n");
		return ret;
	}

	pr_info("flashlight_monet Init done.\n");

	return 0;
}

static void __exit flashlight_monet_exit(void)
{
	pr_info("flashlight_monet-Exit start.\n");

	platform_driver_unregister(&monet_platform_driver);

	pr_info("flashlight_monet Exit done.\n");
}


module_init(flashlight_monet_init);
module_exit(flashlight_monet_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Joseph <zhangzetao@awinic.com.cn>");
MODULE_DESCRIPTION("AW Flashlight MONET Driver");

