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

#include "flashlight-core.h"
#include "flashlight-dt.h"
#ifdef VENDOR_EDIT
/*Henry.Chang@Camera.Driver add for 18161 torch duty 94ma 20190626*/
#include<soc/oppo/oppo_project.h>
#endif

/* device tree should be defined in flashlight-dt.h */
#ifndef AW3642_DTNAME
#define AW3642_DTNAME "mediatek,flashlights_aw3642"
#endif
#ifndef AW3642_DTNAME_I2C
#ifdef VENDOR_EDIT
/*Feng.Hu@Camera.Driver 20171121 ad for i2c probe*/
#define AW3642_DTNAME_I2C "mediatek,strobe_main_2"
#else
#define AW3642_DTNAME_I2C "mediatek,flashlights_aw3642_i2c"
#endif
#endif

#define AW3642_NAME "flashlights-aw3642"

/* define registers */
#define AW3642_REG_SILICON_REVISION (0x00)

#define AW3642_REG_FLASH_FEATURE      (0x08)
#define AW3642_INDUCTOR_CURRENT_LIMIT (0x40)
#define AW3642_FLASH_RAMP_TIME        (0x00)
#define AW3642_FLASH_TIMEOUT          (0x07)

#define AW3642_REG_CURRENT_CONTROL (0x09)

#define AW3642_REG_ENABLE (0x0A)
#define AW3642_ENABLE_STANDBY (0x00)
#define AW3642_ENABLE_TORCH (0x02)
#define AW3642_ENABLE_FLASH (0x03)

/* define level */
#define AW3642_LEVEL_NUM 18
#ifndef VENDOR_EDIT
/*Feng.Hu@Camera.Driver 20171227 modify for torch current start from 100ma*/
#define AW3642_LEVEL_TORCH 4
#else
#define AW3642_LEVEL_TORCH 3
#endif
#define AW3642_HW_TIMEOUT 800 /* ms */

/* define mutex and work queue */
static DEFINE_MUTEX(aw3642_mutex);
static struct work_struct aw3642_work;

/* aw3642 revision */
//static int is_aw3642lt;

/* define usage count */
static int use_count;

static u8 chip_id = 0x36;


/* define i2c */
static struct i2c_client *aw3642_i2c_client;

/* platform data */
struct aw3642_platform_data {
	int channel_num;
	struct flashlight_device_id *dev_id;
};

/* aw3642 chip data */
struct aw3642_chip_data {
	struct i2c_client *client;
	struct aw3642_platform_data *pdata;
	struct mutex lock;
};


/******************************************************************************
 * aw3642 operations
 *****************************************************************************/
static const int aw3642_current[AW3642_LEVEL_NUM] = {
	105,  105,  105,  281,  375,  469,  563,  656,  750,
	844,  938, 1008, 1055, 1055, 1055, 1055, 1055, 1055
};

#ifndef VENDOR_EDIT
/*Feng.Hu@Camera.Driver 20171227 modify for torch current start from 100ma*/
static const unsigned char aw3642_flash_level[AW3642_LEVEL_NUM] = {
	0x00, 0x10, 0x20, 0x30, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
};
#else
static const int aw3642_current_18161[AW3642_LEVEL_NUM] = {
	94,   94,  94,  281,  375,  469,  563,  656,  750,
	844,  938, 1008, 1055, 1055, 1055, 1055, 1055, 1055
};
static const unsigned char aw3642_flash_level[AW3642_LEVEL_NUM] = {
	17, 17, 17, 11, 15, 19, 23, 27, 31,
	35, 39, 42, 44, 44, 44, 44, 44, 44};
static const unsigned char aw3642_flash_level_18161[AW3642_LEVEL_NUM] = {
	15, 15, 15, 11, 15, 19, 23, 27, 31,
	35, 39, 42, 44, 44, 44, 44, 44, 44};
#endif

static int aw3642_level = -1;

static int aw3642_is_torch(int level)
{
	if (level >= AW3642_LEVEL_TORCH)
		return -1;

	return 0;
}

static int aw3642_verify_level(int level)
{
	if (level < 0)
		level = 0;
	else if (level >= AW3642_LEVEL_NUM)
		level = AW3642_LEVEL_NUM - 1;

	return level;
}

/* i2c wrapper function */
static int aw3642_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	int ret;
	struct aw3642_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	ret = i2c_smbus_write_byte_data(client, reg, val);
	mutex_unlock(&chip->lock);

	if (ret < 0)
		pr_err("failed writing at 0x%02x\n", reg);

	return ret;
}

static int aw3642_read_reg(struct i2c_client *client, u8 reg)
{
	int val = 0;
	struct aw3642_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	val = i2c_smbus_read_byte_data(client, reg);
	mutex_unlock(&chip->lock);

	return val;
}


int aw3642_readReg(int reg)
{
	int val = 0xff;
	if(!aw3642_i2c_client)
		return val;

	val = aw3642_read_reg(aw3642_i2c_client, reg);
	pr_err("aw3642_readReg val = %d \n", val);
	return (int)val;
}

/* flashlight enable function */
static int aw3642_enable(void)
{
	unsigned char reg, val;
	#ifdef VENDOR_EDIT
	/*Yijun.Tan@Camera add for reset flag register 20171223*/
	int ret = 0;
	#endif

	reg = 0x01;
	if (!aw3642_is_torch(aw3642_level)) {
		/* torch mode */
		val = 0x0B;
	} else {
		/* flash mode */
		val = 0x0F;
	}
	#ifdef VENDOR_EDIT
	/*Yijun.Tan@Camera add for reset flag register 20171223*/
	ret = aw3642_write_reg(aw3642_i2c_client, reg, val);
	return ret;
	#else
	return aw3642_write_reg(aw3642_i2c_client, reg, val);
	#endif


}

/* flashlight disable function */
static int aw3642_disable(void)
{
	int ret = 0;
	#ifdef VENDOR_EDIT
	/*Yijun.Tan@Camera add for reset flag register 20171223*/
	#endif

	aw3642_write_reg(aw3642_i2c_client, 0x01, 0x03);
	//aw3642_pinctrl_set(AW3642_PINCTRL_PIN_HWEN, AW3642_PINCTRL_PINSTATE_LOW);
	return ret;
}


/* set flashlight level */
static int aw3642_set_level(int level)
{
	unsigned char reg, val;
	#ifdef VENDOR_EDIT
	/*Yijun.Tan@Camera add for reset flag register 20171223*/
	int ret = 0;
	#endif

	level = aw3642_verify_level(level);
	aw3642_level = level;

	if (!aw3642_is_torch(aw3642_level)) {
		/* torch mode */
		reg = 0x05;
	} else {
		/* flash mode */
		reg = 0x03;
	}
	#ifdef VENDOR_EDIT
	/*Henry.Chang@Camera.Driver add for 18161 torch duty 94ma 20190626*/
	if (is_project(OPPO_18161)) {
		val = aw3642_flash_level_18161[level];
	} else {
		val = aw3642_flash_level[level];
	}
	/*Yijun.Tan@Camera add for reset flag register 20171223*/
	ret = aw3642_write_reg(aw3642_i2c_client, reg, val);
	return ret;
	#else
	val = aw3642_flash_level[level];
	return aw3642_write_reg(aw3642_i2c_client, reg, val);
	#endif
}

/* flashlight init */
int aw3642_init(void)
{
	int ret = 0;
	//aw3642_pinctrl_set(AW3642_PINCTRL_PIN_HWEN, AW3642_PINCTRL_PINSTATE_HIGH);
	chip_id = aw3642_readReg(0x00);
	pr_err("chip_id = 0x%x\n", chip_id);

	aw3642_write_reg(aw3642_i2c_client, 0x01, 0x03);
	aw3642_write_reg(aw3642_i2c_client, 0x03, 0x3f);
	aw3642_write_reg(aw3642_i2c_client, 0x05, 0x3f);
	aw3642_write_reg(aw3642_i2c_client, 0x08, 0x19);

	return ret;
}


/* flashlight uninit */
int aw3642_uninit(void)
{
	aw3642_disable();

	return 0;
}

/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static struct hrtimer aw3642_timer;
static unsigned int aw3642_timeout_ms;

static void aw3642_work_disable(struct work_struct *data)
{
	pr_err("work queue callback\n");
	aw3642_disable();
}

static enum hrtimer_restart aw3642_timer_func(struct hrtimer *timer)
{
	schedule_work(&aw3642_work);
	return HRTIMER_NORESTART;
}


/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
static int aw3642_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;
	ktime_t ktime;

	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		pr_err("FLASH_IOC_SET_TIME_OUT_TIME_MS(%d): %d\n",
				channel, (int)fl_arg->arg);
		aw3642_timeout_ms = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		pr_err("FLASH_IOC_SET_DUTY(%d): %d\n",
				channel, (int)fl_arg->arg);
		aw3642_set_level(fl_arg->arg);
		break;

	case FLASH_IOC_SET_ONOFF:
		pr_err("FLASH_IOC_SET_ONOFF(%d): %d\n",
				channel, (int)fl_arg->arg);
		if (fl_arg->arg == 1) {
			if (aw3642_timeout_ms) {
				ktime = ktime_set(aw3642_timeout_ms / 1000,
						(aw3642_timeout_ms % 1000) * 1000000);
				hrtimer_start(&aw3642_timer, ktime, HRTIMER_MODE_REL);
			}
			aw3642_enable();
		} else {
			aw3642_disable();
			hrtimer_cancel(&aw3642_timer);
		}
		break;

	case FLASH_IOC_GET_DUTY_NUMBER:
		pr_err("FLASH_IOC_GET_DUTY_NUMBER(%d)\n", channel);
		fl_arg->arg = AW3642_LEVEL_NUM;
		break;

	case FLASH_IOC_GET_MAX_TORCH_DUTY:
		pr_err("FLASH_IOC_GET_MAX_TORCH_DUTY(%d)\n", channel);
		fl_arg->arg = AW3642_LEVEL_TORCH - 1;
		break;

	case FLASH_IOC_GET_DUTY_CURRENT:
		fl_arg->arg = aw3642_verify_level(fl_arg->arg);
		pr_err("FLASH_IOC_GET_DUTY_CURRENT(%d): %d\n",
				channel, (int)fl_arg->arg);
		#ifdef VENDOR_EDIT
		/*Henry.Chang@Camera.Driver add for 18161 torch duty 94ma 20190626*/
		if (is_project(OPPO_18161)) {
			fl_arg->arg = aw3642_current_18161[fl_arg->arg];
		} else {
			fl_arg->arg = aw3642_current[fl_arg->arg];
		}
		#else
		fl_arg->arg = aw3642_current[fl_arg->arg];
		#endif
		break;

	case FLASH_IOC_GET_HW_TIMEOUT:
		pr_err("FLASH_IOC_GET_HW_TIMEOUT(%d)\n", channel);
		fl_arg->arg = AW3642_HW_TIMEOUT;
		break;

	default:
		pr_info("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int aw3642_open(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int aw3642_release(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int aw3642_set_driver(int set)
{
	int ret = 0;

	/* set chip and usage count */
	mutex_lock(&aw3642_mutex);
	if (set) {
		if (!use_count)
			ret = aw3642_init();
		#ifdef VENDOR_EDIT
		/*Feng.Hu@Camera.Driver modify as when init failed, don't increase users*/
		if (ret >= 0) {
			use_count++;
		}
		#else
		use_count++;
		#endif
		pr_err("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			ret = aw3642_uninit();
		if (use_count < 0)
			use_count = 0;
		pr_err("Unset driver: %d\n", use_count);
	}
	mutex_unlock(&aw3642_mutex);

	return ret;
}

static ssize_t aw3642_strobe_store(struct flashlight_arg arg)
{
	aw3642_set_driver(1);
	aw3642_set_level(arg.level);
	aw3642_timeout_ms = 0;
	aw3642_enable();
	msleep(arg.dur);
	aw3642_disable();
	aw3642_set_driver(0);

	return 0;
}

static struct flashlight_operations aw3642_ops = {
	aw3642_open,
	aw3642_release,
	aw3642_ioctl,
	aw3642_strobe_store,
	aw3642_set_driver
};


/******************************************************************************
 * I2C device and driver
 *****************************************************************************/
static int aw3642_chip_init(struct aw3642_chip_data *chip)
{
	/* NOTE: Chip initialication move to "set driver" operation for power saving issue.
	 * aw3642_init();
	 */

	return 0;
}

static int aw3642_parse_dt(struct device *dev,
		struct aw3642_platform_data *pdata)
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
	pr_err("1111 Channel number(%d).\n", pdata->channel_num);

	if (of_property_read_u32(np, "decouple", &decouple))
		pr_info("Parse no dt, decouple.\n");

	pdata->dev_id = devm_kzalloc(dev,
			pdata->channel_num * sizeof(struct flashlight_device_id),
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
		snprintf(pdata->dev_id[i].name, FLASHLIGHT_NAME_SIZE, AW3642_NAME);
		pdata->dev_id[i].channel = i;
		pdata->dev_id[i].decouple = decouple;

		pr_err("1111 Parse dt (type,ct,part,name,channel,decouple)=(%d,%d,%d,%s,%d,%d).\n",
				pdata->dev_id[i].type, pdata->dev_id[i].ct,
				pdata->dev_id[i].part, pdata->dev_id[i].name,
				pdata->dev_id[i].channel, pdata->dev_id[i].decouple);
		i++;
	}

	return 0;

err_node_put:
	of_node_put(cnp);
	return -EINVAL;
}

static int aw3642_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct aw3642_platform_data *pdata = dev_get_platdata(&client->dev);
	struct aw3642_chip_data *chip;
	int err;
	int i;

	pr_err("Probe start.\n");

	/* check i2c */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("Failed to check i2c functionality.\n");
		err = -ENODEV;
		goto err_out;
	}
	pr_err("Probe start. 1111 \n");

	/* init chip private data */
	chip = kzalloc(sizeof(struct aw3642_chip_data), GFP_KERNEL);
	if (!chip) {
		err = -ENOMEM;
		goto err_out;
	}
	chip->client = client;
	pr_err("Probe start. 22222 \n");

	/* init platform data */
	if (!pdata) {
		pr_err("Probe start. 33333 \n");
		pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			pr_err("Probe start.3333 1111\n");
			err = -ENOMEM;
			goto err_free;
		}
		client->dev.platform_data = pdata;
		err = aw3642_parse_dt(&client->dev, pdata);
		pr_err("Probe start.33333 22222\n");
		if (err)
			goto err_free;
	}
	pr_err("Probe start. 44444 \n");
	chip->pdata = pdata;
	i2c_set_clientdata(client, chip);
	aw3642_i2c_client = client;
	aw3642_i2c_client->addr = 0x63;

	/* init mutex and spinlock */
	mutex_init(&chip->lock);

	/* init work queue */
	INIT_WORK(&aw3642_work, aw3642_work_disable);

	/* init timer */
	hrtimer_init(&aw3642_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw3642_timer.function = aw3642_timer_func;
	aw3642_timeout_ms = 800;
	pr_err("Probe start. 55555 \n");

	/* init chip hw */
	aw3642_chip_init(chip);

	/* clear usage count */
	use_count = 0;

	/* register flashlight device */
	if (pdata->channel_num) {
		for (i = 0; i < pdata->channel_num; i++)
			pr_err("Probe start. 5555 1111\n");
			if (flashlight_dev_register_by_device_id(&pdata->dev_id[i], &aw3642_ops)) {
				err = -EFAULT;
				goto err_free;
			}
	} else {
	pr_err("Probe start. 5555  2222\n");
		if (flashlight_dev_register(AW3642_NAME, &aw3642_ops)) {
			err = -EFAULT;
			goto err_free;
		}
	}

	pr_err("Probe done. 66666\n");

	return 0;

err_free:
	i2c_set_clientdata(client, NULL);
	kfree(chip);
err_out:
	return err;
}

static int aw3642_i2c_remove(struct i2c_client *client)
{
	struct aw3642_platform_data *pdata = dev_get_platdata(&client->dev);
	struct aw3642_chip_data *chip = i2c_get_clientdata(client);
	int i;

	pr_err("Remove start.\n");

	client->dev.platform_data = NULL;

	/* unregister flashlight device */
	if (pdata && pdata->channel_num)
		for (i = 0; i < pdata->channel_num; i++)
			flashlight_dev_unregister_by_device_id(&pdata->dev_id[i]);
	else
		flashlight_dev_unregister(AW3642_NAME);

	/* flush work queue */
	flush_work(&aw3642_work);

	/* free resource */
	kfree(chip);

	pr_err("Remove done.\n");

	return 0;
}

static const struct i2c_device_id aw3642_i2c_id[] = {
	{AW3642_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, aw3642_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id aw3642_i2c_of_match[] = {
	{.compatible = AW3642_DTNAME_I2C},
	{},
};
MODULE_DEVICE_TABLE(of, aw3642_i2c_of_match);
#endif

static struct i2c_driver aw3642_i2c_driver = {
	.driver = {
		.name = AW3642_NAME,
#ifdef CONFIG_OF
		.of_match_table = aw3642_i2c_of_match,
#endif
	},
	.probe = aw3642_i2c_probe,
	.remove = aw3642_i2c_remove,
	.id_table = aw3642_i2c_id,
};

module_i2c_driver(aw3642_i2c_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xi Chen <xixi.chen@mediatek.com>");
MODULE_DESCRIPTION("MTK Flashlight AW3642 Driver");

