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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/leds.h>

#include "flashlight-core.h"
#include "flashlight-dt.h"

/* device tree should be defined in flashlight-dt.h */
#ifndef MP3331_DTNAME
#define MP3331_DTNAME "mediatek,flashlights_mp3331"
#endif
#ifndef MP3331_DTNAME_I2C
#define MP3331_DTNAME_I2C "mediatek,flashlights_mp3331_i2c"
#endif

#define MP3331_NAME "flashlights-mp3331"


/* define channel, level */
#define MP3331_CHANNEL_NUM 2
#define MP3331_CHANNEL_CH1 0
#define MP3331_CHANNEL_CH2 1

#define MP3331_LEVEL_NUM 12
#define MP3331_LEVEL_TORCH 7

/* define mutex and work queue */
static DEFINE_MUTEX(mp3331_mutex);
static struct work_struct mp3331_work_ch1;
static struct work_struct mp3331_work_ch2;

/* define pinctrl */
#define MP3331_PINCTRL_PIN_HWEN 0
#define MP3331_PINCTRL_PINSTATE_LOW 0
#define MP3331_PINCTRL_PINSTATE_HIGH 1
#define MP3331_PINCTRL_STATE_HWEN_HIGH "hwen_high"
#define MP3331_PINCTRL_STATE_HWEN_LOW  "hwen_low"
static struct pinctrl *mp3331_pinctrl;
static struct pinctrl_state *mp3331_hwen_high;
static struct pinctrl_state *mp3331_hwen_low;

/* define usage count */
static int use_count;

/* define i2c */
static struct i2c_client *mp3331_i2c_client;

/* platform data */
struct mp3331_platform_data {
	u8 torch_pin_enable;         /* 1: TX1/TORCH pin isa hardware TORCH enable */
	u8 pam_sync_pin_enable;      /* 1: TX2 Mode The ENVM/TX2 is a PAM Sync. on input */
	u8 thermal_comp_mode_enable; /* 1: LEDI/NTC pin in Thermal Comparator Mode */
	u8 strobe_pin_disable;       /* 1: STROBE Input disabled */
	u8 vout_mode_enable;         /* 1: Voltage Out Mode enable */
};

/* mp3331 chip data */
struct mp3331_chip_data {
	struct i2c_client *client;
	struct mp3331_platform_data *pdata;
	struct mutex lock;
	u8 last_flag;
	u8 no_pdata;
};



static int gDuty;


/******************************************************************************
 * Pinctrl configuration
 *****************************************************************************/
static int mp3331_pinctrl_init(struct platform_device *pdev)
{
	int ret = 0;

	/* get pinctrl */
	mp3331_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(mp3331_pinctrl)) {
		pr_err("Failed to get flashlight pinctrl.\n");
		ret = PTR_ERR(mp3331_pinctrl);
	}

	/* Flashlight HWEN pin initialization */
	mp3331_hwen_high = pinctrl_lookup_state(mp3331_pinctrl, MP3331_PINCTRL_STATE_HWEN_HIGH);
	if (IS_ERR(mp3331_hwen_high)) {
		pr_err("Failed to init (%s)\n", MP3331_PINCTRL_STATE_HWEN_HIGH);
		ret = PTR_ERR(mp3331_hwen_high);
	}
	mp3331_hwen_low = pinctrl_lookup_state(mp3331_pinctrl, MP3331_PINCTRL_STATE_HWEN_LOW);
	if (IS_ERR(mp3331_hwen_low)) {
		pr_err("Failed to init (%s)\n", MP3331_PINCTRL_STATE_HWEN_LOW);
		ret = PTR_ERR(mp3331_hwen_low);
	}

	return ret;
}

static int mp3331_pinctrl_set(int pin, int state)
{
	int ret = 0;

	if (IS_ERR(mp3331_pinctrl)) {
		pr_err("pinctrl is not available\n");
		return -1;
	}

	switch (pin) {
	case MP3331_PINCTRL_PIN_HWEN:
		if (state == MP3331_PINCTRL_PINSTATE_LOW && !IS_ERR(mp3331_hwen_low))
			pinctrl_select_state(mp3331_pinctrl, mp3331_hwen_low);
		else if (state == MP3331_PINCTRL_PINSTATE_HIGH && !IS_ERR(mp3331_hwen_high))
			pinctrl_select_state(mp3331_pinctrl, mp3331_hwen_high);
		else
			pr_err("set err, pin(%d) state(%d)\n", pin, state);
		break;
	default:
		pr_err("set err, pin(%d) state(%d)\n", pin, state);
		break;
	}
	pr_err("pin(%d) state(%d)\n", pin, state);

	return ret;
}


/******************************************************************************
 * mp3331 operations
 *****************************************************************************/

static int gIsTorch[18] = { 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static int gLedDuty[18] = { 3, 4, 6, 7, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42, 45, 48 };//{ 0, 32, 64, 96, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };

#if 0
static volatile unsigned char mp3331_reg_enable;
static volatile int mp3331_level_ch1 = -1;
static volatile int mp3331_level_ch2 = -1;
#endif

#if 0
static int mp3331_is_torch(int level)
{
	if (level >= MP3331_LEVEL_TORCH)
		return -1;

	return 0;
}
#endif

static int mp3331_verify_level(int level)
{
	if (level < 0)
		level = 0;
	else if (level >= MP3331_LEVEL_NUM)
		level = MP3331_LEVEL_NUM ;

	return level;
}

/* i2c wrapper function */
static int mp3331_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	int ret;
	struct mp3331_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	ret = i2c_smbus_write_byte_data(client, reg, val);
	mutex_unlock(&chip->lock);

	if (ret < 0)
		pr_err("failed writing at 0x%02x\n", reg);

	return ret;
}

static int mp3331_read_reg(struct i2c_client *client, u8 reg)
{
	int ret;
	struct mp3331_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	ret = i2c_smbus_read_byte_data(client, reg);
	mutex_unlock(&chip->lock);

	if (ret < 0)
		pr_err("failed writing at 0x%02x\n", reg);

	return ret;
}


int mp3331_readReg(int reg)
{

	int val = 0xff;
	int retry= 2;
	if(!mp3331_i2c_client)
	    return val;
	while (retry > 0) {
	    val = mp3331_read_reg(mp3331_i2c_client, reg);
	    if ((val & 0x00FF) == 0x18)
	        break;
	    retry--;
	}
	pr_err("MP3331 read reg 0x%x = 0x%x\n",reg,val);
	return (int)val;
}


/* flashlight enable function */
static int mp3331_enable_ch1(void)
{
#if 0
	unsigned char reg, val;

	reg = MP3331_REG_ENABLE;
	if (!mp3331_is_torch(mp3331_level_ch1)) {
		/* torch mode */
		mp3331_reg_enable |= MP3331_ENABLE_LED1_TORCH;
	} else {
		/* flash mode */
		mp3331_reg_enable |= MP3331_ENABLE_LED1_FLASH;
	}
	val = mp3331_reg_enable;

	return mp3331_write_reg(mp3331_i2c_client, reg, val);

#endif
	int ret;
	int buf[2];
	int temp1,temp2;
	buf[0] = 0x01;
	if (gIsTorch[gDuty] == 1)
		buf[1] = 0x14;
	else
		buf[1] = 0x16;
	ret = mp3331_write_reg(mp3331_i2c_client, buf[0], buf[1]);
	pr_err(" FL_Enable line=%d gDuty = 0x%x\n", __LINE__, gDuty);
	temp1 = mp3331_readReg(0x0a);
	temp2 = mp3331_readReg(0x0b);
	pr_err(" FL_Enable line=%d, enable value is 0x%x,flags value is 0x%x\n", __LINE__, temp1,temp2);
	return ret;

}

static int mp3331_enable_ch2(void)
{
#if 0
	unsigned char reg, val;

	reg = MP3331_REG_ENABLE;
	if (!mp3331_is_torch(mp3331_level_ch2)) {
		/* torch mode */
		mp3331_reg_enable |= MP3331_ENABLE_LED2_TORCH;
	} else {
		/* flash mode */
		mp3331_reg_enable |= MP3331_ENABLE_LED2_FLASH;
	}
	val = mp3331_reg_enable;

	return mp3331_write_reg(mp3331_i2c_client, reg, val);
#endif
	return 0;
}

static int mp3331_enable(int channel)
{
	if (channel == MP3331_CHANNEL_CH1)
		mp3331_enable_ch1();
	else if (channel == MP3331_CHANNEL_CH2)
		mp3331_enable_ch2();
	else {
		pr_err("Error channel\n");
		return -1;
	}

	return 0;
}

/* flashlight disable function */
static int mp3331_disable_ch1(void)
{
#if 0
	unsigned char reg, val;

	reg = MP3331_REG_ENABLE;
	if (mp3331_reg_enable & MP3331_MASK_ENABLE_LED2) {
		/* if LED 2 is enable, disable LED 1 */
		mp3331_reg_enable &= (~MP3331_ENABLE_LED1);
	} else {
		/* if LED 2 is enable, disable LED 1 and clear mode */
		mp3331_reg_enable &= (~MP3331_ENABLE_LED1_FLASH);
	}
	val = mp3331_reg_enable;

	return mp3331_write_reg(mp3331_i2c_client, reg, val);
#endif

	int ret;
	int buf[2];
	int temp1,temp2;
	buf[0] = 0x01;
	buf[1] = 0xe0;
	ret = mp3331_write_reg(mp3331_i2c_client, buf[0], buf[1]);
	temp1 = mp3331_readReg(0x0a);
	temp2 = mp3331_readReg(0x0b);
	pr_err(" FL_Disable line=%d, enable value is 0x%x,flags value is 0x%x\n", __LINE__, temp1,temp2);
	/*pr_err(" FL_Disable line=%d\n", __LINE__);*/
	return ret;

}

static int mp3331_disable_ch2(void)
{
#if 0
	unsigned char reg, val;

	reg = MP3331_REG_ENABLE;
	if (mp3331_reg_enable & MP3331_MASK_ENABLE_LED1) {
		/* if LED 1 is enable, disable LED 2 */
		mp3331_reg_enable &= (~MP3331_ENABLE_LED2);
	} else {
		/* if LED 1 is enable, disable LED 2 and clear mode */
		mp3331_reg_enable &= (~MP3331_ENABLE_LED2_FLASH);
	}
	val = mp3331_reg_enable;

	return mp3331_write_reg(mp3331_i2c_client, reg, val);
#endif
	return 0;
}

static int mp3331_disable(int channel)
{
	if (channel == MP3331_CHANNEL_CH1)
		mp3331_disable_ch1();
	else if (channel == MP3331_CHANNEL_CH2)
		mp3331_disable_ch2();
	else {
		pr_err("Error channel\n");
		return -1;
	}

	return 0;
}

/* set flashlight level */
static int mp3331_set_level_ch1(int level)
{
	int ret;
	int buf[2];
	int temp1,temp2;

	level = mp3331_verify_level(level);


	gDuty = level;
	if (gIsTorch[gDuty] == 1){
		buf[0] = 0x0a;
		pr_err("gIsTorch[gDuty] = 1 \n");
	}
	else {
		buf[0] = 0x06;
	}
	buf[1] = gLedDuty[level];
	ret = mp3331_write_reg(mp3331_i2c_client, buf[0], buf[1]);
	pr_err(" FL_dim_duty line=%d, current value is 0x%x\n", __LINE__, buf[1]);
	temp1 = mp3331_readReg(0x09);
	temp2 = mp3331_readReg(0x0b);
	pr_err(" FL_dim_duty line=%d, duty value is 0x%x,flags value is 0x%x\n", __LINE__, temp1,temp2);
	return ret;



#if 0
	/* set torch brightness level */
	reg = MP3331_REG_TORCH_LEVEL_LED1;
	val = mp3331_torch_level[level];
	ret = mp3331_write_reg(mp3331_i2c_client, reg, val);

	mp3331_level_ch1 = level;

	/* set flash brightness level */
	reg = MP3331_REG_FLASH_LEVEL_LED1;
	val = mp3331_flash_level[level];
	ret = mp3331_write_reg(mp3331_i2c_client, reg, val);

	return ret;
#endif
}

int mp3331_set_level_ch2(int level)
{

	level = mp3331_verify_level(level);

#if 0
	/* set torch brightness level */
	reg = MP3331_REG_TORCH_LEVEL_LED2;
	val = mp3331_torch_level[level];
	ret = mp3331_write_reg(mp3331_i2c_client, reg, val);

	mp3331_level_ch2 = level;

	/* set flash brightness level */
	reg = MP3331_REG_FLASH_LEVEL_LED2;
	val = mp3331_flash_level[level];
	ret = mp3331_write_reg(mp3331_i2c_client, reg, val);
#endif
	return 0;
}

static int mp3331_set_level(int channel, int level)
{
	if (channel == MP3331_CHANNEL_CH1)
		mp3331_set_level_ch1(level);
	else if (channel == MP3331_CHANNEL_CH2)
		mp3331_set_level_ch2(level);
	else {
		pr_err("Error channel\n");
		return -1;
	}

	return 0;
}

/* flashlight init */
int mp3331_init(void)
{
	int buf[2];

	mp3331_pinctrl_set(MP3331_PINCTRL_PIN_HWEN, MP3331_PINCTRL_PINSTATE_HIGH);

	buf[0] = 0x01;
	buf[1] = 0xa0;
	mp3331_write_reg(mp3331_i2c_client, buf[0], buf[1]);

	buf[0] = 0x02;
	buf[1] = 0x0b;
	mp3331_write_reg(mp3331_i2c_client, buf[0], buf[1]);

	buf[0] = 0x03;
	buf[1] = 0xf0;
	mp3331_write_reg(mp3331_i2c_client, buf[0], buf[1]);

	buf[0] = 0x04;
	buf[1] = 0xff;
	mp3331_write_reg(mp3331_i2c_client, buf[0], buf[1]);

    buf[0] = 0x05;
	buf[1] = 0x14;
	mp3331_write_reg(mp3331_i2c_client, buf[0], buf[1]);
	return 0;

}

/* flashlight uninit */
int mp3331_uninit(void)
{
	mp3331_disable(MP3331_CHANNEL_CH1);
	mp3331_disable(MP3331_CHANNEL_CH2);
	mp3331_pinctrl_set(MP3331_PINCTRL_PIN_HWEN, MP3331_PINCTRL_PINSTATE_LOW);

	return 0;
}


/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static struct hrtimer mp3331_timer_ch1;
static struct hrtimer mp3331_timer_ch2;
static unsigned int mp3331_timeout_ms[MP3331_CHANNEL_NUM];

static void mp3331_work_disable_ch1(struct work_struct *data)
{
	pr_err("ht work queue callback\n");
	mp3331_disable_ch1();
}

static void mp3331_work_disable_ch2(struct work_struct *data)
{
	pr_err("lt work queue callback\n");
	mp3331_disable_ch2();
}

static enum hrtimer_restart mp3331_timer_func_ch1(struct hrtimer *timer)
{
	schedule_work(&mp3331_work_ch1);
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart mp3331_timer_func_ch2(struct hrtimer *timer)
{
	schedule_work(&mp3331_work_ch2);
	return HRTIMER_NORESTART;
}

int mp3331_timer_start(int channel, ktime_t ktime)
{
	if (channel == MP3331_CHANNEL_CH1)
		hrtimer_start(&mp3331_timer_ch1, ktime, HRTIMER_MODE_REL);
	else if (channel == MP3331_CHANNEL_CH2)
		hrtimer_start(&mp3331_timer_ch2, ktime, HRTIMER_MODE_REL);
	else {
		pr_err("Error channel\n");
		return -1;
	}

	return 0;
}

int mp3331_timer_cancel(int channel)
{
	if (channel == MP3331_CHANNEL_CH1)
		hrtimer_cancel(&mp3331_timer_ch1);
	else if (channel == MP3331_CHANNEL_CH2)
		hrtimer_cancel(&mp3331_timer_ch2);
	else {
		pr_err("Error channel\n");
		return -1;
	}

	return 0;
}


/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
static int mp3331_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;
	ktime_t ktime;

	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	/* verify channel */
	if (channel < 0 || channel >= MP3331_CHANNEL_NUM) {
		pr_err("Failed with error channel\n");
		return -EINVAL;
	}

	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		pr_err("FLASH_IOC_SET_TIME_OUT_TIME_MS(%d): %d\n",
				channel, (int)fl_arg->arg);
		mp3331_timeout_ms[channel] = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		pr_err("FLASH_IOC_SET_DUTY(%d): %d\n",
				channel, (int)fl_arg->arg);
		mp3331_set_level(channel, fl_arg->arg);
		break;

	case FLASH_IOC_SET_ONOFF:
		pr_err("FLASH_IOC_SET_ONOFF(%d): %d\n",
				channel, (int)fl_arg->arg);
		if (fl_arg->arg == 1) {
			if (mp3331_timeout_ms[channel]) {
				ktime = ktime_set(mp3331_timeout_ms[channel] / 1000,
						(mp3331_timeout_ms[channel] % 1000) * 1000000);
				mp3331_timer_start(channel, ktime);
			}
			mp3331_enable(channel);
		} else {
			mp3331_disable(channel);
			mp3331_timer_cancel(channel);
		}
		break;

	default:
		pr_err("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int mp3331_open(void)
{
	/* Actual behavior move to set driver function since power saving issue */
	return 0;
}

static int mp3331_release(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int mp3331_set_driver(int set)
{
	int ret = 0;

	/* init chip and set usage count */
	mutex_lock(&mp3331_mutex);
	if (set) {
		if (!use_count)
			ret = mp3331_init();
		use_count++;
		pr_debug("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			ret = mp3331_uninit();
		if (use_count < 0)
			use_count = 0;
		pr_debug("Unset driver: %d\n", use_count);
	}
	mutex_unlock(&mp3331_mutex);

	return ret;
}

static ssize_t mp3331_strobe_store(struct flashlight_arg arg)
{
	mp3331_set_driver(1);
	mp3331_set_level(arg.ct, arg.level);
	mp3331_enable(arg.ct);
	msleep(arg.dur);
	mp3331_disable(arg.ct);
	mp3331_set_driver(0);

	return 0;
}

static struct flashlight_operations mp3331_ops = {
	mp3331_open,
	mp3331_release,
	mp3331_ioctl,
	mp3331_strobe_store,
	mp3331_set_driver
};


/******************************************************************************
 * I2C device and driver
 *****************************************************************************/
static int mp3331_chip_init(struct mp3331_chip_data *chip)
{
	/* NOTE: Chip initialication move to "set driver" operation for power saving issue.
	 * mp3331_init();
	 */

	return 0;
}

static int mp3331_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct mp3331_chip_data *chip;
	struct mp3331_platform_data *pdata = client->dev.platform_data;
	int err;

	pr_err("mp3331 i2c Probe start.\n");

	/* check i2c */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("Failed to check i2c functionality.\n");
		err = -ENODEV;
		goto err_out;
	}

	/* init chip private data */
	chip = kzalloc(sizeof(struct mp3331_chip_data), GFP_KERNEL);
	if (!chip) {
		err = -ENOMEM;
		goto err_out;
	}
	chip->client = client;

	/* init platform data */
	if (!pdata) {
		pr_err("Platform data does not exist\n");
		pdata = kzalloc(sizeof(struct mp3331_platform_data), GFP_KERNEL);
		if (!pdata) {
			pr_err("Failed to allocate memory.\n");
			err = -ENOMEM;
			goto err_init_pdata;
		}
		chip->no_pdata = 1;
	}
	chip->pdata = pdata;
	i2c_set_clientdata(client, chip);
	mp3331_i2c_client = client;

	/* init mutex and spinlock */
	mutex_init(&chip->lock);

	/* init work queue */
	INIT_WORK(&mp3331_work_ch1, mp3331_work_disable_ch1);
	INIT_WORK(&mp3331_work_ch2, mp3331_work_disable_ch2);

	/* init timer */
	hrtimer_init(&mp3331_timer_ch1, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	mp3331_timer_ch1.function = mp3331_timer_func_ch1;
	hrtimer_init(&mp3331_timer_ch2, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	mp3331_timer_ch2.function = mp3331_timer_func_ch2;
	mp3331_timeout_ms[MP3331_CHANNEL_CH1] = 100;
	mp3331_timeout_ms[MP3331_CHANNEL_CH2] = 100;

	/* init chip hw */
	mp3331_chip_init(chip);

	/* register flashlight operations */
	if (flashlight_dev_register(MP3331_NAME, &mp3331_ops)) {
		pr_err("Failed to register flashlight device.\n");
		err = -EFAULT;
		goto err_free;
	}

	/* clear usage count */
	use_count = 0;

	pr_err("Probe done.\n");

	return 0;

err_free:
	kfree(chip->pdata);
err_init_pdata:
	i2c_set_clientdata(client, NULL);
	kfree(chip);
err_out:
	return err;
}

static int mp3331_i2c_remove(struct i2c_client *client)
{
	struct mp3331_chip_data *chip = i2c_get_clientdata(client);

	pr_err("Remove start.\n");

	/* flush work queue */
	flush_work(&mp3331_work_ch1);
	flush_work(&mp3331_work_ch2);

	/* unregister flashlight operations */
	flashlight_dev_unregister(MP3331_NAME);

	/* free resource */
	if (chip->no_pdata)
		kfree(chip->pdata);
	kfree(chip);

	pr_err("Remove done.\n");

	return 0;
}

static const struct i2c_device_id mp3331_i2c_id[] = {
	{MP3331_NAME, 0},
	{}
};

#ifdef CONFIG_OF
static const struct of_device_id mp3331_i2c_of_match[] = {
	{.compatible = MP3331_DTNAME_I2C},
	{},
};
#endif

static struct i2c_driver mp3331_i2c_driver = {
	.driver = {
		   .name = MP3331_NAME,
#ifdef CONFIG_OF
		   .of_match_table = mp3331_i2c_of_match,
#endif
		   },
	.probe = mp3331_i2c_probe,
	.remove = mp3331_i2c_remove,
	.id_table = mp3331_i2c_id,
};


/******************************************************************************
 * Platform device and driver
 *****************************************************************************/
static int mp3331_probe(struct platform_device *dev)
{
	pr_err("Probe start[mp3331_probe].\n");


	/* init pinctrl */
	if (mp3331_pinctrl_init(dev)) {
		pr_err("Failed to init pinctrl.\n");
		return -1;
	}


	if (i2c_add_driver(&mp3331_i2c_driver)) {
		pr_err("Failed to add i2c driver.\n");
		return -1;
	}

	pr_err("Probe done[mp3331_probe].\n");

	return 0;
}

static int mp3331_remove(struct platform_device *dev)
{
	pr_err("Remove start.\n");

	i2c_del_driver(&mp3331_i2c_driver);

	pr_err("Remove done.\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mp3331_of_match[] = {
	{.compatible = "mediatek,flashlights_mp3331"},
	{},
};
MODULE_DEVICE_TABLE(of, mp3331_of_match);
#else
static struct platform_device mp3331_platform_device[] = {
	{
		.name = MP3331_NAME,
		.id = 0,
		.dev = {}
	},
	{}
};
MODULE_DEVICE_TABLE(platform, mp3331_platform_device);
#endif

static struct platform_driver mp3331_platform_driver = {
	.probe = mp3331_probe,
	.remove = mp3331_remove,
	.driver = {
		.name = MP3331_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = mp3331_of_match,
#endif
	},
};

static int __init flashlight_MP3331_init(void)
{
	int ret;

	pr_err("Init start.\n");

#ifndef CONFIG_OF
	ret = platform_device_register(&mp3331_platform_device);
	pr_err("zhaoyi  : not define CONFIG_OF.\n");
	if (ret) {
		pr_err("Failed to register platform device\n");
		return ret;
	}
#endif

	ret = platform_driver_register(&mp3331_platform_driver);
	if (ret) {
		pr_err("Failed to register platform driver\n");
		return ret;
	}

	pr_err("Init done.\n");

	return 0;
}

static void __exit flashlight_MP3331_exit(void)
{
	pr_err("Exit start.\n");

	platform_driver_unregister(&mp3331_platform_driver);

	pr_err("Exit done.\n");
}

module_init(flashlight_MP3331_init);
module_exit(flashlight_MP3331_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("YiZhao <yi.zhao@mediatek.com>");
MODULE_DESCRIPTION("Flashlight mp3331 Driver");

