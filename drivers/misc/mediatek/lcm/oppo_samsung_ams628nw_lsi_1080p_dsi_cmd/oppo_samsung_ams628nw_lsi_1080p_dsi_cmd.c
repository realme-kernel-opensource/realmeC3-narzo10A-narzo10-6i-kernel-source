/******************************************************************
** Copyright (C), 2004-2017, OPPO Mobile Comm Corp., Ltd.
** VENDOR_EDIT
** File: - oppo_samsung_ams628nw_1080p_dsi_cmd.c
** Description: Source file for lcd drvier.
** lcd driver including parameter and power control.
** Version: 1.0
** Date : 2017/12/27
**
** ------------------------------- Revision History:---------------
** liping 2017/12/27 1.0 build this module
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

#ifndef VENDOR_EDIT
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

static struct dsi_debug debug;
static struct dsi_debug debug_read;
#endif /*VENDOR_EDIT*/

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

#define LCM_DSI_CMD_MODE 1
#define FRAME_WIDTH (1080)
#define FRAME_HEIGHT (2280)


#define PHYSICAL_WIDTH (68)
#define PHYSICAL_HEIGHT (144)
#define PHYSICAL_WIDTH_UM (68000)
#define PHYSICAL_HEIGHT_UM (144000)


#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/* LiPing-m@PSW.MM.Display.LCD.Machine 2017/12/29, Add for lcm ic esd recovery backlight */
extern unsigned int esd_recovery_backlight_level;


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

static struct LCM_setting_table lcm_initialization_cmd_setting[] = {
	{0x11, 0, {}},
	{REGFLAG_DELAY, 6, {}},
	{0xB0, 1, {0x1C}},
	{0xB5, 1, {0x24}},
	{REGFLAG_DELAY, 16, {}},
	{0x35, 1, {0x00}},
	{0xF0, 2, {0x5A,0x5A}},
	{0x53, 1, {0x20}},
	{0x51, 2, {0x00,0x00}},
	{0xF0, 2, {0xA5,0xA5}},
	{0xF0, 2, {0x5A,0x5A}}, /* Level2 key access enable */
	{0xBC, 1, {0x01}}, /* SEED enable */
	{0xB0, 1, {0x01}}, /* CRC mode enable */
	{0xBC, 1, {0x12}},
	{0xB0, 1, {0x2C}},
	{0xBC, 21, {0xB0,0x03,0x05,0x04,0xFF,0x00,0x00,0x00,0xFF,0x00,0xFF,0xFF,0xF2,0x00,0xF2,0xE4,0xDB,0x14,0xFC,0xFD,0xFF}},
	{0xB0, 1, {0x42}}, /* LBR/SKIN Enable */
	{0xBC, 1, {0x03}}, /* 0x01:LBR off, skin off; 0x03:LBR off, skin on; 0x05: LBR on, skin off; 0x07:LBR on, skin on */
	{0xB0, 1, {0x49}},
	{0xBC, 1, {0x00}}, /* LBR level */
	{0xB0, 1, {0x4B}},
	{0xBC, 1, {0x49}}, /* SKIN level */
	{0xF0, 2, {0xA5,0xA5}},
	{REGFLAG_DELAY, 101, {}},
	{0x29, 0, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_sleep_in_setting[] = {
	{0x28, 0, {}},
	{REGFLAG_DELAY, 11, {}},
	{0x10, 0, {}},
	{REGFLAG_DELAY, 151, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_aod_doze_resume_setting[] = {
	{0x11, 0, {}},
	{REGFLAG_DELAY, 6, {}},
	{0xB0, 1, {0x1C}},
	{0xB5, 1, {0x28}},
	{REGFLAG_DELAY, 16, {}},
	{0x35, 1, {0x00}},
	{0xF0, 2, {0x5A,0x5A}},
	{0x53, 1, {0x20}},
	{0x51, 2, {0x00,0x00}},
	{0xF0, 2, {0xA5,0xA5}},
	{0xF0, 2, {0x5A,0x5A}}, /* Level2 key access enable */
	{0xBC, 1, {0x01}}, /* SEED enable */
	{0xB0, 1, {0x01}}, /* CRC mode enable */
	{0xBC, 1, {0x12}},
	{0xB0, 1, {0x2C}},
	{0xBC, 21, {0xB0,0x03,0x05,0x04,0xFF,0x00,0x00,0x00,0xFF,0x00,0xFF,0xFF,0xF2,0x00,0xF2,0xE4,0xDB,0x14,0xFC,0xFD,0xFF}},
	{0xB0, 1, {0x42}}, /* LBR/SKIN Enable */
	{0xBC, 1, {0x03}}, /* 0x01:LBR off, skin off; 0x03:LBR off, skin on; 0x05: LBR on, skin off; 0x07:LBR on, skin on */
	{0xB0, 1, {0x49}},
	{0xBC, 1, {0x00}}, /* LBR level */
	{0xB0, 1, {0x4B}},
	{0xBC, 1, {0x49}}, /* SKIN level */
	{0xF0, 2, {0xA5,0xA5}},
	{REGFLAG_DELAY, 101, {}},
	{0x29, 0, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_aod_in_setting[] = {
	{0x51, 2, {0x00,0x02}},
	{0x28, 0, {}},
	{REGFLAG_DELAY, 17, {}},
	{0xF0, 2, {0x5A,0x5A}},
	{0x53, 1, {0x02}},
	{0xBB, 1, {0x05}},
	{0xF0, 2, {0xA5,0xA5}},
	{0x29, 0, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_aod_out_setting[] = {
	{0x28, 0, {}},
	{0xF0, 2, {0x5A,0x5A}},
	{0xB0, 1, {0x1C}},
	{0xB5, 1, {0x24}},
	{REGFLAG_DELAY, 17, {}},
	{0x53, 1, {0x28}},
	{0xF0, 2, {0xA5,0xA5}},
	{0x29, 0, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_backlight_level_setting[] = {
	{0x51, 2, {0x00,0x00}}
};

static struct LCM_setting_table lcm_backlight_HBM_setting[] = {
	{0xF0, 2, {0x5A,0x5A}},
	{0x53, 1, {0x20}},
	{0xB1, 3, {0x00,0x10,0x10}},
	{0xF7, 1, {0x03}},
	{0xF0, 2, {0xA5,0xA5}},
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

    params->dsi.mode   = CMD_MODE;
    params->dbi.te_mode = LCM_DBI_TE_MODE_VSYNC_ONLY;
    params->dbi.te_edge_polarity = LCM_POLARITY_RISING;

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

	params->dsi.vertical_sync_active                = 4;
	params->dsi.vertical_backporch                    = 6;
	params->dsi.vertical_frontporch                    = 20;
	params->dsi.vertical_active_line                = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active                = 20;
	params->dsi.horizontal_backporch                = 118;
	params->dsi.horizontal_frontporch                = 128;
	params->dsi.horizontal_active_pixel         = FRAME_WIDTH;

	params->dsi.PLL_CLOCK = 514;
	params->dsi.data_rate = 1029;
	params->dsi.ssc_disable = 1;

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
#ifndef BUILD_LK
	if (boot_mode == 0) {
		LCD_DEBUG("neither META_BOOT or FACTORY_BOOT\n");
		params->dsi.esd_check_enable = 1;
		params->dsi.customization_esd_check_enable = 0;
		//params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
		//params->dsi.lcm_esd_check_table[0].count = 1;
		//params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;
	}
#endif
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	params->round_corner_en = 1;
	params->full_content = 1;
	params->corner_pattern_width = 1080;
	params->corner_pattern_height = 91;
	params->corner_pattern_height_bot = 72;
#endif

#ifndef BUILD_LK
	register_device_proc("lcd", "SOFEG01_S-MTK02", "samsung1024 cmd mode");
#endif
}


static void poweron_before_ulps(void)
{
	LCD_DEBUG("[soso] poweron_before_ulps samsung 3v\n");

	lcd_vpoc_setting(1);
	MDELAY(2);
	lcd_vci_setting(1);
	MDELAY(10);
}


static void lcm_init_power(void)
{
	LCD_DEBUG("[soso] lcm_init_power\n");

	MDELAY(1);
	lcd_rst_setting(1);
	MDELAY(2);
	lcd_rst_setting(0);
	MDELAY(2);
	lcd_rst_setting(1);
	MDELAY(12);
}

static void lcm_suspend_power(void)
{
	LCD_DEBUG("[soso] lcm_suspend_power\n");
}


static void poweroff_after_ulps(void)
{
	LCD_DEBUG("[soso] poweroff_after_ulps samsung 3v\n");

	lcd_rst_setting(0);
	MDELAY(12);
	lcd_vci_setting(0);
	MDELAY(2);
	lcd_vpoc_setting(0);
	aod_mode = 0;
}

static void lcm_aod(int enter)
{
	if ((enter == 1) && (aod_mode == 0)) {
		aod_mode = 1;
		push_table(lcm_aod_in_setting, sizeof(lcm_aod_in_setting) / sizeof(struct LCM_setting_table), 1);
		printk("[soso] lcm_aod enter = %d\n",enter);
	} else if ((enter == 0) && (aod_mode == 1)) {
		aod_mode = 0;
		push_table(lcm_aod_out_setting, sizeof(lcm_aod_out_setting) / sizeof(struct LCM_setting_table), 1);
		printk("[soso] lcm_aod enter = %d\n",enter);
	}
}

static void lcm_aod_doze_resume(void)
{
	LCD_DEBUG("[soso] lcm_aod_doze_resume \n");
	push_table(lcm_aod_doze_resume_setting, sizeof(lcm_aod_doze_resume_setting) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_resume_power(void)
{
	LCD_DEBUG("[soso] lcm_resume_power \n");
	lcm_init_power();
}


static void lcm_init(void)
{
	LCD_DEBUG("[soso]lcm_initialization_setting\n");

	push_table(lcm_initialization_cmd_setting, sizeof(lcm_initialization_cmd_setting) / sizeof(struct LCM_setting_table), 1);
}


static void lcm_suspend(void)
{
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

extern unsigned int islcmconnected;

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{

	unsigned int mapped_level = 0;
	unsigned int BL_MSB = 0;
	unsigned int BL_LSB = 0;

	printk("%s level is %d\n", __func__, level);
	if (!islcmconnected) {
		return;
	}

	if(level >= 1024) {
		mapped_level = 1023;
	} else if (level < 0) {
		mapped_level = 0;
	} else {
		mapped_level = level;
	}


	if(mapped_level!=0)
		esd_recovery_backlight_level = mapped_level;

	BL_LSB = mapped_level / 256;
	BL_MSB = mapped_level % 256;

	lcm_backlight_level_setting[0].para_list[0] = BL_LSB;
	lcm_backlight_level_setting[0].para_list[1] = BL_MSB;

	push_table22(handle,lcm_backlight_level_setting,
		sizeof(lcm_backlight_level_setting)/sizeof(lcm_backlight_level_setting[0]), 1);
}

#ifdef VENDOR_EDIT
static char set_hbm_state = 0x20;
static char set_hbm_select[2] = {0x80,0x10};
static char set_hbm_mode[2] = {0x01, 0x70};

enum hbm_level
{
	HBM_L0 = 0,  // hbm disable
	HBM_MAX_1,   // hbm level 5
	HBM_DISABLE, // hbm disable
	HBM_MAX_2,   // hbm level 5
	HBM_L1,
	HBM_L2,
	HBM_L3,
	HBM_L4,
	HBM_L5,
	HBM_MAX,
	HBM_ON,
};

static int hbm_mode = HBM_L0; //default mode off

static void lcm_set_hbm_mode(void *handle,unsigned int hbm_level)
{
	pr_err("%s: HBM level:%d mode: %d setted!\n",__func__, hbm_level, hbm_mode);
	switch(hbm_level)
	{
		case HBM_L0:
		case HBM_DISABLE:
			set_hbm_state = 0x20;
			hbm_mode = HBM_L0;
			set_hbm_mode[0] = 0x80;
			set_hbm_mode[1] = 0x10;
			set_hbm_select[0] = 0x01;
			set_hbm_select[1] = 0x70;
			break;
		case HBM_L1:
			set_hbm_state = 0xe0;
			hbm_mode = HBM_L1;
			set_hbm_mode[0] = 0x81;
			set_hbm_mode[1] = 0x18;
			set_hbm_select[0] = 0x80;
			set_hbm_select[1] = 0x30;
			break;
		case HBM_L2:
			set_hbm_state = 0xe0;
			hbm_mode = HBM_L2;
			set_hbm_mode[0] = 0x80;
			set_hbm_mode[1] = 0xD6;
			set_hbm_select[0] = 0x80;
			set_hbm_select[1] = 0x30;
			break;
		case HBM_L3:
			set_hbm_state = 0xe0;
			hbm_mode = HBM_L3;
			set_hbm_mode[0] = 0x80;
			set_hbm_mode[1] = 0x94;
			set_hbm_select[0] = 0x80;
			set_hbm_select[1] = 0x30;
			break;
		case HBM_L4:
			set_hbm_state = 0xe0;
			hbm_mode = HBM_L4;
			set_hbm_mode[0] = 0x80;
			set_hbm_mode[1] = 0x52;
			set_hbm_select[0] = 0x80;
			set_hbm_select[1] = 0x30;
			break;
		case HBM_L5:
		case HBM_MAX_1:
		case HBM_MAX_2:
			set_hbm_state = 0xe0;
			hbm_mode = HBM_L5;
			set_hbm_mode[0] = 0x80;
			set_hbm_mode[1] = 0x10;
			set_hbm_select[0] = 0x01;
			set_hbm_select[1] = 0x70;
			break;

		default:
			pr_err("%s: Unsuporrted HBM level:%d\n", __func__, hbm_level);
			return;
	}

	lcm_backlight_HBM_setting[1].para_list[0] = set_hbm_state;
	push_table22(handle,lcm_backlight_HBM_setting,
		 sizeof(lcm_backlight_HBM_setting)/sizeof(lcm_backlight_HBM_setting[0]),1);
}
#endif /*VENDOR_EDIT*/


#ifndef VENDOR_EDIT
int parse_input(void *handle,const char *buf)
{
	int retval = 0;
	int index = 0;
	int cmdindex = 0;
	int ret = 0;
	char *input = (char *)buf;
	char *token = NULL;
	unsigned long value = 0;

	pr_err("[DISP] parse_input %s\n", buf);

	input[strlen(input)] = '\0';

	while (input != NULL && index < BUFFER_LENGTH) {
		token = strsep(&input, " ");
		retval = kstrtoul(token, 16, &value);
		if (retval < 0) {
			pr_err("%s: Failed to convert from string (%s) to hex number\n", __func__, token);
			continue;
		}
		debug.buffer[index] = (unsigned char)value;
		index++;
	}

	if (index > 1) {
		debug.length = index - 1;
	}

	if (debug.length <= 0) {
		return 0;
	}

	while (cmdindex < debug.length) {
		debug.cmds[cmdindex] = debug.buffer[cmdindex+1];
		cmdindex++;
	}
	printk("%s [soso] debug.buffer[0] is 0x%x debug.length = %d \n",
		__func__, debug.buffer[0],debug.length);

	while (ret < debug.length) {
		printk("[soso] debug.cmds is 0x%x\n",  debug.cmds[ret]);
		ret++;
	}
	dsi_set_cmdq_V22(handle, debug.buffer[0], debug.length, debug.cmds, 1);

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
			pr_err("%s: Failed to convert from string (%s) to hex number\n",
				__func__, token);
			continue;
		}
		debug_read.buffer[index] = (unsigned char)value;
		index++;
	}

	if (index > 1) {
		debug_read.length = debug_read.buffer[1];
	}

	if (debug_read.length <= 0) {
		return 0;
	}

	reg_rlengh = debug_read.length;

	printk("%s [soso] debug.buffer[0] is 0x%x debug.length = %d \n",
		__func__, debug_read.buffer[0],debug_read.length);

	retval = read_reg_v2(debug_read.buffer[0],
				debug_read.read_buffer, debug_read.length);

	if (retval<0) {
		printk("%s [soso] error can not read the reg 0x%x \n",
			__func__,debug_read.buffer[0]);
		return -1;
	}

	while (ret < debug_read.length) {
		printk("[soso] reg cmd 0x%x read_buffer is 0x%x \n",
				debug_read.buffer[0], debug_read.read_buffer[ret]);
		read_buffer[ret] = debug_read.read_buffer[ret];
		ret++;
	}
	return 1;
}
#endif /*VENDOR_EDIT*/


struct LCM_DRIVER oppo_samsung_ams628nw_lsi_1080p_dsi_cmd_lcm_drv=
{
	.name = "oppo_samsung_ams628nw_lsi_1080p_dsi_cmd",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.update = lcm_update,
	.poweron_before_ulps = poweron_before_ulps,
	.poweroff_after_ulps = poweroff_after_ulps,
	.set_hbm_mode_cmdq = lcm_set_hbm_mode,
	.aod = lcm_aod,
	.aod_doze_resume = lcm_aod_doze_resume,
};
