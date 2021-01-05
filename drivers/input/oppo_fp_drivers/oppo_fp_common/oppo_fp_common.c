/************************************************************************************
 ** File: - SDM670.LA.1.0\android\kernel\msm-4.4\drivers\soc\oppo\oppo_fp_common\oppo_fp_common.c
 ** VENDOR_EDIT
 ** Copyright (C), 2008-2017, OPPO Mobile Comm Corp., Ltd
 **
 ** Description:
 **      fp_common compatibility configuration
 **
 ** Version: 1.0
 ** Date created: 15:03:11,23/05/2018
 ** Author: Ran.Chen@Prd.BaseDrv
 **
 ** --------------------------- Revision History: --------------------------------
 **  <author>         <data>         <desc>
 **  Ran.Chen       2018/05/23     create the file,add goodix_optical_95s
 **  Ran.Chen       2018/05/28     add for fpc1023
 **  Ran.Chen       2018/06/15     add for Silead_Optical_fp
 **  Ran.Chen       2018/06/25     modify fpc1023+2060 to fpc1023_glass
 **  Ziqing.Guo     2018/07/12     add Silead_Optical_fp for 18181 18385
 **  Ziqing.Guo     2018/07/13     set silead fp for 18181 18385 at this moment, will modify later
 **  Ran.Chen       2018/08/01     modify for 18385 fp_id
 **  Long.Liu       2018/09/29     add 18531 FPC1023 fp_id
 **  Dongnan.Wu     2018/10/23	   modify for 17081(android P) fp_id
 **  Long.Liu       2018/11/15     modify for 18151 FPC1023_GLASS fp_id
 **  Ran.Chen       2018/11/26     add for SDM855
 **  Yang.Tan       2018/11/09     add for 18531 fpc1511
 **  Long.Liu       2019/01/03     add for 18161 fpc1511
 **  Bangxiong.wu   2019/01/24     add for 18073 silead_optical_fp
 **  Long.Liu       2019/02/01     add for 18561 GOODIX GF5658_FP
 **  Ziqing.Guo     2019/02/12     fix coverity error 64261,64267 to avoid buffer overflow
 **  Dongnan.Wu     2019/01/28     add for 18073 goodix optical fingerprint
 **  Bangxiong.wu   2019/02/24     add for 18593 silead_optical_fp
 **  Qing.Guan      2019/04/01     add for egis et711 fp
 **  Hongyu.lu      2019/04/18     add for 19021 fpc1511
 **  Hongyu.lu      2019/04/18     add for 19321 19026 fpc1511
 **  Long.Liu       2019/05/08     add for 17085(android P) fp_id
 **  Qing.Guan      2019/05/08     add for 18081 silead fp O to P
 **  Bangxiong.Wu   2019/05/10     add for SM7150 (MSM_19031 MSM_19331)
 **  Bangxiong.Wu   2019/05/16     enable optical GF irq handle for SM7150(MSM_19031 MSM_19331)
 **  Dongnan.Wu     2019/05/21     add for 19011&19301 platform
 **  Hongyu.lu      2019/05/24     add for 19328 platform
 **  Bangxiong.Wu   2019/06/03     add for MSM_19111 MSM_19513
 **  Qijia.Zhou     2019/06/12     add for 19531 platform
 **  Qijia.Zhou     2019/10/02     add for 19151 19350
 ##  oujinrong      2019/10/08     add for 19053
 ************************************************************************************/

#include <linux/module.h>
#include <linux/proc_fs.h>
#if CONFIG_OPPO_FINGERPRINT_PLATFORM == 6763 || CONFIG_OPPO_FINGERPRINT_PLATFORM == 6771 || CONFIG_OPPO_FINGERPRINT_PLATFORM == 6779 || CONFIG_OPPO_FINGERPRINT_PLATFORM == 6785 || CONFIG_OPPO_FINGERPRINT_PLATFORM == 6768
#include <sec_boot_lib.h>
#include <linux/uaccess.h>
#elif CONFIG_OPPO_FINGERPRINT_PLATFORM == 855 || CONFIG_OPPO_FINGERPRINT_PLATFORM == 6125 || CONFIG_OPPO_FINGERPRINT_PLATFORM == 7150
#include <linux/uaccess.h>
#else
#include <soc/qcom/smem.h>
#endif
#include <soc/oppo/oppo_project.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/fs.h>
#include <linux/of_gpio.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/string.h>
#include "../include/oppo_fp_common.h"

#define CHIP_PRIMAX     "primax"
#define CHIP_CT         "CT"
#define CHIP_OFILM      "ofilm"
#define CHIP_QTECH      "Qtech"
#define CHIP_TRULY      "truly"

#define CHIP_GOODIX     "G"
#define CHIP_FPC        "F"
#define CHIP_SILEAD     "S"
#define CHIP_EGIS		"E"

#define CHIP_UNKNOWN    "unknown"

#define ENGINEER_MENU_FPC1140  "-1,-1"  /* content in " " represents SNR,inclination test item in order in engineer menu, and -1/1 means off/on */
#define ENGINEER_MENU_FPC1022  "-1,-1"
#define ENGINEER_MENU_FPC1023  "1,-1"
#define ENGINEER_MENU_FPC1260  "1,-1"
#define ENGINEER_MENU_FPC1270  "-1,-1"
#define ENGINEER_MENU_FPC1511  "-1,-1"
#define ENGINEER_MENU_FPC1541   "-1,-1"
#define ENGINEER_MENU_GOODIX   "1,1"
#define ENGINEER_MENU_GOODIX_3626   "-1,-1"
#define ENGINEER_MENU_GOODIX_3268   "-1,-1"
#define ENGINEER_MENU_GOODIX_5288   "-1,-1"
#define ENGINEER_MENU_GOODIX_5228   "-1,-1"
#define ENGINEER_MENU_GOODIX_OPTICAL   "-1,-1"
#define ENGINEER_MENU_SILEAD_OPTICAL   "-1,-1"
#define ENGINEER_MENU_EGIS_OPTICAL  "-1,-1"
#define ENGINEER_MENU_GOODIX_5298   "-1,-1"
#define ENGINEER_MENU_GOODIX_5658   "-1,-1"
#define ENGINEER_MENU_EGIS_520  	"-1,-1"
#define ENGINEER_MENU_SILEAD_6150   "-1,-1"
#define ENGINEER_MENU_DEFAULT  "-1,-1"

static struct proc_dir_entry *fp_id_dir = NULL;
static char *fp_id_name = "fp_id";
static struct proc_dir_entry *oppo_fp_common_dir = NULL;
static char *oppo_fp_common_dir_name = "oppo_fp_common";

static char fp_manu[FP_ID_MAX_LENGTH] = CHIP_UNKNOWN; /* the length of this string should be less than FP_ID_MAX_LENGTH */

static struct fp_data *fp_data_ptr = NULL;
char g_engineermode_menu_config[ENGINEER_MENU_SELECT_MAXLENTH] = ENGINEER_MENU_DEFAULT;

#if CONFIG_OPPO_FINGERPRINT_PLATFORM == 6785

fp_module_config_t fp_module_config_list[] = {
    {{1, -1, -1},  FP_GOODIX_3626,  CHIP_GOODIX,   ENGINEER_MENU_GOODIX_3626},
    {{0, -1, -1},  FP_FPC_1541,     CHIP_FPC,      ENGINEER_MENU_FPC1541},
};
#elif CONFIG_OPPO_FINGERPRINT_PLATFORM == 6768
fp_module_config_t fp_module_config_list[] = {
	{{1, -1, -1},  FP_EGIS_520,    	CHIP_EGIS,    ENGINEER_MENU_EGIS_520},
	{{0, -1, -1},  FP_SILEAD_6150,	CHIP_SILEAD,  ENGINEER_MENU_SILEAD_6150},
};
#else
fp_module_config_t fp_module_config_list[] = {
    {{1, -1, -1},  FP_UNKNOWN,    	CHIP_UNKNOWN,     ENGINEER_MENU_FPC1023},
    {{1, -1, -1},  FP_UNKNOWN,    	CHIP_UNKNOWN,     ENGINEER_MENU_FPC1023},

#endif

static int fp_request_named_gpio(struct fp_data *fp_data,
        const char *label, int *gpio)
{
    struct device *dev = fp_data->dev;
    struct device_node *np = dev->of_node;

    int ret = of_get_named_gpio(np, label, 0);
    if (ret < 0) {
        dev_err(dev, "failed to get '%s'\n", label);
        return FP_ERROR_GPIO;
    }

    *gpio = ret;
    ret = devm_gpio_request(dev, *gpio, label);
    if (ret) {
        dev_err(dev, "failed to request gpio %d\n", *gpio);
        devm_gpio_free(dev, *gpio);
        return FP_ERROR_GPIO;
    }

    dev_err(dev, "%s - gpio: %d\n", label, *gpio);
    return FP_OK;
}

static int fp_gpio_parse_dts(struct fp_data *fp_data)
{
    int ret = FP_OK;

    if (!fp_data) {
        ret = FP_ERROR_GENERAL;
        goto exit;
    }

#if CONFIG_OPPO_FINGERPRINT_PLATFORM == 6785
		if (fp_data->pdev) {
			fp_data->gpio_id0_pinctrl = devm_pinctrl_get(fp_data->dev);
			if (IS_ERR(fp_data->gpio_id0_pinctrl)) {
				ret = PTR_ERR(fp_data->gpio_id0_pinctrl);
				pr_err("%s can't find gpio_id0_pinctrl pinctrl\n", __func__);
				return ret;
			}
		} else {
			pr_err("%s platform device is null\n", __func__);
			return -1;
		}
#endif

    ret = fp_request_named_gpio(fp_data, "oppo,fp-id0",
            &fp_data->gpio_id0);
    if (ret) {
        ret = FP_ERROR_GPIO;
        goto exit;
    }

exit:
    return ret;
}


static ssize_t fp_id_node_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
    char page[FP_ID_MAX_LENGTH] = { 0 };
    char *p = page;
    int len = 0;

    p += snprintf(p, FP_ID_MAX_LENGTH - 1, "%s", fp_manu);
    len = p - page;
    if (len > *pos) {
        len -= *pos;
    }
    else {
        len = 0;
    }

    if (copy_to_user(buf, page, len < count ? len  : count)) {
        return -EFAULT;
    }

    *pos = *pos + (len < count ? len  : count);

    return len < count ? len  : count;
}

static ssize_t fp_id_node_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
    size_t local_count;
    if (count <= 0) {
        return 0;
    }
    strncpy(fp_manu, CHIP_UNKNOWN, FP_ID_MAX_LENGTH - 1);

    local_count = (FP_ID_MAX_LENGTH - 1) < count ? (FP_ID_MAX_LENGTH - 1) : count;
    if (copy_from_user(fp_manu , buf, local_count) != 0) {
        dev_err(fp_data_ptr->dev, "write fp manu value fail\n");
        return -EFAULT;
    }
    fp_manu[local_count] = '\0';
    dev_info(fp_data_ptr->dev, "write fp manu = %s\n", fp_manu);
    return count;
}

static struct file_operations fp_id_node_ctrl = {
    .read = fp_id_node_read,
    .write = fp_id_node_write,
};

static int fp_get_matched_chip_module(struct device *dev, int fp_id1, int fp_id2, int fp_id3)
{
    int i;
    for (i = 0; i < sizeof(fp_module_config_list)/sizeof(fp_module_config_t); ++i) {
        if ((fp_module_config_list[i].gpio_id_config_list[0] == fp_id1) &&
                (fp_module_config_list[i].gpio_id_config_list[1] == fp_id2) &&
                (fp_module_config_list[i].gpio_id_config_list[2] == fp_id3)) {
            switch (fp_module_config_list[i].fp_vendor_chip) {
                case FP_FPC_1541:
                    strncpy(fp_manu, CHIP_FPC, FP_ID_MAX_LENGTH - FP_ID_SUFFIX_MAX_LENGTH);
                    strncat(fp_manu, "_1541", FP_ID_SUFFIX_MAX_LENGTH - 1);
                    strncpy(g_engineermode_menu_config, fp_module_config_list[i].engineermode_menu_config, ENGINEER_MENU_SELECT_MAXLENTH - 1);
                    return FP_FPC_1541;
                case FP_GOODIX_3626:
                    strncpy(fp_manu, CHIP_GOODIX, FP_ID_MAX_LENGTH - FP_ID_SUFFIX_MAX_LENGTH);
                    strncat(fp_manu, "_3626", FP_ID_SUFFIX_MAX_LENGTH - 1);
                    strncpy(g_engineermode_menu_config, fp_module_config_list[i].engineermode_menu_config, ENGINEER_MENU_SELECT_MAXLENTH - 1);
                    return FP_GOODIX_3626;
                case FP_EGIS_520:
                    strncpy(fp_manu, CHIP_EGIS, FP_ID_MAX_LENGTH - FP_ID_SUFFIX_MAX_LENGTH);
                    strncat(fp_manu, "_520", FP_ID_SUFFIX_MAX_LENGTH - 1);
                    strncpy(g_engineermode_menu_config, fp_module_config_list[i].engineermode_menu_config, ENGINEER_MENU_SELECT_MAXLENTH - 1);
                    return FP_EGIS_520;
                case FP_SILEAD_6150:
                    strncpy(fp_manu, CHIP_SILEAD, FP_ID_MAX_LENGTH - FP_ID_SUFFIX_MAX_LENGTH);
                    strncat(fp_manu, "_6150", FP_ID_SUFFIX_MAX_LENGTH - 1);
                    strncpy(g_engineermode_menu_config, fp_module_config_list[i].engineermode_menu_config, ENGINEER_MENU_SELECT_MAXLENTH - 1);
                    return FP_SILEAD_6150;
                default:
                    dev_err(dev, "gpio ids matched but no matched vendor chip!");
                    return FP_UNKNOWN;
            }
        }
    }
    strncpy(fp_manu, CHIP_UNKNOWN, FP_ID_MAX_LENGTH - 1);
    strncpy(g_engineermode_menu_config, ENGINEER_MENU_DEFAULT, ENGINEER_MENU_SELECT_MAXLENTH - 1);
    return FP_UNKNOWN;
}

static int fp_register_proc_fs(struct fp_data *fp_data)
{
    uint32_t fp_id_retry;
    fp_id_retry = 0;

#if CONFIG_OPPO_FINGERPRINT_PLATFORM == 6785
    int fp_id_pull_up_value = 0;
    int fp_id_pull_down_value = 0;
#endif

	fp_data->fp_id0 = gpio_get_value(fp_data->gpio_id0);

#if CONFIG_OPPO_FINGERPRINT_PLATFORM == 6785

	fp_data->gpio_id0_pull_up = pinctrl_lookup_state(fp_data->gpio_id0_pinctrl, "gpio_id0_pull_up");
	if (IS_ERR(fp_data->gpio_id0_pull_up)) {
			dev_err(fp_data->dev, "Can't find gpio_id0_pull_up pinctrl state\n");
			return PTR_ERR(fp_data->gpio_id0_pull_up);
	}
	fp_data->gpio_id0_pull_down = pinctrl_lookup_state(fp_data->gpio_id0_pinctrl, "gpio_id0_pull_down");
	if (IS_ERR(fp_data->gpio_id0_pull_down)) {
			dev_err(fp_data->dev, "Can't find gpio_id0_pull_down pinctrl state\n");
			return PTR_ERR(fp_data->gpio_id0_pull_down);
	}
	gpio_direction_output(fp_data->gpio_id0, 1);
	pinctrl_select_state(fp_data->gpio_id0_pinctrl, fp_data->gpio_id0_pull_up);
	gpio_direction_input(fp_data->gpio_id0);
	fp_id_pull_up_value = gpio_get_value(fp_data->gpio_id0);

	gpio_direction_output(fp_data->gpio_id0, 0);
	pinctrl_select_state(fp_data->gpio_id0_pinctrl, fp_data->gpio_id0_pull_down);
	gpio_direction_input(fp_data->gpio_id0);
	fp_id_pull_down_value = gpio_get_value(fp_data->gpio_id0);

	if(fp_id_pull_up_value == 1 && fp_id_pull_down_value == 0){
		fp_data->fp_id0 = 1;
	}else if(fp_id_pull_up_value == 0 && fp_id_pull_down_value == 0){
		fp_data->fp_id0 = 0;
	}else if(fp_id_pull_up_value == 1 && fp_id_pull_down_value == 1){
	    fp_data->fp_id0 = 1;
	}
    dev_err(fp_data->dev, "fp_register_proc_fs check: fp_id_pull_up_value=%d, fp_id_pull_down_value=%d: fp_id0= %d\n", \
            fp_id_pull_up_value, fp_id_pull_down_value,fp_data->fp_id0 );
#endif

    fp_data->fp_id1 = -1;
    fp_data->fp_id2 = -1;

    dev_err(fp_data->dev, "fp_register_proc_fs check: fp_id0= %d, fp_id1= %d, fp_id2= %d, fp_id3 = %d, fp_id_retry= %d\n", \
            fp_data->fp_id0, fp_data->fp_id1, fp_data->fp_id2, fp_data->fp_id3, fp_id_retry);

    fp_data->fpsensor_type = fp_get_matched_chip_module(fp_data->dev, fp_data->fp_id0, fp_data->fp_id1, fp_data->fp_id2);

    /*  make the proc /proc/fp_id  */
    fp_id_dir = proc_create(fp_id_name, 0666, NULL, &fp_id_node_ctrl);
    if (fp_id_dir == NULL) {
        return FP_ERROR_GENERAL;
    }

    return FP_OK;
}

fp_vendor_t get_fpsensor_type(void)
{
    fp_vendor_t fpsensor_type = FP_UNKNOWN;

    if (NULL == fp_data_ptr) {
        pr_err("%s no device", __func__);
        return FP_UNKNOWN;
    }

    fpsensor_type = fp_data_ptr->fpsensor_type;

    return fpsensor_type;
}


static int oppo_fp_common_probe(struct platform_device *fp_dev)
{
    int ret = 0;
    struct device *dev = &fp_dev->dev;
    struct fp_data *fp_data = NULL;
    dev_err(dev, "line = %d: --- %s ---\n",__LINE__,__func__);
    fp_data = devm_kzalloc(dev, sizeof(struct fp_data), GFP_KERNEL);
    if (fp_data == NULL) {
        dev_err(dev, "fp_data kzalloc failed\n");
        ret = -ENOMEM;
        goto exit;
    }

    fp_data->dev = dev;
#if CONFIG_OPPO_FINGERPRINT_PLATFORM == 6785
    fp_data->pdev = fp_dev;
#endif
    fp_data_ptr = fp_data;
    ret = fp_gpio_parse_dts(fp_data);
    if (ret) {
        goto exit;
    }

#if CONFIG_OPPO_FINGERPRINT_PLATFORM == 6763 || CONFIG_OPPO_FINGERPRINT_PLATFORM == 6771 || CONFIG_OPPO_FINGERPRINT_PLATFORM == 6779 || CONFIG_OPPO_FINGERPRINT_PLATFORM == 6785 || CONFIG_OPPO_FINGERPRINT_PLATFORM == 6768
    msleep(20);
#endif

    ret = fp_register_proc_fs(fp_data);
    if (ret) {
        goto exit;
    }

    return FP_OK;

exit:

    if (oppo_fp_common_dir) {
        remove_proc_entry(oppo_fp_common_dir_name, NULL);
    }

    if (fp_id_dir) {
        remove_proc_entry(fp_id_name, NULL);
    }

    dev_err(dev, "fp_data probe failed ret = %d\n", ret);
    if (fp_data) {
        devm_kfree(dev, fp_data);
    }

    return ret;
}

static int oppo_fp_common_remove(struct platform_device *pdev)
{
    return FP_OK;
}

static struct of_device_id oppo_fp_common_match_table[] = {
    {   .compatible = "oppo,fp_common", },
    {}
};

static struct platform_driver oppo_fp_common_driver = {
    .probe = oppo_fp_common_probe,
    .remove = oppo_fp_common_remove,
    .driver = {
        .name = "oppo_fp_common",
        .owner = THIS_MODULE,
        .of_match_table = oppo_fp_common_match_table,
    },
};

static int __init oppo_fp_common_init(void)
{   printk("line = %d: --- %s ---\n",__LINE__,__func__);
    return platform_driver_register(&oppo_fp_common_driver);
}

static void __exit oppo_fp_common_exit(void)
{   printk("line = %d: --- %s ---\n",__LINE__,__func__);
    platform_driver_unregister(&oppo_fp_common_driver);
}

subsys_initcall(oppo_fp_common_init);
module_exit(oppo_fp_common_exit)
