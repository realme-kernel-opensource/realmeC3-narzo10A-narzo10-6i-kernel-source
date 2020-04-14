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

#define LOG_TAG "LCM"

#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "data_hw_roundedpattern.h"
#endif

#include "lcm_drv.h"


#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#endif

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

#define LCM_ID_NT36672C_JDI (0x01)

static const unsigned int BL_MIN_LEVEL = 20;
static struct LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))
#define MDELAY(n)		(lcm_util.mdelay(n))
#define UDELAY(n)		(lcm_util.udelay(n))

#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
		lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) \
	  lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
		lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)
/*DynFPS*/
#define dfps_dsi_send_cmd( \
		cmdq, cmd, count, para_list, force_update) \
		lcm_util.dsi_dynfps_send_cmd( \
		cmdq, cmd, count, para_list, force_update)

#ifndef BUILD_LK
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
/* #include <linux/jiffies.h> */
/* #include <linux/delay.h> */
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#endif

#define LCM_DSI_CMD_MODE 0
#define FRAME_WIDTH (1080)
#define FRAME_HEIGHT (2400)

/* physical size in um */
#define LCM_PHYSICAL_WIDTH (68040)
#define LCM_PHYSICAL_HEIGHT (136080)
#define LCM_DENSITY (480)

#define REGFLAG_DELAY		0xFFFC
#define REGFLAG_UDELAY	0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD
#define REGFLAG_RESET_LOW	0xFFFE
#define REGFLAG_RESET_HIGH	0xFFFF

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28, 0, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} },
	{0x4F, 1, {0x01} },
	{REGFLAG_DELAY, 120, {} }
};

static struct LCM_setting_table init_setting[] = {
	{0xFF, 1, {0x10} },
	{REGFLAG_DELAY, 100, {} },
	{0xFB, 1, {0x01} },
	{0xB0, 1, {0x00} },
	{0xC0, 1, {0x00} },
	{0xC2, 2, {0x1B, 0xA0} },

	{0xFF, 1, {0x20} },
	{REGFLAG_DELAY, 100, {} },
	{0xFB, 1, {0x01} },
	{0x01, 1, {0x66} },
	{0x06, 1, {0x40} },
	{0x07, 1, {0x38} },
	{0x95, 1, {0xD1} },
	{0x96, 1, {0xD1} },
	{0xF2, 1, {0x64} },
	{0xF4, 1, {0x64} },
	{0xF6, 1, {0x64} },
	{0xF8, 1, {0x64} },

	{0xFF, 1, {0x24} },
	{REGFLAG_DELAY, 100, {} },
	{0xFB, 1, {0x01} },
	{0x01, 1, {0x0F} },
	{0x03, 1, {0x0C} },
	{0x05, 1, {0x1D} },
	{0x08, 1, {0x2F} },
	{0x09, 1, {0x2E} },
	{0x0A, 1, {0x2D} },
	{0x0B, 1, {0x2C} },
	{0x11, 1, {0x17} },
	{0x12, 1, {0x13} },
	{0x13, 1, {0x15} },
	{0x15, 1, {0x14} },
	{0x16, 1, {0x16} },
	{0x17, 1, {0x18} },
	{0x1B, 1, {0x01} },
	{0x1D, 1, {0x1D} },
	{0x20, 1, {0x2F} },
	{0x21, 1, {0x2E} },
	{0x22, 1, {0x2D} },
	{0x23, 1, {0x2C} },
	{0x29, 1, {0x17} },
	{0x2A, 1, {0x13} },
	{0x2B, 1, {0x15} },
	{0x2F, 1, {0x14} },
	{0x30, 1, {0x16} },
	{0x31, 1, {0x18} },
	{0x32, 1, {0x04} },
	{0x34, 1, {0x10} },
	{0x35, 1, {0x1F} },
	{0x36, 1, {0x1F} },
	{0x37, 1, {0x20} },
	{0x4D, 1, {0x1B} },
	{0x4E, 1, {0x4B} },
	{0x4F, 1, {0x4B} },
	{0x53, 1, {0x4B} },
	{0x71, 1, {0x30} },
	{0x79, 1, {0x11} },
	{0x7A, 1, {0x82} },
	{0x7B, 1, {0x96} },
	{0x7D, 1, {0x04} },
	{0x80, 1, {0x04} },
	{0x81, 1, {0x04} },
	{0x82, 1, {0x13} },
	{0x84, 1, {0x31} },
	{0x85, 1, {0x00} },
	{0x86, 1, {0x00} },
	{0x87, 1, {0x00} },
	{0x90, 1, {0x13} },
	{0x92, 1, {0x31} },
	{0x93, 1, {0x00} },
	{0x94, 1, {0x00} },
	{0x95, 1, {0x00} },
	{0x9C, 1, {0xF4} },
	{0x9D, 1, {0x01} },
	{0xA0, 1, {0x16} },
	{0xA2, 1, {0x16} },
	{0xA3, 1, {0x02} },
	{0xA4, 1, {0x04} },
	{0xA5, 1, {0x04} },
	{0xC9, 1, {0x00} },
	{0xD9, 1, {0x80} },
	{0xE9, 1, {0x02} },

	{0xFF, 1, {0x25} },
	{REGFLAG_DELAY, 100, {} },
	{0xFB, 1, {0x01} },
	{0x19, 1, {0xE4} },
	{0x21, 1, {0x40} },
	{0x66, 1, {0xD8} },
	{0x68, 1, {0x50} },
	{0x69, 1, {0x10} },
	{0x6B, 1, {0x00} },
	{0x6D, 1, {0x0D} },
	{0x6E, 1, {0x48} },
	{0x72, 1, {0x41} },
	{0x73, 1, {0x4A} },
	{0x74, 1, {0xD0} },
	{0x76, 1, {0x83} },
	{0x77, 1, {0x62} },
	{0x7D, 1, {0x03} },
	{0x7E, 1, {0x15} },
	{0x7F, 1, {0x00} },
	{0x84, 1, {0x4D} },
	{0xCF, 1, {0x80} },
	{0xD6, 1, {0x80} },
	{0xD7, 1, {0x80} },
	{0xEF, 1, {0x20} },
	{0xF0, 1, {0x84} },

	{0xFF, 1, {0x26} },
	{REGFLAG_DELAY, 100, {} },
	{0xFB, 1, {0x01} },
	{0x80, 1, {0x01} },
	{0x81, 1, {0x16} },
	{0x83, 1, {0x03} },
	{0x84, 1, {0x03} },
	{0x85, 1, {0x01} },
	{0x86, 1, {0x03} },
	{0x87, 1, {0x01} },
	{0x8A, 1, {0x1A} },
	{0x8B, 1, {0x11} },
	{0x8C, 1, {0x24} },
	{0x8E, 1, {0x42} },
	{0x8F, 1, {0x11} },
	{0x90, 1, {0x11} },
	{0x91, 1, {0x11} },
	{0x9A, 1, {0x81} },
	{0x9B, 1, {0x03} },
	{0x9C, 1, {0x00} },
	{0x9D, 1, {0x00} },
	{0x9E, 1, {0x00} },

	{0xFF, 1, {0x27} },
	{REGFLAG_DELAY, 100, {} },
	{0xFB, 1, {0x01} },
	{0x01, 1, {0x60} },
	{0x20, 1, {0x81} },
	{0x21, 1, {0xEA} },
	{0x25, 1, {0x82} },
	{0x26, 1, {0x1F} },
	{0x6E, 1, {0x00} },
	{0x6F, 1, {0x00} },
	{0x70, 1, {0x00} },
	{0x71, 1, {0x00} },
	{0x72, 1, {0x00} },
	{0x75, 1, {0x00} },
	{0x76, 1, {0x00} },
	{0x77, 1, {0x00} },
	{0x7D, 1, {0x09} },
	{0x7E, 1, {0x5F} },
	{0x80, 1, {0x23} },
	{0x82, 1, {0x09} },
	{0x83, 1, {0x5F} },
	{0x88, 1, {0x01} },
	{0x89, 1, {0x10} },
	{0xA5, 1, {0x10} },
	{0xA6, 1, {0x23} },
	{0xA7, 1, {0x01} },
	{0xB6, 1, {0x40} },
	{0xE3, 1, {0x02} },
	{0xE4, 1, {0xE0} },
	{0xE9, 1, {0x03} },
	{0xEA, 1, {0x2F} },

	{0xFF, 1, {0x2A} },
	{REGFLAG_DELAY, 100, {} },
	{0xFB, 1, {0x01} },
	{0x00, 1, {0x91} },
	{0x03, 1, {0x20} },
	{0x07, 1, {0x3A} },
	{0x0A, 1, {0x60} },
	{0x0C, 1, {0x06} },
	{0x0D, 1, {0x40} },
	{0x0E, 1, {0x02} },
	{0x0F, 1, {0x01} },
	{0x11, 1, {0x58} },
	{0x15, 1, {0x0E} },
	{0x16, 1, {0x79} },
	{0x19, 1, {0x0D} },
	{0x1A, 1, {0xF2} },
	{0x1B, 1, {0x14} },
	{0x1D, 1, {0x36} },
	{0x1E, 1, {0x55} },
	{0x1F, 1, {0x55} },
	{0x20, 1, {0x55} },
	{0x27, 1, {0x80} },
	{0x28, 1, {0x0A} },
	{0x29, 1, {0x0B} },
	{0x2A, 1, {0x42} },
	{0x2B, 1, {0x04} },
	{0x2F, 1, {0x01} },
	{0x30, 1, {0x47} },
	{0x31, 1, {0xC8} },
	{0x33, 1, {0x2E} },
	{0x34, 1, {0xFF} },
	{0x35, 1, {0x30} },
	{0x36, 1, {0x7D} },
	{0x37, 1, {0xFA} },
	{0x38, 1, {0x33} },
	{0x39, 1, {0x7A} },
	{0x3A, 1, {0x47} },
	{0x44, 1, {0x4C} },
	{0x45, 1, {0x09} },
	{0x46, 1, {0x40} },
	{0x47, 1, {0x02} },
	{0x48, 1, {0x00} },
	{0x4A, 1, {0xF0} },
	{0x4E, 1, {0x0E} },
	{0x4F, 1, {0x79} },
	{0x52, 1, {0x0D} },
	{0x53, 1, {0xF2} },
	{0x54, 1, {0x14} },
	{0x56, 1, {0x36} },
	{0x57, 1, {0x7E} },
	{0x58, 1, {0x7E} },
	{0x59, 1, {0x7E} },
	{0x60, 1, {0x80} },
	{0x61, 1, {0x0A} },
	{0x62, 1, {0x03} },
	{0x63, 1, {0xE9} },
	{0x66, 1, {0x01} },
	{0x67, 1, {0x04} },
	{0x68, 1, {0x6F} },
	{0x6B, 1, {0xCA} },
	{0x6C, 1, {0x20} },
	{0x6D, 1, {0xDE} },
	{0x6E, 1, {0xC5} },
	{0x6F, 1, {0x23} },
	{0x70, 1, {0xDB} },
	{0x71, 1, {0x04} },

	{0xFF, 1, {0x2C} },
	{REGFLAG_DELAY, 100, {} },
	{0xFB, 1, {0x01} },
	{0x00, 1, {0x02} },
	{0x01, 1, {0x02} },
	{0x02, 1, {0x02} },
	{0x03, 1, {0x16} },
	{0x04, 1, {0x16} },
	{0x05, 1, {0x16} },
	{0x0D, 1, {0x1F} },
	{0x0E, 1, {0x1F} },
	{0x16, 1, {0x1B} },
	{0x17, 1, {0x4B} },
	{0x18, 1, {0x4B} },
	{0x19, 1, {0x4B} },
	{0x2A, 1, {0x03} },
	{0x3B, 1, {0x01} },
	{0x4D, 1, {0x16} },
	{0x4E, 1, {0x03} },

	{0xFF, 1, {0x25} },
	{REGFLAG_DELAY, 100, {} },
	{0xFB, 1, {0x01} },
	{0x18, 1, {0x21} },

	{0xFF, 1, {0xF0} },
	{REGFLAG_DELAY, 100, {} },
	{0xFB, 1, {0x01} },
	{0x5A, 1, {0x00} },

	{0xFF, 1, {0x10} },
	{REGFLAG_DELAY, 100, {} },
	{0xFB, 1, {0x01} },
	{0xBA, 1, {0x02} },
#if (LCM_DSI_CMD_MODE)
	{0xBB, 1, {0x10} },
#else
	{0xBB, 1, {0x03} },
#endif
	{0xC0, 1, {0x00} },

	{0x11, 0, {} },

#ifndef LCM_SET_DISPLAY_ON_DELAY
	{REGFLAG_DELAY, 120, {} },
	{0x29, 0, {} },
#endif

};

static struct LCM_setting_table bl_level[] = {
	{0x51, 1, {0xFF} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

/*******************Dynfps start*************************/
#ifdef CONFIG_MTK_HIGH_FRAME_RATE

#define DFPS_MAX_CMD_NUM 10

struct LCM_dfps_cmd_table {
	bool need_send_cmd;
	struct LCM_setting_table prev_f_cmd[DFPS_MAX_CMD_NUM];
};

static struct LCM_dfps_cmd_table
	dfps_cmd_table[DFPS_LEVELNUM][DFPS_LEVELNUM] = {

/**********level 0 to 0,1 cmd*********************/
[DFPS_LEVEL0][DFPS_LEVEL0] = {
	false,
	/*prev_frame cmd*/
	{
	{REGFLAG_END_OF_TABLE, 0x00, {} },
	},
},
/*60->90*/
[DFPS_LEVEL0][DFPS_LEVEL1] = {
	true,
	/*prev_frame cmd*/
	{
	{0xFF, 1, {0x25} },
	{0xFB, 1, {0x01} },
	{0x18, 1, {0x20} },
	{REGFLAG_END_OF_TABLE, 0x00, {} },
	},
},

/**********level 1 to 0,1 cmd*********************/
/*90->60*/
[DFPS_LEVEL1][DFPS_LEVEL0] = {
	true,
	/*prev_frame cmd*/
	{
	{0xFF, 1, {0x25} },
	{0xFB, 1, {0x01} },
	{0x18, 1, {0x21} },
	{REGFLAG_END_OF_TABLE, 0x00, {} },
	},
},

[DFPS_LEVEL1][DFPS_LEVEL1] = {
	false,
	/*prev_frame cmd*/
	{
	{REGFLAG_END_OF_TABLE, 0x00, {} },
	},

},

};
#endif
/*******************Dynfps end*************************/

static void push_table(void *cmdq, struct LCM_setting_table *table,
		       unsigned int count, unsigned char force_update)
{
	unsigned int i;
	unsigned int cmd;

	for (i = 0; i < count; i++) {
		cmd = table[i].cmd;

		switch (cmd) {

		case REGFLAG_DELAY:
			if (table[i].count <= 10)
				MDELAY(table[i].count);
			else
				MDELAY(table[i].count);
			break;

		case REGFLAG_UDELAY:
			UDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE:
			break;

		default:
			dsi_set_cmdq_V22(cmdq, cmd, table[i].count,
					 table[i].para_list, force_update);
		}
	}
}


static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

#ifdef CONFIG_MTK_HIGH_FRAME_RATE
/*DynFPS*/
static void lcm_dfps_int(struct LCM_DSI_PARAMS *dsi)
{
	struct dfps_info *dfps_params = dsi->dfps_params;

	dsi->dfps_enable = 1;
	dsi->dfps_default_fps = 6000;/*real fps * 100, to support float*/
	dsi->dfps_def_vact_tim_fps = 6000;/*real vact timing fps * 100*/

	/*traversing array must less than DFPS_LEVELS*/
	/*DPFS_LEVEL0*/
	dfps_params[0].level = DFPS_LEVEL0;
	dfps_params[0].fps = 6000;/*real fps * 100, to support float*/
	dfps_params[0].vact_timing_fps = 6000;/*real vact timing fps * 100*/
	/*if mipi clock solution*/
	/*dfps_params[0].PLL_CLOCK = xx;*/
	/*dfps_params[0].data_rate = xx; */
	/*if HFP solution*/
	dfps_params[0].horizontal_frontporch = 944;
	dfps_params[0].vertical_frontporch_for_low_power = 862;
	/*if VFP solution*/
	/*dfps_params[0].vertical_frontporch = 1291;*/
	/*dfps_params[0].vertical_frontporch_for_low_power = 2500;*/

	/*if need mipi hopping params add here*/
	/*dfps_params[0].PLL_CLOCK_dyn =xx;
	 *dfps_params[0].horizontal_frontporch_dyn =xx ;
	 * dfps_params[0].vertical_frontporch_dyn = 1291;
	 */

	/*DPFS_LEVEL1*/
	dfps_params[1].level = DFPS_LEVEL1;
	dfps_params[1].fps = 9000;/*real fps * 100, to support float*/
	dfps_params[1].vact_timing_fps = 9000;/*real vact timing fps * 100*/
	/*if mipi clock solution*/
	/*dfps_params[1].PLL_CLOCK = xx;*/
	/*dfps_params[1].data_rate = xx; */
	/*if HFP solution*/
	dfps_params[1].horizontal_frontporch = 256;
	dfps_params[1].vertical_frontporch_for_low_power = 2500;
	/*if VFP solution*/
	/*dfps_params[1].vertical_frontporch = 54;*/
	/*dfps_params[1].vertical_frontporch_for_low_power = 2500;*/

	/*if need mipi hopping params add here*/
	/*dfps_params[1].PLL_CLOCK_dyn =xx;
	 *dfps_params[1].horizontal_frontporch_dyn =xx ;
	 * dfps_params[1].vertical_frontporch_dyn= 54;
	 * dfps_params[1].vertical_frontporch_for_low_power_dyn =xx;
	 */

	dsi->dfps_num = 2;
}
#endif
static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->dsi.IsCphy = 1;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
	params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
	lcm_dsi_mode = CMD_MODE;
#else
	params->dsi.mode = SYNC_PULSE_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
	lcm_dsi_mode = SYNC_PULSE_VDO_MODE;
#endif
	LCM_LOGI("%s lcm_dsi_mode %d\n", __func__, lcm_dsi_mode);
	params->dsi.switch_mode_enable = 0;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_THREE_LANE;

	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 256;
	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	/* video mode timing */
	params->dsi.vertical_sync_active = 10;
	params->dsi.vertical_backporch = 10;
	params->dsi.vertical_frontporch = 54;
	params->dsi.vertical_frontporch_for_low_power = 862;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 20;
	params->dsi.horizontal_backporch = 22;
	params->dsi.horizontal_frontporch = 944;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_disable = 1;

#if (LCM_DSI_CMD_MODE)
	/* this value must be in MTK suggested table */
	params->dsi.PLL_CLOCK = 480;
#else
	/* this value must be in MTK suggested table */
	params->dsi.PLL_CLOCK = 538;
#endif

	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 0;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;

	params->dsi.lane_swap_en = 0;

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	params->round_corner_en = 1;
	params->corner_pattern_height = ROUND_CORNER_H_TOP;
	params->corner_pattern_height_bot = ROUND_CORNER_H_BOT;
	params->corner_pattern_tp_size = sizeof(top_rc_pattern);
	params->corner_pattern_lt_addr = (void *)top_rc_pattern;
#endif

#ifdef CONFIG_MTK_HIGH_FRAME_RATE
	/****DynFPS start****/
	lcm_dfps_int(&(params->dsi));
	/****DynFPS end****/
#endif
}

static void lcm_init_power(void)
{
	display_bias_enable();
}

static void lcm_suspend_power(void)
{
	display_bias_disable();
}

static void lcm_resume_power(void)
{
	SET_RESET_PIN(0);
	display_bias_enable();
}

static void lcm_init(void)
{
	SET_RESET_PIN(0);
	MDELAY(15);
	SET_RESET_PIN(1);
	MDELAY(1);
	SET_RESET_PIN(0);
	MDELAY(10);

	SET_RESET_PIN(1);
	MDELAY(10);
	push_table(NULL, init_setting,
		sizeof(init_setting)/sizeof(struct LCM_setting_table), 1);
}

static void lcm_suspend(void)
{
	push_table(NULL, lcm_suspend_setting,
		sizeof(lcm_suspend_setting)/sizeof(struct LCM_setting_table),
		1);
	MDELAY(10);
	/* SET_RESET_PIN(0); */
}

static void lcm_resume(void)
{
	lcm_init();
}

static void lcm_update(unsigned int x, unsigned int y, unsigned int width,
		       unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);
	unsigned char y0_MSB = ((y0 >> 8) & 0xFF);
	unsigned char y0_LSB = (y0 & 0xFF);
	unsigned char y1_MSB = ((y1 >> 8) & 0xFF);
	unsigned char y1_LSB = (y1 & 0xFF);

	unsigned int data_array[16];

	data_array[0] = 0x00053902;
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = (y1_MSB << 24) | (y0_LSB << 16) | (y0_MSB << 8) | 0x2b;
	data_array[2] = (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);
}

static unsigned int lcm_compare_id(void)
{
	unsigned int id = 0;
	unsigned char buffer[2];
	unsigned int array[16];

	LCM_LOGI("%S: enter\n", __func__);

	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(10);

	array[0] = 0x00013700;	/* read id return two byte,version and id */
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0xDA, buffer, 1);
	id = buffer[0];     /* we only need ID */

	LCM_LOGI("%s,nt3672C_id_JDI=0x%08x\n", __func__, id);

	if (id == LCM_ID_NT36672C_JDI)
		return 1;
	else
		return 0;

}


/* return TRUE: need recovery */
/* return FALSE: No need recovery */
static unsigned int lcm_esd_check(void)
{
#ifndef BUILD_LK
	char buffer[3];
	int array[4];

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x0A, buffer, 1);

	if (buffer[0] != 0x9C) {
		LCM_LOGI("[LCM ERROR] [0x0A]=0x%02x\n", buffer[0]);
		return TRUE;
	}
	LCM_LOGI("[LCM NORMAL] [0x0A]=0x%02x\n", buffer[0]);
	return FALSE;
#else
	return FALSE;
#endif

}

static unsigned int lcm_ata_check(unsigned char *buffer)
{
#ifndef BUILD_LK
	unsigned int ret = 0;
	unsigned int x0 = FRAME_WIDTH / 4;
	unsigned int x1 = FRAME_WIDTH * 3 / 4;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);

	unsigned int data_array[3];
	unsigned char read_buf[4];

	LCM_LOGI("ATA check size = 0x%x,0x%x,0x%x,0x%x\n",
		 x0_MSB, x0_LSB, x1_MSB, x1_LSB);
	data_array[0] = 0x0005390A;	/* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);
	/* read id return two byte,version and id */
	data_array[0] = 0x00043700;
	dsi_set_cmdq(data_array, 1, 1);

	read_reg_v2(0x2A, read_buf, 4);

	if ((read_buf[0] == x0_MSB) && (read_buf[1] == x0_LSB)
	    && (read_buf[2] == x1_MSB) && (read_buf[3] == x1_LSB))
		ret = 1;
	else
		ret = 0;

	x0 = 0;
	x1 = FRAME_WIDTH - 1;

	x0_MSB = ((x0 >> 8) & 0xFF);
	x0_LSB = (x0 & 0xFF);
	x1_MSB = ((x1 >> 8) & 0xFF);
	x1_LSB = (x1 & 0xFF);

	data_array[0] = 0x0005390A;	/* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);
	return ret;
#else
	return 0;
#endif
}

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{

	LCM_LOGI("%s,nt35695 backlight: level = %d\n", __func__, level);

	bl_level[0].para_list[0] = level;

	push_table(handle, bl_level,
		   sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}

static void *lcm_switch_mode(int mode)
{
	return NULL;
}

#if (LCM_DSI_CMD_MODE)
/* partial update restrictions:
 * 1. roi width must be 1080 (full lcm width)
 * 2. vertical start (y) must be multiple of 16
 * 3. vertical height (h) must be multiple of 16
 */
static void lcm_validate_roi(int *x, int *y, int *width, int *height)
{
	unsigned int y1 = *y;
	unsigned int y2 = *height + y1 - 1;
	unsigned int x1, w, h;

	x1 = 0;
	w = FRAME_WIDTH;

	y1 = round_down(y1, 16);
	h = y2 - y1 + 1;

	/* in some cases, roi maybe empty.
	 * In this case we need to use minimu roi
	 */
	if (h < 16)
		h = 16;

	h = round_up(h, 16);

	/* check height again */
	if (y1 >= FRAME_HEIGHT || y1 + h > FRAME_HEIGHT) {
		/* assign full screen roi */
		pr_debug("%s calc error,assign full roi:y=%d,h=%d\n",
			 __func__, *y, *height);
		y1 = 0;
		h = FRAME_HEIGHT;
	}

	/*pr_debug("lcm_validate_roi (%d,%d,%d,%d) to (%d,%d,%d,%d)\n", */
	/*      *x, *y, *width, *height, x1, y1, w, h); */

	*x = x1;
	*width = w;
	*y = y1;
	*height = h;
}
#endif

/*******************Dynfps start*************************/
#ifdef CONFIG_MTK_HIGH_FRAME_RATE
static void dfps_dsi_push_table(
	void *cmdq, struct LCM_setting_table *table,
	unsigned int count, unsigned char force_update)
{
	unsigned int i;
	unsigned int cmd;

	for (i = 0; i < count; i++) {
		cmd = table[i].cmd;
		switch (cmd) {
		case REGFLAG_END_OF_TABLE:
			return;
		default:
			dfps_dsi_send_cmd(
				cmdq, cmd, table[i].count,
				table[i].para_list, force_update);
			break;
		}
	}

}
static bool lcm_dfps_need_inform_lcm(
	unsigned int from_level, unsigned int to_level)
{
	struct LCM_dfps_cmd_table *p_dfps_cmds = NULL;

	if (from_level == to_level) {
		LCM_LOGI("%s,same level\n", __func__);
		return false;
	}
	p_dfps_cmds =
		&(dfps_cmd_table[from_level][to_level]);

	return p_dfps_cmds->need_send_cmd;
}

static void lcm_dfps_inform_lcm(void *cmdq_handle,
unsigned int from_level, unsigned int to_level)
{
	struct LCM_dfps_cmd_table *p_dfps_cmds = NULL;

	if (from_level == to_level) {
		LCM_LOGI("%s,same level\n", __func__);
		goto done;
	}
	p_dfps_cmds =
		&(dfps_cmd_table[from_level][to_level]);

	if (p_dfps_cmds &&
		!(p_dfps_cmds->need_send_cmd)) {
		LCM_LOGI("%s,no cmd[L%d->L%d]\n",
			__func__, from_level, to_level);
		goto done;
	}

	dfps_dsi_push_table(
		cmdq_handle, p_dfps_cmds->prev_f_cmd,
		ARRAY_SIZE(p_dfps_cmds->prev_f_cmd), 1);
done:
	LCM_LOGI("%s,done %d->%d\n",
		__func__, from_level, to_level);

}
#endif
/*******************Dynfps end*************************/


#if (LCM_DSI_CMD_MODE)
struct LCM_DRIVER nt36672c_fhdp_dsi_cmd_auo_cphy_90hz_jdi_hfp_lcm_drv = {
	.name = "nt36672c_fhdp_dsi_cmd_auo_cphy_90hz_jdi_hfp_lcm_drv",
#else
struct LCM_DRIVER nt36672c_fhdp_dsi_vdo_auo_cphy_90hz_jdi_hfp_lcm_drv = {
	.name = "nt36672c_fhdp_dsi_vdo_auo_cphy_90hz_jdi_hfp_lcm_drv",
#endif
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.esd_check = lcm_esd_check,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = lcm_ata_check,
	.update = lcm_update,
	.switch_mode = lcm_switch_mode,
#if (LCM_DSI_CMD_MODE)
	.validate_roi = lcm_validate_roi,
#endif
#ifdef CONFIG_MTK_HIGH_FRAME_RATE
	/*DynFPS*/
	.dfps_send_lcm_cmd = lcm_dfps_inform_lcm,
	.dfps_need_send_cmd = lcm_dfps_need_inform_lcm,
#endif

};
