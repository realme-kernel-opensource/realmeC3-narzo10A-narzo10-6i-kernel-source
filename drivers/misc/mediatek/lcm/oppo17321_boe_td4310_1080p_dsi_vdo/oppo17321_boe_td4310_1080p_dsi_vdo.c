/******************************************************************
** Copyright (C), 2004-2017, OPPO Mobile Comm Corp., Ltd.
** VENDOR_EDIT
** File: - oppo_boe_td4310_fhd1080_dsi_vdo.c
** Description: Source file for lcd drvier.
** lcd driver including parameter and power control.
** Version: 1.0
** Date : 2017/05/06
** Author: Rongchun.Zhang@EXP.MultiMedia.Display.LCD.Machine
**
** ------------------------------- Revision History:---------------
** zhangrongchun 2017/05/06 1.0 build this module
*******************************************************************/

#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#include <mt-plat/mtk_boot_common.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#include <platform/boot_mode.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
/*#include <mach/mt_pm_ldo.h>*/
#ifdef CONFIG_MTK_LEGACY
#include <mach/mt_gpio.h>
#endif
#endif
#ifdef CONFIG_MTK_LEGACY
#include <cust_gpio_usage.h>
#endif
#ifndef CONFIG_FPGA_EARLY_PORTING
#if defined(CONFIG_MTK_LEGACY)
#include <cust_i2c.h>
#endif
#endif
#include <linux/slab.h>
#include <linux/string.h>
#include <soc/oppo/device_info.h>

#include "ddp_hal.h"


#define DEBUG_INTERFACE


#define SYSFS_FOLDER "dsi_access"
#define BUFFER_LENGTH 128


enum read_write {
	CMD_READ = 0,
	CMD_WRITE = 1,
};

/*
struct dsi_debug {
	bool long_rpkt;
	unsigned char length;
	unsigned char rlength;
	unsigned char buffer[BUFFER_LENGTH];
	unsigned char read_buffer[BUFFER_LENGTH];
	unsigned char command_len;
	unsigned char *command_buf;
	unsigned char cmds[64];
};

static struct dsi_debug *debug = NULL;
static struct dsi_debug *debug_read = NULL;
*/

#ifdef BUILD_LK
#define LCD_DEBUG(fmt)  dprintf(CRITICAL,fmt)
#else
#define LCD_DEBUG(fmt)  printk(fmt)
#endif

static struct LCM_UTIL_FUNCS *lcm_util = NULL;

#define SET_RESET_PIN(v) (lcm_util->set_reset_pin((v)))
#define MDELAY(n) (lcm_util->mdelay(n))
#define UDELAY(n) (lcm_util->udelay(n))

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util->dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util->dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)						lcm_util->dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)			lcm_util->dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)						lcm_util->dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)			lcm_util->dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)
#define dsi_set_cmdq_V22(cmdq,cmd, count, ppara, force_update)	lcm_util->dsi_set_cmdq_V22(cmdq,cmd, count, ppara, force_update)

#define LCM_DSI_CMD_MODE 0
#define FRAME_WIDTH (1080)
#define FRAME_HEIGHT (2160)
#define PHYSICAL_WIDTH (68)
#define PHYSICAL_HEIGHT (136)

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

extern int lcm_power_write_byte(unsigned char addr,  unsigned char value);
extern int lm3697_write_byte(unsigned char addr,  unsigned char value);
extern int lm3697_setbacklight(unsigned int level);

#define REGFLAG_DELAY      0xFC
#define REGFLAG_UDELAY     0xFB

#define REGFLAG_END_OF_TABLE   0xFD
#define REGFLAG_RESET_LOW  0xFE
#define REGFLAG_RESET_HIGH  0xFF

struct LCM_setting_table {
	unsigned char cmd;
	unsigned char count;
	unsigned char para_list[64];
};

/*
 * YongPeng.Yi@PSW.MultiMedia.Display.LCD.Stability, 2017/06/23,
 * add for Lcd cabc default enable
 */
static struct LCM_setting_table lcm_initialization_video_setting[] = {
	{0x11,0,{}},
	{REGFLAG_DELAY, 100, {}},
	{0xB0,1,{0x00}},
	{0xB0,1,{0x04}},
	{0xD6,1,{0x01}},

	/* BOT Timing */
	{0xC1,48,{0x04,0x40,0x00,0xFF,0x5F,0x6B,0x89,0xB0,0x6A,0x85,0x0F,0xA3,0xFC,0xFF,0xFF,0xFF,0x65,0x84,0x1F,0x2C,0x65,0xCB,0xB0,0xF6,0xFE,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x02,0x22,0x02,0x06,0x0A,0x00,0x00,0x01,0x00,0x05,0x00,0x00,0x00}},
	{0xCB,27,{0xFF,0xFF,0xFF,0xFF,0x0F,0x00,0x00,0x00,0x00,0x00,0x11,0xF0,0x00,0x0D,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{0xCE,25,{0x55,0x40,0x49,0x53,0x59,0x5E,0x63,0x68,0x6E,0x74,0x7E,0x8A,0x98,0xA8,0xBB,0xD0,0xFF,0x01,0x70,0x01,0x04,0x42,0x00,0x69,0x5A}},
	/* cabc setting */
	{0xB9,7,{0x8F,0x59,0x13,0x20,0x10,0x40,0x40}},

	{0x51,1,{0xFF}},
	{0x53,1,{0x24}},
	{0x55,1,{0x02}},

	/* TE ON */
	{0x35,1,{0x00}},
	{0x29,0,{}},
	{REGFLAG_DELAY, 20, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_sleep_in_setting[] = {
	{0x28,0,{}},
	{REGFLAG_DELAY, 20, {}},
	{0xB0,1,{0x00}},

	{0xC1,48,{0x04,0x40,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xEA,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x7F,0xF5,0xFF,0xFF,0xFF,0xFF,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x02,0x22,0x02,0x06,0x0A,0x00,0x00,0x01,0x00,0x01,0x00,0x00,0x00}},
	{0xCB,27,{0xFF,0xFF,0xFF,0xFF,0x0F,0xFC,0x76,0xE0,0xF6,0x03,0x11,0xF0,0x00,0x0D,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},

	{0x10,0,{}},
	{REGFLAG_DELAY, 120, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_cabc_enter_setting[] = {
	{0x51,1,{0xFF}},
	{0x53,1,{0x2C}},
	{0x55,1,{0x02}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_cabc_exit_setting[] = {
	{0x51,1,{0xFF}},
	{0x53,1,{0x2C}},
	{0x55,1,{0x00}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		unsigned cmd;
		cmd = table[i].cmd;

		switch (cmd) {
		case REGFLAG_DELAY :
			if (table[i].count <= 10) {
				MDELAY(table[i].count);
			} else {
				MDELAY(table[i].count);
			}
			break;
		case REGFLAG_UDELAY :
			UDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE :
			break;

		default:
			dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
		}
	}
}
#if (LCM_DSI_CMD_MODE)
static void push_table22(void *handle,struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		unsigned cmd;
		cmd = table[i].cmd;

		switch (cmd) {
		case REGFLAG_DELAY :
			if (table[i].count <= 10) {
				MDELAY(table[i].count);
			} else {
				MDELAY(table[i].count);
			}
		break;

		case REGFLAG_UDELAY :
			UDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE :
			break;

		default:
			dsi_set_cmdq_V22(handle, cmd, table[i].count, table[i].para_list, force_update);
		}
	}
}
#endif
static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	if (lcm_util == NULL) {
		lcm_util = kmalloc(sizeof(struct LCM_UTIL_FUNCS),GFP_KERNEL);
	}
	memcpy(lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

static void lcm_get_params(struct LCM_PARAMS *params)
{
	int boot_mode = 0;

	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type   = LCM_TYPE_DSI;

	params->width  = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

	params->physical_width = PHYSICAL_WIDTH;
	params->physical_height = PHYSICAL_HEIGHT;

	params->dsi.mode   = BURST_VDO_MODE;//SYNC_EVENT_VDO_MODE;

	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_FOUR_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 256;

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active                = 2;
	params->dsi.vertical_backporch                    = 10;
	params->dsi.vertical_frontporch                    = 25;
	/*params->dsi.vertical_frontporch_for_low_power = 220;*/
	params->dsi.vertical_active_line                = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active                = 2;
	params->dsi.horizontal_backporch                = 30;
	params->dsi.horizontal_frontporch                = 110;
	params->dsi.horizontal_active_pixel         = FRAME_WIDTH;

	params->dsi.PLL_CLOCK = 514;
	//params->dsi.data_rate = 1029;

	params->dsi.ssc_disable = 1;
	params->dsi.CLK_TRAIL = 9;

	/* clk continuous video mode */
	params->dsi.cont_clock = 0;

	params->dsi.clk_lp_per_line_enable = 0;
	if (get_boot_mode() == META_BOOT) {
		boot_mode++;
		LCD_DEBUG("META_BOOT\n");
	}
	if (get_boot_mode() == ADVMETA_BOOT) {
		boot_mode++;
		LCD_DEBUG("ADVMETA_BOOT\n");
	}
	if (get_boot_mode() == ATE_FACTORY_BOOT) {
		boot_mode++;
		LCD_DEBUG("ATE_FACTORY_BOOT\n");
	}
	if (get_boot_mode() == FACTORY_BOOT) {
		boot_mode++;
		LCD_DEBUG("FACTORY_BOOT\n");
	}

	/*
	 * YongPeng.Yi@PSW.MultiMedia.Display.LCD.Stability, 2017/06/28,
	 * add for ESD check
	 */
	if (boot_mode == 0) {
		LCD_DEBUG("neither META_BOOT or FACTORY_BOOT\n");
		params->dsi.esd_check_enable = 1;
		params->dsi.customization_esd_check_enable = 0;
		//params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
		//params->dsi.lcm_esd_check_table[0].count = 1;
		//params->dsi.lcm_esd_check_table[0].para_list[0] = 0x1C;
	}
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	params->round_corner_en = 0;
	params->corner_pattern_width = 1080;
	params->corner_pattern_height = 32;
#endif

#ifndef BUILD_LK
	register_device_proc("lcd", "td4310", "tianma vdo mode");
#endif
}

static void lcm_init_power(void)
{
	LCD_DEBUG("[soso] lcm_init_power\n");

	lcm_power_write_byte(0x00, 0x0F);
	lcm_power_write_byte(0x03, 0x0F);
	lcm_power_write_byte(0x01, 0x0F);
	lcm_power_write_byte(0xFF, 0xF0);

	lcd_1p8_en_setting(1);
	MDELAY(11);

	lcd_rst_setting(1);
	MDELAY(5);
	lcd_rst_setting(0);
	MDELAY(5);
	lcd_rst_setting(1);
	MDELAY(10);

	lcd_enp_bias_setting(1);

	MDELAY(11);

	lcd_enn_bias_setting(1);
	MDELAY(15);
}

static void lcm_suspend_power(void)
{
	LCD_DEBUG("[soso] lcm_suspend_power\n");
	lcd_rst_setting(0);
	MDELAY(6);

	lcd_enn_bias_setting(0);
	MDELAY(11);

	lcd_enp_bias_setting(0);
	MDELAY(11);
}

static void lcm_resume_power(void)
{
	LCD_DEBUG("[soso] lcm_resume_power \n");
	lcm_init_power();
}


static void lcm_init(void)
{
	LCD_DEBUG("[soso]lcm_initialization_setting\n");

	push_table(lcm_initialization_video_setting, sizeof(lcm_initialization_video_setting) / sizeof(struct LCM_setting_table), 1);

	lcd_bl_en_setting(1);
	MDELAY(5);

	if (is_lm3697) {
		lm3697_write_byte(0x10,0x04);
		if (LM3697_EXPONENTIAL) {
			lm3697_write_byte(0x16,0x00);
		} else {
			lm3697_write_byte(0x16,0x01);
		}
		lm3697_write_byte(0x17, 0x13);
		lm3697_write_byte(0x18, 0x13);
		lm3697_write_byte(0x19, 0x03);
		lm3697_write_byte(0x1A, 0x04);
		lm3697_write_byte(0x1C, 0x0D);
	} else {
		lm3697_write_byte(0x10, 0x0c);
		if (MP3188_EXPONENTIAL) {
			lm3697_write_byte(0x11, 0xc9);
		} else {
			lm3697_write_byte(0x11, 0x49);
		}
		lm3697_write_byte(0x12, 0x72);
		lm3697_write_byte(0x13, 0x79);
		lm3697_write_byte(0x15, 0xe1);
		lm3697_write_byte(0x16, 0xa8);
		lm3697_write_byte(0x17, 0x50);
		lm3697_write_byte(0x1e, 0x64);
	}
}


static void lcm_suspend(void)
{
	lcd_bl_en_setting(0);
	LCD_DEBUG(" lcm_suspend BL has disabled\n");

	push_table(lcm_sleep_in_setting, sizeof(lcm_sleep_in_setting) / sizeof(struct LCM_setting_table), 1);

	LCD_DEBUG("[soso] lcm_suspend \n");
}

static void lcm_resume(void)
{
	LCD_DEBUG("[soso] lcm_resume\n");
	lcm_init();
}

static void lcm_update(unsigned int x, unsigned int y, unsigned int width, unsigned int height)
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
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB<<8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = (y1_MSB << 24) | (y0_LSB << 16)|(y0_MSB << 8) | 0x2b;
	data_array[2] = (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);
}

static void lcm_setbacklight(unsigned int level)
{
#ifdef BUILD_LK
	dprintf(CRITICAL, "%s [soso] level is %d\n", __func__, level);
#else
	printk("%s [soso] level is %d\n", __func__, level);
#endif
	lm3697_setbacklight(level);
}
static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{
#ifdef BUILD_LK
	dprintf(CRITICAL, "%s [soso] level is %d\n", __func__, level);
#else
	/* printk("%s [soso] level is %d\n", __func__, level); */
#endif
	lm3697_setbacklight(level);
}

static void lcm_set_cabc_mode_cmdq(void *handle, unsigned int level)
{
	printk("%s [lcd] cabc_mode is %d \n", __func__, level);
#if (LCM_DSI_CMD_MODE)
	if (level) {
		push_table22(handle,lcm_cabc_enter_setting, sizeof(lcm_cabc_enter_setting) / sizeof(struct LCM_setting_table), 1);
	} else {
		push_table22(handle,lcm_cabc_exit_setting, sizeof(lcm_cabc_exit_setting) / sizeof(struct LCM_setting_table), 1);
	}
#else
	if (level) {
		push_table(lcm_cabc_enter_setting, sizeof(lcm_cabc_enter_setting) / sizeof(struct LCM_setting_table), 1);
	} else {
		push_table(lcm_cabc_exit_setting, sizeof(lcm_cabc_exit_setting) / sizeof(struct LCM_setting_table), 1);
	}
#endif
}

/*
int parse_input(void *handle,const char *buf)
{
	int retval = 0;
	int index = 0;
	int cmdindex = 0;
	int ret = 0;
	char *input = (char *)buf;
	char *token = NULL;
	unsigned long value = 0;

	input[strlen(input)] = '\0';

	while (input != NULL && index < BUFFER_LENGTH) {
		token = strsep(&input, " ");
		retval = kstrtoul(token, 16, &value);
		if (retval < 0) {
			pr_err("%s: Failed to convert from string (%s) to hex number\n", __func__, token);
			continue;
		}
		debug->buffer[index] = (unsigned char)value;
		index++;
	}

	if (index > 1) {
		debug->length = index - 1;
	}

	if (debug->length <= 0) {
		return 0;
	}

	while (cmdindex < debug->length) {
		debug->cmds[cmdindex] = debug->buffer[cmdindex+1];
		cmdindex++;
	}
	printk("%s [soso] debug->buffer[0] is 0x%x debug->length = %d \n", __func__, debug->buffer[0],debug->length);

	while (ret < debug->length) {
		printk("[soso] debug->cmds is 0x%x\n",  debug->cmds[ret]);
		ret++;
	}
	dsi_set_cmdq_V22(handle, debug->buffer[0], debug->length, debug->cmds, 1);

	return 1;
}

extern unsigned char read_buffer[128];
extern int reg_rlengh;

int parse_reg_output(void *handle,const char *buf)
{
	int retval = 0;
	int index = 0;
	int ret = 0;
	char *input = (char *)buf;
	char *token = NULL;
	unsigned long value = 0;

	input[strlen(input)] = '\0';

	while (input != NULL && index < BUFFER_LENGTH) {
		token = strsep(&input, " ");
		retval = kstrtoul(token, 16, &value);
		if (retval < 0) {
			pr_err("%s: Failed to convert from string (%s) to hex number\n", __func__, token);
			continue;
		}
		debug_read->buffer[index] = (unsigned char)value;
		index++;
	}

	if (index > 1) {
		debug_read->length = debug_read->buffer[1];
	}

	if (debug_read->length <= 0) {
		return 0;
	}

	reg_rlengh = debug_read->length;

	printk("%s [soso] debug->buffer[0] is 0x%x debug->length = %d \n", __func__, debug_read->buffer[0],debug_read->length);

	retval = read_reg_v2(debug_read->buffer[0],debug_read->read_buffer,debug_read->length);

	if (retval<0) {
		printk("%s [soso] error can not read the reg 0x%x \n", __func__,debug_read->buffer[0]);
		return -1;
	}

	while (ret < debug_read->length) {
		printk("[soso] reg cmd 0x%x read_buffer is 0x%x \n", debug_read->buffer[0], debug_read->read_buffer[ret]);
		read_buffer[ret] = debug_read->read_buffer[ret];
		ret++;
	}
	return 1;
}
*/

struct LCM_DRIVER oppo17321_boe_td4310_1080p_dsi_vdo_lcm_drv=
{
	.name = "oppo17321_boe_td4310_1080p_dsi_vdo",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.set_backlight = lcm_setbacklight,
	.set_cabc_mode_cmdq = lcm_set_cabc_mode_cmdq,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.update = lcm_update,
};
