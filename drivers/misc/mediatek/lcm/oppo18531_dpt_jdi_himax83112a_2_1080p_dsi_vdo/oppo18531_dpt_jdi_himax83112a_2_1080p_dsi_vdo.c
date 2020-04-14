/******************************************************************
** Copyright (C), 2004-2018, OPPO Mobile Comm Corp., Ltd.
** VENDOR_EDIT
** File: - oppo18531_dpt_jdi_himax83112a_2_1080p_dsi_vdo.c
** Description: Source file for lcd drvier.
** lcd driver including parameter and power control.
** Version: 1.0
** Date : 2018/06/8
** Author: LiPing-M@PSW.MultiMedia.Display.LCD.Machine
**
** ------------------------------- Revision History:---------------
** liping 2018/06/8 1.0 build this module
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
#define FRAME_HEIGHT (2340)
#define PHYSICAL_WIDTH (69)
#define PHYSICAL_HEIGHT (150)
#define PHYSICAL_WIDTH_UM (69498)
#define PHYSICAL_HEIGHT_UM (150579)

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

extern int lcm_power_write_byte(unsigned char addr,  unsigned char value);
extern int lm3697_write_byte(unsigned char addr,  unsigned char value);
extern int lm3697_setbacklight(unsigned int level);

extern int tp_control_reset_gpio(bool enable);
//extern int is_dpt_hx83112a_lcd;

extern void lcd_queue_load_tp_fw(void);

#define REGFLAG_DELAY      0xA0
#define REGFLAG_UDELAY     0xA1

#define REGFLAG_END_OF_TABLE   0xA2
#define REGFLAG_RESET_LOW  0xA3
#define REGFLAG_RESET_HIGH  0xA4

struct LCM_setting_table {
	unsigned char cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_initialization_video_setting[] = {
    {0xB9,3, {0x83,0x11,0x2A}},
    {0x11,0, {}},
    {REGFLAG_DELAY, 130, {}},
    {0xB9,3, {0x83,0x11,0x2A}},
    {0xBD,1, {0x02}},
    {0xE7,29,{0x00,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
              0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
              0x00,0x00,0x0E,0x00,0x00,0x00,0x00,0x22,0x00}},
    {0xBD,1, {0x00}},
    {0xC4,6, {0x00,0xA2,0xA5,0x97,0x00,0x02}},
    {0xBD,3, {0x00,0x00,0x00}},

    {0xB9,3,{0x83, 0x11, 0x2A}},
    {0xE9,1,{0xC3}},
    {0xCB,2,{0xC1, 0xC4}},
    {0xE9,1,{0xC3}},
    {0xCB,2,{0xD1, 0xC4}},
    {0xE9,1,{0x3F}},
    {0xE9,1,{0xC9}},
    {0xB2,1,{0xE5}},
    {0xE9,1,{0x3F}},
    {0xE7,6,{0x0E,0x0E,0x1E,0x63,0x1E,0x63}},
    {0xE9,1,{0xCC}},
    {0xB4,1,{0xC0}},
    {0xE9,1,{0x3F}},

    /* TE ON */
    {0x35,1, {0x00}},
    {0xC9,4, {0x04,0x08,0xA0,0x00}},
    {0x55,1, {0x01}},
    {REGFLAG_DELAY, 5, {}},
    {0xE4,22,{0x2D,0x01,0x2C,0x00,0x08,0x00,0x10,0x08,0x04,0x04,\
              0x3F,0x56,0x6D,0x84,0x9B,0xB2,0xC9,0xE0,0xF2,0xFF,\
              0xFF,0xEF}},
    {0x51,2, {0x0F,0xFF}},
    {REGFLAG_DELAY, 5, {}},
    {0x53,1, {0x24}},
    {REGFLAG_DELAY, 5, {}},
    {0x29,0,{}},
    {REGFLAG_DELAY, 60, {}},
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_sleep_in_setting[] = {
	{0x28,0,{}},
	{REGFLAG_DELAY, 20, {}},
	{0x10,0,{}},
	{REGFLAG_DELAY, 120, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_cabc_enter_setting[] = {
    {0x55,1,{0x01}},
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_cabc_exit_setting[] = {
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

	//params->physical_width = PHYSICAL_WIDTH;
	//params->physical_height = PHYSICAL_HEIGHT;
	params->physical_width_um = PHYSICAL_WIDTH_UM;
	params->physical_height_um = PHYSICAL_HEIGHT_UM;

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

	params->dsi.vertical_sync_active        = 2;
	params->dsi.vertical_backporch          = 15;
	params->dsi.vertical_frontporch         = 32;
	params->dsi.vertical_frontporch_for_low_power = 860;
	params->dsi.vertical_active_line        = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active      = 2;
	params->dsi.horizontal_backporch        = 16;
	params->dsi.horizontal_frontporch       = 16;
	params->dsi.horizontal_active_pixel     = FRAME_WIDTH;

	params->dsi.PLL_CLOCK = 514;
	params->dsi.data_rate = 1029;

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

	if (boot_mode == 0) {
		LCD_DEBUG("neither META_BOOT or FACTORY_BOOT\n");
		params->dsi.esd_check_enable = 1;
		params->dsi.customization_esd_check_enable = 0;
		//params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
		//params->dsi.lcm_esd_check_table[0].count = 1;
		//params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9D;
	}
//#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
//	params->round_corner_en = 1;
//	params->full_content = 1;
//	params->corner_pattern_width = 1080;
//	params->corner_pattern_height = 91;
//	params->corner_pattern_height_bot = 72;
//#endif
	/*
	if (debug == NULL) {
		debug = kmalloc(sizeof(dsi_debug),GFP_KERNEL);
	}
	if (debug_read == NULL) {
		debug_read = kmalloc(sizeof(dsi_debug),GFP_KERNEL);
	}
	*/
#ifndef BUILD_LK
/*
	if(is_dpt_hx83112a_lcd == 2){
		register_device_proc("lcd", "hx83112a_dpt", "jdi vdo mode");
	} else if(is_dpt_hx83112a_lcd == 8){
		register_device_proc("lcd", "hx83112a_2_dpt", "jdi vdo mode");
	} else if(is_dpt_hx83112a_lcd == 7){
		register_device_proc("lcd", "hx83112a_2", "dsjm vdo mode");
	} else {
		register_device_proc("lcd", "hx83112a", "dsjm vdo mode");
	}
*/
	register_device_proc("lcd", "hx83112a_m03", "dpt_himax vdo mode");
#endif
}

static void poweron_before_ulps(void)
{
	LCD_DEBUG("[himax] poweron_before_ulps 1.8v\n");
	lcd_1p8_en_setting(1);
	MDELAY(6);
	tp_control_reset_gpio(true);
	/* ZhongWenjie@PSW.BSP.TP.FUNCTION, 2018/6/7, Add for no-flash TP */
	spi_csn_en_setting(1);
	lcm_power_write_byte(0x00, 0x0F);
	lcm_power_write_byte(0x03, 0x0F);
	lcm_power_write_byte(0x01, 0x0F);
	lcm_power_write_byte(0xFF, 0xF0);
	MDELAY(5);
	lcd_enp_bias_setting(1);

	MDELAY(10);

	lcd_enn_bias_setting(1);
	MDELAY(10);
}

static void lcm_init_power(void)
{
	LCD_DEBUG("[dsjm] lcm_init_power\n");

	lcd_rst_setting(1);
	MDELAY(5);
	lcd_rst_setting(0);
	MDELAY(5);
	lcd_rst_setting(1);
	MDELAY(55);

}

static void lcm_suspend_power(void)
{
	LCD_DEBUG("[dsjm] lcm_suspend_power\n");
	lcd_rst_setting(0);
	MDELAY(6);

	/* ZhongWenjie@PSW.BSP.TP.FUNCTION, 2018/6/7, Add for no-flash TP */
	spi_csn_en_setting(0);
	MDELAY(5);

	tp_control_reset_gpio(false);
	MDELAY(5);

	lcd_enn_bias_setting(0);
	MDELAY(10);

	lcd_enp_bias_setting(0);
	MDELAY(10);
}

static void lcm_resume_power(void)
{
	LCD_DEBUG("[dsjm] lcm_resume_power \n");
	lcm_init_power();
        /* ZhongWenjie@PSW.BSP.TP.FUNCTION, 2018/6/7, Add for no-flash TP */
        lcd_queue_load_tp_fw();
}

static void poweroff_after_ulps(void)
{
	LCD_DEBUG("[lcd] poweroff_after_ulps dsjm nt36672 1.8\n");
	lcd_1p8_en_setting(0);
	MDELAY(100);
}

static void lcm_init(void)
{
	LCD_DEBUG("[dsjm]lcm_initialization_setting\n");

	push_table(lcm_initialization_video_setting, sizeof(lcm_initialization_video_setting) / sizeof(struct LCM_setting_table), 1);

	lcd_bl_en_setting(1);
	MDELAY(5);

	/*
	 * Guoqiang.jiang@MM.Display.LCD.Machine, 2018/03/13,
	 * add for backlight IC KTD3136
	 */
	if (is_lm3697 == 2) {   /*KTD3136*/
		lm3697_write_byte(0x02, 0x98);
		if (KTD3136_EXPONENTIAL) {
			lm3697_write_byte(0x03, 0x28);   /*32V500KHz exp*/
		} else {
			lm3697_write_byte(0x03, 0x2a);   /*32V500KHz Liner*/
		}
		lm3697_write_byte(0x06, 0x27);
		lm3697_write_byte(0x07, 0x00);
		lm3697_write_byte(0x08, 0x12);
	} else if (is_lm3697 == 1) {
		lm3697_write_byte(0x10,0x00);
		if (LM3697_EXPONENTIAL) {
			lm3697_write_byte(0x16,0x00);
		} else {
			lm3697_write_byte(0x16,0x01);
		}
		lm3697_write_byte(0x16, 0x00);
		lm3697_write_byte(0x17, 0x13);
		lm3697_write_byte(0x18, 0x13);
		lm3697_write_byte(0x19, 0x03);
		lm3697_write_byte(0x1A, 0x04); /*32v/500kHz*/
		lm3697_write_byte(0x24, 0x03);
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

	LCD_DEBUG("[lcd] lcm_suspend \n");
}

static void lcm_resume(void)
{
	LCD_DEBUG("[lcd] lcm_resume\n");
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
	dprintf(CRITICAL, "%s [lcd] level is %d\n", __func__, level);
#else
	printk("%s [lcd] level is %d\n", __func__, level);
#endif
	lm3697_setbacklight(level);
}
static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{
#ifdef BUILD_LK
	dprintf(CRITICAL, "%s [lcd] level is %d\n", __func__, level);
#else
	/* printk("%s [soso] level is %d\n", __func__, level); */
#endif
	lm3697_setbacklight(level);
}

static void lcm_set_cabc_mode_cmdq(void *handle, unsigned int level)
{
	printk("%s [lcd] cabc_mode is %d \n", __func__, level);
	if (level) {
		push_table22(handle,lcm_cabc_enter_setting, sizeof(lcm_cabc_enter_setting) / sizeof(struct LCM_setting_table), 1);
	} else {
		push_table22(handle,lcm_cabc_exit_setting, sizeof(lcm_cabc_exit_setting) / sizeof(struct LCM_setting_table), 1);
	}
}

struct LCM_DRIVER oppo18531_dpt_jdi_himax83112a_2_1080p_dsi_vdo_lcm_drv=
{
	.name = "oppo18531_dpt_jdi_himax83112a_2_1080p_dsi_vdo",
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
	.poweron_before_ulps = poweron_before_ulps,
	.poweroff_after_ulps = poweroff_after_ulps,
};
