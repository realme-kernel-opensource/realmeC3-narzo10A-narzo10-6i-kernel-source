/***************************************************************
** Copyright (C),  2018,  OPPO Mobile Comm Corp.,  Ltd
** VENDOR_EDIT
** File : oppo_display_private_api.h
** Description : oppo display private api implement
** Version : 1.0
** Date : 2018/03/20
** Author : Jie.Hu@PSW.MM.Display.Stability
**
** ------------------------------- Revision History: -----------
**  <author>        <data>        <version >        <desc>
**   Hu.Jie          2018/03/20        1.0           Build this moudle
**   Guo.Ling        2018/10/11        1.1           Modify for SDM660
**   Guo.Ling        2018/11/27        1.2           Modify for mt6779
******************************************************************/
#include "oppo_display_private_api.h"

/*
 * we will create a sysfs which called /sys/kernel/oppo_display,
 * In that directory, oppo display private api can be called
 */

unsigned long oppo_display_brightness = 0;
unsigned int oppo_set_brightness = 0;
unsigned int aod_light_mode = 0;
bool oppo_display_ffl_support;
bool oppo_display_sau_support;
bool oppo_display_cabc_support;

extern int primary_display_setbacklight(unsigned int level);
extern void _primary_path_switch_dst_lock(void);
extern void _primary_path_switch_dst_unlock(void);
extern void _primary_path_lock(const char *caller);
extern void _primary_path_unlock(const char *caller);
#ifdef ODM_WT_EDIT
extern int primary_display_set_cabc(unsigned int enable);
extern  int primary_display_get_cabc(int *status);
#endif
#ifdef ODM_WT_EDIT
extern unsigned int g_lcd_backlight;
extern int mtkfb_set_backlight_level(unsigned int level);
//Hao.Liang@ODM_WT.MM.Display.Lcd, 2019/10/22, Add clk_change function
extern int mipi_clk_change(int msg, int en);
//Hao.Liang@ODM_WT.MM.Display.Lcd, 2019/10/19, Add ffl function
extern void ffl_set_enable(unsigned int enable);
extern unsigned int ffl_set_mode ;
extern unsigned int ffl_backlight_on ;
extern bool ffl_trigger_finish;
//Hao.Liang@ODM_WT.MM.Display.Lcd, 2019/12/9, dre function in cabc
extern void disp_aal_set_dre_en(int enable);
extern int primary_display_set_cabc_mode(unsigned int level);
#endif
static ssize_t oppo_display_get_brightness(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
#ifdef ODM_WT_EDIT
	return sprintf(buf, "%d\n", g_lcd_backlight);
#endif

}

static ssize_t oppo_display_set_brightness(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t num)
{
	int ret;

	ret = kstrtouint(buf, 10, &oppo_set_brightness);

	printk("%s %d\n", __func__, oppo_set_brightness);
#ifdef ODM_WT_EDIT
	if (oppo_set_brightness > LED_2047 || oppo_set_brightness < LED_OFF) {
		return num;
	}
#else
	if (oppo_set_brightness > LED_FULL || oppo_set_brightness < LED_OFF) {
		return num;
	}
#endif
	mtkfb_set_backlight_level(oppo_set_brightness);

	return num;
}
#ifdef ODM_WT_EDIT
unsigned int oppo_set_cabc = 0;
static ssize_t oppo_display_get_cabc(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	primary_display_get_cabc(&oppo_set_cabc);
	return sprintf(buf, "%d\n", oppo_set_cabc);
}

static ssize_t oppo_display_set_cabc(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t num)
{
	int ret;

	ret = kstrtouint(buf, 10, &oppo_set_cabc);
	if (oppo_set_cabc > 3 || oppo_set_cabc < 0) {
			printk("%s %d is out of range \n", __func__, oppo_set_cabc);
			return num;
	}else{
			if (oppo_set_cabc == 0) {
				disp_aal_set_dre_en(1);
				printk("%s enable dre\n", __func__);
			} else {
				disp_aal_set_dre_en(1);
				printk("%s enable dre\n", __func__);
			}
			printk("%s %d\n", __func__, oppo_set_cabc);
			//primary_display_set_cabc(oppo_set_cabc);
			primary_display_set_cabc_mode(oppo_set_cabc);
			return num;
		}
}

unsigned int oppo_get_sn =0;
	static ssize_t oppo_display_get_panel_serial_number(struct device *dev,
									struct device_attribute *attr, char *buf)
	{
		printk("%s unsupport TFT panel \n", __func__);
		return sprintf(buf, "%d\n", oppo_get_sn);
	}
	//Hao.Liang@ODM_WT.MM.Display.Lcd, 2019/10/22, Add clk_change function
	unsigned int oppo_clk_chage= 0;
		static ssize_t oppo_display_clk_change(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t num)
	{
		int ret;

		ret = kstrtouint(buf, 10, &oppo_clk_chage);

		printk("%s oppo_clk_chage=%d\n", __func__, oppo_clk_chage);
	if(oppo_clk_chage==1){
			 mipi_clk_change(0, oppo_clk_chage);
		}
		return num;

	}
	//Hao.Liang@ODM_WT.MM.Display.Lcd, 2019/10/19, Add ffl function
	static ssize_t oppo_display_get_ffl(struct device *dev,
									struct device_attribute *attr, char *buf)
	{
		printk("%s ffl_set_mode=%d\n", __func__, ffl_set_mode);
		return sprintf(buf, "%d\n", ffl_set_mode);

	}

	static ssize_t oppo_display_set_ffl(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t num)
	{
		int ret;

		ret = kstrtouint(buf, 10, &ffl_set_mode);

		printk("%s ffl_set_mode=%d,ffl_backlight_on=%d,ffl_trigger_finish=%d\n", __func__, ffl_set_mode,ffl_backlight_on,ffl_trigger_finish);

		if (ffl_trigger_finish && (ffl_backlight_on == 1) && (ffl_set_mode == 1)) {
		printk("%s come in ffl enable \n", __func__);
			ffl_set_enable(1);
		}

		return num;

	}

#endif

static ssize_t oppo_display_get_max_brightness(struct device *dev,
		struct device_attribute *attr, char *buf)
{

#ifdef ODM_WT_EDIT
	return sprintf(buf, "%u\n", LED_2047);
#else
	return sprintf(buf, "%u\n", LED_FULL);
#endif
}



int oppo_panel_alpha = 0;
int oppo_underbrightness_alpha = 0;
int alpha_save = 0;
struct ba {
	u32 brightness;
	u32 alpha;
};

struct ba brightness_alpha_lut[] = {
	{0, 0xff},
	{1, 0xee},
	{2, 0xe8},
	{3, 0xe6},
	{4, 0xe5},
	{6, 0xe4},
	{10, 0xe0},
	{20, 0xd5},
	{30, 0xce},
	{45, 0xc6},
	{70, 0xb7},
	{100, 0xad},
	{150, 0xa0},
	{227, 0x8a},
	{300, 0x80},
	{400, 0x6e},
	{500, 0x5b},
	{600, 0x50},
	{800, 0x38},
	{1023, 0x18},
};

static int interpolate(int x, int xa, int xb, int ya, int yb)
{
	int bf, factor, plus;
	int sub = 0;

	bf = 2 * (yb - ya) * (x - xa) / (xb - xa);
	factor = bf / 2;
	plus = bf % 2;
	if ((xa - xb) && (yb - ya))
		sub = 2 * (x - xa) * (x - xb) / (yb - ya) / (xa - xb);

	return ya + factor + plus + sub;
}

int bl_to_alpha(int brightness)
{
	int level = ARRAY_SIZE(brightness_alpha_lut);
	int i = 0;
	int alpha;

	for (i = 0; i < ARRAY_SIZE(brightness_alpha_lut); i++){
		if (brightness_alpha_lut[i].brightness >= brightness)
			break;
	}

	if (i == 0)
		alpha = brightness_alpha_lut[0].alpha;
	else if (i == level)
		alpha = brightness_alpha_lut[level - 1].alpha;
	else
		alpha = interpolate(brightness,
			brightness_alpha_lut[i-1].brightness,
			brightness_alpha_lut[i].brightness,
			brightness_alpha_lut[i-1].alpha,
			brightness_alpha_lut[i].alpha);
	return alpha;
}

int brightness_to_alpha(int brightness)
{
	int alpha;

	if (brightness <= 3)
		return alpha_save;

	alpha = bl_to_alpha(brightness);

	alpha_save = alpha;

	return alpha;
}

int oppo_get_panel_brightness_to_alpha(void)
{
	if (oppo_panel_alpha)
		return oppo_panel_alpha;

	return brightness_to_alpha(oppo_display_brightness);
}


int oppo_dc_alpha = 0;
#if defined(CONFIG_OPPO_SPECIAL_BUILD)
int oppo_dc_enable = 1;
#else
int oppo_dc_enable = 0;
#endif
static ssize_t oppo_display_get_dc_enable(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", oppo_dc_enable);
}

static ssize_t oppo_display_set_dc_enable(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t count)
{
	sscanf(buf, "%x", &oppo_dc_enable);
	return count;
}

static ssize_t oppo_display_get_dim_dc_alpha(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", oppo_dc_alpha);
}

static ssize_t oppo_display_set_dim_dc_alpha(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t count)
{
	sscanf(buf, "%x", &oppo_dc_alpha);
	return count;
}

unsigned long silence_mode = 0;

static ssize_t silence_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	printk("%s silence_mode=%ld\n", __func__, silence_mode);
	return sprintf(buf, "%ld\n", silence_mode);
}

static ssize_t silence_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t num)
{
	int ret;
	ret = kstrtoul(buf, 10, &silence_mode);
	printk("%s silence_mode=%ld\n", __func__, silence_mode);
	return num;
}

static struct kobject *oppo_display_kobj;

static DEVICE_ATTR(oppo_brightness, S_IRUGO|S_IWUSR, oppo_display_get_brightness, oppo_display_set_brightness);
static DEVICE_ATTR(oppo_max_brightness, S_IRUGO|S_IWUSR, oppo_display_get_max_brightness, NULL);
#ifdef ODM_WT_EDIT
static DEVICE_ATTR(cabc, S_IRUGO|S_IWUSR, oppo_display_get_cabc, oppo_display_set_cabc);
static DEVICE_ATTR(panel_serial_number, S_IRUGO|S_IWUSR, oppo_display_get_panel_serial_number, NULL);
//Hao.Liang@ODM_WT.MM.Display.Lcd, 2019/10/22, Add clk_change function
static DEVICE_ATTR(clk_change, S_IRUGO|S_IWUSR, NULL, oppo_display_clk_change);
//Hao.Liang@ODM_WT.MM.Display.Lcd, 2019/10/19, Add ffl function
static DEVICE_ATTR(ffl, S_IRUGO|S_IWUSR, oppo_display_get_ffl, oppo_display_set_ffl);
#endif
static DEVICE_ATTR(dimlayer_bl_en, S_IRUGO|S_IWUSR, oppo_display_get_dc_enable, oppo_display_set_dc_enable);
static DEVICE_ATTR(dim_dc_alpha, S_IRUGO|S_IWUSR, oppo_display_get_dim_dc_alpha, oppo_display_set_dim_dc_alpha);
#ifdef VENDOR_EDIT
static DEVICE_ATTR(sau_closebl_node, S_IRUGO|S_IWUSR, silence_show, silence_store);
#endif
/*
 * Create a group of attributes so that we can create and destroy them all
 * at once.
 */
static struct attribute *oppo_display_attrs[] = {
	&dev_attr_oppo_brightness.attr,
	&dev_attr_oppo_max_brightness.attr,
#ifdef ODM_WT_EDIT
	&dev_attr_cabc.attr,
	&dev_attr_panel_serial_number.attr,
	//Hao.Liang@ODM_WT.MM.Display.Lcd, 2019/10/22, Add clk_change function
	&dev_attr_clk_change.attr,
	//Hao.Liang@ODM_WT.MM.Display.Lcd, 2019/10/19, Add ffl function
	&dev_attr_ffl.attr,
#endif
	&dev_attr_dimlayer_bl_en.attr,
	&dev_attr_dim_dc_alpha.attr,
#ifdef VENDOR_EDIT
	&dev_attr_sau_closebl_node.attr,
#endif
	NULL,	/* need to NULL terminate the list of attributes */
};

static struct attribute_group oppo_display_attr_group = {
	.attrs = oppo_display_attrs,
};

static int __init oppo_display_private_api_init(void)
{
	int retval;

	oppo_display_kobj = kobject_create_and_add("oppo_display", kernel_kobj);
	if (!oppo_display_kobj)
		return -ENOMEM;

	/* Create the files associated with this kobject */
	retval = sysfs_create_group(oppo_display_kobj, &oppo_display_attr_group);
	if (retval)
		kobject_put(oppo_display_kobj);

	return retval;
}

static void __exit oppo_display_private_api_exit(void)
{
	kobject_put(oppo_display_kobj);
}

module_init(oppo_display_private_api_init);
module_exit(oppo_display_private_api_exit);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Hujie <hujie@oppo.com>");
