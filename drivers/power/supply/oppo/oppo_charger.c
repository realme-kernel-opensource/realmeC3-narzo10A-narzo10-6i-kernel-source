/**********************************************************************************
* Copyright (c)  2008-2015  Guangdong OPPO Mobile Comm Corp., Ltd
* VENDOR_EDIT
* Description: Charger IC management module for charger system framework.
*                          Manage all charger IC and define abstarct function flow.
* Version   : 1.0
* Date          : 2015-06-22
* Author        : fanhui@PhoneSW.BSP
*                         : Fanhong.Kong@ProDrv.CHG
* ------------------------------ Revision History: --------------------------------
* <version>           <date>                <author>                            <desc>
* Revision 1.0        2015-06-22        fanhui@PhoneSW.BSP             Created for new architecture
* Revision 1.0        2015-06-22        Fanhong.Kong@ProDrv.CHG        Created for new architecture
* Revision 1.1        2016-03-07        wenbin.liu@SW.Bsp.Driver       edit for log optimize
* Revision 2.0        2018-04-14        Fanhong.Kong@ProDrv.CHG        Upgrade for SVOOC
***********************************************************************************/
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/of_gpio.h>
#include <linux/kthread.h>
#include <linux/version.h>
#include <linux/reboot.h>
#include<linux/uaccess.h>
#include <uapi/linux/sched/types.h>
#ifdef CONFIG_OPPO_CHARGER_MTK

//#include <mtk_boot_common.h>
#include <mt-plat/mtk_boot.h>
#include <linux/gpio.h>
#else /* CONFIG_OPPO_CHARGER_MTK */
#include <linux/spinlock.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/of.h>

#include <linux/bitops.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/spmi.h>
#include <linux/printk.h>
#include <linux/ratelimit.h>
#include <linux/debugfs.h>
#include <linux/leds.h>
#include <linux/rtc.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0))
#include <linux/qpnp/qpnp-adc.h>
#else
#include <uapi/linux/sched/types.h>
#endif
#include <linux/batterydata-lib.h>
#include <linux/of_batterydata.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0))
#include <linux/msm_bcl.h>
#endif
#include <linux/ktime.h>
#include <linux/kernel.h>
#endif

//#ifdef VENDOR_EDIT
//zhouhengguo@BSP.Stabliity, 2019.10.14, add for aging version
#include <soc/oppo/oppo_project.h>
//#endif

#include "oppo_charger.h"
#include "oppo_gauge.h"
#include "oppo_vooc.h"
#include "oppo_short.h"
#include "oppo_adapter.h"
#include "charger_ic/oppo_short_ic.h"

#ifdef ODM_WT_EDIT
/* Tongxing.Liu@ODM_WT.BSP.Tp.Funtion, 20191111,Add for usb status notify tp funtion*/
#include <linux/tp_usb_notifier.h>
#endif /* ODM_WT_EDIT*/

#ifdef CONFIG_OPPO_EMMC_LOG
/*Jingchun.Wang@BSP.Kernel.Debug, 2016/12/21,*/
/*add for emmc log*/
#include <soc/oppo/oppo_emmclog.h>
#endif /*CONFIG_OPPO_EMMC_LOG*/

static struct oppo_chg_chip *g_charger_chip = NULL;

#define OPPO_CHG_UPDATE_INTERVAL_SEC        5
#define OPPO_CHG_UPDATE_INIT_DELAY        round_jiffies_relative(msecs_to_jiffies(500))        /* first run after init 10s */
#define OPPO_CHG_UPDATE_INTERVAL        round_jiffies_relative(msecs_to_jiffies(OPPO_CHG_UPDATE_INTERVAL_SEC*1000))        /* update cycle 5s */

#define OPPO_CHG_DEFAULT_CHARGING_CURRENT        512

int enable_charger_log = 2;
int charger_abnormal_log = 0;
int tbatt_pwroff_enable = 1;
#ifdef ODM_WT_EDIT
/*Shouli.Wang@ODM_WT.BSP.CHG 2019/11/05, add for batt_param_noplug proc*/
static int noplug_temperature = 0;
static int noplug_batt_volt_max = 0; 
/*Shouli.Wang@ODM_WT.BSP.CHG 2019/11/25, add for battery info node*/
extern int bat_id_num;
extern int battery_id_vol;
static int noplug_batt_volt_min = 0;
/*Sidong.Zhao@ODM_WT.BSP.CHG 2019/11/22,debug add*/
char *aicr_voter[8] = {"usb/ac","batt_temp","lcd","camera","ap_temp","cpu_temp","phonecall"};
enum aicl_by {
	CHARGER = 0,
	TBATT,
	LCD,
	CAMERA,
	AP,
	CPU,
	PHONECALL,
};
/*Shouli.Wang@ODM_WT.BSP.CHG 2019/11/25, add for charger*/
#define TOTAL_CHG_PERCENT 100
#define PRIMARY_CHG_PERCENT 70
#define SECONDARY_CHG_PERCENT (TOTAL_CHG_PERCENT-PRIMARY_CHG_PERCENT)
/*Shouli.Wang@ODM_WT.BSP.CHG 2019/12/04, add for chg debug*/
extern int ap_temp_debug;
#endif /*ODM_WT_EDIT*/
/* wenbin.liu@SW.Bsp.Driver, 2016/03/01  Add for log tag*/
#define charger_xlog_printk(num, fmt, ...) \
        do { \
                if (enable_charger_log >= (int)num) { \
                        printk(KERN_NOTICE pr_fmt("[OPPO_CHG][%s]"fmt), __func__, ##__VA_ARGS__); \
                } \
        } while (0)

void oppo_chg_turn_off_charging(struct oppo_chg_chip *chip);
void oppo_chg_turn_on_charging(struct oppo_chg_chip *chip);

static void oppo_chg_variables_init(struct oppo_chg_chip *chip);
static void oppo_chg_update_work(struct work_struct *work);
static void oppo_chg_protection_check(struct oppo_chg_chip *chip);
static void oppo_chg_get_battery_data(struct oppo_chg_chip *chip);
static void oppo_chg_check_tbatt_status(struct oppo_chg_chip *chip);
static void oppo_chg_get_chargerid_voltage(struct oppo_chg_chip *chip);
static void oppo_chg_set_input_current_limit(struct oppo_chg_chip *chip);
static void oppo_chg_battery_update_status(struct oppo_chg_chip *chip);

#ifdef  CONFIG_FB
static int fb_notifier_callback(struct notifier_block *nb, unsigned long event, void *data);
#endif
/****************************************/
static int reset_mcu_delay = 0;
static bool vbatt_higherthan_4180mv = false;
static bool vbatt_lowerthan_3300mv = false;

enum power_supply_property oppo_usb_props[] = {
        POWER_SUPPLY_PROP_ONLINE,
        POWER_SUPPLY_PROP_OTG_SWITCH,
        POWER_SUPPLY_PROP_OTG_ONLINE,
};


enum power_supply_property oppo_ac_props[] = {
        POWER_SUPPLY_PROP_ONLINE,
#ifdef CONFIG_OPPO_FAST2NORMAL_CHG
        POWER_SUPPLY_PROP_FAST2NORMAL_CHG,
#endif
};

enum power_supply_property oppo_batt_props[] = {
        POWER_SUPPLY_PROP_STATUS,
        POWER_SUPPLY_PROP_HEALTH,
        POWER_SUPPLY_PROP_PRESENT,
        POWER_SUPPLY_PROP_TECHNOLOGY,
        POWER_SUPPLY_PROP_CAPACITY,
        POWER_SUPPLY_PROP_TEMP,
        POWER_SUPPLY_PROP_VOLTAGE_NOW,
		POWER_SUPPLY_PROP_VOLTAGE_MIN,
        POWER_SUPPLY_PROP_CURRENT_NOW,
        POWER_SUPPLY_PROP_CHARGE_NOW,
        POWER_SUPPLY_PROP_AUTHENTICATE,
        POWER_SUPPLY_PROP_CHARGE_TIMEOUT,
        POWER_SUPPLY_PROP_CHARGE_TECHNOLOGY,
        POWER_SUPPLY_PROP_FAST_CHARGE,
        POWER_SUPPLY_PROP_MMI_CHARGING_ENABLE,        /*add for MMI_CHG_TEST*/
#ifdef CONFIG_OPPO_CHARGER_MTK
        POWER_SUPPLY_PROP_STOP_CHARGING_ENABLE,
        POWER_SUPPLY_PROP_CHARGE_COUNTER,
        POWER_SUPPLY_PROP_CURRENT_MAX,
#endif
#ifndef CONFIG_OPPO_SDM670_CHARGER
        POWER_SUPPLY_PROP_CHARGE_FULL,
#endif
        POWER_SUPPLY_PROP_BATTERY_FCC,
        POWER_SUPPLY_PROP_BATTERY_SOH,
        POWER_SUPPLY_PROP_BATTERY_CC,
        POWER_SUPPLY_PROP_BATTERY_RM,
        POWER_SUPPLY_PROP_BATTERY_NOTIFY_CODE,
        POWER_SUPPLY_PROP_ADAPTER_FW_UPDATE,
        POWER_SUPPLY_PROP_VOOCCHG_ING,
#ifdef CONFIG_OPPO_CHECK_CHARGERID_VOLT
        POWER_SUPPLY_PROP_CHARGERID_VOLT,
#endif
#ifdef CONFIG_OPPO_SHIP_MODE_SUPPORT
        POWER_SUPPLY_PROP_SHIP_MODE,
#endif
#ifdef CONFIG_OPPO_CALL_MODE_SUPPORT
        POWER_SUPPLY_PROP_CALL_MODE,
#endif
#ifdef CONFIG_OPPO_SHORT_C_BATT_CHECK
#ifdef CONFIG_OPPO_SHORT_USERSPACE
        POWER_SUPPLY_PROP_SHORT_C_LIMIT_CHG,
        POWER_SUPPLY_PROP_SHORT_C_LIMIT_RECHG,
        POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
        POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED,
#else
        POWER_SUPPLY_PROP_SHORT_C_BATT_UPDATE_CHANGE,
        POWER_SUPPLY_PROP_SHORT_C_BATT_IN_IDLE,
        POWER_SUPPLY_PROP_SHORT_C_BATT_CV_STATUS,
#endif /*CONFIG_OPPO_SHORT_USERSPACE*/
#endif
#ifdef CONFIG_OPPO_SHORT_HW_CHECK
        POWER_SUPPLY_PROP_SHORT_C_HW_FEATURE,
        POWER_SUPPLY_PROP_SHORT_C_HW_STATUS,
#endif
#ifdef CONFIG_OPPO_SHORT_IC_CHECK
        POWER_SUPPLY_PROP_SHORT_C_IC_OTP_STATUS,
        POWER_SUPPLY_PROP_SHORT_C_IC_VOLT_THRESH,
        POWER_SUPPLY_PROP_SHORT_C_IC_OTP_VALUE,
#endif
};

#ifdef CONFIG_OPPO_CHARGER_MTK
int oppo_usb_get_property(struct power_supply *psy,
        enum power_supply_property psp,
        union power_supply_propval *val)
{
        int ret = 0;
    
        //struct oppo_chg_chip *chip = container_of(psy->desc, struct oppo_chg_chip, usb_psd);
		struct oppo_chg_chip *chip = g_charger_chip;

        if (chip->charger_exist) {
            if ((chip->charger_type == POWER_SUPPLY_TYPE_USB || chip->charger_type == POWER_SUPPLY_TYPE_USB_CDP) && chip->stop_chg == 1) {
                chip->usb_online = true;
                chip->usb_psd.type = POWER_SUPPLY_TYPE_USB;
                }
        } else {
                chip->usb_online = false;
        }

        switch (psp) {
        case POWER_SUPPLY_PROP_CURRENT_MAX:
		        val->intval = 500000;
		        break;
	    case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		        val->intval = 5000000;
		        break;
        case POWER_SUPPLY_PROP_ONLINE:
                val->intval = chip->usb_online;
                break;                                           
        case POWER_SUPPLY_PROP_OTG_SWITCH:
                val->intval = chip->otg_switch;
                break;
        case POWER_SUPPLY_PROP_OTG_ONLINE:
                val->intval = chip->otg_online;
                break;
        default:
                pr_err("get prop %d is not supported in usb\n", psp);
                ret = -EINVAL;
                break;
        }
        return ret;
}

int oppo_usb_property_is_writeable(struct power_supply *psy,
        enum power_supply_property psp)
{
        int ret = 0;
        switch (psp) {
        case POWER_SUPPLY_PROP_OTG_SWITCH:
                return 1;
        default:
                pr_err("writeable prop %d is not supported in usb\n", psp);
                ret = -EINVAL;
                break;
        }

        return 0;
}

void __attribute__((weak)) oppo_set_otg_switch_status(bool value)
{
	return;
}

int oppo_usb_set_property(struct power_supply *psy,
        enum power_supply_property psp,
        const union power_supply_propval *val)
{
        int ret = 0;
        //struct oppo_chg_chip *chip = container_of(psy->desc, struct oppo_chg_chip, usb_psd);
		struct oppo_chg_chip *chip = g_charger_chip;

        switch (psp) {
        case POWER_SUPPLY_PROP_OTG_SWITCH:
                if (val->intval == 1) {
                        chip->otg_switch = true;
                        oppo_set_otg_switch_status(true);
                } else {
                        chip->otg_switch = false;
                        chip->otg_online = false;
                        oppo_set_otg_switch_status(false);
                }
                charger_xlog_printk(CHG_LOG_CRTI, "otg_switch: %d\n", chip->otg_switch);
                break;

        default:
                pr_err("set prop %d is not supported in usb\n", psp);
                ret = -EINVAL;
                break;
        }
        return ret;
}

static void usb_update(struct oppo_chg_chip *chip)
{
        if (chip->charger_exist) {
                /*if (chip->charger_type==STANDARD_HOST || chip->charger_type==CHARGING_HOST) {*/
                if (chip->charger_type == POWER_SUPPLY_TYPE_USB || chip->charger_type == POWER_SUPPLY_TYPE_USB_CDP) {
                        chip->usb_online = true;
            chip->usb_psd.type = POWER_SUPPLY_TYPE_USB;
                }
        } else {
                chip->usb_online = false;
        }
        power_supply_changed(chip->usb_psy);
}
#endif

int oppo_ac_get_property(struct power_supply *psy,
        enum power_supply_property psp,
        union power_supply_propval *val)
{
        int ret = 0;
        //struct oppo_chg_chip *chip = container_of(psy->desc, struct oppo_chg_chip, ac_psd);
		struct oppo_chg_chip *chip = g_charger_chip;

        if (chip->charger_exist) {
                if ((chip->charger_type == POWER_SUPPLY_TYPE_USB_DCP) || (oppo_vooc_get_fastchg_started() == true)
                        || (oppo_vooc_get_fastchg_to_normal() == true) || (oppo_vooc_get_fastchg_to_warm() == true)
                        || (oppo_vooc_get_adapter_update_status() == ADAPTER_FW_NEED_UPDATE) || (oppo_vooc_get_btb_temp_over() == true)) {
                        chip->ac_online = true;
                } else {
                        chip->ac_online = false;
                }
        } else {
                if ((oppo_vooc_get_fastchg_started() == true) || (oppo_vooc_get_fastchg_to_normal() == true)
                        || (oppo_vooc_get_fastchg_to_warm() == true) || (oppo_vooc_get_adapter_update_status() == ADAPTER_FW_NEED_UPDATE)
                        || (oppo_vooc_get_btb_temp_over() == true) || chip->mmi_fastchg == 0) {
						chip->ac_online = true;
                } else {
                        chip->ac_online = false;
						
                }
        }
        switch (psp) {
        case POWER_SUPPLY_PROP_ONLINE:
                val->intval = chip->ac_online;
                break;
#ifdef CONFIG_OPPO_FAST2NORMAL_CHG
        case POWER_SUPPLY_PROP_FAST2NORMAL_CHG:
                if (oppo_vooc_get_fastchg_to_normal() == true || oppo_vooc_get_fastchg_to_warm() == true
                        || oppo_vooc_get_btb_temp_over() == true || oppo_vooc_get_fastchg_low_temp_full() == true) {
                        val->intval = 1;
                } else {
                        val->intval = 0;
                }
                break;
#endif
        default:
                pr_err("get prop %d is not supported in ac\n", psp);
                ret = -EINVAL;
                break;
        }
        if (chip->ac_online) {
                charger_xlog_printk(CHG_LOG_CRTI, "chg_exist:%d, ac_online:%d\n", chip->charger_exist, chip->ac_online);
        }
		
        return ret;
}


int oppo_battery_property_is_writeable(struct power_supply *psy,
        enum power_supply_property psp)
{
        int rc = 0;

        switch (psp) {
        case POWER_SUPPLY_PROP_MMI_CHARGING_ENABLE:
                rc = 1;
                break;
#ifdef CONFIG_OPPO_CHARGER_MTK
        case POWER_SUPPLY_PROP_STOP_CHARGING_ENABLE:
                rc = 1;
                break;
#endif
#ifdef CONFIG_OPPO_SHIP_MODE_SUPPORT
        case POWER_SUPPLY_PROP_SHIP_MODE:
                rc = 1;
                break;
#endif
#ifdef CONFIG_OPPO_CALL_MODE_SUPPORT
        case POWER_SUPPLY_PROP_CALL_MODE:
                rc = 1;
                break;
#endif
#ifdef CONFIG_OPPO_SHORT_C_BATT_CHECK
#ifdef CONFIG_OPPO_SHORT_USERSPACE
        case POWER_SUPPLY_PROP_SHORT_C_LIMIT_CHG:
        case POWER_SUPPLY_PROP_SHORT_C_LIMIT_RECHG:
#else
        case POWER_SUPPLY_PROP_SHORT_C_BATT_UPDATE_CHANGE:
        case POWER_SUPPLY_PROP_SHORT_C_BATT_IN_IDLE:
#endif /*CONFIG_OPPO_SHORT_USERSPACE*/
                rc = 1;
                break;
#endif
#ifdef CONFIG_OPPO_SHORT_HW_CHECK
		case POWER_SUPPLY_PROP_SHORT_C_HW_FEATURE:
                rc = 1;
                break;
#endif
#ifdef CONFIG_OPPO_SHORT_IC_CHECK
		case POWER_SUPPLY_PROP_SHORT_C_IC_VOLT_THRESH:
				rc = 1;
				break;
#endif
#ifdef CONFIG_OPPO_CHIP_SOC_NODE
	case POWER_SUPPLY_PROP_CHIP_SOC:
		rc = 1;
		break;
#endif
        default:
//                pr_err("writeable prop %d is not supported in batt\n", psp);
                rc = 0;
                break;
        }
        return rc;
}

int oppo_battery_set_property(struct power_supply *psy,
        enum power_supply_property psp,
        const union power_supply_propval *val)
{
        int ret = 0;
        //struct oppo_chg_chip *chip = container_of(psy->desc, struct oppo_chg_chip, battery_psd);
		struct oppo_chg_chip *chip = g_charger_chip;

        switch (psp) {
        case POWER_SUPPLY_PROP_MMI_CHARGING_ENABLE:
                charger_xlog_printk(CHG_LOG_CRTI, "set mmi_chg = [%d].\n", val->intval);
                if (val->intval == 0) {
                        if(chip->unwakelock_chg == 1) {
                            ret = -EINVAL;
                            charger_xlog_printk(CHG_LOG_CRTI, "unwakelock testing , this test not allowed.\n");
                        } else {
                            chip->mmi_chg = 0;
                            oppo_chg_turn_off_charging(chip);
                            if (oppo_vooc_get_fastchg_started() == true) {
                                    oppo_chg_set_chargerid_switch_val(0);
                                    oppo_vooc_switch_mode(NORMAL_CHARGER_MODE);
                                    chip->mmi_fastchg = 0;
                            }
                        }
		        } else {
                        if(chip->unwakelock_chg == 1) {
                            ret = -EINVAL;
                            charger_xlog_printk(CHG_LOG_CRTI, "unwakelock testing , this test not allowed.\n");
                        } else {
                            chip->mmi_chg = 1;
                            if (chip->mmi_fastchg == 0) {
                                    oppo_chg_clear_chargerid_info();
                            }
                            chip->mmi_fastchg = 1;
                            oppo_chg_turn_on_charging(chip);
                        }
                }
                break;
#ifdef CONFIG_OPPO_CHARGER_MTK
		case POWER_SUPPLY_PROP_STOP_CHARGING_ENABLE:
			charger_xlog_printk(CHG_LOG_CRTI, "set stop_chg = [%d].\n", val->intval);
			if (val->intval == 0) {
				chip->stop_chg = 0;
			} else {
				chip->stop_chg = 1;
			}
#ifdef ODM_WT_EDIT
//Sidong.Zhao@ODM_WT.BSP.CHG, 2019/12/2, implement stop charge
				oppo_chg_turn_on_charging(chip);
#endif /*ODM_WT_EDIT*/
                break;
#endif
#ifdef CONFIG_OPPO_SHIP_MODE_SUPPORT
        case POWER_SUPPLY_PROP_SHIP_MODE:
                chip->enable_shipmode = val->intval;
				oppo_gauge_update_soc_smooth_parameter();
                break;
#endif
#ifdef CONFIG_OPPO_CALL_MODE_SUPPORT
        case POWER_SUPPLY_PROP_CALL_MODE:
                chip->calling_on = val->intval;
                break;
#endif
#ifdef CONFIG_OPPO_SHORT_C_BATT_CHECK
#ifdef CONFIG_OPPO_SHORT_USERSPACE
		case POWER_SUPPLY_PROP_SHORT_C_LIMIT_CHG:
				printk(KERN_ERR "[OPPO_CHG] [short_c_bat] set limit chg[%d]\n", !!val->intval);
				chip->short_c_batt.limit_chg = !!val->intval;
				if (!!val->intval == 0)//for userspace logic
					chip->short_c_batt.is_switch_on = 0;
				break;

		case POWER_SUPPLY_PROP_SHORT_C_LIMIT_RECHG:
				printk(KERN_ERR "[OPPO_CHG] [short_c_bat] set limit rechg[%d]\n", !!val->intval);
				chip->short_c_batt.limit_rechg = !!val->intval;
				break;
#else
		case POWER_SUPPLY_PROP_SHORT_C_BATT_UPDATE_CHANGE:
				printk(KERN_ERR "[OPPO_CHG] [short_c_batt]: set update change[%d]\n", val->intval);
				oppo_short_c_batt_update_change(chip, val->intval);
				chip->short_c_batt.update_change = val->intval;
			break;

		case POWER_SUPPLY_PROP_SHORT_C_BATT_IN_IDLE:
				printk(KERN_ERR "[OPPO_CHG] [short_c_batt]: set in idle[%d]\n", !!val->intval);
				chip->short_c_batt.in_idle = !!val->intval;
			break;
#endif /*CONFIG_OPPO_SHORT_USERSPACE*/
#endif /* CONFIG_OPPO_SHORT_C_BATT_CHECK */
#ifdef CONFIG_OPPO_SHORT_HW_CHECK
        case POWER_SUPPLY_PROP_SHORT_C_HW_FEATURE:
            printk(KERN_ERR "[OPPO_CHG] [short_c_hw_check]: set is_feature_hw_on [%d]\n", val->intval);
            chip->short_c_batt.is_feature_hw_on = val->intval;
        break;
#endif /* CONFIG_OPPO_SHORT_C_BATT_CHECK */
#ifdef CONFIG_OPPO_SHORT_IC_CHECK
		case POWER_SUPPLY_PROP_SHORT_C_IC_VOLT_THRESH:
			if (chip) {
				chip->short_c_batt.ic_volt_threshold = val->intval;
				oppo_short_ic_set_volt_threshold(chip);
				//pr_err("%s:[OPPO_CHG][oppo_short_ic],ic_volt_threshold val->intval[%d]\n", __FUNCTION__, val->intval);
			}
			break;
#endif

        default:
                pr_err("set prop %d is not supported in batt\n", psp);
                ret = -EINVAL;
                break;
        }
        return ret;
}

int oppo_battery_get_property(struct power_supply *psy,
        enum power_supply_property psp,
        union power_supply_propval *val)
{
        int ret = 0;
        //struct oppo_chg_chip *chip = container_of(psy->desc, struct oppo_chg_chip, battery_psd);
		struct oppo_chg_chip *chip = g_charger_chip;

        switch (psp) {
        case POWER_SUPPLY_PROP_STATUS:
                if (oppo_chg_show_vooc_logo_ornot() == 1) {
                        val->intval = POWER_SUPPLY_STATUS_CHARGING;
                } else if (!chip->authenticate) {
                        val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
                } else {
                        val->intval = chip->prop_status;
                }
                break;
        case POWER_SUPPLY_PROP_HEALTH:
                val->intval = oppo_chg_get_prop_batt_health(chip);
                break;
        case POWER_SUPPLY_PROP_PRESENT:
                val->intval = chip->batt_exist;
                break;
        case POWER_SUPPLY_PROP_TECHNOLOGY:
                val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
                break;
        case POWER_SUPPLY_PROP_CAPACITY:
                val->intval = chip->ui_soc;
                break;
#ifdef CONFIG_OPPO_CHIP_SOC_NODE
	case POWER_SUPPLY_PROP_CHIP_SOC:
		val->intval = chip->soc;
		break;
#endif
        case POWER_SUPPLY_PROP_VOLTAGE_NOW:
#ifdef CONFIG_OPPO_CHARGER_MTK
                val->intval = chip->batt_volt;
#else
                val->intval = chip->batt_volt * 1000;
#endif
                break;
		case POWER_SUPPLY_PROP_VOLTAGE_MIN:
				val->intval = chip->batt_volt_min * 1000;
				break; 
        case POWER_SUPPLY_PROP_CURRENT_NOW:
                if (oppo_vooc_get_fastchg_started() == true) {
                    chip->icharging = oppo_gauge_get_prev_batt_current();
                } else {
                    chip->icharging = oppo_gauge_get_batt_current();
                }
                val->intval = chip->icharging;
                break;
        case POWER_SUPPLY_PROP_TEMP:
                val->intval = chip->temperature - chip->offset_temp;
                break;
        case POWER_SUPPLY_PROP_CHARGE_NOW:
				if (oppo_vooc_get_fastchg_started() == true
                    && (chip->vbatt_num == 2)
                    && oppo_vooc_get_fast_chg_type() != CHARGER_SUBTYPE_FASTCHG_VOOC) {       
					chip->charger_volt = 10000;
				}
				val->intval = chip->charger_volt;
                break;
        case POWER_SUPPLY_PROP_AUTHENTICATE:
                val->intval = chip->authenticate;
                break;
        case POWER_SUPPLY_PROP_CHARGE_TIMEOUT:
                val->intval = chip->chging_over_time;
                break;
        case POWER_SUPPLY_PROP_CHARGE_TECHNOLOGY:
                val->intval = chip->vooc_project;
                break;
        case POWER_SUPPLY_PROP_FAST_CHARGE:
#ifdef ODM_WT_EDIT
//Junbo.Guo@ODM_WT.BSP.CHG, 2019/11/11, Modify for fastcharger mode
			if (!chip->chg_ops->get_charger_subtype){
			     val->intval = 0;
			 }else{
				if(chip->chg_ops->get_charger_subtype()!=CHARGER_SUBTYPE_DEFAULT){
					val->intval = 1;
				}else{
					val->intval = 0;
				}
			 }
#else
                val->intval = oppo_chg_show_vooc_logo_ornot();
#ifdef CONFIG_OPPO_CHARGER_MTK
                if (val->intval) {
                        charger_xlog_printk(CHG_LOG_CRTI, "vooc_logo:%d\n", val->intval);
                }
#endif
#endif
                break;
        case POWER_SUPPLY_PROP_MMI_CHARGING_ENABLE:        /*add for MMI_CHG TEST*/
                val->intval = chip->mmi_chg;
                break;
#ifdef CONFIG_OPPO_CHARGER_MTK
		case POWER_SUPPLY_PROP_STOP_CHARGING_ENABLE:
			val->intval = chip->stop_chg;
			break;
		case POWER_SUPPLY_PROP_CHARGE_COUNTER:
			val->intval = chip->ui_soc * chip->batt_capacity_mah * 1000 / 100;
			break;
		case POWER_SUPPLY_PROP_CURRENT_MAX:
			val->intval = 2000;
			break;
#endif
#ifndef CONFIG_OPPO_SDM670_CHARGER
		case POWER_SUPPLY_PROP_CHARGE_FULL:
			val->intval = chip->batt_capacity_mah * 1000;
			break;
#endif
        case POWER_SUPPLY_PROP_BATTERY_FCC:
                val->intval = chip->batt_fcc;
                break;
        case POWER_SUPPLY_PROP_BATTERY_SOH:
                val->intval = chip->batt_soh;
                break;
        case POWER_SUPPLY_PROP_BATTERY_CC:
                val->intval = chip->batt_cc;
                break;
        case POWER_SUPPLY_PROP_BATTERY_RM:
                val->intval = chip->batt_rm;
                break;
        case POWER_SUPPLY_PROP_BATTERY_NOTIFY_CODE:
                val->intval = chip->notify_code;
                break;
        case POWER_SUPPLY_PROP_ADAPTER_FW_UPDATE:
                val->intval = oppo_vooc_get_adapter_update_status();
                break;
        case POWER_SUPPLY_PROP_VOOCCHG_ING:
                val->intval = oppo_vooc_get_fastchg_ing();
                break;
#ifdef CONFIG_OPPO_CHECK_CHARGERID_VOLT
        case POWER_SUPPLY_PROP_CHARGERID_VOLT:
                val->intval = chip->chargerid_volt;
                break;
#endif
#ifdef CONFIG_OPPO_SHIP_MODE_SUPPORT
        case POWER_SUPPLY_PROP_SHIP_MODE:
                val->intval = chip->enable_shipmode;
                break;
#endif
#ifdef CONFIG_OPPO_CALL_MODE_SUPPORT
        case POWER_SUPPLY_PROP_CALL_MODE:
                val->intval = chip->calling_on;
                break;
#endif
#ifdef CONFIG_OPPO_SHORT_C_BATT_CHECK
#ifdef CONFIG_OPPO_SHORT_USERSPACE
        case POWER_SUPPLY_PROP_SHORT_C_LIMIT_CHG:
                val->intval = (int)chip->short_c_batt.limit_chg;
                break;

        case POWER_SUPPLY_PROP_SHORT_C_LIMIT_RECHG:
                val->intval = (int)chip->short_c_batt.limit_rechg;
                break;

        case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
                val->intval = chip->limits.iterm_ma;
                break;

        case POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED:
                val->intval = 2000;
                if (chip && chip->chg_ops->get_dyna_aicl_result) {
                        val->intval = chip->chg_ops->get_dyna_aicl_result();
                }
                break;
#else
        case POWER_SUPPLY_PROP_SHORT_C_BATT_UPDATE_CHANGE:
                val->intval = chip->short_c_batt.update_change;
                break;

        case POWER_SUPPLY_PROP_SHORT_C_BATT_IN_IDLE:
                val->intval = (int)chip->short_c_batt.in_idle;
                break;

        case POWER_SUPPLY_PROP_SHORT_C_BATT_CV_STATUS:
                val->intval = (int)oppo_short_c_batt_get_cv_status(chip);
                break;
#endif /*CONFIG_OPPO_SHORT_USERSPACE*/
#endif /* CONFIG_OPPO_SHORT_C_BATT_CHECK */
#ifdef CONFIG_OPPO_SHORT_HW_CHECK
        case POWER_SUPPLY_PROP_SHORT_C_HW_FEATURE:
                val->intval = chip->short_c_batt.is_feature_hw_on;
                break;

        case POWER_SUPPLY_PROP_SHORT_C_HW_STATUS:
                val->intval = chip->short_c_batt.shortc_gpio_status;
                break;
#endif /* CONFIG_OPPO_SHORT_C_BATT_CHECK */
#ifdef CONFIG_OPPO_SHORT_IC_CHECK
		case POWER_SUPPLY_PROP_SHORT_C_IC_OTP_STATUS:
			if (chip) {
				val->intval = chip->short_c_batt.ic_short_otp_st;
			}
#ifdef ODM_WT_EDIT
//Junbo.Guo@ODM_WT.BSP.CHG, 2019/11/27, Modify for shortic
			val->intval = 1;
#endif
			break;
		case POWER_SUPPLY_PROP_SHORT_C_IC_VOLT_THRESH:
			if (chip) {
				val->intval = chip->short_c_batt.ic_volt_threshold;
			}
			break;
		case POWER_SUPPLY_PROP_SHORT_C_IC_OTP_VALUE:
			if (chip) {
				val->intval = oppo_short_ic_get_otp_error_value(chip);
			}
			break;
#endif
#ifdef ODM_WT_EDIT
/*Shouli.Wang@ODM_WT.BSP.CHG 2019/11/25, add for battery info node*/
		case POWER_SUPPLY_PROP_BAT_ID_VOLT:
			val->intval = battery_id_vol;
			break;
		case POWER_SUPPLY_PROP_BAT_TYPE:
			val->intval = bat_id_num;
			break;
#endif
        default:
                pr_err("get prop %d is not supported in batt\n", psp);
                ret = -EINVAL;
                break;
        }

        return ret;
}


static ssize_t proc_tbatt_pwroff_write(struct file *filp, const char __user *buf, size_t len, loff_t *data)
{
        char buffer[2] = {0};

        if (len > 2) {
            return -EFAULT;
        }

        if (copy_from_user(buffer, buf, 2)) {
            chg_err("%s:  error.\n", __func__);
            return -EFAULT;
        }

        if (buffer[0] == '0') {
                tbatt_pwroff_enable = 0;
        } else {
                tbatt_pwroff_enable = 1;
                oppo_tbatt_power_off_task_wakeup();
        }
        chg_err("%s:tbatt_pwroff_enable = %d.\n", __func__, tbatt_pwroff_enable);

        return len;
}
static ssize_t proc_tbatt_pwroff_read(struct file *filp, char __user *buff, size_t count, loff_t *off)
{
        char page[256] = {0};
        char read_data[3] = {0};
        int len = 0;

        if (tbatt_pwroff_enable == 1) {
                read_data[0] = '1';
        } else {
                read_data[0] = '0';
        }
        read_data[1] = '\0';
        len = sprintf(page, "%s", read_data);
        if (len > *off) {
                len -= *off;
        } else {
                len = 0;
        }
        if (copy_to_user(buff, page, (len < count ? len : count))) {
                return -EFAULT;
        }
        *off += len < count ? len : count;
        return (len < count ? len : count);
}

static const struct file_operations tbatt_pwroff_proc_fops = {
        .write = proc_tbatt_pwroff_write,
        .read = proc_tbatt_pwroff_read,
};

static int init_proc_tbatt_pwroff(void)
{
        struct proc_dir_entry *p = NULL;

        p = proc_create("tbatt_pwroff", 0664, NULL, &tbatt_pwroff_proc_fops);
        if (!p) {
                chg_err("proc_create  fail!\n");
        }
        return 0;
}


static ssize_t chg_log_write(struct file *filp, const char __user *buff, size_t len, loff_t *data)
{
        char write_data[32] = {0};

        if (copy_from_user(&write_data, buff, len)) {
                chg_err("bat_log_write error.\n");
                return -EFAULT;
        }

        if (write_data[0] == '1') {
                charger_xlog_printk(CHG_LOG_CRTI, "enable battery driver log system\n");
                enable_charger_log = 1;
        } else if ((write_data[0] >= '2') &&(write_data[0] <= '9')) {
                charger_xlog_printk(CHG_LOG_CRTI, "enable battery driver log system:2\n");
                enable_charger_log = 2;
        } else {
                charger_xlog_printk(CHG_LOG_CRTI, "Disable battery driver log system\n");
                enable_charger_log = 0;
        }

        return len;
}
static ssize_t chg_log_read(struct file *filp, char __user *buff, size_t count, loff_t *off)
{
        char page[256] = {0};
        char read_data[32] = {0};
        int len = 0;

        if (enable_charger_log == 1) {
                read_data[0] = '1';
        } else if (enable_charger_log == 2) {
                read_data[0] = '2';
        } else {
                read_data[0] = '0';
        }
        len = sprintf(page, "%s", read_data);
        if (len > *off) {
                len -= *off;
        } else {
                len = 0;
        }
        if (copy_to_user(buff, page, (len < count ? len : count))) {
                return -EFAULT;
        }
        *off += len < count ? len : count;
        return (len < count ? len : count);
}

static const struct file_operations chg_log_proc_fops = {
        .write = chg_log_write,
        .read = chg_log_read,
};

static int init_proc_chg_log(void)
{
        struct proc_dir_entry *p = NULL;

        p = proc_create("charger_log", 0664, NULL, &chg_log_proc_fops);
        if (!p) {
                chg_err("proc_create chg_log_proc_fops fail!\n");
        }
        return 0;
}
static void oppo_chg_set_awake(struct oppo_chg_chip *chip, bool awake);
static ssize_t chg_cycle_write(struct file *file, const char __user *buff, size_t count, loff_t *ppos)
{
        char proc_chg_cycle_data[16];

        if(count >= 16) {
            count = 16;
        }

        if (copy_from_user(&proc_chg_cycle_data, buff, count)) {
                chg_err("chg_cycle_write error.\n");
                return -EFAULT;
        }

        if (strncmp(proc_chg_cycle_data, "en808", 5) == 0) {
                if(g_charger_chip->unwakelock_chg == 1) {
                    charger_xlog_printk(CHG_LOG_CRTI, "unwakelock testing , this test not allowed.\n");
                    return -EPERM;
                }
                charger_xlog_printk(CHG_LOG_CRTI, "allow charging.\n");
                g_charger_chip->chg_ops->charger_unsuspend();
                g_charger_chip->chg_ops->charging_enable();
#ifdef ODM_WT_EDIT
//Junbo.Guo@ODM_WT.BSP.CHG, 2019/11/11, Modify for subcharger
				if(g_charger_chip->is_double_charger_support){
					g_charger_chip->sub_chg_ops->charger_unsuspend();
					g_charger_chip->sub_chg_ops->charging_enable();	
				}
#endif
                g_charger_chip->mmi_chg = 1;
				g_charger_chip->stop_chg= 1;
		oppo_chg_set_input_current_limit(g_charger_chip);
        } else if (strncmp(proc_chg_cycle_data, "dis808", 6) == 0) {
                if(g_charger_chip->unwakelock_chg == 1) {
                    charger_xlog_printk(CHG_LOG_CRTI, "unwakelock testing , this test not allowed.\n");
                    return -EPERM;
                }
                charger_xlog_printk(CHG_LOG_CRTI, "not allow charging.\n");
                g_charger_chip->chg_ops->charging_disable();
                g_charger_chip->chg_ops->charger_suspend();
#ifdef ODM_WT_EDIT
				//Junbo.Guo@ODM_WT.BSP.CHG, 2019/11/11, Modify for subcharger
				if(g_charger_chip->is_double_charger_support){
					g_charger_chip->sub_chg_ops->charging_disable();
					g_charger_chip->sub_chg_ops->charger_suspend(); 
				}
#endif

                g_charger_chip->mmi_chg = 0;
				g_charger_chip->stop_chg= 0;
		} else if (strncmp(proc_chg_cycle_data, "wakelock", 8) == 0) {
				charger_xlog_printk(CHG_LOG_CRTI, "set wakelock.\n");
                g_charger_chip->unwakelock_chg = 0;
				oppo_chg_set_awake(g_charger_chip, true);
                g_charger_chip->chg_ops->charger_unsuspend();
                g_charger_chip->chg_ops->charging_enable();
#ifdef ODM_WT_EDIT
				//Junbo.Guo@ODM_WT.BSP.CHG, 2019/11/11, Modify for subcharger
				if(g_charger_chip->is_double_charger_support){
					g_charger_chip->sub_chg_ops->charger_unsuspend();
					g_charger_chip->sub_chg_ops->charging_enable(); 
				}
#endif

                g_charger_chip->mmi_chg = 1;
				g_charger_chip->stop_chg= 1;
		} else if (strncmp(proc_chg_cycle_data, "unwakelock", 10) == 0) {
				charger_xlog_printk(CHG_LOG_CRTI, "set unwakelock.\n");
                g_charger_chip->chg_ops->charging_disable();
                //g_charger_chip->chg_ops->charger_suspend();
#ifdef ODM_WT_EDIT
				//Junbo.Guo@ODM_WT.BSP.CHG, 2019/11/11, Modify for subcharger
				if(g_charger_chip->is_double_charger_support){
					g_charger_chip->sub_chg_ops->charging_disable();
					//g_charger_chip->sub_chg_ops->charger_suspend(); 
				}
#endif
                g_charger_chip->mmi_chg = 0;
				g_charger_chip->stop_chg= 0;
                g_charger_chip->unwakelock_chg = 1;
				oppo_chg_set_awake(g_charger_chip, false);
		} else {
                return -EFAULT;
        }

        return count;
}

static const struct file_operations chg_cycle_proc_fops = {
        .write = chg_cycle_write,
        .llseek = noop_llseek,
};

static void init_proc_chg_cycle(void)
{
        if (!proc_create("charger_cycle", S_IWUSR | S_IWGRP | S_IWOTH, NULL, &chg_cycle_proc_fops)) {
                chg_err("proc_create chg_cycle_proc_fops fail!\n");
        }
}
static ssize_t critical_log_read(struct file *filp, char __user *buff, size_t count, loff_t *off)
{
        char page[256] = {0};
        char read_data[32] = {0};
        int len = 0;
/*        itoa(charger_abnormal_log, read_data, 10);*/
/*        sprintf(read_data,"%s",charger_abnormal_log);*/
        if (charger_abnormal_log >= 10) {
                charger_abnormal_log = 10;
        }
        read_data[0] = '0' + charger_abnormal_log % 10;
        len = sprintf(page, "%s", read_data);
        if (len > *off) {
                len -= *off;
        } else {
                len = 0;
        }
        if (copy_to_user(buff, page, (len < count ? len : count))) {
                return -EFAULT;
        }
        *off += len < count ? len : count;
        return (len < count ? len : count);
}

static ssize_t critical_log_write(struct file *filp, const char __user *buff, size_t len, loff_t *data)
{
        char write_data[32] = {0};
        int critical_log = 0;
        if (copy_from_user(&write_data, buff, len)) {
                pr_err("bat_log_write error.\n");
                return -EFAULT;
        }
/*        critical_log = atoi(write_data);*/
/*        sprintf(critical_log,"%d",(void *)write_data);*/
        write_data[len] = '\0';
        if (write_data[len - 1] == '\n') {
                write_data[len - 1] = '\0';
        }
        critical_log = (int)simple_strtoul(write_data, NULL, 10);
/*        pr_err("%s:data=%s,critical_log=%d\n",__func__,write_data,critical_log);*/
        if (critical_log > 256) {
                critical_log = 256;
        }
        charger_abnormal_log = critical_log;

        return len;
}

static const struct file_operations chg_critical_log_proc_fops = {
        .write = critical_log_write,
        .read = critical_log_read,
};

static void init_proc_critical_log(void)
{
        struct proc_dir_entry *p = NULL;

        p = proc_create("charger_critical_log", 0664, NULL, &chg_critical_log_proc_fops);
        if (!p) {
                pr_err("proc_create chg_critical_log_proc_fops fail!\n");
        }
}

#ifdef CONFIG_OPPO_RTC_DET_SUPPORT
/* wenbin.liu add for det rtc reset */
static ssize_t rtc_reset_read(struct file *filp, char __user *buff, size_t count, loff_t *off)
{
        char page[256] = {0};
        char read_data[32] = {0};
        int len = 0;
        int rc = 0;

        if (!g_charger_chip) {
                return -EFAULT;
        } else {
                rc = g_charger_chip->chg_ops->check_rtc_reset();
        }
        if (rc < 0 || rc >1) {
                rc = 0;
        }
        read_data[0] = '0' + rc % 10;
        len = sprintf(page, "%s", read_data);
        if (len > *off) {
                len -= *off;
        } else {
                len = 0;
        }
        if (copy_to_user(buff, page, (len < count ? len : count))) {
                return -EFAULT;
        }
        *off += len < count ? len : count;
        return (len < count ? len : count);
}

static const struct file_operations rtc_reset_det_fops = {
        .read = rtc_reset_read,
};

static void init_proc_rtc_det(void)
{
        struct proc_dir_entry *p = NULL;

        p = proc_create("rtc_reset", 0664, NULL, &rtc_reset_det_fops);
        if (!p) {
                pr_err("proc_create rtc_reset_det_fops fail!\n");
        }
}

static ssize_t vbat_low_read(struct file *filp, char __user *buff, size_t count, loff_t *off)
{
        char page[256] = {0};
        char read_data[32] = {0};
        int len = 0;
        int rc = 0;

        if (!g_charger_chip)
                return -EFAULT;

        if (vbatt_lowerthan_3300mv) {
                rc = 1;
        }

        read_data[0] = '0' + rc % 10;
        len = sprintf(page,"%s",read_data); 
        if (len > *off) {
                len -= *off;
        } else {
                len = 0;
        }
        if (copy_to_user(buff,page,(len < count ? len : count))) {
                return -EFAULT;
        }
        *off += len < count ? len : count;
        return (len < count ? len : count); 
}


static const struct file_operations vbat_low_det_fops = {
        .read = vbat_low_read,
};

static void init_proc_vbat_low_det(void)
{
        struct proc_dir_entry *p = NULL;

        p = proc_create("vbat_low", 0664, NULL, &vbat_low_det_fops);
        if (!p) {
                pr_err("proc_create rtc_reset_det_fops fail!\n");
        }
}

#endif /* CONFIG_OPPO_RTC_DET_SUPPORT */

/*ye.zhang@BSP.Sensor.Function, 2017-03-30, add interface for charging special feature in different projects*/
static int charging_limit_time_show(struct seq_file *seq_filp, void *v)
{
        seq_printf(seq_filp, "%d\n", g_charger_chip->limits.max_chg_time_sec);
        return 0;
}
static int charging_limit_time_open(struct inode *inode, struct file *file)
{
        int ret;
        ret = single_open(file, charging_limit_time_show, NULL);
        return ret;
}

static ssize_t charging_limit_time_write(struct file *filp, const char __user *buff, size_t len, loff_t *data)
{
        int limit_time;
        char temp[16];
        if (copy_from_user(temp, buff, len)) {
                pr_err("charging_limit_time_write error.\n");
                return -EFAULT;
        }

        sscanf(temp, "%d", &limit_time);

        if (g_charger_chip) {
                g_charger_chip->limits.max_chg_time_sec = limit_time;
                printk(KERN_EMERG"charging_feature:max_chg_time_sec = %d\n", g_charger_chip->limits.max_chg_time_sec);
        }

        return len;
}

static const struct file_operations charging_limit_time_fops = {
        .open = charging_limit_time_open,
        .write = charging_limit_time_write,
        .read = seq_read,
};
static int charging_limit_current_show(struct seq_file *seq_filp, void *v)
{
	seq_printf(seq_filp, "%d\n", g_charger_chip->limits.input_current_led_ma_high);
	seq_printf(seq_filp, "%d\n", g_charger_chip->limits.input_current_led_ma_warm);
	seq_printf(seq_filp, "%d\n", g_charger_chip->limits.input_current_led_ma_normal);
	return 0;
}
static int charging_limit_current_open(struct inode *inode, struct file *file)
{
        int ret;
        ret = single_open(file, charging_limit_current_show, NULL);
        return ret;
}

static ssize_t charging_limit_current_write(struct file *filp, const char __user *buff, size_t len, loff_t *data)
{
        int limit_current;
        char temp[16];
        if (copy_from_user(temp, buff, len)) {
                pr_err("charging_limit_time_write error.\n");
                return -EFAULT;
        }
        sscanf(temp, "%d", &limit_current);

        if (g_charger_chip) {
		g_charger_chip->limits.input_current_led_ma_high = limit_current;
		g_charger_chip->limits.input_current_led_ma_warm = limit_current;
		g_charger_chip->limits.input_current_led_ma_normal = limit_current;
		printk(KERN_EMERG"charging_feature:limit_current = %d\n",limit_current);
        }
        return len;
}

static const struct file_operations charging_limit_current_fops = {
        .open = charging_limit_current_open,
        .write = charging_limit_current_write,
        .read = seq_read,
};

static void init_proc_charging_feature(void)
{
        struct proc_dir_entry *p_time = NULL;
        struct proc_dir_entry *p_current = NULL;

        p_time = proc_create("charging_limit_time", 0664, NULL, &charging_limit_time_fops);
        if (!p_time) {
                pr_err("proc_create charging_feature_fops fail!\n");
        }
        p_current = proc_create("charging_limit_current", 0664, NULL, &charging_limit_current_fops);
        if (!p_current) {
                pr_err("proc_create charging_feature_fops fail!\n");
        }
}
/*ye.zhang add end*/

#ifdef ODM_WT_EDIT
/*Shouli.Wang@ODM_WT.BSP.CHG 2019/11/05, add for batt_param_noplug proc*/
static ssize_t proc_batt_param_noplug_write(struct file *filp, const char __user *buf, size_t len, loff_t *data)
{
	return len;
}

static ssize_t proc_batt_param_noplug_read(struct file *filp, char __user *buff, size_t count, loff_t *off)
{
	char page[256] = {0};
	char read_data[128] = {0};
	int len = 0;
	
	sprintf(read_data, "%d %d %d", noplug_temperature,
			noplug_batt_volt_max, noplug_batt_volt_min);
	len = sprintf(page, "%s", read_data);
	if (len > *off) {
		len -= *off;
	} else {
		len = 0;
	}
	if (copy_to_user(buff, page, (len < count ? len : count))) {
				return -EFAULT;
	}
	*off += len < count ? len : count;
	return (len < count ? len : count);
}

static const struct file_operations batt_param_noplug_proc_fops = {
		.write = proc_batt_param_noplug_write,
		.read = proc_batt_param_noplug_read,
};

static int init_proc_batt_param_noplug(void)
{
	struct proc_dir_entry *p = NULL;
	
	p = proc_create("batt_param_noplug", 0664, NULL, &batt_param_noplug_proc_fops);
	if(!p) {
		chg_err("creat batt_param_noplug proc fail\n");
	}
	return 0;
}
#endif /*ODM_WT_EDIT*/

static void mmi_adapter_in_work_func(struct work_struct *work)
{
        struct delayed_work *dwork = to_delayed_work(work);
        struct oppo_chg_chip *chip = container_of(dwork, struct oppo_chg_chip, mmi_adapter_in_work);
        chip->mmi_fastchg = 1;
        charger_xlog_printk(CHG_LOG_CRTI, "  mmi_fastchg\n");
}
static void oppo_mmi_fastchg_in(struct oppo_chg_chip *chip)
{
	charger_xlog_printk(CHG_LOG_CRTI, "  call\n");
	schedule_delayed_work(&chip->mmi_adapter_in_work,
				round_jiffies_relative(msecs_to_jiffies(2000)));
}


static void oppo_chg_awake_init(struct oppo_chg_chip *chip)
{
	if (!chip)
		return;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0))
	wake_lock_init(&chip->suspend_lock, WAKE_LOCK_SUSPEND, "battery suspend wakelock");
#else
	chip->suspend_ws = wakeup_source_register("battery suspend wakelock");
#endif
}

static void oppo_chg_set_awake(struct oppo_chg_chip *chip, bool awake)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0))
	if (chip->unwakelock_chg == 1 && awake == true) {
		charger_xlog_printk(CHG_LOG_CRTI, "error, unwakelock testing, can not set wakelock.\n");
		return;
	}

	if (awake)
		wake_lock(&chip->suspend_lock);
	else
		wake_unlock(&chip->suspend_lock);
#else
	static bool pm_flag = false;

	if (chip->unwakelock_chg == 1 && awake == true) {
		charger_xlog_printk(CHG_LOG_CRTI, "error, unwakelock testing, can not set wakelock.\n");
		return;
	}

	if (!chip || !chip->suspend_ws)
		return;

	if (awake && !pm_flag) {
		pm_flag = true;
		__pm_stay_awake(chip->suspend_ws);
	} else if (!awake && pm_flag) {
		__pm_relax(chip->suspend_ws);
		pm_flag = false;
	}
#endif
}

static int __ref shortc_thread_main(void *data)
{
        struct oppo_chg_chip *chip = data;
        struct cred *new;
        int rc = 0;

        new = prepare_creds();
		if (!new) {
                chg_err("init err\n");
				rc = -1;
				return rc;
        }
        new->fsuid = new->euid = KUIDT_INIT(1000);
        commit_creds(new);

        while (!kthread_should_stop()) {
                set_current_state(TASK_RUNNING);
                oppo_chg_short_c_battery_check(chip);
                set_current_state(TASK_UNINTERRUPTIBLE);
                schedule();
        }
        return rc;
}
#ifdef ODM_WT_EDIT
//Junbo.Guo@ODM_WT.BSP.CHG, 2019/11/11, Modify for pe20
static int __ref FAST_charger_thread_main(void *data);
#endif
int oppo_chg_init(struct oppo_chg_chip *chip)
{
        int rc = 0;
        char *thread_name = "shortc_thread";

        struct power_supply *usb_psy;
        struct power_supply *batt_psy;
        struct power_supply *ac_psy;

        if (!chip->chg_ops) {
                dev_err(chip->dev, "charger operations cannot be NULL\n");
                return -1;
        }
        oppo_chg_variables_init(chip);
        oppo_chg_get_battery_data(chip);

        usb_psy = power_supply_get_by_name("usb");
        if (!usb_psy) {
                dev_err(chip->dev, "USB psy not found; deferring probe\n");
                /*return -EPROBE_DEFER;*/
                goto power_psy_reg_failed;
        }
        chip->usb_psy = usb_psy;


        ac_psy = power_supply_get_by_name("ac");
        if (!ac_psy) {
                dev_err(chip->dev, "ac psy not found; deferring probe\n");
                goto power_psy_reg_failed;
        }
        chip->ac_psy = ac_psy;



        batt_psy = power_supply_get_by_name("battery");
        if (!batt_psy) {
                dev_err(chip->dev, "battery psy not found; deferring probe\n");
                goto power_psy_reg_failed;
        }
        chip->batt_psy = batt_psy;


#ifndef CONFIG_OPPO_CHARGER_MTK
        chip->pmic_spmi.psy_registered = true;
#endif
        g_charger_chip = chip;
        oppo_chg_awake_init(chip);

        INIT_DELAYED_WORK(&chip->update_work, oppo_chg_update_work);

        chip->shortc_thread = kthread_create(shortc_thread_main, (void *)chip, thread_name);
        if (!chip->shortc_thread) {
                chg_err("Can't create shortc_thread\n");
                rc = -EPROBE_DEFER;
                goto power_psy_reg_failed;
        }

#ifdef ODM_WT_EDIT
		//Junbo.Guo@ODM_WT.BSP.CHG, 2019/11/11, Modify for pe20
		memset(&chip->ptime[0], 0, sizeof(struct timespec));
		memset(&chip->ptime[1], 0, sizeof(struct timespec));
		chip->pd_chging = false;
		chip->qc_chging = false;
			
		chip->FAST_charger_thread = kthread_create(FAST_charger_thread_main, (void *)chip, "FAST_charger_thread");
        if (!chip->FAST_charger_thread) {
                chg_err("Can't create FAST_charger_thread\n");
                rc = -EPROBE_DEFER;
                goto power_psy_reg_failed;
        }
#endif

#ifdef CONFIG_FB
        chip->chg_fb_notify.notifier_call = fb_notifier_callback;
#ifdef CONFIG_DRM_MSM
        rc = msm_drm_register_client(&chip->chg_fb_notify);
#else
        rc = fb_register_client(&chip->chg_fb_notify);
#endif /*CONFIG_DRM_MSM*/
        if (rc) {
                pr_err("Unable to register chg_fb_notify: %d\n", rc);
        }
#endif 

        init_proc_chg_log();
        init_proc_chg_cycle();
        init_proc_critical_log();
        init_proc_tbatt_pwroff();
#ifdef ODM_WT_EDIT
/*Shouli.Wang@ODM_WT.BSP.CHG 2019/11/05, add for batt_param_noplug proc*/
		init_proc_batt_param_noplug();
#endif /*ODM_WT_EDIT*/
#ifdef CONFIG_OPPO_RTC_DET_SUPPORT
        init_proc_rtc_det();
        init_proc_vbat_low_det();
#endif
        /*ye.zhang@BSP.Sensor.Function, 2017-03-30, add interface for charging special feature in different projects*/
        init_proc_charging_feature();
        /*ye.zhang add end*/
        schedule_delayed_work(&chip->update_work, OPPO_CHG_UPDATE_INIT_DELAY);
        INIT_DELAYED_WORK(&chip->mmi_adapter_in_work, mmi_adapter_in_work_func);
        charger_xlog_printk(CHG_LOG_CRTI, " end\n");

        return 0;
power_psy_reg_failed:
if (chip->ac_psy)
		power_supply_unregister(chip->ac_psy);
if (chip->usb_psy)
		power_supply_unregister(chip->usb_psy);
if (chip->batt_psy)
		power_supply_unregister(chip->batt_psy);
        charger_xlog_printk(CHG_LOG_CRTI, " Failed, rc = %d\n", rc);
        return rc;
}


/*--------------------------------------------------------*/
int oppo_chg_parse_svooc_dt(struct oppo_chg_chip *chip)
{
        int rc;
        struct device_node *node = chip->dev->of_node;


        if (!node) {
                dev_err(chip->dev, "device tree info. missing\n");
                return -EINVAL;
        }

        rc = of_property_read_u32(node, "qcom,vbatt_num", &chip->vbatt_num);
        if (rc) {
                chip->vbatt_num = 1;
        }
		
		rc = of_property_read_u32(node, "qcom,vooc_project", &chip->vooc_project);
        if (rc < 0) {
                chip->vooc_project = 0;
        }
		chip->platform_fg_flag = of_property_read_bool(node, "qcom,platform_fg_flag");
		chg_err("oppo_parse_svooc_dt, chip->vbatt_num = %d,chip->vooc_project = %d.\n",chip->vbatt_num,chip->vooc_project);
		return 0;
}

int oppo_chg_parse_charger_dt(struct oppo_chg_chip *chip)
{
	int rc;
	struct device_node *node = chip->dev->of_node;
	int batt_cold_degree_negative, batt_removed_degree_negative;

	if (!node) {
		dev_err(chip->dev, "device tree info. missing\n");
		return -EINVAL;
	}

	rc = of_property_read_u32(node, "sw_full_count", &chip->sw_full_count);
	if (rc) {
		chip->sw_full_count = 5;
	}

	rc = of_property_read_u32(node, "qcom,input_current_charger_ma", &chip->limits.input_current_charger_ma);
	if (rc) {
		chip->limits.input_current_charger_ma = OPCHG_INPUT_CURRENT_LIMIT_CHARGER_MA;
	}
    rc = of_property_read_u32(node, "qcom,pd_input_current_charger_ma", &chip->limits.pd_input_current_charger_ma);
	if (rc) {
		chip->limits.pd_input_current_charger_ma = OPCHG_INPUT_CURRENT_LIMIT_CHARGER_MA;
	}
   	rc = of_property_read_u32(node, "qcom,qc_input_current_charger_ma", &chip->limits.qc_input_current_charger_ma);
	if (rc) {
		chip->limits.qc_input_current_charger_ma = OPCHG_INPUT_CURRENT_LIMIT_CHARGER_MA;
	}

//#ifdef VENDOR_EDIT
//zhouhengguo@BSP.Stabliity, 2019.10.14, add for aging version
	if(get_eng_version() == 1) {
		chip->limits.input_current_usb_ma = 1000;
	} else {
		rc = of_property_read_u32(node, "qcom,input_current_usb_ma", &chip->limits.input_current_usb_ma);
		if (rc) {
			chip->limits.input_current_usb_ma = OPCHG_INPUT_CURRENT_LIMIT_USB_MA;
		}
	}
//#endif
	rc = of_property_read_u32(node, "qcom,input_current_cdp_ma", &chip->limits.input_current_cdp_ma);
	if (rc) {
		chip->limits.input_current_cdp_ma = OPCHG_INPUT_CURRENT_LIMIT_USB_MA;
	}

#ifdef CONFIG_HIGH_TEMP_VERSION
	chip->limits.input_current_led_ma = OPCHG_INPUT_CURRENT_LIMIT_CHARGER_MA;
	chg_err(" CONFIG_HIGH_TEMP_VERSION enable here,led on current 2A \n");
#else
	rc = of_property_read_u32(node, "qcom,input_current_led_ma", &chip->limits.input_current_led_ma);
	if (rc) {
		chip->limits.input_current_led_ma = OPCHG_INPUT_CURRENT_LIMIT_LED_MA;
	}
#endif

#ifdef CONFIG_HIGH_TEMP_VERSION
	chip->limits.input_current_led_ma_high = OPCHG_INPUT_CURRENT_LIMIT_CHARGER_MA;
	chg_err(" CONFIG_HIGH_TEMP_VERSION enable here, led_ma_high on current 2A \n");
#else
	rc = of_property_read_u32(node, "qcom,input_current_led_ma_high", &chip->limits.input_current_led_ma_high);
	if (rc) {
		chip->limits.input_current_led_ma_high = chip->limits.input_current_led_ma;
	}
#endif
	rc = of_property_read_u32(node, "qcom,led_high_bat_decidegc", &chip->limits.led_high_bat_decidegc);
	if (rc) {
		chip->limits.led_high_bat_decidegc = 370;
	}
#ifdef CONFIG_HIGH_TEMP_VERSION
	chip->limits.input_current_led_ma_warm = OPCHG_INPUT_CURRENT_LIMIT_CHARGER_MA;
	chg_err(" CONFIG_HIGH_TEMP_VERSION enable here, led_ma_warm on current 2A \n");
#else
	rc = of_property_read_u32(node, "qcom,input_current_led_ma_warm", &chip->limits.input_current_led_ma_warm);
	if (rc) {
		chip->limits.input_current_led_ma_warm = chip->limits.input_current_led_ma;
	}
#endif
	rc = of_property_read_u32(node, "qcom,led_warm_bat_decidegc", &chip->limits.led_warm_bat_decidegc);
	if (rc) {
		chip->limits.led_warm_bat_decidegc = 350;
	}
#ifdef CONFIG_HIGH_TEMP_VERSION
	chip->limits.input_current_led_ma_normal = OPCHG_INPUT_CURRENT_LIMIT_CHARGER_MA;
	chg_err(" CONFIG_HIGH_TEMP_VERSION enable here, led_ma_normal on current 2A \n");
#else
	rc = of_property_read_u32(node, "qcom,input_current_led_ma_normal", &chip->limits.input_current_led_ma_normal);
	if (rc) {
		chip->limits.input_current_led_ma_normal = chip->limits.input_current_led_ma;;
	}
#endif

        rc = of_property_read_u32(node, "qcom,input_current_camera_ma", &chip->limits.input_current_camera_ma);
        if (rc) {
                chip->limits.input_current_camera_ma = OPCHG_INPUT_CURRENT_LIMIT_CAMERA_MA;
        }

        chip->limits.iterm_disabled = of_property_read_bool(node, "qcom,iterm_disabled");

        rc = of_property_read_u32(node, "qcom,iterm_ma", &chip->limits.iterm_ma);
        if (rc < 0) {
                chip->limits.iterm_ma = -EINVAL;
        }

        rc = of_property_read_u32(node, "qcom,input_current_calling_ma", &chip->limits.input_current_calling_ma);
        if (rc) {
                chip->limits.input_current_calling_ma = OPCHG_INPUT_CURRENT_LIMIT_CALLING_MA;
        }

        rc = of_property_read_u32(node, "qcom,recharge-mv", &chip->limits.recharge_mv);
        if (rc < 0) {
                chip->limits.recharge_mv = -EINVAL;
        }

        /*-19C*/
        rc = of_property_read_u32(node, "qcom,removed_bat_decidegc", &batt_removed_degree_negative);
        if (rc < 0) {
                chip->limits.removed_bat_decidegc = -19;
        } else {
                chip->limits.removed_bat_decidegc = -batt_removed_degree_negative;
        }

/*-3~0 C*/
		#ifdef CONFIG_HIGH_TEMP_VERSION
        chg_err(" CONFIG_HIGH_TEMP_VERSION enable here,disable low tbat chg \n");
        batt_cold_degree_negative = 170;
        chip->limits.cold_bat_decidegc = -batt_cold_degree_negative;
		#else
        chg_err(" CONFIG_HIGH_TEMP_VERSION disabled\n");
        rc = of_property_read_u32(node, "qcom,cold_bat_decidegc", &batt_cold_degree_negative);
        if (rc < 0) {
        		chip->limits.cold_bat_decidegc = -EINVAL;
        } else {
        		chip->limits.cold_bat_decidegc = -batt_cold_degree_negative;		
        }
		#endif

        rc = of_property_read_u32(node, "qcom,temp_cold_vfloat_mv", &chip->limits.temp_cold_vfloat_mv);
        if (rc < 0) {
                chg_err(" temp_cold_vfloat_mv fail\n");
        }

        rc = of_property_read_u32(node, "qcom,temp_cold_fastchg_current_ma", &chip->limits.temp_cold_fastchg_current_ma);
        if (rc < 0) {
                chg_err(" temp_cold_fastchg_current_ma fail\n");
        }
/*0~5 C*/
        rc = of_property_read_u32(node, "qcom,little_cold_bat_decidegc", &chip->limits.little_cold_bat_decidegc);
        if (rc < 0) {
                chip->limits.little_cold_bat_decidegc = -EINVAL;
        }

        rc = of_property_read_u32(node, "qcom,temp_little_cold_vfloat_mv", &chip->limits.temp_little_cold_vfloat_mv);
        if (rc < 0) {
                chip->limits.temp_little_cold_vfloat_mv = -EINVAL;
        }

        rc = of_property_read_u32(node, "qcom,temp_little_cold_fastchg_current_ma", &chip->limits.temp_little_cold_fastchg_current_ma);
        if (rc < 0) {
                chip->limits.temp_little_cold_fastchg_current_ma = -EINVAL;
        }
        rc = of_property_read_u32(node, "qcom,temp_little_cold_fastchg_current_ma_high", &chip->limits.temp_little_cold_fastchg_current_ma_high);
        if (rc < 0) {
                chip->limits.temp_little_cold_fastchg_current_ma_high = chip->limits.temp_little_cold_fastchg_current_ma;
        }
        rc = of_property_read_u32(node, "qcom,temp_little_cold_fastchg_current_ma_low", &chip->limits.temp_little_cold_fastchg_current_ma_low);
        if (rc < 0) {
                chip->limits.temp_little_cold_fastchg_current_ma_low = chip->limits.temp_little_cold_fastchg_current_ma;
        }
        rc = of_property_read_u32(node, "qcom,pd_temp_little_cold_fastchg_current_ma_high", &chip->limits.pd_temp_little_cold_fastchg_current_ma_high);
        if (rc < 0) {
                chip->limits.pd_temp_little_cold_fastchg_current_ma_high = chip->limits.temp_little_cold_fastchg_current_ma_high;
        }
        rc = of_property_read_u32(node, "qcom,pd_temp_little_cold_fastchg_current_ma_low", &chip->limits.pd_temp_little_cold_fastchg_current_ma_low);
        if (rc < 0) {
                chip->limits.pd_temp_little_cold_fastchg_current_ma_low = chip->limits.temp_little_cold_fastchg_current_ma_low;
        }
	 rc = of_property_read_u32(node, "qcom,qc_temp_little_cold_fastchg_current_ma_high", &chip->limits.qc_temp_little_cold_fastchg_current_ma_high);
        if (rc < 0) {
                chip->limits.qc_temp_little_cold_fastchg_current_ma_high = chip->limits.temp_little_cold_fastchg_current_ma_high;
        }
        rc = of_property_read_u32(node, "qcom,qc_temp_little_cold_fastchg_current_ma_low", &chip->limits.qc_temp_little_cold_fastchg_current_ma_low);
        if (rc < 0) {
                chip->limits.qc_temp_little_cold_fastchg_current_ma_low = chip->limits.temp_little_cold_fastchg_current_ma_low;
        }
/*5~12 C*/
        rc = of_property_read_u32(node, "qcom,cool_bat_decidegc", &chip->limits.cool_bat_decidegc);
        if (rc < 0) {
                chip->limits.cool_bat_decidegc = -EINVAL;
        }

        rc = of_property_read_u32(node, "qcom,temp_cool_vfloat_mv", &chip->limits.temp_cool_vfloat_mv);
        if (rc < 0) {
                chip->limits.temp_cool_vfloat_mv = -EINVAL;
        }

        rc = of_property_read_u32(node, "qcom,temp_cool_fastchg_current_ma_high", &chip->limits.temp_cool_fastchg_current_ma_high);
        if (rc < 0) {
                chip->limits.temp_cool_fastchg_current_ma_high = -EINVAL;
        }

        rc = of_property_read_u32(node, "qcom,temp_cool_fastchg_current_ma_low", &chip->limits.temp_cool_fastchg_current_ma_low);
        if (rc < 0) {
                chip->limits.temp_cool_fastchg_current_ma_low = -EINVAL;
        }
        rc = of_property_read_u32(node, "qcom,pd_temp_cool_fastchg_current_ma_high", &chip->limits.pd_temp_cool_fastchg_current_ma_high);
        if (rc < 0) {
                chip->limits.pd_temp_cool_fastchg_current_ma_high = chip->limits.temp_cool_fastchg_current_ma_high;
        }

        rc = of_property_read_u32(node, "qcom,pd_temp_cool_fastchg_current_ma_low", &chip->limits.pd_temp_cool_fastchg_current_ma_low);
        if (rc < 0) {
                chip->limits.pd_temp_cool_fastchg_current_ma_low = chip->limits.temp_cool_fastchg_current_ma_low;
        }
	 rc = of_property_read_u32(node, "qcom,qc_temp_cool_fastchg_current_ma_high", &chip->limits.qc_temp_cool_fastchg_current_ma_high);
        if (rc < 0) {
                chip->limits.qc_temp_cool_fastchg_current_ma_high = chip->limits.temp_cool_fastchg_current_ma_high;
        }

        rc = of_property_read_u32(node, "qcom,qc_temp_cool_fastchg_current_ma_low", &chip->limits.qc_temp_cool_fastchg_current_ma_low);
        if (rc < 0) {
                chip->limits.qc_temp_cool_fastchg_current_ma_low = chip->limits.temp_cool_fastchg_current_ma_low;
        }
/*12~16 C*/
        rc = of_property_read_u32(node, "qcom,little_cool_bat_decidegc", &chip->limits.little_cool_bat_decidegc);
        if (rc < 0) {
                chip->limits.little_cool_bat_decidegc = -EINVAL;
        }

        rc = of_property_read_u32(node, "qcom,temp_little_cool_vfloat_mv", &chip->limits.temp_little_cool_vfloat_mv);
        if (rc < 0) {
                chip->limits.temp_little_cool_vfloat_mv = -EINVAL;
        }

        rc = of_property_read_u32(node, "qcom,temp_little_cool_fastchg_current_ma",
                                                        &chip->limits.temp_little_cool_fastchg_current_ma);
        if (rc < 0) {
                chip->limits.temp_little_cool_fastchg_current_ma = -EINVAL;
        }
        rc = of_property_read_u32(node, "qcom,pd_temp_little_cool_fastchg_current_ma",
                                                        &chip->limits.pd_temp_little_cool_fastchg_current_ma);
        if (rc < 0) {
                chip->limits.pd_temp_little_cool_fastchg_current_ma = chip->limits.temp_little_cool_fastchg_current_ma;
        }
	rc = of_property_read_u32(node, "qcom,qc_temp_little_cool_fastchg_current_ma",
                                                        &chip->limits.qc_temp_little_cool_fastchg_current_ma);
        if (rc < 0) {
                chip->limits.qc_temp_little_cool_fastchg_current_ma = chip->limits.temp_little_cool_fastchg_current_ma;
        }

/*16~45 C*/
        rc = of_property_read_u32(node, "qcom,normal_bat_decidegc", &chip->limits.normal_bat_decidegc);
        if (rc < 0) {
                chg_err(" normal_bat_decidegc fail\n");
        }
        rc = of_property_read_u32(node, "qcom,temp_normal_fastchg_current_ma", &chip->limits.temp_normal_fastchg_current_ma);
        if (rc) {
                chip->limits.temp_normal_fastchg_current_ma = OPCHG_FAST_CHG_MAX_MA;
        }

        rc = of_property_read_u32(node, "qcom,temp_normal_vfloat_mv", &chip->limits.temp_normal_vfloat_mv);
        if (rc < 0) {
                chip->limits.temp_normal_vfloat_mv = 4320;
        }
        rc = of_property_read_u32(node, "qcom,pd_temp_normal_fastchg_current_ma", &chip->limits.pd_temp_normal_fastchg_current_ma);
        if (rc) {
                chip->limits.pd_temp_normal_fastchg_current_ma = OPCHG_FAST_CHG_MAX_MA;
        }
	rc = of_property_read_u32(node, "qcom,qc_temp_normal_fastchg_current_ma", &chip->limits.qc_temp_normal_fastchg_current_ma);
        if (rc) {
                chip->limits.qc_temp_normal_fastchg_current_ma = OPCHG_FAST_CHG_MAX_MA;
        }
/*45~55 C*/
        rc = of_property_read_u32(node, "qcom,warm_bat_decidegc", &chip->limits.warm_bat_decidegc);
        if (rc < 0) {
                chip->limits.warm_bat_decidegc = -EINVAL;
        }

        rc = of_property_read_u32(node, "qcom,temp_warm_vfloat_mv", &chip->limits.temp_warm_vfloat_mv);
        if (rc < 0) {
                chip->limits.temp_warm_vfloat_mv = -EINVAL;
        }

        rc = of_property_read_u32(node, "qcom,temp_warm_fastchg_current_ma",
                                                        &chip->limits.temp_warm_fastchg_current_ma);
        if (rc < 0) {
                chip->limits.temp_warm_fastchg_current_ma = -EINVAL;
        }
        rc = of_property_read_u32(node, "qcom,pd_temp_warm_fastchg_current_ma",
                                                        &chip->limits.pd_temp_warm_fastchg_current_ma);
        if (rc < 0) {
                chip->limits.pd_temp_warm_fastchg_current_ma = chip->limits.temp_warm_fastchg_current_ma;
        }
	 rc = of_property_read_u32(node, "qcom,qc_temp_warm_fastchg_current_ma",
                                                        &chip->limits.qc_temp_warm_fastchg_current_ma);
        if (rc < 0) {
                chip->limits.qc_temp_warm_fastchg_current_ma = chip->limits.temp_warm_fastchg_current_ma;
        }
        rc = of_property_read_u32(node, "qcom,temp_warm_fastchg_current_ma_led_on",
        		                                        &chip->limits.temp_warm_fastchg_current_ma_led_on);
        if (rc < 0) {
                chip->limits.temp_warm_fastchg_current_ma_led_on = chip->limits.temp_warm_fastchg_current_ma;
        }

/*>55 C*/
	rc = of_property_read_u32(node, "qcom,hot_bat_decidegc", &chip->limits.hot_bat_decidegc);
	if (rc < 0) {
		chip->limits.hot_bat_decidegc = -EINVAL;
	}
#ifdef ODM_WT_EDIT
/*Sidong.Zhao@ODM_WT.BSP.CHG 2019/11/11,add charge parameter*/
    rc = of_property_read_u32(node, "monet,temp_little_cold_usbchg_current_ma", &chip->limits.temp_little_cold_usbchg_current_ma);/*0-5*/
    if (rc < 0) {
        chip->limits.temp_little_cold_usbchg_current_ma = -EINVAL;
    }
    rc = of_property_read_u32(node, "monet,temp_little_cold_usbchg_current_ma_high", &chip->limits.temp_little_cold_usbchg_current_ma_high);
    if (rc < 0) {
        chip->limits.temp_little_cold_usbchg_current_ma_high = chip->limits.temp_little_cold_usbchg_current_ma;
    }
    rc = of_property_read_u32(node, "monet,temp_little_cold_usbchg_current_ma_low", &chip->limits.temp_little_cold_usbchg_current_ma_low);
    if (rc < 0) {
        chip->limits.temp_little_cold_usbchg_current_ma_low = chip->limits.temp_little_cold_usbchg_current_ma;
    }
    rc = of_property_read_u32(node, "monet,temp_cool_usbchg_current_ma_high", &chip->limits.temp_cool_usbchg_current_ma_high);/*5-12*/
    if (rc < 0) {
        chip->limits.temp_cool_usbchg_current_ma_high = -EINVAL;
    }
    rc = of_property_read_u32(node, "monet,temp_cool_usbchg_current_ma_low", &chip->limits.temp_cool_usbchg_current_ma_low);
    if (rc < 0) {
        chip->limits.temp_cool_usbchg_current_ma_low = -EINVAL;
    }
	rc = of_property_read_u32(node, "monet,temp_little_cool_usbchg_current_ma", &chip->limits.temp_little_cool_usbchg_current_ma);/* 12C ~ 16C */
    if (rc < 0) {
        chip->limits.temp_little_cool_usbchg_current_ma = -EINVAL;
    }
	rc = of_property_read_u32(node, "monet,temp_normal_usbchg_current_ma", &chip->limits.temp_normal_usbchg_current_ma);/* 16C ~ 44C */
    if (rc) {
        chip->limits.temp_normal_usbchg_current_ma = 500;
    }
	rc = of_property_read_u32(node, "monet,temp_warm_usbchg_current_ma",
													&chip->limits.temp_warm_usbchg_current_ma);/* 44C ~ 53C */
	if (rc < 0) {
			chip->limits.temp_warm_usbchg_current_ma = -EINVAL;
	}
#endif
#ifdef ODM_WT_EDIT
//Junbo.Guo@ODM_WT.BSP.CHG, 2019/11/11, Modify for pe20
	rc = of_property_read_u32(node, "monet,pe20_temp_normal_fastchg_current_ma",
													&chip->limits.pe20_temp_normal_fastchg_current_ma);
	if (rc < 0) {
			chip->limits.pe20_temp_normal_fastchg_current_ma =OPCHG_FAST_CHG_MAX_MA;
	}
	rc = of_property_read_u32(node, "monet,pe20_temp_little_cool_fastchg_current_ma",
													&chip->limits.pe20_temp_little_cool_fastchg_current_ma);
	if (rc < 0) {
			chip->limits.pe20_temp_little_cool_fastchg_current_ma = chip->limits.temp_cool_fastchg_current_ma_high;
	}
		rc = of_property_read_u32(node, "monet,pe20_temp_little_cold_fastchg_current_ma_high",
													&chip->limits.pe20_temp_little_cold_fastchg_current_ma_high);
	if (rc < 0) {
			chip->limits.pe20_temp_little_cold_fastchg_current_ma_high = chip->limits.temp_little_cold_fastchg_current_ma_high;
	}
		rc = of_property_read_u32(node, "monet,pe20_temp_little_cold_fastchg_current_ma_low",
													&chip->limits.pe20_temp_little_cold_fastchg_current_ma_low);
	if (rc < 0) {
			chip->limits.pe20_temp_little_cold_fastchg_current_ma_low = chip->limits.temp_little_cold_fastchg_current_ma_low;
	}
		rc = of_property_read_u32(node, "monet,pe20_temp_cool_fastchg_current_ma_high",
													&chip->limits.pe20_temp_cool_fastchg_current_ma_high);
	if (rc < 0) {
			chip->limits.pe20_temp_cool_fastchg_current_ma_high = chip->limits.temp_cool_fastchg_current_ma_high;
	}
		rc = of_property_read_u32(node, "monet,pe20_temp_cool_fastchg_current_ma_low",
													&chip->limits.pe20_temp_cool_fastchg_current_ma_low);
	if (rc < 0) {
			chip->limits.pe20_temp_cool_fastchg_current_ma_low = chip->limits.temp_cool_fastchg_current_ma_low;
	}
		rc = of_property_read_u32(node, "monet,pe20_temp_warm_fastchg_current_ma",
													&chip->limits.pe20_temp_warm_fastchg_current_ma);
	if (rc < 0) {
			chip->limits.pe20_temp_warm_fastchg_current_ma = chip->limits.temp_warm_fastchg_current_ma;
	}
	rc = of_property_read_u32(node, "monet,pe20_input_current_charger_ma",
													&chip->limits.pe20_input_current_charger_ma);
	if (rc < 0) {
			chip->limits.pe20_input_current_charger_ma = chip->limits.input_current_charger_ma;
	}
#endif

/*offset temperature, only for userspace, default 0*/
        rc = of_property_read_u32(node, "qcom,offset_temp", &chip->offset_temp);
        if (rc < 0) {
                chip->offset_temp = 0;
        }

/*non standard battery*/
        rc = of_property_read_u32(node, "qcom,non_standard_vfloat_mv",
                                                &chip->limits.non_standard_vfloat_mv);
        if (rc < 0) {
                chip->limits.non_standard_vfloat_mv = -EINVAL;
        }
        rc = of_property_read_u32(node, "qcom,non_standard_fastchg_current_ma",
                                                &chip->limits.non_standard_fastchg_current_ma);
        if (rc < 0) {
                chip->limits.non_standard_fastchg_current_ma = -EINVAL;
        }

/*short circuit battery*/
        rc = of_property_read_u32(node, "qcom,short_c_bat_cv_mv",
						&chip->short_c_batt.short_c_bat_cv_mv);
        if (rc < 0) {
                chip->short_c_batt.short_c_bat_cv_mv = -EINVAL;
        }
        rc = of_property_read_u32(node, "qcom,short_c_bat_vfloat_mv",
						&chip->limits.short_c_bat_vfloat_mv);
        if (rc < 0) {
                chip->limits.short_c_bat_vfloat_mv = -EINVAL;
        }
        rc = of_property_read_u32(node, "qcom,short_c_bat_fastchg_current_ma",
						&chip->limits.short_c_bat_fastchg_current_ma);
        if (rc < 0) {
                chip->limits.short_c_bat_fastchg_current_ma = -EINVAL;
        }
        rc = of_property_read_u32(node, "qcom,short_c_bat_vfloat_sw_limit", &chip->limits.short_c_bat_vfloat_sw_limit);
        if (rc < 0) {
                chip->limits.short_c_bat_vfloat_sw_limit = -EINVAL;
        }
        		
/*vfloat_sw_limit*/
        rc = of_property_read_u32(node, "qcom,non_standard_vfloat_sw_limit", &chip->limits.non_standard_vfloat_sw_limit);
        if (rc < 0) {
                chip->limits.non_standard_vfloat_sw_limit = 3960;
        }

        rc = of_property_read_u32(node, "qcom,cold_vfloat_sw_limit", &chip->limits.cold_vfloat_sw_limit);
        if (rc < 0) {
                chip->limits.cold_vfloat_sw_limit = 3960;
        }

        rc = of_property_read_u32(node, "qcom,little_cold_vfloat_sw_limit", &chip->limits.little_cold_vfloat_sw_limit);
        if (rc < 0) {
                chip->limits.little_cold_vfloat_sw_limit = 4330;
        }

        rc = of_property_read_u32(node, "qcom,cool_vfloat_sw_limit", &chip->limits.cool_vfloat_sw_limit);
        if (rc < 0) {
                chip->limits.cool_vfloat_sw_limit = 4330;
        }

        rc = of_property_read_u32(node, "qcom,little_cool_vfloat_sw_limit", &chip->limits.little_cool_vfloat_sw_limit);
        if (rc < 0) {
                chip->limits.little_cool_vfloat_sw_limit = 4330;
        }

        rc = of_property_read_u32(node, "qcom,normal_vfloat_sw_limit", &chip->limits.normal_vfloat_sw_limit);
        if (rc < 0) {
                chip->limits.normal_vfloat_sw_limit = 4330;
        }

        rc = of_property_read_u32(node, "qcom,warm_vfloat_sw_limit", &chip->limits.warm_vfloat_sw_limit);
        if (rc < 0) {
                chip->limits.warm_vfloat_sw_limit = 4060;
        }

		/*vfloat_over_sw_limit*/
		chip->limits.sw_vfloat_over_protect_enable = of_property_read_bool(node,
								"qcom,sw_vfloat_over_protect_enable");
	
		rc = of_property_read_u32(node, "qcom,non_standard_vfloat_over_sw_limit",
								&chip->limits.non_standard_vfloat_over_sw_limit);
	    if (rc < 0) {
	        chip->limits.non_standard_vfloat_over_sw_limit = 3980;
	    }

		rc = of_property_read_u32(node, "qcom,cold_vfloat_over_sw_limit",
								&chip->limits.cold_vfloat_over_sw_limit);
	    if (rc < 0) {
	        chip->limits.cold_vfloat_over_sw_limit = 3980;
	    }

		rc = of_property_read_u32(node, "qcom,little_cold_vfloat_over_sw_limit",
								&chip->limits.little_cold_vfloat_over_sw_limit);
	    if (rc < 0) {
	        chip->limits.little_cold_vfloat_over_sw_limit = 4390;
	    }

		rc = of_property_read_u32(node, "qcom,cool_vfloat_over_sw_limit",
								&chip->limits.cool_vfloat_over_sw_limit);
	    if (rc < 0) {
	        chip->limits.cool_vfloat_over_sw_limit = 4390;
	    }

		rc = of_property_read_u32(node, "qcom,little_cool_vfloat_over_sw_limit",
								&chip->limits.little_cool_vfloat_over_sw_limit);
	    if (rc < 0) {
	        chip->limits.little_cool_vfloat_over_sw_limit = 4390;
	    }

		rc = of_property_read_u32(node, "qcom,normal_vfloat_over_sw_limit",
								&chip->limits.normal_vfloat_over_sw_limit);
	    if (rc < 0) {
	        chip->limits.normal_vfloat_over_sw_limit = 4390;
	    }

		rc = of_property_read_u32(node, "qcom,warm_vfloat_over_sw_limit",
								&chip->limits.warm_vfloat_over_sw_limit);
	    if (rc < 0) {
	        chip->limits.warm_vfloat_over_sw_limit = 4080;
	    }
        rc = of_property_read_u32(node, "qcom,max_chg_time_sec",
                                                &chip->limits.max_chg_time_sec);
        if (rc < 0) {
                chip->limits.max_chg_time_sec = 36000;
        }
        rc = of_property_read_u32(node, "qcom,charger_hv_thr",
                                                &chip->limits.charger_hv_thr);
        if (rc < 0) {
                chip->limits.charger_hv_thr = 5800;
        }

        rc = of_property_read_u32(node, "qcom,charger_recv_thr",
                                                &chip->limits.charger_recv_thr);
        if (rc < 0) {
                chip->limits.charger_recv_thr = 5800;
        }

        rc = of_property_read_u32(node, "qcom,charger_lv_thr",
                                                &chip->limits.charger_lv_thr);
        if (rc < 0) {
                chip->limits.charger_lv_thr = 3400;
        }
        rc = of_property_read_u32(node, "qcom,vbatt_full_thr",
                                                &chip->limits.vbatt_full_thr);
        if (rc < 0) {
                chip->limits.vbatt_full_thr = 4400;
        }
        rc = of_property_read_u32(node, "qcom,vbatt_hv_thr",
                                                &chip->limits.vbatt_hv_thr);
        if (rc < 0) {
                chip->limits.vbatt_hv_thr = 4500;
        }
        rc = of_property_read_u32(node, "qcom,vfloat_step_mv",
                                                &chip->limits.vfloat_step_mv);
        if (rc < 0) {
                chip->limits.vfloat_step_mv = 16;
        }

		rc = of_property_read_u32(node, "qcom,vbatt_power_off", &chip->vbatt_power_off);
        if (rc < 0) {
                chip->vbatt_power_off = 3300;
        }

		rc = of_property_read_u32(node, "qcom,vbatt_soc_1", &chip->vbatt_soc_1);
        if (rc < 0) {
                chip->vbatt_soc_1 = 3410;
        }
		
		rc = of_property_read_u32(node, "qcom,normal_vterm_hw_inc", &chip->limits.normal_vterm_hw_inc);
        if (rc < 0) {
                chip->limits.normal_vterm_hw_inc = 18;
        }

		rc = of_property_read_u32(node, "qcom,non_normal_vterm_hw_inc", &chip->limits.non_normal_vterm_hw_inc);
        if (rc < 0) {
                chip->limits.non_normal_vterm_hw_inc = 18;
        }
	 rc = of_property_read_u32(node, "qcom,vbatt_pdqc_to_5v_thr", &chip->limits.vbatt_pdqc_to_5v_thr);
        if (rc < 0) {
                chip->limits.vbatt_pdqc_to_5v_thr = -EINVAL;
        }
		charger_xlog_printk(CHG_LOG_CRTI, "vbatt_power_off = %d, vbatt_soc_1 = %d,normal_vterm_hw_inc = %d,, non_normal_vterm_hw_inc = %d,  vbatt_pdqc_to_5v_thr = %d\n",
			chip->vbatt_power_off, chip->vbatt_soc_1,chip->limits.normal_vterm_hw_inc, chip->limits.non_normal_vterm_hw_inc, chip->limits.vbatt_pdqc_to_5v_thr);

		rc = of_property_read_u32(node, "qcom,ff1_normal_fastchg_ma", &chip->limits.ff1_normal_fastchg_ma);
        if (rc) {
                chip->limits.ff1_normal_fastchg_ma = 1000;
        }
        rc = of_property_read_u32(node, "qcom,ff1_warm_fastchg_ma", &chip->limits.ff1_warm_fastchg_ma);
        if (rc) {
                chip->limits.ff1_warm_fastchg_ma = chip->limits.ff1_normal_fastchg_ma;
        }
        rc = of_property_read_u32(node, "qcom,ffc2_temp_warm_decidegc", &chip->limits.ffc2_temp_warm_decidegc);
        if (rc) {
                chip->limits.ffc2_temp_warm_decidegc = 350;
        }
        rc = of_property_read_u32(node, "qcom,ffc2_temp_high_decidegc", &chip->limits.ffc2_temp_high_decidegc);
        if (rc) {
                chip->limits.ffc2_temp_high_decidegc = 400;
        }
        rc = of_property_read_u32(node, "qcom,ffc2_temp_low_decidegc", &chip->limits.ffc2_temp_low_decidegc);
        if (rc) {
                chip->limits.ffc2_temp_low_decidegc = 160;
        }
		rc = of_property_read_u32(node, "qcom,ffc2_normal_fastchg_ma", &chip->limits.ffc2_normal_fastchg_ma);
        if (rc < 0) {
                chip->limits.ffc2_normal_fastchg_ma = 700;
        }
		rc = of_property_read_u32(node, "qcom,ffc2_warm_fastchg_ma", &chip->limits.ffc2_warm_fastchg_ma);
        if (rc < 0) {
                chip->limits.ffc2_warm_fastchg_ma = 750;
        }
		rc = of_property_read_u32(node, "qcom,ffc2_exit_step_ma", &chip->limits.ffc2_exit_step_ma);
        if (rc < 0) {
                chip->limits.ffc2_exit_step_ma = 100;
        }
		rc = of_property_read_u32(node, "qcom,ffc2_warm_exit_step_ma", &chip->limits.ffc2_warm_exit_step_ma);
        if (rc < 0) {
                chip->limits.ffc2_warm_exit_step_ma = chip->limits.ffc2_exit_step_ma;
        }

		rc = of_property_read_u32(node, "qcom,ff1_exit_step_ma", &chip->limits.ff1_exit_step_ma);
        if (rc < 0) {
                chip->limits.ff1_exit_step_ma = 400;
        }
		rc = of_property_read_u32(node, "qcom,ff1_warm_exit_step_ma", &chip->limits.ff1_warm_exit_step_ma);
        if (rc < 0) {
                chip->limits.ff1_warm_exit_step_ma = 350;
        }

		rc = of_property_read_u32(node, "qcom,ffc_normal_vfloat_sw_limit", &chip->limits.ffc1_normal_vfloat_sw_limit);
        if (rc < 0) {
                chip->limits.ffc1_normal_vfloat_sw_limit = 4450;
        }
		rc = of_property_read_u32(node, "qcom,ffc2_normal_vfloat_sw_limit", &chip->limits.ffc2_normal_vfloat_sw_limit);
        if (rc < 0) {
                chip->limits.ffc2_normal_vfloat_sw_limit = chip->limits.ffc1_normal_vfloat_sw_limit;
        }
		rc = of_property_read_u32(node, "qcom,ffc_temp_normal_vfloat_mv", &chip->limits.ffc_temp_normal_vfloat_mv);
        if (rc < 0) {
                chip->limits.ffc_temp_normal_vfloat_mv = 4500;
        }
		rc = of_property_read_u32(node, "qcom,ffc_normal_vfloat_over_sw_limit", &chip->limits.ffc_normal_vfloat_over_sw_limit);
        if (rc < 0) {
                chip->limits.ffc_normal_vfloat_over_sw_limit = 4500;
        }
		charger_xlog_printk(CHG_LOG_CRTI, "ff1_normal_fastchg_ma = %d, \
                                           ffc2_temp_warm_decidegc = %d, \
                                           ffc2_temp_high_decidegc = %d, \
                                           ffc2_normal_fastchg_ma = %d, \
                                           chip->limits.ffc2_warm_fastchg_ma = %d, \
                                           ffc2_exit_step_ma = %d, \
                                           ffc_normal_vfloat_sw_limit = %d, \
                                           ffc2_normal_vfloat_sw_limit = %d, \
                                           ffc2_temp_normal_vfloat_mv = %d, \
                                           ffc_normal_vfloat_over_sw_limit = %d \
                                           ffc2_temp_low_decidegc = %d \
                                           limits.ff1_exit_step_ma = %d \
                                           limits.ff1_warm_exit_step_ma = %d \
                                           pd_input_current_charger_ma = %d \
                                           qc_input_current_charger_ma = %d\n",
			                            chip->limits.ff1_normal_fastchg_ma,
			                            chip->limits.ffc2_temp_warm_decidegc,
			                            chip->limits.ffc2_temp_high_decidegc,
			                            chip->limits.ffc2_normal_fastchg_ma,
			                            chip->limits.ffc2_warm_fastchg_ma,
			                            chip->limits.ffc2_exit_step_ma,
			                            chip->limits.ffc1_normal_vfloat_sw_limit,
			                            chip->limits.ffc2_normal_vfloat_sw_limit,
			                            chip->limits.ffc_temp_normal_vfloat_mv,
			                            chip->limits.ffc_normal_vfloat_over_sw_limit,
			                            chip->limits.ffc2_temp_low_decidegc,
			                            chip->limits.ff1_exit_step_ma,
			                            chip->limits.ff1_warm_exit_step_ma,
			                            chip->limits.pd_input_current_charger_ma,
			                            chip->limits.qc_input_current_charger_ma);


		rc = of_property_read_u32(node, "qcom,default_iterm_ma", &chip->limits.default_iterm_ma);
        if (rc < 0) {
                chip->limits.default_iterm_ma = 100;
        }
		rc = of_property_read_u32(node, "qcom,default_temp_normal_fastchg_current_ma", &chip->limits.default_temp_normal_fastchg_current_ma);
        if (rc < 0) {
                chip->limits.default_temp_normal_fastchg_current_ma = OPCHG_FAST_CHG_MAX_MA;
        }
		rc = of_property_read_u32(node, "qcom,default_normal_vfloat_sw_limit", &chip->limits.default_normal_vfloat_sw_limit);
        if (rc < 0) {
                chip->limits.default_normal_vfloat_sw_limit = 4350;
        }
		rc = of_property_read_u32(node, "qcom,default_temp_normal_vfloat_mv", &chip->limits.default_temp_normal_vfloat_mv);
        if (rc < 0) {
                chip->limits.default_temp_normal_vfloat_mv = 4370;
        }
		rc = of_property_read_u32(node, "qcom,default_normal_vfloat_over_sw_limit", &chip->limits.default_normal_vfloat_over_sw_limit);
        if (rc < 0) {
                chip->limits.default_normal_vfloat_over_sw_limit = 4373;
        }
		charger_xlog_printk(CHG_LOG_CRTI, "default_iterm_ma = %d, default_temp_normal_fastchg_current_ma = %d, default_normal_vfloat_sw_limit = %d, default_temp_normal_vfloat_mv = %d, default_normal_vfloat_over_sw_limit = %d\n",
			chip->limits.default_iterm_ma, chip->limits.default_temp_normal_fastchg_current_ma, chip->limits.default_normal_vfloat_sw_limit, chip->limits.default_temp_normal_vfloat_mv, chip->limits.default_normal_vfloat_over_sw_limit);	

		rc = of_property_read_u32(node, "qcom,default_temp_little_cool_fastchg_current_ma", &chip->limits.default_temp_little_cool_fastchg_current_ma);
        if (rc < 0) {
                chip->limits.default_temp_little_cool_fastchg_current_ma = chip->limits.temp_little_cool_fastchg_current_ma;
        }
		rc = of_property_read_u32(node, "qcom,default_little_cool_vfloat_sw_limit", &chip->limits.default_little_cool_vfloat_sw_limit);
        if (rc < 0) {
                chip->limits.default_little_cool_vfloat_sw_limit = chip->limits.little_cool_vfloat_sw_limit;
        }
		rc = of_property_read_u32(node, "qcom,default_temp_little_cool_vfloat_mv", &chip->limits.default_temp_little_cool_vfloat_mv);
        if (rc < 0) {
                chip->limits.default_temp_little_cool_vfloat_mv = chip->limits.temp_little_cool_vfloat_mv;
        }
		rc = of_property_read_u32(node, "qcom,default_little_cool_vfloat_over_sw_limit", &chip->limits.default_little_cool_vfloat_over_sw_limit);
        if (rc < 0) {
                chip->limits.default_little_cool_vfloat_over_sw_limit = chip->limits.little_cool_vfloat_over_sw_limit;
        }
		charger_xlog_printk(CHG_LOG_CRTI, "default_temp_little_cool_fastchg_current_ma = %d, default_little_cool_vfloat_sw_limit = %d, default_temp_little_cool_vfloat_mv = %d, default_little_cool_vfloat_over_sw_limit = %d\n",
			chip->limits.default_temp_little_cool_fastchg_current_ma, chip->limits.default_little_cool_vfloat_sw_limit, chip->limits.default_temp_little_cool_vfloat_mv, chip->limits.default_little_cool_vfloat_over_sw_limit);

		chip->limits.default_temp_little_cold_fastchg_current_ma_high = chip->limits.temp_little_cold_fastchg_current_ma_high;
		chip->limits.default_temp_little_cold_fastchg_current_ma_low = chip->limits.temp_little_cold_fastchg_current_ma_low;
		chip->limits.default_temp_cool_fastchg_current_ma_high = chip->limits.temp_cool_fastchg_current_ma_high;
		chip->limits.default_temp_cool_fastchg_current_ma_low = chip->limits.temp_cool_fastchg_current_ma_low;
		chip->limits.default_temp_warm_fastchg_current_ma = chip->limits.temp_warm_fastchg_current_ma;
		chip->limits.default_input_current_charger_ma = chip->limits.input_current_charger_ma;

        rc = of_property_read_u32(node, "qcom,batt_capacity_mah", &chip->batt_capacity_mah);
        if (rc < 0) {
                chip->batt_capacity_mah = 2000;
        }

	chip->chg_ctrl_by_vooc = of_property_read_bool(node, "qcom,chg_ctrl_by_vooc");
	chip->chg_ctrl_by_vooc_default = of_property_read_bool(node, "qcom,chg_ctrl_by_vooc");
	rc = of_property_read_u32(node, "qcom,input_current_vooc_ma_normal", &chip->limits.input_current_vooc_ma_normal);
        if (rc) {
                chip->limits.input_current_vooc_ma_normal = 3600;
        }
		rc = of_property_read_u32(node, "qcom,vooc_temp_bat_normal_decidegc", &chip->limits.vooc_normal_bat_decidegc);
        if (rc) {
                chip->limits.vooc_normal_bat_decidegc = 340;
        }
		rc = of_property_read_u32(node, "qcom,input_current_vooc_ma_warm", &chip->limits.input_current_vooc_ma_warm);
        if (rc) {
                chip->limits.input_current_vooc_ma_warm = 3000;
        }
		rc = of_property_read_u32(node, "qcom,vooc_temp_bat_warm_decidegc", &chip->limits.vooc_warm_bat_decidegc);
        if (rc) {
                chip->limits.vooc_warm_bat_decidegc = 380;
        }
		rc = of_property_read_u32(node, "qcom,input_current_vooc_ma_high", &chip->limits.input_current_vooc_ma_high);
        if (rc) {
                chip->limits.input_current_vooc_ma_high = 2600;
        }
		rc = of_property_read_u32(node, "qcom,vooc_temp_bat_hot_decidegc", &chip->limits.vooc_high_bat_decidegc);
        if (rc) {
                chip->limits.vooc_high_bat_decidegc = 450;
        }
        rc = of_property_read_u32(node, "qcom,charger_current_vooc_ma_normal", &chip->limits.charger_current_vooc_ma_normal);
        if (rc) {
                chip->limits.charger_current_vooc_ma_normal = 1000;
        }

        chip->suspend_after_full = of_property_read_bool(node, "qcom,suspend_after_full");

        chip->check_batt_full_by_sw = of_property_read_bool(node, "qcom,check_batt_full_by_sw");

        chip->external_gauge = of_property_read_bool(node, "qcom,external_gauge");
        chip->fg_bcl_poll = of_property_read_bool(node, "qcom,fg_bcl_poll_enable");
	 
        chip->chg_ctrl_by_lcd = of_property_read_bool(node, "qcom,chg_ctrl_by_lcd");
		chip->chg_ctrl_by_lcd_default = of_property_read_bool(node, "qcom,chg_ctrl_by_lcd");
        chip->chg_ctrl_by_camera = of_property_read_bool(node, "qcom,chg_ctrl_by_camera");
	    chip->bq25890h_flag = of_property_read_bool(node,"qcom,bq25890_flag");
        chip->chg_ctrl_by_calling = of_property_read_bool(node, "qcom,chg_ctrl_by_calling");
        chip->ffc_support = of_property_read_bool(node, "qcom,ffc_support");


	    charger_xlog_printk(CHG_LOG_CRTI, "input_current_charger_ma = %d, \
			input_current_usb_ma = %d, input_current_led_ma = %d, \
			input_current_led_ma_normal = %d, \
			input_current_led_ma_warm = %d, \
			input_current_led_ma_high = %d, \
			temp_normal_fastchg_current_ma = %d, \
			temp_normal_vfloat_mv = %d, \
			iterm_ma = %d, recharge_mv = %d, \
			cold_bat_decidegc = %d, temp_cold_vfloat_mv = %d, \
			temp_cold_fastchg_current_ma = %d, little_cold_bat_decidegc = %d, \
			temp_little_cold_vfloat_mv = %d, \
			temp_little_cold_fastchg_current_ma = %d, cool_bat_decidegc = %d, \
			temp_cool_vfloat_mv = %d, temp_cool_fastchg_current_ma_high = %d, \
			temp_cool_fastchg_current_ma_low = %d, \
			little_cool_bat_decidegc = %d, temp_little_cool_vfloat_mv = %d, \
			temp_little_cool_fastchg_current_ma = %d, \
			normal_bat_decidegc = %d, warm_bat_decidegc = %d, \
			temp_warm_vfloat_mv = %d, temp_warm_fastchg_current_ma = %d, \
			hot_bat_decidegc = %d, non_standard_vfloat_mv = %d, \
			non_standard_fastchg_current_ma = %d, max_chg_time_sec = %d, \
			charger_hv_thr = %d, charger_lv_thr = %d, vbatt_full_thr = %d, \
			vbatt_hv_thr = %d, vfloat_step_mv = %d, vooc_project = %d, \
			suspend_after_full = %d, ext_gauge = %d, sw_vfloat_enable = %d, \
			chip->limits.temp_little_cold_fastchg_current_ma_low = %d,  \
			chip->limits.temp_little_cold_fastchg_current_ma_high = %d, \
			chip->limits.charger_current_vooc_ma_normal = %d, \
			chip->ffc_support = %d\n",
			chip->limits.input_current_charger_ma,
			chip->limits.input_current_usb_ma,
			chip->limits.input_current_led_ma,
			chip->limits.input_current_led_ma_normal,
			chip->limits.input_current_led_ma_warm,
			chip->limits.input_current_led_ma_high,
			chip->limits.temp_normal_fastchg_current_ma,
			chip->limits.temp_normal_vfloat_mv,
			chip->limits.iterm_ma, chip->limits.recharge_mv,
			chip->limits.cold_bat_decidegc, chip->limits.temp_cold_vfloat_mv,
			chip->limits.temp_cold_fastchg_current_ma,
			chip->limits.little_cold_bat_decidegc,
			chip->limits.temp_little_cold_vfloat_mv,
			chip->limits.temp_little_cold_fastchg_current_ma,
			chip->limits.cool_bat_decidegc,chip->limits.temp_cool_vfloat_mv,
			chip->limits.temp_cool_fastchg_current_ma_high,
			chip->limits.temp_cool_fastchg_current_ma_low,
			chip->limits.little_cool_bat_decidegc,
			chip->limits.temp_little_cool_vfloat_mv,
			chip->limits.temp_little_cool_fastchg_current_ma,
			chip->limits.normal_bat_decidegc,
			chip->limits.warm_bat_decidegc, chip->limits.temp_warm_vfloat_mv,
			chip->limits.temp_warm_fastchg_current_ma,
			chip->limits.hot_bat_decidegc, chip->limits.non_standard_vfloat_mv,
			chip->limits.non_standard_fastchg_current_ma,
			chip->limits.max_chg_time_sec, chip->limits.charger_hv_thr,
			chip->limits.charger_lv_thr, chip->limits.vbatt_full_thr,
			chip->limits.vbatt_hv_thr, chip->limits.vfloat_step_mv,
			chip->vooc_project, chip->suspend_after_full,
			chip->external_gauge,
			chip->limits.sw_vfloat_over_protect_enable,
			chip->limits.temp_little_cold_fastchg_current_ma_low,
			chip->limits.temp_little_cold_fastchg_current_ma_high,
			chip->limits.charger_current_vooc_ma_normal,
			chip->ffc_support);
        return 0;
}

#ifdef ODM_WT_EDIT
/*Sidong.Zhao@ODM_WT.BSP.CHG 2019/11/11,config charge parameter*/
void oppo_chg_usb_variables_reset(struct oppo_chg_chip *chip)
{
    chip->limits.temp_little_cold_fastchg_current_ma_high = chip->limits.temp_little_cold_usbchg_current_ma_high;/* 0C ~ 5C */
    chip->limits.temp_little_cold_fastchg_current_ma_low = chip->limits.temp_little_cold_usbchg_current_ma_low;
	chip->limits.temp_little_cold_fastchg_current_ma = chip->limits.temp_little_cold_usbchg_current_ma;
    chip->limits.temp_cool_fastchg_current_ma_high = chip->limits.temp_cool_usbchg_current_ma_high;/* 5C ~ 12C */
    chip->limits.temp_cool_fastchg_current_ma_low = chip->limits.temp_cool_usbchg_current_ma_low;
    chip->limits.temp_little_cool_fastchg_current_ma = chip->limits.temp_little_cool_usbchg_current_ma;/* 12C ~ 16C */
	chip->limits.temp_normal_fastchg_current_ma = chip->limits.temp_normal_usbchg_current_ma;/* 16C ~ 44C */
    chip->limits.temp_warm_fastchg_current_ma = chip->limits.temp_warm_usbchg_current_ma;/* 44C ~ 53C */

	charger_xlog_printk(CHG_LOG_CRTI, "0C-5C:(%d %d) 5C-12C:%d %d 12C-16C:%d 16C-44C:%d 44C-53C:%d",
		chip->limits.temp_little_cold_fastchg_current_ma_low, chip->limits.temp_little_cold_fastchg_current_ma_high,
		chip->limits.temp_cool_fastchg_current_ma_low, chip->limits.temp_cool_fastchg_current_ma_high,
		chip->limits.temp_little_cool_fastchg_current_ma, chip->limits.temp_normal_fastchg_current_ma,
		chip->limits.temp_warm_fastchg_current_ma);
}
#endif /*ODM_WT_EDIT*/
#ifdef ODM_WT_EDIT
/*Junbo.Guo@ODM_WT.BSP.CHG 2019/12/07,9v charger spec*/
static void oppo_chg_set_charger_vol_9v(struct oppo_chg_chip *chip,bool en)
{
	if(!chip->chg_ops->oppo_chg_set_high_vbus)
		return;
	if(!chip->chg_ops->get_charger_subtype)
		return;
	
	 charger_xlog_printk(CHG_LOG_CRTI, "oppo_chg_set_charger_vol_9v = %d\n", en);
	if(chip->chg_ops->get_charger_subtype() !=CHARGER_SUBTYPE_DEFAULT)
			chip->chg_ops->oppo_chg_set_high_vbus(en);
	
}
#endif

#ifdef ODM_WT_EDIT
/*Sidong.Zhao@ODM_WT.BSP.CHG 2019/11/22,debug log*/
static void oppo_chg_set_charging_current(struct oppo_chg_chip *chip)
{
        int charging_current = OPPO_CHG_DEFAULT_CHARGING_CURRENT;
		enum aicl_by voter = CHARGER;

		if(chip->charger_type == POWER_SUPPLY_TYPE_USB) {
			oppo_chg_usb_variables_reset(chip);
		}

        switch (chip->tbatt_status) {
        case BATTERY_STATUS__INVALID:
        case BATTERY_STATUS__REMOVED:
        case BATTERY_STATUS__LOW_TEMP:
        case BATTERY_STATUS__HIGH_TEMP:
                return;
        case BATTERY_STATUS__COLD_TEMP:
                charging_current = chip->limits.temp_cold_fastchg_current_ma;
				voter = TBATT;
                break;
        case BATTERY_STATUS__LITTLE_COLD_TEMP:
                //charging_current = chip->limits.temp_little_cold_fastchg_current_ma;
                if (vbatt_higherthan_4180mv) {
                        charging_current = chip->limits.temp_little_cold_fastchg_current_ma_low;
                } else {
                        charging_current = chip->limits.temp_little_cold_fastchg_current_ma_high;
                }
				voter = TBATT;
                charger_xlog_printk(CHG_LOG_CRTI, "vbatt_higherthan_4180mv [%d], charging_current[%d]\n", vbatt_higherthan_4180mv, charging_current);
                break;
        case BATTERY_STATUS__COOL_TEMP:
                if (vbatt_higherthan_4180mv) {
                        charging_current = chip->limits.temp_cool_fastchg_current_ma_low;
                } else {
                        charging_current = chip->limits.temp_cool_fastchg_current_ma_high;
                }
				voter = TBATT;
                charger_xlog_printk(CHG_LOG_CRTI, "vbatt_higherthan_4180mv [%d], charging_current[%d]\n", vbatt_higherthan_4180mv, charging_current);
                break;
        case BATTERY_STATUS__LITTLE_COOL_TEMP:
                charging_current = chip->limits.temp_little_cool_fastchg_current_ma;
				voter = TBATT;
                break;
        case BATTERY_STATUS__NORMAL:
                charging_current = chip->limits.temp_normal_fastchg_current_ma;
                break;
        case BATTERY_STATUS__WARM_TEMP:
                charging_current = chip->limits.temp_warm_fastchg_current_ma;
				voter = TBATT;
                break;
        default:
                break;
        }
#ifdef ODM_WT_EDIT
		/*Junbo.Guo@ODM_WT.BSP.CHG 2019/12/07,9v charger spec*/
		if((chip->is_double_charger_support)&&(chip->chg_ops->get_charger_subtype() !=CHARGER_SUBTYPE_DEFAULT)){

			if(chip->tbatt_status==BATTERY_STATUS__NORMAL){
				if(chip->jeita_sm == BATT_TEMP_EXTEND_T0_TO_T1){
					charging_current=chip->limits.batt_temp_extend_t0_to_t1_ichg;
					voter = TBATT;
				}else if(chip->jeita_sm == BATT_TEMP_EXTEND_T1_TO_T2){
					if (vbatt_higherthan_4180mv){
						charging_current=chip->limits.batt_temp_extend_t1_to_t2_ichg_part2;
						voter = TBATT;
					}else{
						charging_current=chip->limits.batt_temp_extend_t1_to_t2_ichg_part1;
						voter = TBATT;
					}
				}else if(chip->jeita_sm == BATT_TEMP_EXTEND_T2_TO_T3){
					charging_current=chip->limits.batt_temp_extend_t2_to_t3_ichg;
					voter = TBATT;
				}else if(chip->jeita_sm == BATT_TEMP_EXTEND_T3_TO_T4){
					charging_current=chip->limits.batt_temp_extend_t3_to_t4_ichg;
					voter = TBATT;
				}else if(chip->jeita_sm == BATT_TEMP_EXTEND_T4_TO_T5){
					charging_current=chip->limits.batt_temp_extend_t4_to_t5_ichg;
					voter = TBATT;
				}
			}

			if(chip->jeita_sm>=BATT_TEMP_EXTEND_T4_TO_T5){
				oppo_chg_set_charger_vol_9v(chip,0);
			}else{
				oppo_chg_set_charger_vol_9v(chip,1);
			}
		
		}
#endif

        if ((!chip->authenticate) && (charging_current > chip->limits.non_standard_fastchg_current_ma)) {
                charging_current = chip->limits.non_standard_fastchg_current_ma;
                charger_xlog_printk(CHG_LOG_CRTI, "no high battery, set charging current = %d\n", chip->limits.non_standard_fastchg_current_ma);
        }

        if (oppo_short_c_batt_is_prohibit_chg(chip)) {
                if (charging_current > chip->limits.short_c_bat_fastchg_current_ma) {
                        charging_current = chip->limits.short_c_bat_fastchg_current_ma;
                        charger_xlog_printk(CHG_LOG_CRTI, "short circuit battery, set charging current = %d\n",
					            chip->limits.short_c_bat_fastchg_current_ma);
                }
        }

	if ((chip->chg_ctrl_by_lcd) && (chip->led_on) && (charging_current > chip->limits.temp_warm_fastchg_current_ma_led_on)) {
		if (chip->tbatt_status == BATTERY_STATUS__WARM_TEMP) {
			charging_current = chip->limits.temp_warm_fastchg_current_ma_led_on;
			voter = LCD;
		}
	}
#ifdef ODM_WT_EDIT
/*Sidong.Zhao@ODM_WT.BSP.CHG 2019/11/27,ap thermal state machine*/
#if 0
	if (chip->led_on) {
		if ((chip->ap_sm == AP_TEMP_ABOVE_T3)
			&& (chip->limits.ap_temp_above_t3_ichg < charging_current)) {
			charging_current = chip->limits.ap_temp_above_t3_ichg;
			voter = AP;
		} else if ((chip->ap_sm == AP_TEMP_T2_TO_T3)
			&& (chip->limits.ap_temp_t2_to_t3_ichg < charging_current)) {
			charging_current = chip->limits.ap_temp_t2_to_t3_ichg;
			voter = AP;
		} else if ((chip->ap_sm == AP_TEMP_T1_TO_T2)
			&& (chip->limits.ap_temp_t1_to_t2_ichg < charging_current)) {
			charging_current = chip->limits.ap_temp_t1_to_t2_ichg;
			voter = AP;
		} else if ((chip->ap_sm == AP_TEMP_T0_TO_T1)
			&& (chip->limits.ap_temp_t0_to_t1_ichg < charging_current)) {
			charging_current = chip->limits.ap_temp_t0_to_t1_ichg;
			voter = AP;
		} else if ((chip->ap_sm == AP_TEMP_BELOW_T0)
			&& (chip->limits.ap_temp_below_t0_ichg < charging_current)) {
			charging_current = chip->limits.ap_temp_below_t0_ichg;
			voter = AP;
		}
	}
#endif
	if ((chip->chg_ctrl_by_calling) && (chip->calling_on) && (chip->limits.charge_current_calling_ma < charging_current)) {
		charging_current = chip->limits.charge_current_calling_ma;
		voter = PHONECALL;
	}
	
/*Shouli.Wang@ODM_WT.BSP.CHG 2019/12/11, limit charging current when LCD on for monet X*/
	if ((chip->chg_ctrl_by_lcd) && (chip->led_on) && chip->is_double_charger_support) {
		if (chip->led_temp_status == LED_TEMP_STATUS__HIGH) {
			if (charging_current > chip->limits.input_current_led_ma_high)
				charging_current = chip->limits.input_current_led_ma_high;
				voter = LCD;
		} else if (chip->led_temp_status == LED_TEMP_STATUS__WARM) {
			if (charging_current > chip->limits.input_current_led_ma_warm)
				charging_current = chip->limits.input_current_led_ma_warm;
				voter = LCD;
		} else {
			if (charging_current > chip->limits.input_current_led_ma_normal)
				charging_current = chip->limits.input_current_led_ma_normal;
				voter = LCD;
		}
	}
#endif /*ODM_WT_EDIT*/
        if (charging_current == 0) {
                return;
        }

    charger_xlog_printk(CHG_LOG_CRTI, "[%d]jeita_sm:[%d]ap_sm:[%d]tbatt:[%d] %s set ichg:%d\n", chip->is_double_charger_support,chip->jeita_sm,chip->ap_sm,chip->tbatt_status,
		aicr_voter[voter], charging_current);

	chip->chg_ops->charging_current_write_fast(charging_current);

}
#else
static void oppo_chg_set_charging_current(struct oppo_chg_chip *chip)
{
        int charging_current = OPPO_CHG_DEFAULT_CHARGING_CURRENT;

#ifdef ODM_WT_EDIT
/*Sidong.Zhao@ODM_WT.BSP.CHG 2019/11/11,config charge parameter*/
		if(chip->charger_type == POWER_SUPPLY_TYPE_USB)
			oppo_chg_usb_variables_reset(chip);
#endif /*ODM_WT_EDIT*/

        switch (chip->tbatt_status) {
        case BATTERY_STATUS__INVALID:
        case BATTERY_STATUS__REMOVED:
        case BATTERY_STATUS__LOW_TEMP:
        case BATTERY_STATUS__HIGH_TEMP:
                return;
        case BATTERY_STATUS__COLD_TEMP:
                charging_current = chip->limits.temp_cold_fastchg_current_ma;
                break;
        case BATTERY_STATUS__LITTLE_COLD_TEMP:
                //charging_current = chip->limits.temp_little_cold_fastchg_current_ma;
                if (vbatt_higherthan_4180mv) {
                        charging_current = chip->limits.temp_little_cold_fastchg_current_ma_low;
                } else {
                        charging_current = chip->limits.temp_little_cold_fastchg_current_ma_high;
                }
                charger_xlog_printk(CHG_LOG_CRTI, "vbatt_higherthan_4180mv [%d], charging_current[%d]\n", vbatt_higherthan_4180mv, charging_current);
                break;
        case BATTERY_STATUS__COOL_TEMP:
                if (vbatt_higherthan_4180mv) {
                        charging_current = chip->limits.temp_cool_fastchg_current_ma_low;
                } else {
                        charging_current = chip->limits.temp_cool_fastchg_current_ma_high;
                }
                charger_xlog_printk(CHG_LOG_CRTI, "vbatt_higherthan_4180mv [%d], charging_current[%d]\n", vbatt_higherthan_4180mv, charging_current);
                break;
        case BATTERY_STATUS__LITTLE_COOL_TEMP:
                charging_current = chip->limits.temp_little_cool_fastchg_current_ma;
                break;
        case BATTERY_STATUS__NORMAL:
                charging_current = chip->limits.temp_normal_fastchg_current_ma;
                break;
        case BATTERY_STATUS__WARM_TEMP:
                charging_current = chip->limits.temp_warm_fastchg_current_ma;
                break;
        default:
                break;
        }

        if ((!chip->authenticate) && (charging_current > chip->limits.non_standard_fastchg_current_ma)) {
                charging_current = chip->limits.non_standard_fastchg_current_ma;
                charger_xlog_printk(CHG_LOG_CRTI, "no high battery, set charging current = %d\n", chip->limits.non_standard_fastchg_current_ma);
        }

        if (oppo_short_c_batt_is_prohibit_chg(chip)) {
                if (charging_current > chip->limits.short_c_bat_fastchg_current_ma) {
                        charging_current = chip->limits.short_c_bat_fastchg_current_ma;
                        charger_xlog_printk(CHG_LOG_CRTI, "short circuit battery, set charging current = %d\n",
					            chip->limits.short_c_bat_fastchg_current_ma);
                }
        }

	if ((chip->chg_ctrl_by_lcd) && (chip->led_on) && (charging_current > chip->limits.temp_warm_fastchg_current_ma_led_on)) {
		if (chip->tbatt_status == BATTERY_STATUS__WARM_TEMP)
			charging_current = chip->limits.temp_warm_fastchg_current_ma_led_on;

		charger_xlog_printk(CHG_LOG_CRTI, "[BATTERY]LED ON, charging current: %d\n", charging_current);
	}
        if (charging_current == 0) {
                return;
        }
#ifdef ODM_WT_EDIT
//Sidong.Zhao@ODM_WT.BSP.CHG, 2019/11/11, print charge current log
    charger_xlog_printk(CHG_LOG_CRTI, "double_charger:%d tbatt_status:%d charging_current:%d\n", chip->is_double_charger_support,chip->tbatt_status,charging_current);
#endif /*ODM_WT_EDIT*/
        chip->chg_ops->charging_current_write_fast(charging_current);
}
#endif /*ODM_WT_EDIT*/

#ifdef ODM_WT_EDIT
/*Sidong.Zhao@ODM_WT.BSP.CHG 2019/11/14,check AP thermal and set charge current*/
#define AP_TEMP_T0 37000
#define AP_TEMP_T0_MINUS_X 35000
#define AP_TEMP_T1 39000
#define AP_TEMP_T1_MINUS_X 37000
#define AP_TEMP_T2 41000
#define AP_TEMP_T2_MINUS_X 39000
#define AP_TEMP_T3 43000
#define AP_TEMP_T3_MINUS_X 41000

#define AP_TEMP_BELOW_T0_ICHG 3000
#define AP_TEMP_T0_TO_T1_ICHG 1500
#define AP_TEMP_T1_TO_T2_ICHG 1000
#define AP_TEMP_T2_TO_T3_ICHG 800
#define AP_TEMP_ABOVE_T3_ICHG 500

#define BATT_TEMP_EXTEND_T0 160
#define BATT_TEMP_EXTEND_T0_MINUS_X 140
#define BATT_TEMP_EXTEND_T1 220
#define BATT_TEMP_EXTEND_T1_MINUS_X 200
#define BATT_TEMP_EXTEND_T2 340
#define BATT_TEMP_EXTEND_T2_MINUS_X 320
#define BATT_TEMP_EXTEND_T3 370
#define BATT_TEMP_EXTEND_T3_MINUS_X 350
#define BATT_TEMP_EXTEND_T4 400
#define BATT_TEMP_EXTEND_T4_MINUS_X 380
#define BATT_TEMP_EXTEND_T5 450
#define BATT_TEMP_EXTEND_T5_MINUS_X 430


#define BATT_TEMP_EXTEND_T0_TO_T1_ICHG 3000
#define BATT_TEMP_EXTEND_T1_TO_T2_ICHG_PART1 3400 //VBAT<=4.18 
#define BATT_TEMP_EXTEND_T1_TO_T2_ICHG_PART2 3200  //VBAT>4.18 
#define BATT_TEMP_EXTEND_T2_TO_T3_ICHG 3200
#define BATT_TEMP_EXTEND_T3_TO_T4_ICHG 2800
#define BATT_TEMP_EXTEND_T4_TO_T5_ICHG 2700 //need vbus to 5v

#define CHARGE_CURRENT_CALLING_MA 1000
extern int get_ap_temp(void);
#endif /*ODM_WT_EDIT*/
#ifdef ODM_WT_EDIT
/*Sidong.Zhao@ODM_WT.BSP.CHG 2019/11/22,debug log*/
static void oppo_chg_set_input_current_limit(struct oppo_chg_chip *chip)
{
	int current_limit = 0;
	enum aicl_by voter = CHARGER;

	switch (chip->charger_type) {
	case POWER_SUPPLY_TYPE_UNKNOWN:
		return;
	case POWER_SUPPLY_TYPE_USB:
		current_limit = chip->limits.input_current_usb_ma;
		break;
	case POWER_SUPPLY_TYPE_USB_DCP:
		current_limit = chip->limits.input_current_charger_ma;
		break;
	case POWER_SUPPLY_TYPE_USB_CDP:
		current_limit = chip->limits.input_current_cdp_ma;
		break;
	default:
		return;
	}

	if ((chip->chg_ctrl_by_lcd) && (chip->led_on) && !chip->is_double_charger_support) {
		if (chip->led_temp_status == LED_TEMP_STATUS__HIGH) {
			if (current_limit > chip->limits.input_current_led_ma_high)
				current_limit = chip->limits.input_current_led_ma_high;
				voter = LCD;
		} else if (chip->led_temp_status == LED_TEMP_STATUS__WARM) {
			if (current_limit > chip->limits.input_current_led_ma_warm)
				current_limit = chip->limits.input_current_led_ma_warm;
				voter = LCD;
		} else {
			if (current_limit > chip->limits.input_current_led_ma_normal)
				current_limit = chip->limits.input_current_led_ma_normal;
				voter = LCD;
		}
		if ((chip->chg_ctrl_by_camera) && (chip->camera_on) && (current_limit > chip->limits.input_current_camera_ma)) {
			current_limit = chip->limits.input_current_camera_ma;
			voter = CAMERA;
		}

	} else if ((chip->chg_ctrl_by_camera) && (chip->camera_on) && (current_limit > chip->limits.input_current_camera_ma)) {
		current_limit = chip->limits.input_current_camera_ma;
		voter = CAMERA;
	}

#if 0
	if ((chip->chg_ctrl_by_calling) && (chip->calling_on) && (current_limit > chip->limits.input_current_calling_ma)) {
		current_limit = chip->limits.input_current_calling_ma;
		voter = PHONECALL;
	}
#endif

	 if (chip->chg_ctrl_by_vooc && chip->vbatt_num == 2
		&& oppo_vooc_get_fast_chg_type() == CHARGER_SUBTYPE_FASTCHG_VOOC && oppo_vooc_get_fastchg_started() == true) {
		if(chip->led_on) {
			current_limit = 1800;
		} else {
			if(chip->vooc_temp_status == VOOC_TEMP_STATUS__HIGH) {
				current_limit = chip->limits.input_current_vooc_ma_high;
			} else if(chip->vooc_temp_status == VOOC_TEMP_STATUS__WARM) {
				current_limit = chip->limits.input_current_vooc_ma_warm;
			} else if(chip->vooc_temp_status == VOOC_TEMP_STATUS__NORMAL) {
				current_limit = chip->limits.input_current_vooc_ma_normal;
			}
		}

		chg_err("chg_ctrl_by_vooc,  led_on = %d,  current_limit[%d], chip->vooc_temp_status[%d]\n", chip->led_on, current_limit, chip->vooc_temp_status);
		if ( chip->chg_ops->input_current_ctrl_by_vooc_write) {
			chip->chg_ops->input_current_ctrl_by_vooc_write(current_limit);
			return;
		}
	}

	charger_xlog_printk(CHG_LOG_CRTI, "led_on:%d, led_temp_status:%d, camera_on:%d, calling_on:%d, %s set aicr:%d\n", chip->led_on, chip->led_temp_status,
		chip->camera_on, chip->calling_on, aicr_voter[voter], current_limit);

		chip->chg_ops->input_current_write(current_limit);
}
#else
static void oppo_chg_set_input_current_limit(struct oppo_chg_chip *chip)
{
	int current_limit = 0;

	switch (chip->charger_type) {
	case POWER_SUPPLY_TYPE_UNKNOWN:
		return;
	case POWER_SUPPLY_TYPE_USB:
		current_limit = chip->limits.input_current_usb_ma;
		break;
	case POWER_SUPPLY_TYPE_USB_DCP:
		current_limit = chip->limits.input_current_charger_ma;
		break;
	case POWER_SUPPLY_TYPE_USB_CDP:
		current_limit = chip->limits.input_current_cdp_ma;
		break;
	default:
		return;
	}

	if ((chip->chg_ctrl_by_lcd) && (chip->led_on)) {
		if (chip->led_temp_status == LED_TEMP_STATUS__HIGH) {
			if (current_limit > chip->limits.input_current_led_ma_high)
				current_limit = chip->limits.input_current_led_ma_high;
		} else if (chip->led_temp_status == LED_TEMP_STATUS__WARM) {
			if (current_limit > chip->limits.input_current_led_ma_warm)
				current_limit = chip->limits.input_current_led_ma_warm;
		} else {
			if (current_limit > chip->limits.input_current_led_ma_normal)
				current_limit = chip->limits.input_current_led_ma_normal;
		}
		charger_xlog_printk(CHG_LOG_CRTI, "[BATTERY]LED STATUS CHANGED, IS ON\n");
		if ((chip->chg_ctrl_by_camera) && (chip->camera_on) && (current_limit > chip->limits.input_current_camera_ma)) {
			current_limit = chip->limits.input_current_camera_ma;
			charger_xlog_printk(CHG_LOG_CRTI, "[BATTERY]CAMERA STATUS CHANGED, IS ON\n");
		}
	} else if ((chip->chg_ctrl_by_camera) && (chip->camera_on) && (current_limit > chip->limits.input_current_camera_ma)) {
		current_limit = chip->limits.input_current_camera_ma;
		charger_xlog_printk(CHG_LOG_CRTI, "[BATTERY]CAMERA STATUS CHANGED, IS ON\n");
	}

	if ((chip->chg_ctrl_by_calling) && (chip->calling_on) && (current_limit > chip->limits.input_current_calling_ma)) {
		current_limit = chip->limits.input_current_calling_ma;
		charger_xlog_printk(CHG_LOG_CRTI, "[BATTERY]calling STATUS CHANGED, IS ON\n");
	}

	 if (chip->chg_ctrl_by_vooc && chip->vbatt_num == 2 
		&& oppo_vooc_get_fast_chg_type() == CHARGER_SUBTYPE_FASTCHG_VOOC && oppo_vooc_get_fastchg_started() == true) {
		if(chip->led_on) {
			current_limit = 1800;
		} else {
			if(chip->vooc_temp_status == VOOC_TEMP_STATUS__HIGH) {
				current_limit = chip->limits.input_current_vooc_ma_high;
			} else if(chip->vooc_temp_status == VOOC_TEMP_STATUS__WARM) {
				current_limit = chip->limits.input_current_vooc_ma_warm;
			} else if(chip->vooc_temp_status == VOOC_TEMP_STATUS__NORMAL) {
				current_limit = chip->limits.input_current_vooc_ma_normal;
			}
		}

		chg_err("chg_ctrl_by_vooc,  led_on = %d,  current_limit[%d], chip->vooc_temp_status[%d]\n", chip->led_on, current_limit, chip->vooc_temp_status);
		if ( chip->chg_ops->input_current_ctrl_by_vooc_write) {
			chip->chg_ops->input_current_ctrl_by_vooc_write(current_limit);
			return;
		}
	}

	charger_xlog_printk(CHG_LOG_CRTI, " led_on = %d, current_limit = %d, led_temp_status = %d\n", chip->led_on, current_limit, chip->led_temp_status);	
	chip->chg_ops->input_current_write(current_limit);
}
#endif /*ODM_WT_EDIT*/
#ifdef ODM_WT_EDIT
/*Sidong.Zhao@ODM_WT.BSP.CHG 2019/11/14,monitor thermal and set charge current*/
static void oppo_chg_check_thermal_on_ichging(struct oppo_chg_chip *chip)
{
	OPPO_CHG_AP_TEMP_STAT pre_ap_sm = chip->ap_sm;
	OPPO_CHG_BATT_TEMP_EXTEND_STAT pre_jeita_sm =  chip->jeita_sm;
	int batt_temp = chip->temperature;
	
	if (chip->ap_temp >= chip->limits.ap_temp_t3) {
		chip->ap_sm = AP_TEMP_ABOVE_T3;
	} else if (chip->ap_temp >= chip->limits.ap_temp_t2) {
		if ((AP_TEMP_ABOVE_T3 != chip->ap_sm)
			|| (chip->ap_temp <= chip->limits.ap_temp_t3_minus_x)) {
			chip->ap_sm = AP_TEMP_T2_TO_T3;
		}
	} else if (chip->ap_temp >= chip->limits.ap_temp_t1) {
		if ((AP_TEMP_T2_TO_T3 != chip->ap_sm)
			|| (chip->ap_temp <= chip->limits.ap_temp_t2_minus_x)) {
			chip->ap_sm = AP_TEMP_T1_TO_T2;
		}
	} else if (chip->ap_temp >= chip->limits.ap_temp_t0) {
		if ((AP_TEMP_T1_TO_T2 != chip->ap_sm)
			|| (chip->ap_temp <= chip->limits.ap_temp_t1_minus_x)) {
			chip->ap_sm = AP_TEMP_T0_TO_T1;
		}
	} else {
		if ((AP_TEMP_T0_TO_T1 != chip->ap_sm)
			|| (chip->ap_temp <= chip->limits.ap_temp_t0_minus_x)) {
			chip->ap_sm = AP_TEMP_BELOW_T0;
		}
	}

	if (chip->ap_sm != pre_ap_sm) {
		charger_xlog_printk(CHG_LOG_CRTI, "pre_ap_sm:%d, now_ap_sm:%d, ap_temp:%d\n", pre_ap_sm, chip->ap_sm, chip->ap_temp);
		oppo_chg_set_charging_current(chip);
	}
	
	if(chip->is_double_charger_support){
		if (batt_temp >= BATT_TEMP_EXTEND_T5) {
			chip->jeita_sm = BATT_TEMP_EXTEND_ABOVE_T5;
		} else if (batt_temp >= BATT_TEMP_EXTEND_T4) {
			if ((BATT_TEMP_EXTEND_ABOVE_T5 != chip->jeita_sm )
				|| (batt_temp <= BATT_TEMP_EXTEND_T5_MINUS_X)) {
				chip->jeita_sm	= BATT_TEMP_EXTEND_T4_TO_T5;
			}
		} else if (batt_temp >= BATT_TEMP_EXTEND_T3) {
			if ((BATT_TEMP_EXTEND_T4_TO_T5 != chip->jeita_sm )
				|| (batt_temp <= BATT_TEMP_EXTEND_T4_MINUS_X)) {
				chip->jeita_sm	= BATT_TEMP_EXTEND_T3_TO_T4;
			}
		} else if (batt_temp >= BATT_TEMP_EXTEND_T2) {
			if ((BATT_TEMP_EXTEND_T3_TO_T4 != chip->jeita_sm )
				|| (batt_temp <= BATT_TEMP_EXTEND_T3_MINUS_X)) {
				chip->jeita_sm	= BATT_TEMP_EXTEND_T2_TO_T3;
			}
		} else if (batt_temp >= BATT_TEMP_EXTEND_T1) {
			if ((BATT_TEMP_EXTEND_T2_TO_T3 != chip->jeita_sm )
				|| (batt_temp <= BATT_TEMP_EXTEND_T2_MINUS_X)) {
				chip->jeita_sm	= BATT_TEMP_EXTEND_T1_TO_T2;
			}
		} else if (batt_temp >= BATT_TEMP_EXTEND_T0) {
			if ((BATT_TEMP_EXTEND_T1_TO_T2 != chip->jeita_sm )
				|| (batt_temp <= BATT_TEMP_EXTEND_T1_MINUS_X)) {
				chip->jeita_sm	= BATT_TEMP_EXTEND_T0_TO_T1;
			}
		}else {
			if ((BATT_TEMP_EXTEND_T0_TO_T1 != chip->jeita_sm)
				|| (batt_temp <= BATT_TEMP_EXTEND_T0_MINUS_X)) {
				chip->jeita_sm = BATT_TEMP_EXTEND_BELOW_T0;
			}
		}

		if (chip->jeita_sm != pre_jeita_sm) {
			charger_xlog_printk(CHG_LOG_CRTI, "pre_jeita_sm:%d, now_jeita_sm:%d, bat_temp:%d\n", pre_jeita_sm, chip->jeita_sm, batt_temp);
			oppo_chg_set_charging_current(chip);
		}
   }
}
#endif /*ODM_WT_EDIT*/

static int oppo_chg_get_float_voltage(struct oppo_chg_chip *chip)
{
        int flv = chip->limits.temp_normal_vfloat_mv;
        switch (chip->tbatt_status) {
        case BATTERY_STATUS__INVALID:
        case BATTERY_STATUS__REMOVED:
        case BATTERY_STATUS__LOW_TEMP:
        case BATTERY_STATUS__HIGH_TEMP:
                return flv;
        case BATTERY_STATUS__COLD_TEMP:
                flv = chip->limits.temp_cold_vfloat_mv;
                break;
        case BATTERY_STATUS__LITTLE_COLD_TEMP:
                flv = chip->limits.temp_little_cold_vfloat_mv;
                break;
        case BATTERY_STATUS__COOL_TEMP:
                flv = chip->limits.temp_cool_vfloat_mv;
                break;
        case BATTERY_STATUS__LITTLE_COOL_TEMP:
                flv = chip->limits.temp_little_cool_vfloat_mv;
                break;
        case BATTERY_STATUS__NORMAL:
                flv = chip->limits.temp_normal_vfloat_mv;
                break;
        case BATTERY_STATUS__WARM_TEMP:
                flv = chip->limits.temp_warm_vfloat_mv;
                break;
        default:
                break;
        }

        if (oppo_short_c_batt_is_prohibit_chg(chip) && flv > chip->limits.short_c_bat_vfloat_mv) {
                flv = chip->limits.short_c_bat_vfloat_mv;
        }
        return flv;
}

static void oppo_chg_set_float_voltage(struct oppo_chg_chip *chip)
{
        int flv = oppo_chg_get_float_voltage(chip);

        if ((!chip->authenticate) && (flv > chip->limits.non_standard_vfloat_mv)) {
                flv = chip->limits.non_standard_vfloat_mv;
                charger_xlog_printk(CHG_LOG_CRTI, "no high battery, set float voltage = %d\n", chip->limits.non_standard_vfloat_mv);
        }
#ifdef ODM_WT_EDIT
		//Junbo.Guo@ODM_WT.BSP.CHG, 2019/11/11, Modify for subcharger
		if(chip->is_double_charger_support){
			chip->chg_ops->float_voltage_write(flv * chip->vbatt_num);
			chip->sub_chg_ops->float_voltage_write(flv * chip->vbatt_num);
		}else{
			chip->chg_ops->float_voltage_write(flv * chip->vbatt_num);
		}
#else
        chip->chg_ops->float_voltage_write(flv * chip->vbatt_num);
#endif
        chip->limits.vfloat_sw_set = flv;
}

#define VFLOAT_OVER_NUM                2
static void oppo_chg_vfloat_over_check(struct oppo_chg_chip *chip)
{
        if (!chip->mmi_chg) {
                return;
        }
        if (chip->charging_state == CHARGING_STATUS_FULL) {
                return;
        }
        if (oppo_vooc_get_allow_reading() == false) {
                return;
        }
        if (chip->check_batt_full_by_sw && (chip->limits.sw_vfloat_over_protect_enable == false)) {
                return;
        }

		if (oppo_vooc_get_fastchg_ing() == true) {
				return;
		}

		//if (!((oppo_vooc_get_fastchg_to_normal()== true) || (oppo_vooc_get_fastchg_to_warm() == true))) {

			if(chip->limits.sw_vfloat_over_protect_enable) {
					if ((chip->batt_volt >= chip->limits.cold_vfloat_over_sw_limit
						&& chip->tbatt_status == BATTERY_STATUS__COLD_TEMP) ||
					(chip->batt_volt >= chip->limits.little_cold_vfloat_over_sw_limit
						&& chip->tbatt_status == BATTERY_STATUS__LITTLE_COLD_TEMP) ||
					(chip->batt_volt >= chip->limits.cool_vfloat_over_sw_limit
						&& chip->tbatt_status == BATTERY_STATUS__COOL_TEMP) ||
					(chip->batt_volt >= chip->limits.little_cool_vfloat_over_sw_limit
						&& chip->tbatt_status == BATTERY_STATUS__LITTLE_COOL_TEMP) ||
					(chip->batt_volt >= chip->limits.normal_vfloat_over_sw_limit
						&& chip->tbatt_status == BATTERY_STATUS__NORMAL) ||
					(chip->batt_volt >= chip->limits.warm_vfloat_over_sw_limit
						&& chip->tbatt_status == BATTERY_STATUS__WARM_TEMP)	||
					(!chip->authenticate && (chip->batt_volt >= chip->limits.non_standard_vfloat_over_sw_limit))) {
					chip->limits.vfloat_over_counts++;
					if (chip->limits.vfloat_over_counts > VFLOAT_OVER_NUM) {
						chip->limits.vfloat_over_counts = 0;
						chip->limits.vfloat_sw_set -= chip->limits.vfloat_step_mv;
						chip->chg_ops->float_voltage_write(chip->limits.vfloat_sw_set * chip->vbatt_num);
#ifdef ODM_WT_EDIT
/*Shouli.Wang@ODM_WT.BSP.CHG 2019/12/10, add for dual charger*/
						if(chip->is_double_charger_support)
							chip->sub_chg_ops->float_voltage_write(chip->limits.vfloat_sw_set * chip->vbatt_num);
#endif /*ODM_WT_EDIT*/
						charger_xlog_printk(CHG_LOG_CRTI, "bat_volt:%d, tbatt:%d, sw_vfloat_set:%d\n",chip->batt_volt, chip->tbatt_status, chip->limits.vfloat_sw_set);
					}
				} else {
					chip->limits.vfloat_over_counts = 0;
				}
				return;
			}
		
}

static void oppo_chg_check_vbatt_higher_than_4180mv(struct oppo_chg_chip *chip)
{
        static bool vol_high_pre = false;
        static int lower_count = 0, higher_count = 0;
        static int tbatt_status_pre = BATTERY_STATUS__NORMAL;

        if (!chip->mmi_chg) {
                vbatt_higherthan_4180mv = false;
                vol_high_pre = false;
                lower_count = 0;
                higher_count = 0;
                return;
        }
		
		if (oppo_vooc_get_fastchg_started() == true) {		
			return;
		}

		if (tbatt_status_pre != chip->tbatt_status) {
			tbatt_status_pre = chip->tbatt_status;
			vbatt_higherthan_4180mv = false;
			vol_high_pre = false;
			lower_count = 0;
			higher_count = 0;
		}

		//if (chip->batt_volt >(chip->vbatt_num * 4180)) {
		if (chip->batt_volt > 4180) {
                higher_count++;
                if (higher_count > 2) {
                        lower_count = 0;
                        higher_count = 3;
                        vbatt_higherthan_4180mv = true;
                }
	        } else if (vbatt_higherthan_4180mv) {
				//if (chip->batt_volt >(chip->vbatt_num * 4000)) {
				if (chip->batt_volt < 4000) {
	                        lower_count++;
	                        if (lower_count > 2) {
	                                lower_count = 3;
	                                higher_count = 0;
	                                vbatt_higherthan_4180mv = false;
	                        }
	                }
	        }
	        /*chg_err(" tbatt_status:%d,batt_volt:%d,vol_high_pre:%d,vbatt_higherthan_4180mv:%d\n",*/
	                /*chip->tbatt_status,chip->batt_volt,vol_high_pre,vbatt_higherthan_4180mv);*/
	        if (vol_high_pre != vbatt_higherthan_4180mv) {
                vol_high_pre = vbatt_higherthan_4180mv;
                oppo_chg_set_charging_current(chip);
        }
}

#define TEMP_FFC_COUNTS 2
static void oppo_chg_check_ffc_temp_status(struct oppo_chg_chip *chip)
{
        int batt_temp = chip->temperature;
		OPPO_CHG_FFC_TEMP_STATUS temp_status = chip->ffc_temp_status;
		static int high_counts = 0, warm_counts = 0, normal_counts = 0;
		static int low_counts = 0;

		if (batt_temp >= chip->limits.ffc2_temp_high_decidegc) {                               /*>=40C*/
                //tled_status = FFC_TEMP_STATUS__HIGH;
                high_counts ++;
				if(high_counts >= TEMP_FFC_COUNTS)
				{
					low_counts = 0;
					high_counts = 0;
					warm_counts = 0;
					normal_counts = 0;
					temp_status = FFC_TEMP_STATUS__HIGH;
				}
        } else if (batt_temp >= chip->limits.ffc2_temp_warm_decidegc) {               /*>=35C && < 40*/
                //temp_status = FFC_TEMP_STATUS__WARM;
                warm_counts ++;

				if (warm_counts >= TEMP_FFC_COUNTS)
					{
						low_counts = 0;
						high_counts = 0;
						warm_counts = 0;
						normal_counts = 0;
						temp_status = FFC_TEMP_STATUS__WARM;
				}
        } else if (batt_temp >= chip->limits.ffc2_temp_low_decidegc) {	/*>=16C&&<35C*/
                //temp_status = FFC_TEMP_STATUS__NORMAL;
                normal_counts ++;
				if (normal_counts >= TEMP_FFC_COUNTS)
				{
					low_counts = 0;
					high_counts = 0;
					warm_counts = 0;
					normal_counts = 0;
					temp_status = FFC_TEMP_STATUS__NORMAL;

				}
        } else {	/*<16C*/
		low_counts++;
		if (low_counts >= TEMP_FFC_COUNTS) {
			low_counts = 0;
			high_counts = 0;
			warm_counts = 0;
			normal_counts = 0;
			temp_status = FFC_TEMP_STATUS__LOW;
		}
	}

    if (temp_status != chip->ffc_temp_status) {
        chip->ffc_temp_status = temp_status;
    }

}

void oppo_chg_turn_on_ffc1(struct oppo_chg_chip *chip)
{
	if (!chip->authenticate) {
		return;
	}

	if (!chip->mmi_chg) {
		return;
	}

	if (oppo_vooc_get_allow_reading() == false) {
		return;
	}

	chip->chg_ops->hardware_init();

#ifdef ODM_WT_EDIT
//Junbo.Guo@ODM_WT.BSP.CHG, 2019/11/11, Modify for subcharger
	if(chip->is_double_charger_support){
		chip->sub_chg_ops->hardware_init();	
	}
#endif

	if (chip->stop_chg == 0 && (chip->charger_type == POWER_SUPPLY_TYPE_USB || chip->charger_type == POWER_SUPPLY_TYPE_USB_CDP)) {
		chip->chg_ops->charger_suspend();
	}

	if (chip->check_batt_full_by_sw) {
		chip->chg_ops->set_charging_term_disable();
#ifdef ODM_WT_EDIT
//Junbo.Guo@ODM_WT.BSP.CHG, 2019/11/11, Modify for subcharger
		if(chip->is_double_charger_support){
			chip->sub_chg_ops->set_charging_term_disable();
        }
#endif		
	}

	pr_err("oppo_chg_turn_on_ffc1--------\r\n");
	chip->chg_ctrl_by_lcd = false;
	chip->fastchg_to_ffc = true;
	chip->fastchg_ffc_status = 1;
	chip->chg_ctrl_by_vooc = false;

	if (chip->temperature >= chip->limits.ffc2_temp_warm_decidegc) {
		chip->limits.temp_normal_fastchg_current_ma = chip->limits.ff1_warm_fastchg_ma;
		chip->limits.temp_little_cool_fastchg_current_ma = chip->limits.ff1_warm_fastchg_ma;
	} else {
		chip->limits.temp_normal_fastchg_current_ma = chip->limits.ff1_normal_fastchg_ma;
		chip->limits.temp_little_cool_fastchg_current_ma = chip->limits.ff1_normal_fastchg_ma;
	}

	chip->limits.normal_vfloat_sw_limit = chip->limits.ffc1_normal_vfloat_sw_limit;
	chip->limits.temp_normal_vfloat_mv = chip->limits.ffc_temp_normal_vfloat_mv;
	chip->limits.normal_vfloat_over_sw_limit = chip->limits.ffc_normal_vfloat_over_sw_limit;

	chip->limits.little_cool_vfloat_sw_limit = chip->limits.ffc1_normal_vfloat_sw_limit;
	chip->limits.temp_little_cool_vfloat_mv = chip->limits.ffc_temp_normal_vfloat_mv;
	chip->limits.little_cool_vfloat_over_sw_limit = chip->limits.ffc_normal_vfloat_over_sw_limit;

	oppo_chg_check_tbatt_status(chip);
	oppo_chg_set_float_voltage(chip);
	oppo_chg_set_charging_current(chip);
	oppo_chg_set_input_current_limit(chip);
	chip->chg_ops->term_current_set(chip->limits.iterm_ma);
#ifdef ODM_WT_EDIT
//Junbo.Guo@ODM_WT.BSP.CHG, 2019/11/11, Modify for subcharger
	if(chip->is_double_charger_support){
		chip->sub_chg_ops->term_current_set(chip->limits.iterm_ma+200);	
	}
#endif
}

void oppo_chg_turn_on_ffc2(struct oppo_chg_chip *chip)
{
	if (!chip->authenticate) {
		return;
	}

	if (!chip->mmi_chg) {
		return;
	}

	if (oppo_vooc_get_allow_reading() == false) {
		return;
	}

	chip->chg_ops->hardware_init();
#ifdef ODM_WT_EDIT
//Junbo.Guo@ODM_WT.BSP.CHG, 2019/11/11, Modify for subcharger
	if(chip->is_double_charger_support){
		chip->sub_chg_ops->hardware_init();	
	}
#endif

	if (chip->check_batt_full_by_sw) {
		chip->chg_ops->set_charging_term_disable();
#ifdef ODM_WT_EDIT
//Junbo.Guo@ODM_WT.BSP.CHG, 2019/11/11, Modify for subcharger
		if(chip->is_double_charger_support){
			chip->sub_chg_ops->set_charging_term_disable();
        }
#endif
	}

	pr_err("oppo_chg_turn_on_ffc2--------\r\n");
	chip->chg_ctrl_by_lcd = false;
	chip->fastchg_to_ffc = true;
	chip->fastchg_ffc_status = 2;
	chip->chg_ctrl_by_vooc = false;

	if (chip->temperature >= chip->limits.ffc2_temp_warm_decidegc) {
		chip->limits.temp_normal_fastchg_current_ma = chip->limits.ffc2_warm_fastchg_ma;
		chip->limits.temp_little_cool_fastchg_current_ma = chip->limits.ffc2_warm_fastchg_ma;
	} else {
		chip->limits.temp_normal_fastchg_current_ma = chip->limits.ffc2_normal_fastchg_ma;
		chip->limits.temp_little_cool_fastchg_current_ma = chip->limits.ffc2_normal_fastchg_ma;
	}

	chip->limits.normal_vfloat_sw_limit = chip->limits.ffc2_normal_vfloat_sw_limit;
	chip->limits.temp_normal_vfloat_mv = chip->limits.ffc_temp_normal_vfloat_mv;
	chip->limits.normal_vfloat_over_sw_limit = chip->limits.ffc_normal_vfloat_over_sw_limit;

	chip->limits.little_cool_vfloat_sw_limit = chip->limits.ffc2_normal_vfloat_sw_limit;
	chip->limits.temp_little_cool_vfloat_mv = chip->limits.ffc_temp_normal_vfloat_mv;
	chip->limits.little_cool_vfloat_over_sw_limit = chip->limits.ffc_normal_vfloat_over_sw_limit;

	oppo_chg_check_tbatt_status(chip);
	oppo_chg_set_float_voltage(chip);
	oppo_chg_set_charging_current(chip);
	oppo_chg_set_input_current_limit(chip);
	chip->chg_ops->term_current_set(chip->limits.iterm_ma);
#ifdef ODM_WT_EDIT
//Junbo.Guo@ODM_WT.BSP.CHG, 2019/11/11, Modify for subcharger
	if(chip->is_double_charger_support){
		chip->sub_chg_ops->term_current_set(chip->limits.iterm_ma+200);	
	}
#endif
}

void oppo_chg_turn_on_charging(struct oppo_chg_chip *chip)
{
        if (!chip->authenticate) {
                return;
        }
        if (!chip->mmi_chg) {
                return;
        }
        if (oppo_vooc_get_allow_reading() == false) {
                return;
        }
        chip->chg_ops->hardware_init();
#ifdef ODM_WT_EDIT
//Junbo.Guo@ODM_WT.BSP.CHG, 2019/11/11, Modify for subcharger
	if(chip->is_double_charger_support){
		chip->sub_chg_ops->hardware_init();	
	}
#endif
        if (chip->stop_chg == 0 && (chip->charger_type == POWER_SUPPLY_TYPE_USB || chip->charger_type == POWER_SUPPLY_TYPE_USB_CDP)) {
#ifdef ODM_WT_EDIT
//Sidong.Zhao@ODM_WT.BSP.CHG, 2019/12/2, implement stop charge
                oppo_chg_turn_off_charging(chip);
#endif /*ODM_WT_EDIT*/
                chip->chg_ops->charger_suspend();
        }
        if (chip->check_batt_full_by_sw) {
                chip->chg_ops->set_charging_term_disable();
#ifdef ODM_WT_EDIT
//Junbo.Guo@ODM_WT.BSP.CHG, 2019/11/11, Modify for subcharger
		if(chip->is_double_charger_support){
			chip->sub_chg_ops->set_charging_term_disable();
        }
#endif
        }
        oppo_chg_check_tbatt_status(chip);
        oppo_chg_set_float_voltage(chip);
        oppo_chg_set_charging_current(chip);
        oppo_chg_set_input_current_limit(chip);
        chip->chg_ops->term_current_set(chip->limits.iterm_ma);
#ifdef ODM_WT_EDIT
		//Junbo.Guo@ODM_WT.BSP.CHG, 2019/11/11, Modify for subcharger
		if(chip->is_double_charger_support){
			chip->sub_chg_ops->term_current_set(chip->limits.iterm_ma+200); 
		}
#endif

}

void oppo_chg_turn_off_charging(struct oppo_chg_chip *chip)
{
        if (oppo_vooc_get_allow_reading() == false) {
                return;
        }
        switch (chip->tbatt_status) {
        case BATTERY_STATUS__INVALID:
        case BATTERY_STATUS__REMOVED:
        case BATTERY_STATUS__LOW_TEMP:
                break;
        case BATTERY_STATUS__HIGH_TEMP:
                break;
        case BATTERY_STATUS__COLD_TEMP:
                break;
        case BATTERY_STATUS__LITTLE_COLD_TEMP:
        case BATTERY_STATUS__COOL_TEMP:
                chip->chg_ops->charging_current_write_fast(chip->limits.temp_cold_fastchg_current_ma);
                msleep(50);
                break;
        case BATTERY_STATUS__LITTLE_COOL_TEMP:
        case BATTERY_STATUS__NORMAL:
                chip->chg_ops->charging_current_write_fast(chip->limits.temp_cool_fastchg_current_ma_high);
                msleep(50);
                chip->chg_ops->charging_current_write_fast(chip->limits.temp_cold_fastchg_current_ma);
                msleep(50);
                break;
        case BATTERY_STATUS__WARM_TEMP:
				chip->chg_ops->charging_current_write_fast(chip->limits.temp_cold_fastchg_current_ma);
                msleep(50);
                break;
        default:
                break;
        }
        chip->chg_ops->charging_disable();
#ifdef ODM_WT_EDIT
		//Junbo.Guo@ODM_WT.BSP.CHG, 2019/11/11, Modify for subcharger
		if(chip->is_double_charger_support){
			chip->sub_chg_ops->charging_disable();
		}
#endif
        /*charger_xlog_printk(CHG_LOG_CRTI, "[BATTERY] oppo_chg_turn_off_charging !!\n");*/
}
/*
static int oppo_chg_check_suspend_or_disable(struct oppo_chg_chip *chip)
{
        if(chip->suspend_after_full) {
                if(chip->tbatt_status == BATTERY_STATUS__LITTLE_COLD_TEMP
                        || chip->tbatt_status == BATTERY_STATUS__COOL_TEMP
                        || chip->tbatt_status == BATTERY_STATUS__LITTLE_COOL_TEMP
                        || chip->tbatt_status == BATTERY_STATUS__NORMAL) {
                        return CHG_SUSPEND;
                } else if(chip->tbatt_status == BATTERY_STATUS__COLD_TEMP) {
                        return CHG_DISABLE;
                } else {
                        return CHG_NO_SUSPEND_NO_DISABLE;
                }
        } else {
                if(chip->tbatt_status == BATTERY_STATUS__COLD_TEMP
                        || chip->tbatt_status == BATTERY_STATUS__LITTLE_COLD_TEMP) {
                        return CHG_DISABLE;
                } else {
                        return CHG_NO_SUSPEND_NO_DISABLE;
                }
        }
}
*/

static int oppo_chg_check_suspend_or_disable(struct oppo_chg_chip *chip)
{
        if (chip->suspend_after_full) {
                if ((chip->tbatt_status == BATTERY_STATUS__HIGH_TEMP
                        || chip->tbatt_status == BATTERY_STATUS__WARM_TEMP) && (chip->batt_volt < 4250)) {
                        return CHG_DISABLE;
                } else {
                        return CHG_SUSPEND;
                }
        } else {
                return CHG_DISABLE;
        }
}


static void oppo_chg_voter_charging_start(struct oppo_chg_chip *chip, OPPO_CHG_STOP_VOTER voter)
{
        chip->chging_on = true;
        chip->stop_voter &= ~(int)voter;
        oppo_chg_turn_on_charging(chip);

        switch (voter) {
        case CHG_STOP_VOTER__FULL:
                chip->charging_state = CHARGING_STATUS_CCCV;
                if (oppo_vooc_get_allow_reading() == true) {
                        chip->chg_ops->charger_unsuspend();
                        chip->chg_ops->charging_enable();
                }
                break;
        case CHG_STOP_VOTER__VCHG_ABNORMAL:
                chip->charging_state = CHARGING_STATUS_CCCV;
                if (oppo_vooc_get_allow_reading() == true) {
                        chip->chg_ops->charger_unsuspend();
                }
                break;
        case CHG_STOP_VOTER__BATTTEMP_ABNORMAL:
        case CHG_STOP_VOTER__VBAT_TOO_HIGH:
        case CHG_STOP_VOTER__MAX_CHGING_TIME:
                chip->charging_state = CHARGING_STATUS_CCCV;
                break;
        default:
                break;
        }
}


static void oppo_chg_voter_charging_stop(struct oppo_chg_chip *chip, OPPO_CHG_STOP_VOTER voter)
{
        chip->chging_on = false;
        chip->stop_voter |= (int)voter;

        switch (voter) {
        case CHG_STOP_VOTER__FULL:
                chip->charging_state = CHARGING_STATUS_FULL;
                if (oppo_vooc_get_allow_reading() == true) {
                        if (oppo_chg_check_suspend_or_disable(chip) == CHG_SUSPEND) {
                                chip->chg_ops->charger_suspend();
                        } else {
                                oppo_chg_turn_off_charging(chip);
                        }
                }
                break;
        case CHG_STOP_VOTER__VCHG_ABNORMAL:
                chip->charging_state = CHARGING_STATUS_FAIL;
                chip->total_time = 0;
                if (oppo_vooc_get_allow_reading() == true) {
                        chip->chg_ops->charger_suspend();
                }
                oppo_chg_turn_off_charging(chip);
                break;
        case CHG_STOP_VOTER__BATTTEMP_ABNORMAL:
        case CHG_STOP_VOTER__VBAT_TOO_HIGH:
                chip->charging_state = CHARGING_STATUS_FAIL;
                chip->total_time = 0;
                oppo_chg_turn_off_charging(chip);
                break;
        case CHG_STOP_VOTER__MAX_CHGING_TIME:
                chip->charging_state = CHARGING_STATUS_FAIL;
                oppo_chg_turn_off_charging(chip);
                break;
        default:
                break;
        }
}
#ifdef ODM_WT_EDIT
/*Sidong.Zhao@ODM_WT.BSP.CHG 2019/11/13,JEITA configuration*/
#define HYSTERISIS_DECIDEGC               10
#else
#define HYSTERISIS_DECIDEGC               20
#endif /*ODM_WT_EDIT*/
#define TBATT_PRE_SHAKE_INVALID      999
static void battery_temp_anti_shake_handle(struct oppo_chg_chip *chip)
{
        int tbatt_cur_shake = chip->temperature, low_shake = 0, high_shake = 0;

        if (tbatt_cur_shake > chip->tbatt_pre_shake) {		/*get warmer*/
                low_shake = -HYSTERISIS_DECIDEGC;
                high_shake = 0;
        } else if (tbatt_cur_shake < chip->tbatt_pre_shake) {	/*get cooler*/
                low_shake = 0;
                high_shake = HYSTERISIS_DECIDEGC;
        }

        if (chip->tbatt_status == BATTERY_STATUS__HIGH_TEMP) {                                                                     /*>53C*/
                chip->limits.cold_bat_decidegc = chip->anti_shake_bound.cold_bound;                                        /*-3C*/
                chip->limits.little_cold_bat_decidegc = chip->anti_shake_bound.little_cold_bound;                /*0C*/
                chip->limits.cool_bat_decidegc = chip->anti_shake_bound.cool_bound;                                                /*5C*/
                chip->limits.little_cool_bat_decidegc = chip->anti_shake_bound.little_cool_bound;                /*12C*/
                chip->limits.normal_bat_decidegc = chip->anti_shake_bound.normal_bound;                                        /*16C*/
                chip->limits.warm_bat_decidegc = chip->anti_shake_bound.warm_bound;                                                /*45C*/
                chip->limits.hot_bat_decidegc = chip->anti_shake_bound.hot_bound + low_shake;                        /*53C*/
        } else if (chip->tbatt_status == BATTERY_STATUS__LOW_TEMP) {                       /*<-3C*/
                chip->limits.cold_bat_decidegc = chip->anti_shake_bound.cold_bound + high_shake;
                chip->limits.little_cold_bat_decidegc = chip->anti_shake_bound.little_cold_bound;
                chip->limits.cool_bat_decidegc = chip->anti_shake_bound.cool_bound;
                chip->limits.little_cool_bat_decidegc = chip->anti_shake_bound.little_cool_bound;
                chip->limits.normal_bat_decidegc = chip->anti_shake_bound.normal_bound;
                chip->limits.warm_bat_decidegc = chip->anti_shake_bound.warm_bound;
                chip->limits.hot_bat_decidegc = chip->anti_shake_bound.hot_bound;
        } else if (chip->tbatt_status == BATTERY_STATUS__COLD_TEMP) {                        /*-3C~0C*/
                chip->limits.cold_bat_decidegc = chip->anti_shake_bound.cold_bound + low_shake;
                chip->limits.little_cold_bat_decidegc = chip->anti_shake_bound.little_cold_bound + high_shake;
                chip->limits.cool_bat_decidegc = chip->anti_shake_bound.cool_bound;
                chip->limits.little_cool_bat_decidegc = chip->anti_shake_bound.little_cool_bound;
                chip->limits.normal_bat_decidegc = chip->anti_shake_bound.normal_bound;
                chip->limits.warm_bat_decidegc = chip->anti_shake_bound.warm_bound;
                chip->limits.hot_bat_decidegc = chip->anti_shake_bound.hot_bound;
        } else if (chip->tbatt_status == BATTERY_STATUS__LITTLE_COLD_TEMP) {                       /*0C-5C*/
                chip->limits.cold_bat_decidegc = chip->anti_shake_bound.cold_bound;
                chip->limits.little_cold_bat_decidegc = chip->anti_shake_bound.little_cold_bound + low_shake;
                chip->limits.cool_bat_decidegc = chip->anti_shake_bound.cool_bound + high_shake;
                chip->limits.little_cool_bat_decidegc = chip->anti_shake_bound.little_cool_bound;
                chip->limits.normal_bat_decidegc = chip->anti_shake_bound.normal_bound;
                chip->limits.warm_bat_decidegc = chip->anti_shake_bound.warm_bound;
                chip->limits.hot_bat_decidegc = chip->anti_shake_bound.hot_bound;
        } else if (chip->tbatt_status == BATTERY_STATUS__COOL_TEMP) {                            /*5C~12C*/
                chip->limits.cold_bat_decidegc = chip->anti_shake_bound.cold_bound;
                chip->limits.little_cold_bat_decidegc = chip->anti_shake_bound.little_cold_bound;
                chip->limits.cool_bat_decidegc = chip->anti_shake_bound.cool_bound + low_shake;
                chip->limits.little_cool_bat_decidegc = chip->anti_shake_bound.little_cool_bound + high_shake;
                chip->limits.normal_bat_decidegc = chip->anti_shake_bound.normal_bound;
                chip->limits.warm_bat_decidegc = chip->anti_shake_bound.warm_bound;
                chip->limits.hot_bat_decidegc = chip->anti_shake_bound.hot_bound;
        } else if (chip->tbatt_status == BATTERY_STATUS__LITTLE_COOL_TEMP) {                     /*12C~16C*/
                chip->limits.cold_bat_decidegc = chip->anti_shake_bound.cold_bound;
                chip->limits.little_cold_bat_decidegc = chip->anti_shake_bound.little_cold_bound;
                chip->limits.cool_bat_decidegc = chip->anti_shake_bound.cool_bound;
                chip->limits.little_cool_bat_decidegc = chip->anti_shake_bound.little_cool_bound + low_shake;
                chip->limits.normal_bat_decidegc = chip->anti_shake_bound.normal_bound + high_shake;
                chip->limits.warm_bat_decidegc = chip->anti_shake_bound.warm_bound;
                chip->limits.hot_bat_decidegc = chip->anti_shake_bound.hot_bound;
        } else if (chip->tbatt_status == BATTERY_STATUS__NORMAL) {                                 /*16C~45C*/
                chip->limits.cold_bat_decidegc = chip->anti_shake_bound.cold_bound;
                chip->limits.little_cold_bat_decidegc = chip->anti_shake_bound.little_cold_bound;
                chip->limits.cool_bat_decidegc = chip->anti_shake_bound.cool_bound;
                chip->limits.little_cool_bat_decidegc = chip->anti_shake_bound.little_cool_bound;
                chip->limits.normal_bat_decidegc = chip->anti_shake_bound.normal_bound;
                chip->limits.warm_bat_decidegc = chip->anti_shake_bound.warm_bound;
                chip->limits.hot_bat_decidegc = chip->anti_shake_bound.hot_bound;
        } else if (chip->tbatt_status == BATTERY_STATUS__WARM_TEMP) {                               /*45C~53C*/
                chip->limits.cold_bat_decidegc = chip->anti_shake_bound.cold_bound;
                chip->limits.little_cold_bat_decidegc = chip->anti_shake_bound.little_cold_bound;
                chip->limits.cool_bat_decidegc = chip->anti_shake_bound.cool_bound;
                chip->limits.little_cool_bat_decidegc = chip->anti_shake_bound.little_cool_bound;
                chip->limits.normal_bat_decidegc = chip->anti_shake_bound.normal_bound;
                chip->limits.warm_bat_decidegc = chip->anti_shake_bound.warm_bound + low_shake;
                chip->limits.hot_bat_decidegc = chip->anti_shake_bound.hot_bound + high_shake;
        } else {  /*BATTERY_STATUS__REMOVED                                                                <-19C*/
                chip->limits.cold_bat_decidegc = chip->anti_shake_bound.cold_bound;
                chip->limits.little_cold_bat_decidegc = chip->anti_shake_bound.little_cold_bound;
                chip->limits.cool_bat_decidegc = chip->anti_shake_bound.cool_bound;
                chip->limits.little_cool_bat_decidegc = chip->anti_shake_bound.little_cool_bound;
                chip->limits.normal_bat_decidegc = chip->anti_shake_bound.normal_bound;
                chip->limits.warm_bat_decidegc = chip->anti_shake_bound.warm_bound;
                chip->limits.hot_bat_decidegc = chip->anti_shake_bound.hot_bound;
        }
        chip->tbatt_pre_shake = tbatt_cur_shake;
}


#define TEMP_CNT 1
static bool oppo_chg_check_tbatt_is_good(struct oppo_chg_chip *chip)
{
        static bool ret = true;
        static int temp_counts = 0;
        int batt_temp = chip->temperature;
        OPPO_CHG_TBATT_STATUS tbatt_status = chip->tbatt_status;

        if (batt_temp > chip->limits.hot_bat_decidegc || batt_temp < chip->limits.cold_bat_decidegc) {
                temp_counts++;
                if (temp_counts >= TEMP_CNT) {
                        temp_counts = 0;
                        ret = false;
                        if (batt_temp <= chip->limits.removed_bat_decidegc) {
                                tbatt_status = BATTERY_STATUS__REMOVED;
                        } else if (batt_temp > chip->limits.hot_bat_decidegc) {
                                tbatt_status = BATTERY_STATUS__HIGH_TEMP;
                        } else {
                                tbatt_status = BATTERY_STATUS__LOW_TEMP;
                        }
                }
        } else {
                temp_counts = 0;
                ret = true;
                if (batt_temp >= chip->limits.warm_bat_decidegc) {                                       /*45C*/
                        tbatt_status = BATTERY_STATUS__WARM_TEMP;
                } else if (batt_temp >= chip->limits.normal_bat_decidegc) {                       /*16C*/
                        tbatt_status = BATTERY_STATUS__NORMAL;
                } else if (batt_temp >= chip->limits.little_cool_bat_decidegc) {                /*12C*/
                        tbatt_status = BATTERY_STATUS__LITTLE_COOL_TEMP;
                } else if (batt_temp >= chip->limits.cool_bat_decidegc) {                       /*5C*/
                        tbatt_status = BATTERY_STATUS__COOL_TEMP;
                } else if (batt_temp >= chip->limits.little_cold_bat_decidegc) {                /*0C*/
                        tbatt_status = BATTERY_STATUS__LITTLE_COLD_TEMP;
                } else if (batt_temp >= chip->limits.cold_bat_decidegc) {                /*-3C*/
                        tbatt_status = BATTERY_STATUS__COLD_TEMP;
                } else {
                        tbatt_status = BATTERY_STATUS__COLD_TEMP;
                }
        }

        if (tbatt_status == BATTERY_STATUS__REMOVED) {
                chip->batt_exist = false;
        } else {
                chip->batt_exist = true;
        }

        if (chip->tbatt_pre_shake == TBATT_PRE_SHAKE_INVALID) {
                chip->tbatt_pre_shake = batt_temp;
        }

        if (tbatt_status != chip->tbatt_status) {
                if (chip->tbatt_status == BATTERY_STATUS__WARM_TEMP
                                || chip->tbatt_status == BATTERY_STATUS__COLD_TEMP) {
                        if (chip->soc != 100 && chip->batt_full == true
                                        && chip->charging_state == CHARGING_STATUS_FULL) {
                                chip->batt_full = false;
                                chip->charging_state = CHARGING_STATUS_CCCV;
                        }
                }
                chip->tbatt_status = tbatt_status;
                vbatt_higherthan_4180mv = false;
                if (oppo_vooc_get_allow_reading() == true) {
                        oppo_chg_set_float_voltage(chip);
                        oppo_chg_set_charging_current(chip);
                }
                battery_temp_anti_shake_handle(chip);
        }
        return ret;
}

static void oppo_chg_check_tbatt_status(struct oppo_chg_chip *chip)
{
        int batt_temp = chip->temperature;
        OPPO_CHG_TBATT_STATUS tbatt_status = chip->tbatt_status;

        if (batt_temp > chip->limits.hot_bat_decidegc) {                               /*53C*/
                tbatt_status = BATTERY_STATUS__HIGH_TEMP;
        } else if (batt_temp >= chip->limits.warm_bat_decidegc) {               /*45C*/
                tbatt_status = BATTERY_STATUS__WARM_TEMP;
        } else if (batt_temp >= chip->limits.normal_bat_decidegc) {               /*16C*/
                tbatt_status = BATTERY_STATUS__NORMAL;
        } else if (batt_temp >= chip->limits.little_cool_bat_decidegc) {        /*12C*/
                tbatt_status = BATTERY_STATUS__LITTLE_COOL_TEMP;
        } else if (batt_temp >= chip->limits.cool_bat_decidegc) {                /*5C*/
                tbatt_status = BATTERY_STATUS__COOL_TEMP;
        } else if (batt_temp >= chip->limits.little_cold_bat_decidegc) {                /*0C*/
                tbatt_status = BATTERY_STATUS__LITTLE_COLD_TEMP;
        } else if (batt_temp >= chip->limits.cold_bat_decidegc) {                /*-3C*/
                tbatt_status = BATTERY_STATUS__COLD_TEMP;
        } else if (batt_temp > chip->limits.removed_bat_decidegc) {                /*-20C*/
                tbatt_status = BATTERY_STATUS__LOW_TEMP;
        } else {
                tbatt_status = BATTERY_STATUS__REMOVED;
        }
        if (tbatt_status == BATTERY_STATUS__REMOVED) {
                chip->batt_exist = false;
        } else {
                chip->batt_exist = true;
        }
        chip->tbatt_status = tbatt_status;

   
}


#define VCHG_CNT 2
static bool oppo_chg_check_vchg_is_good(struct oppo_chg_chip *chip)
{
        static bool ret = true;
        static int vchg_counts = 0;
        int chg_volt = chip->charger_volt;
        OPPO_CHG_VCHG_STATUS vchg_status = chip->vchg_status;

        if (oppo_vooc_get_adapter_update_status() == ADAPTER_FW_NEED_UPDATE) {
                return true;
        }

		if (oppo_vooc_get_fastchg_started() == true) {		
			return true;
		}

        if (chg_volt > chip->limits.charger_hv_thr) {
                vchg_counts++;
                if (vchg_counts >= VCHG_CNT) {
                        vchg_counts = 0;
                        ret = false;
                        vchg_status = CHARGER_STATUS__VOL_HIGH;
                }
        } else  if (chg_volt <= chip->limits.charger_recv_thr){
                vchg_counts = 0;
                ret = true;
                vchg_status = CHARGER_STATUS__GOOD;
        }

        if (vchg_status != chip->vchg_status) {
                chip->vchg_status = vchg_status;
        }

        return ret;
}

#define VBAT_CNT 1

static bool oppo_chg_check_vbatt_is_good(struct oppo_chg_chip *chip)
{
        static bool ret = true;
        static int vbat_counts = 0;
        int batt_volt = chip->batt_volt;

        if (batt_volt >= chip->limits.vbatt_hv_thr) {
                vbat_counts++;
                if (vbat_counts >= VBAT_CNT) {
                        vbat_counts = 0;
                        ret = false;
                        chip->vbatt_over = true;
                }
        } else {
                vbat_counts = 0;
                ret = true;
                chip->vbatt_over = false;
        }

        return ret;
}

static bool oppo_chg_check_time_is_good(struct oppo_chg_chip *chip)
{
#ifdef SELL_MODE
		/* Qiao.Hu@BSP.BaseDrv.CHG.Basic, 2017/10/26, delete over_time for sell_mode */
		chip->chging_over_time = false;
		printk("oppo_chg_check_time_is_good_sell_mode\n");
		return true;
#endif //SELL_MODE

//#ifdef VENDOR_EDIT
//zhouhengguo@BSP.Stabliity, 2019.10.14, add for aging version
	if(get_eng_version() == 1) {
		/* Qiao.Hu@BSP.BaseDrv.CHG.Basic, 2019/3/8, delete over_time for aging_mode */
		chip->chging_over_time = false;
		printk("oppo_chg_check_time_is_good_aging_mode\n");
		return true;
	}
//endif
        if (chip->limits.max_chg_time_sec < 0) {
                chip->chging_over_time = false;
                return true;
        }

        if (chip->total_time >= chip->limits.max_chg_time_sec) {
                chip->total_time = chip->limits.max_chg_time_sec;
                chip->chging_over_time = true;
                return false;
        } else {
                chip->chging_over_time = false;
                return true;
        }
}

#ifdef CONFIG_FB
#ifdef CONFIG_DRM_MSM
static int fb_notifier_callback(struct notifier_block *nb, unsigned long event, void *data)
{
	int blank;
	struct msm_drm_notifier *evdata = data;

	if (!g_charger_chip) {
		return 0;
	}
	if (!evdata || (evdata->id != 0))
		return 0;

	if (event == MSM_DRM_EARLY_EVENT_BLANK) {
		blank = *(int *)(evdata->data);
		if (blank == MSM_DRM_BLANK_UNBLANK) {
			g_charger_chip->led_on = true;
			g_charger_chip->led_on_change = true;
		} else if (blank == MSM_DRM_BLANK_POWERDOWN) {
			g_charger_chip->led_on = false;
			g_charger_chip->led_on_change = true;
		} else {
			pr_err("%s: receives wrong data EARLY_BLANK:%d\n", __func__, blank);
		}
	}

	return 0;
}
#else
static int fb_notifier_callback(struct notifier_block *nb, unsigned long event, void *data)
{
        int blank;
        struct fb_event *evdata = data;

        if (!g_charger_chip) {
                return 0;
        }
        if (evdata && evdata->data) {
                if (event == FB_EVENT_BLANK) {
                        blank = *(int *)evdata->data;
                        if (blank == FB_BLANK_UNBLANK) {
                                g_charger_chip->led_on = true;
                                g_charger_chip->led_on_change = true;
                        } else if (blank == FB_BLANK_POWERDOWN) {
                                g_charger_chip->led_on = false;
                                g_charger_chip->led_on_change = true;
                        }
                }
        }
        return 0;
}
#endif /* CONFIG_DRM_MSM */

void oppo_chg_set_allow_switch_to_fastchg(bool allow)
{
    charger_xlog_printk(CHG_LOG_CRTI, " allow = %d\n", allow);
    if (!g_charger_chip) {
            return;
    } else {
            g_charger_chip->allow_swtich_to_fastchg = allow;
    }
}

void oppo_chg_set_led_status(bool val)
{
        /*Do nothing*/
}
EXPORT_SYMBOL(oppo_chg_set_led_status);
#else
void oppo_chg_set_led_status(bool val)
{
        charger_xlog_printk(CHG_LOG_CRTI, " val = %d\n", val);
        if (!g_charger_chip) {
                return;
        } else {
                g_charger_chip->led_on = val;
                g_charger_chip->led_on_change = true;
        }
}
EXPORT_SYMBOL(oppo_chg_set_led_status);
#endif

void oppo_chg_set_camera_status(bool val)
{
        if (!g_charger_chip) {
                return;
        } else {
                g_charger_chip->camera_on = val;
        }
}
EXPORT_SYMBOL(oppo_chg_set_camera_status);

#define TLED_CHANGE_COUNTS		4
#define TLED_HYSTERISIS_DECIDEGC	10
static void oppo_chg_check_tled_status(struct oppo_chg_chip *chip)
{
	OPPO_CHG_TLED_STATUS tled_status = chip->led_temp_status;
	static int high_counts = 0, warm_counts = 0, normal_counts = 0;
	if (chip->temperature > chip->limits.led_high_bat_decidegc_antishake) {		/* >37C */
		high_counts ++;
		if (high_counts >= TLED_CHANGE_COUNTS) {
			high_counts = 0;
			warm_counts = 0;
			normal_counts = 0;
			tled_status = LED_TEMP_STATUS__HIGH;
		}
	} else if (chip->temperature > chip->limits.led_warm_bat_decidegc_antishake) {	/* >35C && <= 37 */
		warm_counts ++;
		if (warm_counts >= TLED_CHANGE_COUNTS) {
			high_counts = 0;
			warm_counts = 0;
			normal_counts = 0;
			tled_status = LED_TEMP_STATUS__WARM;
		}
	} else {
		normal_counts ++;
		if (normal_counts >= TLED_CHANGE_COUNTS) {
			high_counts = 0;
			warm_counts = 0;
			normal_counts = 0;
			tled_status = LED_TEMP_STATUS__NORMAL;
		}
	}
	if (tled_status != chip->led_temp_status) {
		chip->limits.led_warm_bat_decidegc_antishake = chip->limits.led_warm_bat_decidegc;
		chip->limits.led_high_bat_decidegc_antishake = chip->limits.led_high_bat_decidegc;
		if (tled_status > chip->led_temp_status && tled_status == LED_TEMP_STATUS__WARM)
			chip->limits.led_warm_bat_decidegc_antishake = chip->limits.led_warm_bat_decidegc - TLED_HYSTERISIS_DECIDEGC;
		else if (tled_status > chip->led_temp_status && tled_status == LED_TEMP_STATUS__HIGH)
			chip->limits.led_high_bat_decidegc_antishake = chip->limits.led_high_bat_decidegc - TLED_HYSTERISIS_DECIDEGC;
		else if (tled_status < chip->led_temp_status && tled_status == LED_TEMP_STATUS__NORMAL)
			chip->limits.led_warm_bat_decidegc_antishake = chip->limits.led_warm_bat_decidegc + TLED_HYSTERISIS_DECIDEGC;
		else if (tled_status < chip->led_temp_status && tled_status == LED_TEMP_STATUS__WARM)
			chip->limits.led_high_bat_decidegc_antishake = chip->limits.led_high_bat_decidegc + TLED_HYSTERISIS_DECIDEGC;
		chg_debug("tled status change, [%d %d %d %d]\n", tled_status, chip->led_temp_status,
			chip->limits.led_warm_bat_decidegc_antishake, chip->limits.led_high_bat_decidegc_antishake);
		chip->led_temp_change = true;
		chip->led_temp_status = tled_status;
	}
}

static void oppo_chg_check_led_on_ichging(struct oppo_chg_chip *chip)
{
	if (chip->led_on_change || (chip->led_on && chip->led_temp_change)) {
		chip->led_on_change = false;
		chip->led_temp_change = false;
		if (chip->chg_ctrl_by_vooc && chip->vbatt_num == 2) {		
			if (oppo_vooc_get_fastchg_started() == true && oppo_vooc_get_fast_chg_type() != CHARGER_SUBTYPE_FASTCHG_VOOC) {		
				return;
			}
			if (oppo_vooc_get_allow_reading() == true || oppo_vooc_get_fast_chg_type() == CHARGER_SUBTYPE_FASTCHG_VOOC) {
				oppo_chg_set_charging_current(chip);
				oppo_chg_set_input_current_limit(chip);
			}
		} else {
			if (oppo_vooc_get_fastchg_started() == true) {
				return;
			}
			if (oppo_vooc_get_allow_reading() == true) {
				oppo_chg_set_charging_current(chip);
				oppo_chg_set_input_current_limit(chip);
			}
		}
	}
}

#define  TVOOC_COUNTS 2
static void oppo_chg_check_vooc_temp_status(struct oppo_chg_chip *chip)
{
        int batt_temp = chip->temperature;
        OPPO_CHG_TBAT_VOOC_STATUS tbat_vooc_status = chip->vooc_temp_status;
        static int high_counts = 0, warm_counts = 0, normal_counts = 0;
        static bool vooc_first_set_input_current_flag = false;

        if (chip->vbatt_num != 2 || oppo_vooc_get_fast_chg_type() != CHARGER_SUBTYPE_FASTCHG_VOOC || oppo_vooc_get_fastchg_started() == false) {
		 vooc_first_set_input_current_flag = false;	
                return;
        }
        if (batt_temp > chip->limits.vooc_high_bat_decidegc) {                               /*>45C*/
                if (oppo_vooc_get_fastchg_started() == true) {
                            chg_err("tbatt > 45, quick out vooc");
                            oppo_chg_set_chargerid_switch_val(0);
                            oppo_vooc_switch_mode(NORMAL_CHARGER_MODE);
                }
        } else if (batt_temp > chip->limits.vooc_warm_bat_decidegc) {                               /*>38C && <= 45*/
                high_counts ++;
                if(high_counts >= TVOOC_COUNTS)
                {
                        high_counts = 0;
                        warm_counts = 0;
                        normal_counts = 0;
                        tbat_vooc_status = VOOC_TEMP_STATUS__HIGH;
                }
        } else if (batt_temp >= chip->limits.vooc_normal_bat_decidegc) {               /*>34C && <= 38*/
                warm_counts ++;
                if (warm_counts >= TVOOC_COUNTS)
                {
                        high_counts = 0;
                        warm_counts = 0;
                        normal_counts = 0;
                        tbat_vooc_status = VOOC_TEMP_STATUS__WARM;
                }
        } else {   // < 34
                normal_counts ++;
                if (normal_counts >= TVOOC_COUNTS)
                {
                        high_counts = 0;
                        warm_counts = 0;
                        normal_counts = 0;
                        tbat_vooc_status = VOOC_TEMP_STATUS__NORMAL;

                }
        }

        chg_err("tbat_vooc_status[%d],chip->vooc_temp_status[%d] ", tbat_vooc_status, chip->vooc_temp_status);
        if (vooc_first_set_input_current_flag == false) {
		chip->limits.temp_little_cool_fastchg_current_ma = chip->limits.charger_current_vooc_ma_normal;
		chip->limits.temp_normal_fastchg_current_ma = chip->limits.charger_current_vooc_ma_normal;
        	oppo_chg_set_charging_current(chip);
		chg_err("set charger current ctrl by vooc[%d]\n",chip->limits.temp_little_cool_fastchg_current_ma);
        }

        if (tbat_vooc_status != chip->vooc_temp_status || vooc_first_set_input_current_flag == false) {
		 vooc_first_set_input_current_flag = true;
                chip->vooc_temp_change = true;
                chip->vooc_temp_status = tbat_vooc_status;
        }
}

static void oppo_chg_check_vooc_ichging(struct oppo_chg_chip *chip)
{
        if (chip->vbatt_num == 2 && chip->vooc_temp_change) {
                chip->vooc_temp_change = false;

                if (oppo_vooc_get_fast_chg_type() == CHARGER_SUBTYPE_FASTCHG_VOOC) {
                        oppo_chg_set_input_current_limit(chip);
                }
        }
}

static void oppo_chg_check_camera_on_ichging(struct oppo_chg_chip *chip)
{
        static bool camera_pre = false;

        if (chip->camera_on != camera_pre) {
                camera_pre = chip->camera_on;
				if (oppo_vooc_get_fastchg_started() == true) {		
					return;
				}
                if (oppo_vooc_get_allow_reading() == true) {
                        oppo_chg_set_input_current_limit(chip);
                }
        }
}

static void oppo_chg_check_calling_on_ichging(struct oppo_chg_chip *chip)
{
        static bool calling_pre = false;

        if (chip->calling_on != calling_pre) {
                calling_pre = chip->calling_on;
				if (oppo_vooc_get_fastchg_started() == true) {		
					return;
				}
                if (oppo_vooc_get_allow_reading() == true) {
                        oppo_chg_set_input_current_limit(chip);
                }
        }
}

static void oppo_chg_battery_authenticate_check(struct oppo_chg_chip *chip)
{
        static bool charger_exist_pre = false;

        if (charger_exist_pre ^ chip->charger_exist) {
                charger_exist_pre = chip->charger_exist;
                if (chip->charger_exist && !chip->authenticate) {
                        chip->authenticate = oppo_gauge_get_batt_authenticate();
                }
        }
}

void oppo_chg_variables_reset(struct oppo_chg_chip *chip, bool in)
{
        if (in) {
                chip->charger_exist = true;
                chip->chging_on = true;
        } else {
                chip->allow_swtich_to_fastchg = 1;
                chip->charger_exist = false;
                chip->chging_on = false;
                chip->charger_type = POWER_SUPPLY_TYPE_UNKNOWN;
                vbatt_higherthan_4180mv = false;
        }

        /*chip->charger_volt = 5000;*/
        chip->vchg_status = CHARGER_STATUS__GOOD;

        chip->batt_full = false;
        chip->tbatt_status = BATTERY_STATUS__NORMAL;
        chip->tbatt_pre_shake = TBATT_PRE_SHAKE_INVALID;
        chip->vbatt_over = 0;

        chip->total_time = 0;
        chip->chging_over_time = 0;
        chip->in_rechging = 0;
        /*chip->batt_volt = 0;*/
        /*chip->temperature = 0;*/

        chip->stop_voter = 0x00;
        chip->charging_state = CHARGING_STATUS_CCCV;
#ifndef SELL_MODE
        /* Qiao.Hu@BSP.BaseDrv.CHG.Basic, 2017/12/12, delete for sell_mode */
        if(chip->mmi_fastchg == 0)
                chip->mmi_chg = 0;
        else 
                chip->mmi_chg = 1;
#endif //SELL_MODE
        chip->unwakelock_chg = 0;
        chip->notify_code = 0;
        chip->notify_flag = 0;
        chip->limits.cold_bat_decidegc = chip->anti_shake_bound.cold_bound;
        chip->limits.little_cold_bat_decidegc = chip->anti_shake_bound.little_cold_bound;
        chip->limits.cool_bat_decidegc = chip->anti_shake_bound.cool_bound;
        chip->limits.little_cool_bat_decidegc = chip->anti_shake_bound.little_cool_bound;
        chip->limits.normal_bat_decidegc = chip->anti_shake_bound.normal_bound;
        chip->limits.warm_bat_decidegc = chip->anti_shake_bound.warm_bound;
        chip->limits.hot_bat_decidegc = chip->anti_shake_bound.hot_bound;
        chip->limits.vfloat_over_counts = 0;

	chip->limits.led_warm_bat_decidegc_antishake = chip->limits.led_warm_bat_decidegc;
	chip->limits.led_high_bat_decidegc_antishake = chip->limits.led_high_bat_decidegc;
	chip->led_temp_change = false;
	if (chip->temperature > chip->limits.led_high_bat_decidegc)
		chip->led_temp_status = LED_TEMP_STATUS__HIGH;
	else if (chip->temperature > chip->limits.led_warm_bat_decidegc)
		chip->led_temp_status = LED_TEMP_STATUS__WARM;
	else
		chip->led_temp_status = LED_TEMP_STATUS__NORMAL;

        chip->dod0_counts = 0;
        chip->fastchg_to_ffc = false;
	chip->fastchg_ffc_status = 0;
        chip->chg_ctrl_by_lcd = chip->chg_ctrl_by_lcd_default;
        chip->chg_ctrl_by_vooc = chip->chg_ctrl_by_vooc_default;
        chip->ffc_temp_status = FFC_TEMP_STATUS__NORMAL;
        chip->vooc_temp_status = VOOC_TEMP_STATUS__NORMAL;
	 
        chip->limits.iterm_ma = chip->limits.default_iterm_ma;
        chip->limits.temp_normal_fastchg_current_ma = chip->limits.default_temp_normal_fastchg_current_ma;
        chip->limits.normal_vfloat_sw_limit = chip->limits.default_normal_vfloat_sw_limit;
        chip->limits.temp_normal_vfloat_mv = chip->limits.default_temp_normal_vfloat_mv;
        chip->limits.normal_vfloat_over_sw_limit = chip->limits.default_normal_vfloat_over_sw_limit;

        chip->limits.temp_little_cool_fastchg_current_ma = chip->limits.default_temp_little_cool_fastchg_current_ma;
        chip->limits.little_cool_vfloat_sw_limit = chip->limits.default_little_cool_vfloat_sw_limit;
        chip->limits.temp_little_cool_vfloat_mv = chip->limits.default_temp_little_cool_vfloat_mv;
        chip->limits.little_cool_vfloat_over_sw_limit = chip->limits.default_little_cool_vfloat_over_sw_limit;

        chip->limits.temp_little_cold_fastchg_current_ma_high = chip->limits.default_temp_little_cold_fastchg_current_ma_high;
        chip->limits.temp_little_cold_fastchg_current_ma_low = chip->limits.default_temp_little_cold_fastchg_current_ma_low;
        chip->limits.temp_cool_fastchg_current_ma_high = chip->limits.default_temp_cool_fastchg_current_ma_high;
        chip->limits.temp_cool_fastchg_current_ma_low = chip->limits.default_temp_cool_fastchg_current_ma_low;
        chip->limits.temp_warm_fastchg_current_ma = chip->limits.default_temp_warm_fastchg_current_ma;

        chip->limits.input_current_charger_ma = chip->limits.default_input_current_charger_ma;

        reset_mcu_delay = 0;
#ifndef CONFIG_OPPO_CHARGER_MTK
        chip->pmic_spmi.aicl_suspend = false;
#endif

        oppo_chg_battery_authenticate_check(chip);
#ifdef CONFIG_OPPO_CHARGER_MTK
        chip->chargerid_volt = 0;
        chip->chargerid_volt_got = false;
#endif
        chip->short_c_batt.in_idle = true;//defualt in idle for userspace
        chip->short_c_batt.cv_satus = false;//defualt not in cv chg
        chip->short_c_batt.disable_rechg = false;
        chip->short_c_batt.limit_chg = false;
        chip->short_c_batt.limit_rechg = false;
}

static void oppo_chg_variables_init(struct oppo_chg_chip *chip)
{
        chip->charger_exist = false;
        chip->chging_on = false;
        chip->charger_type = POWER_SUPPLY_TYPE_UNKNOWN;
        chip->charger_volt = 0;
        chip->vchg_status = CHARGER_STATUS__GOOD;
        chip->prop_status = POWER_SUPPLY_STATUS_NOT_CHARGING;

        chip->batt_exist = true;
        chip->batt_full = false;
        chip->tbatt_status = BATTERY_STATUS__NORMAL;
        chip->vbatt_over = 0;
        chip->total_time = 0;
        chip->chging_over_time = 0;
        chip->in_rechging = 0;

        //chip->batt_volt = 3800 * chip->vbatt_num;
        chip->batt_volt = 3800;
		
        chip->icharging = 0;
        chip->temperature = 250;
        chip->soc = 0;
        chip->ui_soc = 50;
        chip->notify_code = 0;
        chip->notify_flag = 0;
        chip->tbatt_pre_shake = TBATT_PRE_SHAKE_INVALID;

        chip->led_on = true;
        chip->camera_on = 0;
        
        chip->stop_voter = 0x00;
        chip->charging_state = CHARGING_STATUS_CCCV;
        chip->mmi_chg = 1;
        chip->unwakelock_chg = 0;
        chip->allow_swtich_to_fastchg = 1;
		chip->stop_chg= 1;
        chip->mmi_fastchg = 1;
#ifdef CONFIG_OPPO_CHARGER_MTK
        chip->usb_online = false;
        chip->otg_online = false;
#else
/*        chip->pmic_spmi.usb_online = false;
           IC have init already   */
#endif
        if(chip->external_gauge) {
            chg_debug("use oppo_gauge_get_batt_authenticate\n");
            chip->authenticate = oppo_gauge_get_batt_authenticate();
        } else {
            chg_debug("use get_oppo_high_battery_status\n");
            //chip->authenticate = get_oppo_high_battery_status();
            chip->authenticate = oppo_gauge_get_batt_authenticate();
        }
		
		if (!chip->authenticate) {
			//chip->chg_ops->charger_suspend();
			chip->chg_ops->charging_disable();
#ifdef ODM_WT_EDIT
		//Junbo.Guo@ODM_WT.BSP.CHG, 2019/11/11, Modify for subcharger
		if(chip->is_double_charger_support){
			chip->sub_chg_ops->charging_disable();
		}
#endif
		}

        chip->otg_switch = false;
        chip->ui_otg_switch = false;
        chip->boot_mode = chip->chg_ops->get_boot_mode();
        chip->boot_reason = chip->chg_ops->get_boot_reason();

        chip->anti_shake_bound.cold_bound = chip->limits.cold_bat_decidegc;
        chip->anti_shake_bound.little_cold_bound = chip->limits.little_cold_bat_decidegc;
        chip->anti_shake_bound.cool_bound = chip->limits.cool_bat_decidegc;
        chip->anti_shake_bound.little_cool_bound = chip->limits.little_cool_bat_decidegc;
        chip->anti_shake_bound.normal_bound = chip->limits.normal_bat_decidegc;
        chip->anti_shake_bound.warm_bound = chip->limits.warm_bat_decidegc;
        chip->anti_shake_bound.hot_bound = chip->limits.hot_bat_decidegc;

	chip->limits.led_warm_bat_decidegc_antishake = chip->limits.led_warm_bat_decidegc;
	chip->limits.led_high_bat_decidegc_antishake = chip->limits.led_high_bat_decidegc;
	chip->led_temp_change = false;
	if (chip->temperature > chip->limits.led_high_bat_decidegc)
		chip->led_temp_status = LED_TEMP_STATUS__HIGH;
	else if (chip->temperature > chip->limits.led_warm_bat_decidegc)
		chip->led_temp_status = LED_TEMP_STATUS__WARM;
	else
		chip->led_temp_status = LED_TEMP_STATUS__NORMAL;

//        chip->anti_shake_bound.overtemp_bound = chip->limits.overtemp_bat_decidegc;
        chip->limits.vfloat_over_counts = 0;
        chip->chargerid_volt = 0;
        chip->chargerid_volt_got = false;
        chip->enable_shipmode = 0;

		chip->dod0_counts = 0;
		chip->fastchg_to_ffc = false;
		chip->fastchg_ffc_status = 0;

        chip->ffc_temp_status = FFC_TEMP_STATUS__NORMAL;
        chip->vooc_temp_status = VOOC_TEMP_STATUS__NORMAL;

        chip->short_c_batt.err_code = oppo_short_c_batt_err_code_init();
        chip->short_c_batt.is_switch_on = oppo_short_c_batt_chg_switch_init();
        chip->short_c_batt.is_feature_sw_on = oppo_short_c_batt_feature_sw_status_init();
        chip->short_c_batt.is_feature_hw_on = oppo_short_c_batt_feature_hw_status_init();
        chip->short_c_batt.shortc_gpio_status = 1;
        chip->short_c_batt.disable_rechg = false;
        chip->short_c_batt.limit_chg = false;
        chip->short_c_batt.limit_rechg = false;
#ifdef ODM_WT_EDIT
/*Sidong.Zhao@ODM_WT.BSP.CHG 2019/11/27,AP thermal machine*/
	chip->limits.ap_temp_t0 = AP_TEMP_T0;
	chip->limits.ap_temp_t0_minus_x = AP_TEMP_T0_MINUS_X;
	chip->limits.ap_temp_t1 = AP_TEMP_T1;
	chip->limits.ap_temp_t1_minus_x = AP_TEMP_T1_MINUS_X;
	chip->limits.ap_temp_t2 = AP_TEMP_T2;
	chip->limits.ap_temp_t2_minus_x = AP_TEMP_T2_MINUS_X;
	chip->limits.ap_temp_t3 = AP_TEMP_T3;
	chip->limits.ap_temp_t3_minus_x = AP_TEMP_T3_MINUS_X;

	chip->limits.ap_temp_below_t0_ichg = AP_TEMP_BELOW_T0_ICHG;
	chip->limits.ap_temp_t0_to_t1_ichg = AP_TEMP_T0_TO_T1_ICHG;
	chip->limits.ap_temp_t1_to_t2_ichg = AP_TEMP_T1_TO_T2_ICHG;
	chip->limits.ap_temp_t2_to_t3_ichg = AP_TEMP_T2_TO_T3_ICHG;
	chip->limits.ap_temp_above_t3_ichg = AP_TEMP_ABOVE_T3_ICHG;
	
	chip->limits.batt_temp_extend_t0_to_t1_ichg = BATT_TEMP_EXTEND_T0_TO_T1_ICHG;
	chip->limits.batt_temp_extend_t1_to_t2_ichg_part1 = BATT_TEMP_EXTEND_T1_TO_T2_ICHG_PART1;
	chip->limits.batt_temp_extend_t1_to_t2_ichg_part2 = BATT_TEMP_EXTEND_T1_TO_T2_ICHG_PART2;
	chip->limits.batt_temp_extend_t2_to_t3_ichg = BATT_TEMP_EXTEND_T2_TO_T3_ICHG;
	chip->limits.batt_temp_extend_t3_to_t4_ichg = BATT_TEMP_EXTEND_T3_TO_T4_ICHG;
	chip->limits.batt_temp_extend_t4_to_t5_ichg = BATT_TEMP_EXTEND_T4_TO_T5_ICHG;

	chip->limits.charge_current_calling_ma = CHARGE_CURRENT_CALLING_MA;
#endif /*ODM_WT_EDIT*/
}

static void oppo_chg_fail_action(struct oppo_chg_chip *chip)
{
        chg_err("[BATTERY] BAD Battery status... Charging Stop !!\n");
        chip->charging_state = CHARGING_STATUS_FAIL;
        chip->chging_on = false;

        chip->batt_full = false;
        chip->in_rechging = 0;
}

#define D_RECHGING_CNT                                        5
static void oppo_chg_check_rechg_status(struct oppo_chg_chip *chip)
{
        int recharging_vol;
        int nbat_vol = chip->batt_volt;
        static int rechging_cnt = 0;

        if (chip->tbatt_status == BATTERY_STATUS__COLD_TEMP) {//4.0
                recharging_vol = oppo_chg_get_float_voltage(chip) - 300;
        } else if (chip->tbatt_status == BATTERY_STATUS__LITTLE_COLD_TEMP) {//4.4
                recharging_vol = oppo_chg_get_float_voltage(chip) - 200;
        } else {
                recharging_vol = oppo_chg_get_float_voltage(chip);//warm 4.1
                if (recharging_vol > chip->limits.temp_normal_vfloat_mv) {
                        recharging_vol = chip->limits.temp_normal_vfloat_mv;
                }
                recharging_vol = recharging_vol - chip->limits.recharge_mv;
        }

        if (!chip->authenticate) {
            recharging_vol = chip->limits.non_standard_vfloat_sw_limit - 400;//3.93
        }
        if (nbat_vol <= recharging_vol) {
                rechging_cnt++;
        } else {
                rechging_cnt = 0;
        }

        /*don't rechg here unless prohibit rechg is false*/
        if (oppo_short_c_batt_is_disable_rechg(chip)) {
                if (rechging_cnt >= D_RECHGING_CNT) {
                        charger_xlog_printk(CHG_LOG_CRTI, "[Battery] disable rechg! batt_volt = %d, nReChgingVol = %d\r\n", nbat_vol, recharging_vol);
                        rechging_cnt = D_RECHGING_CNT;
                }
        }

        if (rechging_cnt > D_RECHGING_CNT) {
                charger_xlog_printk(CHG_LOG_CRTI, "[Battery] Battery rechg begin! batt_volt = %d, recharging_vol = %d\n", nbat_vol, recharging_vol);
                rechging_cnt = 0;
                chip->in_rechging = true;
                oppo_chg_voter_charging_start(chip, CHG_STOP_VOTER__FULL);/*now rechging!*/
        }
}

static void oppo_chg_full_action(struct oppo_chg_chip *chip)
{
        charger_xlog_printk(CHG_LOG_CRTI, "[BATTERY] Battery full !!\n");

        oppo_chg_voter_charging_stop(chip, CHG_STOP_VOTER__FULL);
        /*chip->charging_state = CHARGING_STATUS_FULL;*/
        chip->batt_full = true;
        chip->total_time = 0;
	chip->in_rechging = false;
        chip->limits.vfloat_over_counts = 0;
        oppo_chg_check_rechg_status(chip);
}

#ifdef ODM_WT_EDIT
/* Tongxing.Liu@ODM_WT.BSP.Tp.Funtion, 20191204,Add for tp recognoise usb plug in or out flag */
static int tp_usb_flag = 0;
#endif

void oppo_charger_detect_check(struct oppo_chg_chip *chip)
{
		
        static bool charger_resumed = true;
#ifdef CONFIG_OPPO_CHARGER_MTK
        static int charger_flag = 0;
#endif
        if (chip->chg_ops->check_chrdet_status()) 
		{
        	oppo_chg_set_awake(chip, true);

            if (chip->charger_type == POWER_SUPPLY_TYPE_UNKNOWN) 
			{
#ifdef ODM_WT_EDIT
/*Shouli.Wang@ODM_WT.BSP.CHG 2019/10/28, add for batt_param_noplug*/
				noplug_temperature = chip->temperature;
				noplug_batt_volt_max = chip->batt_volt_max;
				noplug_batt_volt_min = chip->batt_volt_min;
#endif /*ODM_WT_EDIT*/
            	oppo_chg_variables_reset(chip, true);
				#ifdef CONFIG_OPPO_CHARGER_MTK	
				if(is_meta_mode() == true){
					chip->charger_type = POWER_SUPPLY_TYPE_USB;
				} else {
					chip->charger_type = chip->chg_ops->get_charger_type();
				}
				if((chip->chg_ops->usb_connect) && (chip->charger_type == POWER_SUPPLY_TYPE_USB || chip->charger_type == POWER_SUPPLY_TYPE_USB_CDP)) {
					chip->chg_ops->usb_connect();
                    charger_flag = 1;
				} 
				#else
					chip->charger_type = chip->chg_ops->get_charger_type();
				#endif

#ifdef ODM_WT_EDIT
/* Tongxing.Liu@ODM_WT.BSP.Tp.Funtion, 20191111,Add for tp recognoise usb plug in or out */
                if(tp_usb_flag == 0){                
                    tp_usb_notifier_call_chain(1,NULL);
                    tp_usb_flag = 1;
                    charger_xlog_printk(CHG_LOG_CRTI, "usb in charger_flag=%d, tp_usb_flag=%d\n", charger_flag, tp_usb_flag);
                }
#endif /*ODM_WT_EDIT*/

				charger_xlog_printk(CHG_LOG_CRTI, "Charger in 1 charger_type=%d\n", chip->charger_type);
                if (oppo_vooc_get_fastchg_to_normal() == true || oppo_vooc_get_fastchg_to_warm() == true) {
                        charger_xlog_printk(CHG_LOG_CRTI, "fast_to_normal or to_warm 1,don't turn on charge here\n");
                } else {
                        charger_resumed = chip->chg_ops->check_charger_resume();
                        oppo_chg_turn_on_charging(chip);
                }
                /*chg_err("Charger in, charger_type=%d\n", chip->charger_type);*/
            } 
			else 
			{
                if (oppo_vooc_get_fastchg_to_normal() == true || oppo_vooc_get_fastchg_to_warm() == true) {
                        /*do nothing*/
                        charger_xlog_printk(CHG_LOG_CRTI, "fast_to_normal or to_warm 2,don't turn on charge here\n");
                } else if (oppo_vooc_get_fastchg_started() == false && charger_resumed == false) {
                        charger_resumed = chip->chg_ops->check_charger_resume();
                        oppo_chg_turn_on_charging(chip);
		        }		
            }
        } 
		else 
		{
            oppo_chg_variables_reset(chip, false);
            if (!chip->mmi_fastchg) {
                    oppo_mmi_fastchg_in(chip);
            }
            oppo_gauge_set_batt_full(false);
			#ifdef CONFIG_OPPO_CHARGER_MTK
            if (chip->chg_ops->usb_disconnect && charger_flag == 1) {
                   chip->chg_ops->usb_disconnect();
                   charger_flag = 0;
            }
			#endif

#ifdef ODM_WT_EDIT
/* Tongxing.Liu@ODM_WT.BSP.Tp.Funtion, 20191111,Add for tp recognoise usb plug in or out */
            if(tp_usb_flag == 1){
                tp_usb_notifier_call_chain(0,NULL);
                tp_usb_flag = 0;
                charger_xlog_printk(CHG_LOG_CRTI, "usb out charger_flag=%d, tp_usb_flag=%d\n", charger_flag, tp_usb_flag);
            }
#endif /*ODM_WT_EDIT*/

            if (chip->chg_ops->get_charging_enable() == true) {
                    oppo_chg_turn_off_charging(chip);
            }
            oppo_chg_set_awake(chip, false);
        }
}

#define RETRY_COUNTS        12
static void oppo_chg_get_battery_data(struct oppo_chg_chip *chip)
{
        static int ui_soc_cp_flag = 0;
        static int soc_load = 0;
        int remain_100_thresh = 97;
        static int retry_counts = 0;

        if (oppo_vooc_get_fastchg_started() == true) {
                chip->batt_volt = oppo_gauge_get_prev_batt_mvolts();
                chip->batt_volt_max = oppo_gauge_get_prev_batt_mvolts_2cell_max();
                chip->batt_volt_min = oppo_gauge_get_prev_batt_mvolts_2cell_min();
                chip->icharging = oppo_gauge_get_prev_batt_current();
                chip->temperature = oppo_gauge_get_prev_batt_temperature();
                chip->soc = oppo_gauge_get_prev_batt_soc();
                chip->batt_rm = oppo_gauge_get_prev_remaining_capacity() * chip->vbatt_num;
        } else {
                chip->batt_volt = oppo_gauge_get_batt_mvolts();
                chip->batt_volt_max = oppo_gauge_get_batt_mvolts_2cell_max();
                chip->batt_volt_min = oppo_gauge_get_batt_mvolts_2cell_min();
                chip->icharging = oppo_gauge_get_batt_current();
                chip->temperature = oppo_gauge_get_batt_temperature();
                chip->soc = oppo_gauge_get_batt_soc();
                chip->batt_fcc = oppo_gauge_get_batt_fcc() * chip->vbatt_num;
                chip->batt_cc = oppo_gauge_get_batt_cc() * chip->vbatt_num;
                chip->batt_soh = oppo_gauge_get_batt_soh();
                chip->batt_rm = oppo_gauge_get_remaining_capacity() * chip->vbatt_num;
        }
        chip->charger_volt = chip->chg_ops->get_charger_volt();
#ifdef ODM_WT_EDIT
/*Shouli.Wang@ODM_WT.BSP.CHG 2019/12/04, add for chg debug*/
	if(ap_temp_debug == 65535)
		chip->ap_temp = get_ap_temp();
	else
		chip->ap_temp = ap_temp_debug;
#endif /*ODM_WT_EDIT*/

        if (ui_soc_cp_flag == 0) {
                if ((chip->soc < 0 || chip->soc > 100) && retry_counts < RETRY_COUNTS) {
                        charger_xlog_printk(CHG_LOG_CRTI, "[Battery]oppo_chg_get_battery_data,chip->soc[%d],retry_counts[%d]\n", chip->soc, retry_counts);
                        retry_counts++;
                        chip->soc = 50;
                        goto next;
                }

                ui_soc_cp_flag = 1;
                if( chip->chg_ops->get_rtc_soc() > 100)
                        soc_load = chip->soc;
                else
                        soc_load = chip->chg_ops->get_rtc_soc();
                chip->soc_load = soc_load;
                if ((chip->soc < 0 || chip->soc > 100) && soc_load > 0 && soc_load <= 100) {
                        chip->soc = soc_load;
                }
                if ((soc_load != 0) && ((abs(soc_load-chip->soc)) <= 20)) {
                        if (chip->suspend_after_full && chip->external_gauge) {
                                remain_100_thresh = 95;
                        } else if (chip->suspend_after_full && !chip->external_gauge) {
                                remain_100_thresh = 94;
                        } else if (!chip->suspend_after_full && chip->external_gauge) {
                                remain_100_thresh = 97;
                        } else if (!chip->suspend_after_full && !chip->external_gauge) {
                                remain_100_thresh = 95;
                        } else {
                                remain_100_thresh = 97;
                        }
                        if (chip->soc < soc_load) {
                                if (soc_load == 100 && chip->soc > remain_100_thresh) {
                                        chip->ui_soc = soc_load;
                                } else {
                                        chip->ui_soc = soc_load - 1;
                                }
                        } else {
                                chip->ui_soc = soc_load;
                        }
                } else {
                        chip->ui_soc = chip->soc;
                        if (!chip->external_gauge && soc_load == 0 && chip->soc < 5) {
                                chip->ui_soc = 0;
                        }
                }
                charger_xlog_printk(CHG_LOG_CRTI, "[Battery]oppo_chg_get_battery_data,soc_load = %d,soc = %d\n", soc_load, chip->soc);
        }
        next:
        return;
}


/*need to extend it*/
static void oppo_chg_set_aicl_point(struct oppo_chg_chip *chip)
{
        if (oppo_vooc_get_allow_reading() == true) {
                chip->chg_ops->set_aicl_point(chip->batt_volt);
#ifdef ODM_WT_EDIT
//Junbo.Guo@ODM_WT.BSP.CHG, 2019/11/11, Modify for pe20
		if(chip->is_double_charger_support)
				chip->sub_chg_ops->set_aicl_point(chip->batt_volt);
#endif				
        }
}

#define AICL_DELAY_15MIN                        180
static void oppo_chg_check_aicl_input_limit(struct oppo_chg_chip *chip)
{
        static int aicl_delay_count = 0;
#ifdef CONFIG_OPPO_CHARGER_MTK
        if (chip->charging_state == CHARGING_STATUS_FAIL || chip->batt_full == true
                || ((chip->tbatt_status != BATTERY_STATUS__NORMAL) && (chip->tbatt_status != BATTERY_STATUS__LITTLE_COOL_TEMP))
                || chip->ui_soc > 85 || oppo_vooc_get_fastchg_started() == true) {
                aicl_delay_count = 0;
                return;
        }

        if (aicl_delay_count > AICL_DELAY_15MIN) {
                aicl_delay_count = 0;
                if (oppo_vooc_get_allow_reading() == true) {
                        oppo_chg_set_input_current_limit(chip);
                }
        } else {
                aicl_delay_count++;
        }
#else
        if (chip->charging_state == CHARGING_STATUS_FAIL || chip->batt_full == true
                || ((chip->tbatt_status != BATTERY_STATUS__NORMAL) && (chip->tbatt_status != BATTERY_STATUS__LITTLE_COOL_TEMP))
                || ((chip->ui_soc > 85) && (chip->pmic_spmi.aicl_suspend == false)) || oppo_vooc_get_fastchg_started() == true) {
                aicl_delay_count = 0;
                return;
        }

        if (aicl_delay_count > AICL_DELAY_15MIN) {
                aicl_delay_count = 0;
                if (oppo_vooc_get_allow_reading() == true) {
                        oppo_chg_set_input_current_limit(chip);
                }
        } else if (chip->pmic_spmi.aicl_suspend == true && chip->charger_volt > 4450 && chip->charger_volt < 5800) {
                aicl_delay_count = 0;
                if (oppo_vooc_get_allow_reading() == true) {
                        chip->chg_ops->rerun_aicl();
                        oppo_chg_set_input_current_limit(chip);
                }
                charger_xlog_printk(CHG_LOG_CRTI, "chip->charger_volt=%d\n", chip->charger_volt);
        } else {
                aicl_delay_count++;
        }
        if (chip->charger_type == POWER_SUPPLY_TYPE_USB || chip->charger_type == POWER_SUPPLY_TYPE_USB_CDP) {
                chip->pmic_spmi.usb_hc_count++;
                if (chip->pmic_spmi.usb_hc_count >= 3) {
                        chip->pmic_spmi.usb_hc_mode = true;
                        chip->pmic_spmi.usb_hc_count = 3;
                }
        }
        if (oppo_vooc_get_allow_reading() == true && chip->pmic_spmi.usb_hc_mode && !chip->pmic_spmi.hc_mode_flag) {
                oppo_chg_set_input_current_limit(chip);
                chip->pmic_spmi.hc_mode_flag = true;
        }
#endif
}

static void oppo_chg_aicl_check(struct oppo_chg_chip *chip)
{
        if (oppo_vooc_get_fastchg_started() == false) {
                oppo_chg_set_aicl_point(chip);
                oppo_chg_check_aicl_input_limit(chip);
        }
}

static void oppo_chg_protection_check(struct oppo_chg_chip *chip)
{
        if (false == oppo_chg_check_tbatt_is_good(chip)) {
                chg_err("oppo_chg_check_tbatt_is_good func ,false!\n");
                oppo_chg_voter_charging_stop(chip, CHG_STOP_VOTER__BATTTEMP_ABNORMAL);
        } else {
                if ((chip->stop_voter & CHG_STOP_VOTER__BATTTEMP_ABNORMAL) == CHG_STOP_VOTER__BATTTEMP_ABNORMAL) {
                        charger_xlog_printk(CHG_LOG_CRTI, "oppo_chg_check_tbatt_is_good func ,true! To Normal\n");
                        oppo_chg_voter_charging_start(chip, CHG_STOP_VOTER__BATTTEMP_ABNORMAL);
                }
        }

        if (false == oppo_chg_check_vchg_is_good(chip)) {
                chg_err("oppo_chg_check_vchg_is_good func ,false!\n");
                oppo_chg_voter_charging_stop(chip, CHG_STOP_VOTER__VCHG_ABNORMAL);
        } else {
                if ((chip->stop_voter & CHG_STOP_VOTER__VCHG_ABNORMAL) == CHG_STOP_VOTER__VCHG_ABNORMAL) {
                        charger_xlog_printk(CHG_LOG_CRTI, "oppo_chg_check_vchg_is_good func ,true! To Normal\n");
                        oppo_chg_voter_charging_start(chip, CHG_STOP_VOTER__VCHG_ABNORMAL);
                }
        }

#ifdef FEATURE_VBAT_PROTECT
        if (false == oppo_chg_check_vbatt_is_good(chip)) {
                chg_err("oppo_chg_check_vbatt_is_good func ,false!\n");
                oppo_chg_voter_charging_stop(chip, CHG_STOP_VOTER__VBAT_TOO_HIGH);
        }
#endif

        if (false == oppo_chg_check_time_is_good(chip)) {
                chg_err("oppo_chg_check_time_is_good func ,false!\n");
                oppo_chg_voter_charging_stop(chip, CHG_STOP_VOTER__MAX_CHGING_TIME);
        }

        oppo_chg_check_vbatt_higher_than_4180mv(chip);

        oppo_chg_vfloat_over_check(chip);

		oppo_chg_check_ffc_temp_status(chip);

        if (chip->chg_ctrl_by_lcd) {
                oppo_chg_check_tled_status(chip);
                oppo_chg_check_led_on_ichging(chip);
        }

        if (chip->chg_ctrl_by_camera) {
                oppo_chg_check_camera_on_ichging(chip);
        }

        if (chip->chg_ctrl_by_calling) {
                oppo_chg_check_calling_on_ichging(chip);
        }
	if (chip->chg_ctrl_by_vooc) {
                oppo_chg_check_vooc_temp_status(chip);
                oppo_chg_check_vooc_ichging(chip);
        }
#ifdef ODM_WT_EDIT
/*Sidong.Zhao@ODM_WT.BSP.CHG 2019/11/15,monitor thermal status*/
	oppo_chg_check_thermal_on_ichging(chip);
#endif /*ODM_WT_EDIT*/
}


static void battery_notify_tbat_check(struct oppo_chg_chip *chip)
{
        static int count_removed = 0;
        static int count_high = 0;
        if (BATTERY_STATUS__HIGH_TEMP == chip->tbatt_status) {
                count_high++;
                charger_xlog_printk(CHG_LOG_CRTI, "[BATTERY] bat_temp(%d), BATTERY_STATUS__HIGH_TEMP count[%d]\n", chip->temperature, count_high);
                if (chip->charger_exist && count_high > 10) {
                        count_high = 11;
                        chip->notify_code |= 1 << NOTIFY_BAT_OVER_TEMP;
                        charger_xlog_printk(CHG_LOG_CRTI, "[BATTERY] bat_temp(%d) > 55'C\n", chip->temperature);
                }
        } else {
                count_high = 0;
        }

        if (BATTERY_STATUS__LOW_TEMP == chip->tbatt_status) {
                if (chip->charger_exist) {
                        chip->notify_code |= 1 << NOTIFY_BAT_LOW_TEMP;
                        charger_xlog_printk(CHG_LOG_CRTI, "[BATTERY] bat_temp(%d) < -10'C\n", chip->temperature);
                }
        }

        if (BATTERY_STATUS__REMOVED == chip->tbatt_status) {
                count_removed ++;
                charger_xlog_printk(CHG_LOG_CRTI, "[BATTERY] bat_temp(%d), BATTERY_STATUS__REMOVED count[%d]\n", chip->temperature, count_removed);
                if (count_removed > 10) {
                        count_removed = 11;
                        chip->notify_code |= 1 << NOTIFY_BAT_NOT_CONNECT;
                        charger_xlog_printk(CHG_LOG_CRTI, "[BATTERY] bat_temp(%d) < -19'C\n", chip->temperature);
                }
        } else {
                count_removed = 0;
        }
}

static void battery_notify_authenticate_check(struct oppo_chg_chip *chip)
{
        if (!chip->authenticate) {
                chip->notify_code |= 1 << NOTIFY_BAT_NOT_CONNECT;
                charger_xlog_printk(CHG_LOG_CRTI, "[BATTERY] bat_authenticate is false!\n");
        }
}

static void battery_notify_vcharger_check(struct oppo_chg_chip *chip)
{
        if (CHARGER_STATUS__VOL_HIGH == chip->vchg_status) {
                chip->notify_code |= 1 << NOTIFY_CHARGER_OVER_VOL;
                charger_xlog_printk(CHG_LOG_CRTI, "[BATTERY] check_charger_off_vol(%d) > 5800mV\n", chip->charger_volt);
        }

        if (CHARGER_STATUS__VOL_LOW == chip->vchg_status) {
                chip->notify_code |= 1 << NOTIFY_CHARGER_LOW_VOL;
                charger_xlog_printk(CHG_LOG_CRTI, "[BATTERY] check_charger_off_vol(%d) < 3400mV\n", chip->charger_volt);
        }
}

static void battery_notify_vbat_check(struct oppo_chg_chip *chip)
{
        static int count = 0;

        if (true == chip->vbatt_over) {
                count++;
                charger_xlog_printk(CHG_LOG_CRTI, "[BATTERY] Battery is over VOL, count[%d] \n", count);
                if (count > 10) {
                        count = 11;
                        chip->notify_code |= 1 << NOTIFY_BAT_OVER_VOL;
                        charger_xlog_printk(CHG_LOG_CRTI, "[BATTERY] Battery is over VOL! Notify \n");
                }
        } else {
                count = 0;
                if ((chip->batt_full) && (chip->charger_exist)) {
                        if (chip->tbatt_status == BATTERY_STATUS__WARM_TEMP && chip->ui_soc != 100) {
                                chip->notify_code |=  1 << NOTIFY_BAT_FULL_PRE_HIGH_TEMP;
                        } else if ((chip->tbatt_status == BATTERY_STATUS__COLD_TEMP) && (chip->ui_soc != 100)) {
                                chip->notify_code |=  1 << NOTIFY_BAT_FULL_PRE_LOW_TEMP;
                        } else if (!chip->authenticate) {
                                /*chip->notify_code |=  1 << NOTIFY_BAT_FULL_THIRD_BATTERY;*/
                        } else {
                                if (chip->ui_soc == 100) {
                                        chip->notify_code |=  1 << NOTIFY_BAT_FULL;
                                }
                        }
                        charger_xlog_printk(CHG_LOG_CRTI, "[BATTERY] FULL,tbatt_status:%d,notify_code:%d\n",
                                chip->tbatt_status, chip->notify_code);
                }
        }
}

static void battery_notify_max_charging_time_check(struct oppo_chg_chip *chip)
{
        if (true == chip->chging_over_time) {
                chip->notify_code |= 1 << NOTIFY_CHGING_OVERTIME;
                charger_xlog_printk(CHG_LOG_CRTI, "[BATTERY] Charging is OverTime!Notify \n");
        }
}

static void battery_notify_short_c_battery_check(struct oppo_chg_chip *chip)
{
        if (chip->short_c_batt.err_code == SHORT_C_BATT_STATUS__CV_ERR_CODE1) {
                chip->notify_code |= 1 << NOTIFY_SHORT_C_BAT_CV_ERR_CODE1;
                charger_xlog_printk(CHG_LOG_CRTI, "[BATTERY] battery is short circuit! err_code1!\n");
        }

        if (chip->short_c_batt.err_code == SHORT_C_BATT_STATUS__FULL_ERR_CODE2) {
                chip->notify_code |= 1 << NOTIFY_SHORT_C_BAT_FULL_ERR_CODE2;
                charger_xlog_printk(CHG_LOG_CRTI, "[BATTERY] battery is short circuit! err_code2!\n");
                }

        if (chip->short_c_batt.err_code == SHORT_C_BATT_STATUS__FULL_ERR_CODE3) {
                chip->notify_code |= 1 << NOTIFY_SHORT_C_BAT_FULL_ERR_CODE3;
                charger_xlog_printk(CHG_LOG_CRTI, "[BATTERY] battery is short circuit! err_code3!\n");
        }

        if (chip->short_c_batt.err_code == SHORT_C_BATT_STATUS__DYNAMIC_ERR_CODE4) {
                chip->notify_code |= 1 << NOTIFY_SHORT_C_BAT_DYNAMIC_ERR_CODE4;
                charger_xlog_printk(CHG_LOG_CRTI, "[BATTERY] battery is short circuit! err_code4!\n");
        }

        if (chip->short_c_batt.err_code == SHORT_C_BATT_STATUS__DYNAMIC_ERR_CODE5) {
                chip->notify_code |= 1 << NOTIFY_SHORT_C_BAT_DYNAMIC_ERR_CODE5;
                charger_xlog_printk(CHG_LOG_CRTI, "[BATTERY] battery is short circuit! err_code5!\n");
        }
}


static void battery_notify_flag_check(struct oppo_chg_chip *chip)
{
        if (chip->notify_code & (1 << NOTIFY_CHGING_OVERTIME)) {
                chip->notify_flag = NOTIFY_CHGING_OVERTIME;
        } else if (chip->notify_code & (1 << NOTIFY_CHARGER_OVER_VOL)) {
                chip->notify_flag = NOTIFY_CHARGER_OVER_VOL;
        } else if (chip->notify_code & (1 << NOTIFY_CHARGER_LOW_VOL)) {
                chip->notify_flag = NOTIFY_CHARGER_LOW_VOL;
        } else if (chip->notify_code & (1 << NOTIFY_BAT_OVER_TEMP)) {
                chip->notify_flag = NOTIFY_BAT_OVER_TEMP;
        } else if (chip->notify_code & (1 << NOTIFY_BAT_LOW_TEMP)) {
                chip->notify_flag = NOTIFY_BAT_LOW_TEMP;
        } else if (chip->notify_code & (1 << NOTIFY_BAT_NOT_CONNECT)) {
                chip->notify_flag = NOTIFY_BAT_NOT_CONNECT;
        } else if (chip->notify_code & (1 << NOTIFY_BAT_OVER_VOL)) {
                chip->notify_flag = NOTIFY_BAT_OVER_VOL;
        } else if (chip->notify_code & (1 << NOTIFY_BAT_FULL_PRE_HIGH_TEMP)) {
                chip->notify_flag = NOTIFY_BAT_FULL_PRE_HIGH_TEMP;
        } else if (chip->notify_code & (1 << NOTIFY_BAT_FULL_PRE_LOW_TEMP)) {
                chip->notify_flag = NOTIFY_BAT_FULL_PRE_LOW_TEMP;
        } else if (chip->notify_code & (1 << NOTIFY_BAT_FULL)) {
                chip->notify_flag = NOTIFY_BAT_FULL;
        } else {
                chip->notify_flag = 0;
        }
}

static void battery_notify_charge_terminal_check(struct oppo_chg_chip *chip)
{
        if (chip->batt_full == true && chip->charger_exist == true)
                chip->notify_code |= 1 << NOTIFY_CHARGER_TERMINAL;
}

static void oppo_chg_battery_notify_check(struct oppo_chg_chip *chip)
{
        chip->notify_code = 0x0000;

        battery_notify_tbat_check(chip);

        battery_notify_authenticate_check(chip);

        battery_notify_vcharger_check(chip);

        battery_notify_vbat_check(chip);

        battery_notify_max_charging_time_check(chip);

        battery_notify_short_c_battery_check(chip);

        battery_notify_charge_terminal_check(chip);

        battery_notify_flag_check(chip);
}

int oppo_chg_get_prop_batt_health(struct oppo_chg_chip *chip)
{
        int bat_health = POWER_SUPPLY_HEALTH_GOOD;
        bool vbatt_over = chip->vbatt_over;
        OPPO_CHG_TBATT_STATUS tbatt_status = chip->tbatt_status;

        if (vbatt_over == true) {
                bat_health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
        } else if (tbatt_status == BATTERY_STATUS__REMOVED) {
                bat_health = POWER_SUPPLY_HEALTH_DEAD;
        } else if (tbatt_status == BATTERY_STATUS__HIGH_TEMP) {
                bat_health = POWER_SUPPLY_HEALTH_OVERHEAT;
        } else if (tbatt_status == BATTERY_STATUS__LOW_TEMP) {
                bat_health = POWER_SUPPLY_HEALTH_COLD;
        } else {
                bat_health = POWER_SUPPLY_HEALTH_GOOD;
        }
        return bat_health;
}

static bool oppo_chg_soc_reduce_slow_when_1(struct oppo_chg_chip *chip)
{
        static int reduce_count = 0;
        int reduce_count_limit = 0;

        if (chip->batt_exist == false) {
                return false;
        }

        if (chip->charger_exist) {
                reduce_count_limit = 12;
        } else {
                reduce_count_limit = 4;
        }
        if (chip->batt_volt_min < chip->vbatt_soc_1) {
                reduce_count++;
        } else {
                reduce_count = 0;
        }

        charger_xlog_printk(CHG_LOG_CRTI, "batt_vol:%d, batt_volt_min:%d, reduce_count:%d\n", chip->batt_volt, chip->batt_volt_min, reduce_count);
        if (reduce_count > reduce_count_limit) {
                reduce_count = reduce_count_limit + 1;
                return true;
        } else {
                return false;
        }
}

#define SOC_SYNC_UP_RATE_10S                                  2
#define SOC_SYNC_UP_RATE_60S                                  12
#define SOC_SYNC_DOWN_RATE_300S                               60
#define SOC_SYNC_DOWN_RATE_150S                               30
#define SOC_SYNC_DOWN_RATE_90S                                18
#define SOC_SYNC_DOWN_RATE_60S                                12
#define SOC_SYNC_DOWN_RATE_40S                                8
#define SOC_SYNC_DOWN_RATE_30S                                6
#define SOC_SYNC_DOWN_RATE_15S                                3
#define TEN_MINUTES                                           600

static void oppo_chg_update_ui_soc(struct oppo_chg_chip *chip)
{
        static int soc_down_count = 0, soc_up_count = 0, ui_soc_pre = 50;
        int soc_down_limit = 0, soc_up_limit = 0;
        unsigned long sleep_tm = 0 , soc_reduce_margin = 0;
        bool vbatt_too_low = false;
        vbatt_lowerthan_3300mv = false;

        if (chip->ui_soc == 100) {
                soc_down_limit = SOC_SYNC_DOWN_RATE_300S;
        } else if (chip->ui_soc >= 95) {
                soc_down_limit = SOC_SYNC_DOWN_RATE_150S;
        } else if (chip->ui_soc >= 60) {
                soc_down_limit = SOC_SYNC_DOWN_RATE_60S;
        } else if (chip->charger_exist && chip->ui_soc == 1) {
                soc_down_limit = SOC_SYNC_DOWN_RATE_90S;
        } else {
                soc_down_limit = SOC_SYNC_DOWN_RATE_40S;
        }

        if (chip->batt_exist && (chip->batt_volt_min < chip->vbatt_power_off) && (chip->batt_volt_min > 2500)) {
                soc_down_limit = SOC_SYNC_DOWN_RATE_15S;
                vbatt_too_low = true;
                vbatt_lowerthan_3300mv = true;
                charger_xlog_printk(CHG_LOG_CRTI, "batt_volt:%d, batt_volt_min:%d, vbatt_too_low:%d\n", chip->batt_volt, chip->batt_volt_min, vbatt_too_low);
        }
        if (chip->batt_full) {
                soc_up_limit = SOC_SYNC_UP_RATE_10S;
        } else {
                soc_up_limit = SOC_SYNC_UP_RATE_10S;
        }
        if (chip->charger_exist && chip->batt_exist && chip->batt_full && chip->mmi_chg && (chip->stop_chg == 1 || chip->charger_type == 5)) {
                chip->sleep_tm_sec = 0;
                if (oppo_short_c_batt_is_prohibit_chg(chip)) {
                        chip->prop_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
#ifdef ODM_WT_EDIT
/*Shouli.Wang@ODM_WT.BSP.CHG 2019/12/05, add for vbat over stop chg*/
                } else if(chip->vbatt_over == true){
						chip->prop_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
#endif /*ODM_WT_EDIT*/
				} else if ((chip->tbatt_status == BATTERY_STATUS__NORMAL) || (chip->tbatt_status == BATTERY_STATUS__LITTLE_COOL_TEMP)
                        || (chip->tbatt_status == BATTERY_STATUS__COOL_TEMP) || (chip->tbatt_status == BATTERY_STATUS__LITTLE_COLD_TEMP)) {
                        soc_down_count = 0;
                        soc_up_count++;
                        if (soc_up_count >= soc_up_limit) {
                                soc_up_count = 0;
                                chip->ui_soc++;
                        }
                        if (chip->ui_soc >= 100) {
                                chip->ui_soc = 100;
                                chip->prop_status = POWER_SUPPLY_STATUS_FULL;
                        } else {
                                chip->prop_status = POWER_SUPPLY_STATUS_CHARGING;
                        }
                } else {
                        chip->prop_status = POWER_SUPPLY_STATUS_FULL;
                }
                if (chip->ui_soc != ui_soc_pre) {
                        charger_xlog_printk(CHG_LOG_CRTI, "full ui_soc:%d,soc:%d,up_limit:%d\n", chip->ui_soc, chip->soc, soc_up_limit);
                }
        } else if (chip->charger_exist && chip->batt_exist && (CHARGING_STATUS_FAIL != chip->charging_state) && chip->mmi_chg && (chip->stop_chg == 1 || chip->charger_type == 5)) {
                chip->sleep_tm_sec = 0;
                chip->prop_status = POWER_SUPPLY_STATUS_CHARGING;
                if (chip->soc == chip->ui_soc) {
                        soc_down_count = 0;
                        soc_up_count = 0;
                } else if (chip->soc > chip->ui_soc) {
                        soc_down_count = 0;
                        soc_up_count++;
                        if (soc_up_count >= soc_up_limit) {
                                soc_up_count = 0;
                                chip->ui_soc++;
                        }
                } else if (chip->soc < chip->ui_soc) {
                        soc_up_count = 0;
                        soc_down_count++;
                        if (soc_down_count >= soc_down_limit) {
                                soc_down_count = 0;
                                chip->ui_soc--;
                        }
                }
                if (chip->ui_soc != ui_soc_pre) {
                        charger_xlog_printk(CHG_LOG_CRTI, "charging ui_soc:%d,soc:%d,down_limit:%d,up_limit:%d\n",
                                chip->ui_soc, chip->soc, soc_down_limit, soc_up_limit);
                }
        } else {
                chip->prop_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
                soc_up_count = 0;
                if (chip->soc <= chip->ui_soc || vbatt_too_low) {
                        if (soc_down_count > soc_down_limit) {
                                soc_down_count = soc_down_limit + 1;
                        } else {
                                soc_down_count++;
                        }
                        sleep_tm = chip->sleep_tm_sec;
                        if (chip->sleep_tm_sec > 0) {
                                soc_reduce_margin = chip->sleep_tm_sec / TEN_MINUTES;
                                if (soc_reduce_margin == 0) {
                                        if ((chip->ui_soc - chip->soc) > 2) {
                                                chip->ui_soc--;
                                                soc_down_count = 0;
                                                chip->sleep_tm_sec = 0;
                                        }
                                } else if (soc_reduce_margin < (chip->ui_soc - chip->soc)) {
                                        chip->ui_soc -= soc_reduce_margin;
                                        soc_down_count = 0;
                                        chip->sleep_tm_sec = 0;
                                } else if (soc_reduce_margin >= (chip->ui_soc - chip->soc)) {
                                        chip->ui_soc = chip->soc;
                                        soc_down_count = 0;
                                        chip->sleep_tm_sec = 0;
                                }
                        }
                        if (soc_down_count >= soc_down_limit && (chip->soc < chip->ui_soc || vbatt_too_low)) {
                                chip->sleep_tm_sec = 0;
                                soc_down_count = 0;
                                chip->ui_soc--;
                        }
                }
                if (chip->ui_soc != ui_soc_pre) {
                        charger_xlog_printk(CHG_LOG_CRTI, "discharging ui_soc:%d,soc:%d,down_limit:%d,sleep_tm:%ld\n",
                                chip->ui_soc, chip->soc, soc_down_limit, sleep_tm);
                }
        }

        if (chip->ui_soc < 2) {
                if (oppo_chg_soc_reduce_slow_when_1(chip) == true) {
                        chip->ui_soc = 0;
                } else {
                        chip->ui_soc = 1;
                }
        }
        if (chip->ui_soc != ui_soc_pre) {
                ui_soc_pre = chip->ui_soc;
                chip->chg_ops->set_rtc_soc(chip->ui_soc);
                if (chip->chg_ops->get_rtc_soc() != chip->ui_soc) {
                        /*charger_xlog_printk(CHG_LOG_CRTI, "set soc fail:[%d, %d], try again...\n", chip->ui_soc, chip->chg_ops->get_rtc_soc());*/
                        chip->chg_ops->set_rtc_soc(chip->ui_soc);
                }
        }
}
static void fg_update(struct oppo_chg_chip *chip)
{
        static int ui_soc_pre_fg = 50;
        static struct power_supply *bms_psy = NULL;
        if (!bms_psy) {
                bms_psy = power_supply_get_by_name("bms");
                charger_xlog_printk(CHG_LOG_CRTI, "bms_psy null\n");
        }
        if (bms_psy) {
                if (chip->ui_soc != ui_soc_pre_fg) {
                        power_supply_changed(bms_psy);
                        charger_xlog_printk(CHG_LOG_CRTI, "ui_soc:%d, soc:%d, ui_soc_pre:%d \n", chip->ui_soc, chip->soc, ui_soc_pre_fg);
                }
                if (chip->ui_soc != ui_soc_pre_fg) {
                        ui_soc_pre_fg = chip->ui_soc;
                }
        }
}
static void battery_update(struct oppo_chg_chip *chip)
{
        oppo_chg_update_ui_soc(chip);

        if (chip->fg_bcl_poll) {
                fg_update(chip);
        }
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
        power_supply_changed(chip->batt_psy);
#else
        power_supply_changed(&chip->batt_psy);
#endif
}

static void oppo_chg_battery_update_status(struct oppo_chg_chip *chip)
{
#ifdef CONFIG_OPPO_CHARGER_MTK
        usb_update(chip);
#endif
        battery_update(chip);
}

#define RESET_MCU_DELAY_30S     6

static void oppo_chg_get_chargerid_voltage(struct oppo_chg_chip *chip)
{
        if (chip->chg_ops->set_chargerid_switch_val == NULL
                || chip->chg_ops->get_chargerid_switch_val == NULL
                || chip->chg_ops->get_chargerid_volt == NULL) {
                return;
        } else if (chip->charger_type != POWER_SUPPLY_TYPE_USB_DCP) {
                return;
        }

	 if (reset_mcu_delay > RESET_MCU_DELAY_30S)
                return;
	 
        if (oppo_vooc_get_vooc_switch_val() == 1) {
                if (chip->chargerid_volt_got == false) {
                        chip->chg_ops->set_chargerid_switch_val(1);
#ifdef CONFIG_OPPO_CHARGER_MTK
						if (oppo_vooc_get_fastchg_started() == false){
							oppo_vooc_switch_mode(NORMAL_CHARGER_MODE);
						}
						usleep_range(100000, 110000);
#else	
                        usleep_range(20000, 22000);
#endif /* CONFIG_OPPO_CHARGER_MTK */
                        chip->chargerid_volt = chip->chg_ops->get_chargerid_volt();
                        chip->chargerid_volt_got = true;
                } else {
                        if (chip->chg_ops->get_chargerid_switch_val() == 0) {
                                chip->chg_ops->set_chargerid_switch_val(1);
                        } else {
                                /* do nothing*/
                        }
                }
        } else if (oppo_vooc_get_vooc_switch_val() == 0) {
                if (chip->chargerid_volt_got == false) {
                        chip->chg_ops->set_chargerid_switch_val(1);
#ifdef CONFIG_OPPO_CHARGER_MTK
						usleep_range(100000, 110000);
#else 
                        usleep_range(20000, 22000);
#endif /* CONFIG_OPPO_CHARGER_MTK */
                        chip->chargerid_volt = chip->chg_ops->get_chargerid_volt();
                        chip->chargerid_volt_got = true;
                        if (chip->vooc_project == false) {
                                chip->chg_ops->set_chargerid_switch_val(0);
                        }
                } else {
                        if (chip->chg_ops->get_chargerid_switch_val() == 1) {
                                chip->chg_ops->set_chargerid_switch_val(0);
                        } else {
                                /* do nothing*/
                        }
                }
        } else {
                charger_xlog_printk(CHG_LOG_CRTI, "do nothing\n");
        }
}

static void oppo_chg_chargerid_switch_check(struct oppo_chg_chip *chip)
{
        return oppo_chg_get_chargerid_voltage(chip);
}

#define RESET_MCU_DELAY_15S                        3
static void oppo_chg_qc_config(struct oppo_chg_chip *chip);
static void oppo_chg_fast_switch_check(struct oppo_chg_chip *chip)
{
	if (oppo_short_c_batt_is_prohibit_chg(chip)) {
		charger_xlog_printk(CHG_LOG_CRTI, " short_c_battery, return\n");
		return;
	}

	if (chip->mmi_chg == 0) {
		charger_xlog_printk(CHG_LOG_CRTI, " mmi_chg,return\n");
		return;
	}

	if (chip->allow_swtich_to_fastchg == false) {
		charger_xlog_printk(CHG_LOG_CRTI, " allow_swtich_to_fastchg == 0,return\n");
		return;
	}

	if (!chip->authenticate) {
		charger_xlog_printk(CHG_LOG_CRTI, "non authenticate,switch return\n");
		return;
	}

	if (chip->notify_flag == NOTIFY_BAT_OVER_VOL) {
		charger_xlog_printk(CHG_LOG_CRTI, " battery over voltage,return\n");
		return;
	}

	if (chip->charger_type == POWER_SUPPLY_TYPE_USB_DCP) {
		if (oppo_vooc_get_fastchg_started() == false && reset_mcu_delay < RESET_MCU_DELAY_30S) {
			oppo_vooc_switch_fast_chg();
		}

		if (!oppo_vooc_get_fastchg_started() && !oppo_vooc_get_fastchg_dummy_started() &&
			!oppo_vooc_get_fastchg_to_normal() && !oppo_vooc_get_fastchg_to_warm()) {
			reset_mcu_delay++;
			if (reset_mcu_delay == RESET_MCU_DELAY_15S) {
				charger_xlog_printk(CHG_LOG_CRTI, "  reset mcu again\n");
				oppo_vooc_set_ap_clk_high();
				oppo_vooc_reset_mcu();
			} else if (reset_mcu_delay == RESET_MCU_DELAY_30S) {
				reset_mcu_delay = RESET_MCU_DELAY_30S + 1;
				charger_xlog_printk(CHG_LOG_CRTI, "  RESET_MCU_DELAY_30S\n");
				if (chip->charger_volt <= 7500) {
					oppo_vooc_reset_fastchg_after_usbout();
					chip->chg_ops->set_chargerid_switch_val(0);
					if (chip->chg_ops->enable_qc_detect)
						chip->chg_ops->enable_qc_detect();
				}
			}
		}
	}
}

#define FULL_COUNTS_HW                4
static int oppo_chg_check_sw_full(struct oppo_chg_chip *chip)
{		
		int vbatt_full_vol_sw = 0;
        static int vbat_counts_sw = 0;
        static bool ret_sw = false;
		
	    if (!chip->charger_exist) {
	            vbat_counts_sw = 0;
                ret_sw = false;
				return false;
        }

	    if (chip->tbatt_status == BATTERY_STATUS__COLD_TEMP) {
                vbatt_full_vol_sw = chip->limits.cold_vfloat_sw_limit;
        } else if (chip->tbatt_status == BATTERY_STATUS__LITTLE_COLD_TEMP) {
                vbatt_full_vol_sw = chip->limits.little_cold_vfloat_sw_limit;
        } else if (chip->tbatt_status == BATTERY_STATUS__COOL_TEMP) {
                vbatt_full_vol_sw = chip->limits.cool_vfloat_sw_limit;
        } else if (chip->tbatt_status == BATTERY_STATUS__LITTLE_COOL_TEMP) {
                vbatt_full_vol_sw = chip->limits.little_cool_vfloat_sw_limit;
        } else if (chip->tbatt_status == BATTERY_STATUS__NORMAL) {
                vbatt_full_vol_sw = chip->limits.normal_vfloat_sw_limit;
        } else if (chip->tbatt_status == BATTERY_STATUS__WARM_TEMP) {
                vbatt_full_vol_sw = chip->limits.warm_vfloat_sw_limit;
        } else {
                vbat_counts_sw = 0;
                ret_sw = 0;
                return false;
        }

        if (!chip->authenticate) {
        		vbatt_full_vol_sw = chip->limits.non_standard_vfloat_sw_limit;
        }

		 if (oppo_short_c_batt_is_prohibit_chg(chip)) {
                vbatt_full_vol_sw = chip->limits.short_c_bat_vfloat_sw_limit;
        }

/* use SW Vfloat to check */
        if (chip->batt_volt > vbatt_full_vol_sw) {
                if (chip->icharging < 0 && (chip->icharging * -1) <= chip->limits.iterm_ma) {
                        vbat_counts_sw++;
                        if (vbat_counts_sw > chip->sw_full_count) {
                                vbat_counts_sw = 0;
                                ret_sw = true;
                        }
                } else if (chip->icharging >= 0) {
                        vbat_counts_sw++;
                        if (vbat_counts_sw > (chip->sw_full_count) * 2) {
                                vbat_counts_sw = 0;
                                ret_sw = true;
                                charger_xlog_printk(CHG_LOG_CRTI, "[BATTERY] Battery full by sw when icharging>=0!!\n");
                        }
                } else {
                        vbat_counts_sw = 0;
                        ret_sw = false;
                }
        } else {
                vbat_counts_sw = 0;
                ret_sw = false;
        }
		
        return ret_sw;
}

static int oppo_chg_check_hw_full(struct oppo_chg_chip *chip)
{
        int vbatt_full_vol_hw = 0;
        static int vbat_counts_hw = 0;
        static bool ret_hw = false;
		
	    if (!chip->charger_exist) {
	            vbat_counts_hw = 0;
                ret_hw = false;
				return false;
        }
		
        vbatt_full_vol_hw = oppo_chg_get_float_voltage(chip);

		if (chip->tbatt_status == BATTERY_STATUS__COLD_TEMP) {
                vbatt_full_vol_hw = chip->limits.temp_cold_vfloat_mv + chip->limits.non_normal_vterm_hw_inc;
        } else if (chip->tbatt_status == BATTERY_STATUS__LITTLE_COLD_TEMP) {
                vbatt_full_vol_hw = chip->limits.temp_little_cold_vfloat_mv + chip->limits.non_normal_vterm_hw_inc;
        } else if (chip->tbatt_status == BATTERY_STATUS__COOL_TEMP) {
                vbatt_full_vol_hw = chip->limits.temp_cool_vfloat_mv + chip->limits.non_normal_vterm_hw_inc;
        } else if (chip->tbatt_status == BATTERY_STATUS__LITTLE_COOL_TEMP) {
                vbatt_full_vol_hw = chip->limits.temp_little_cool_vfloat_mv + chip->limits.non_normal_vterm_hw_inc;
        } else if (chip->tbatt_status == BATTERY_STATUS__NORMAL) {
                vbatt_full_vol_hw = chip->limits.temp_normal_vfloat_mv + chip->limits.normal_vterm_hw_inc;
        } else if (chip->tbatt_status == BATTERY_STATUS__WARM_TEMP) {
                vbatt_full_vol_hw = chip->limits.temp_warm_vfloat_mv + chip->limits.non_normal_vterm_hw_inc;
        } else {
                vbat_counts_hw = 0;
                ret_hw = 0;
                return false;
        }
		
		if (!chip->authenticate) {
        		vbatt_full_vol_hw = chip->limits.non_standard_vfloat_mv + chip->limits.non_normal_vterm_hw_inc;
        }

		if (oppo_short_c_batt_is_prohibit_chg(chip)) {
                vbatt_full_vol_hw = chip->limits.short_c_bat_vfloat_mv + chip->limits.non_normal_vterm_hw_inc;
        }

        /* use HW Vfloat to check */
        if (chip->batt_volt >= vbatt_full_vol_hw) {
                vbat_counts_hw++;
                if (vbat_counts_hw >= FULL_COUNTS_HW) {
                        vbat_counts_hw = 0;
                        ret_hw = true;
                }
        } else {
                vbat_counts_hw = 0;
                ret_hw = false;
        }

        return ret_hw;
}


#define FFC_VOLT_COUNTS		4
#define FFC_CURRENT_COUNTS	2

static void oppo_chg_ffc_variable_reset(struct oppo_chg_chip *chip)
{
	chip->fastchg_to_ffc = false;
	chip->fastchg_ffc_status = 0;
	chip->chg_ctrl_by_lcd = chip->chg_ctrl_by_lcd_default;
	chip->chg_ctrl_by_vooc = chip->chg_ctrl_by_vooc_default;
	chip->limits.iterm_ma = chip->limits.default_iterm_ma;
	chip->limits.normal_vfloat_sw_limit = chip->limits.default_normal_vfloat_sw_limit;
	chip->limits.temp_normal_vfloat_mv = chip->limits.default_temp_normal_vfloat_mv;
	chip->limits.normal_vfloat_over_sw_limit = chip->limits.default_normal_vfloat_over_sw_limit;
	chip->limits.temp_normal_fastchg_current_ma = chip->limits.default_temp_normal_fastchg_current_ma;

    chip->limits.temp_little_cool_fastchg_current_ma = chip->limits.default_temp_little_cool_fastchg_current_ma;
    chip->limits.little_cool_vfloat_sw_limit = chip->limits.default_little_cool_vfloat_sw_limit;
    chip->limits.temp_little_cool_vfloat_mv = chip->limits.default_temp_little_cool_vfloat_mv;
    chip->limits.little_cool_vfloat_over_sw_limit = chip->limits.default_little_cool_vfloat_over_sw_limit;
}


static bool oppo_chg_check_ffc_status(struct oppo_chg_chip *chip)
{
	static int vffc1_counts = 0,vffc2_counts = 0,warm_counts = 0,normal_counts = 0;

	if (chip->fastchg_to_ffc == true) {
		if (chip->fastchg_ffc_status == 1) {
			if (chip->batt_volt >= chip->limits.ffc1_normal_vfloat_sw_limit) {
				vffc1_counts ++;
				if (vffc1_counts >= FFC_VOLT_COUNTS) {
					oppo_chg_turn_on_ffc2(chip);
					return false;
				}
			}

			if (chip->ffc_temp_status == FFC_TEMP_STATUS__NORMAL) {
				if ((chip->icharging * -1) < (chip->limits.ff1_normal_fastchg_ma - chip->limits.ff1_exit_step_ma)) {
					normal_counts ++;
					if (normal_counts >= FFC_CURRENT_COUNTS) {
						oppo_chg_ffc_variable_reset(chip);
						oppo_chg_turn_on_charging(chip);
						return true;
					}
				} else {
					normal_counts = 0;
				}
			} else if (chip->ffc_temp_status == FFC_TEMP_STATUS__WARM) {
				if ((chip->icharging * -1) < (chip->limits.ff1_warm_fastchg_ma - chip->limits.ff1_warm_exit_step_ma)) {
					warm_counts ++;
					if (warm_counts >= FFC_CURRENT_COUNTS) {
						oppo_chg_ffc_variable_reset(chip);
						oppo_chg_turn_on_charging(chip);
						return true;
					}
				} else {
					warm_counts = 0;
				}
			} else {
				warm_counts = normal_counts = 0;
				oppo_chg_ffc_variable_reset(chip);
				oppo_chg_turn_on_charging(chip);
				return true;
			}
			return false;
		}

		if (chip->fastchg_ffc_status == 2) {
			if (chip->batt_volt >= chip->limits.ffc2_normal_vfloat_sw_limit) {
				vffc2_counts ++;
				if (vffc2_counts >= FFC_VOLT_COUNTS) {
					oppo_chg_ffc_variable_reset(chip);
					oppo_chg_turn_on_charging(chip);
					return true;
				}
			}
			if (chip->ffc_temp_status == FFC_TEMP_STATUS__NORMAL) {
				if ((chip->icharging * -1) < (chip->limits.ffc2_normal_fastchg_ma - chip->limits.ffc2_exit_step_ma)) {
					normal_counts ++;
					if (normal_counts >= FFC_CURRENT_COUNTS) {
						oppo_chg_ffc_variable_reset(chip);
						oppo_chg_turn_on_charging(chip);
						return true;
					}
				} else {
					normal_counts = 0;
				}
			} else if (chip->ffc_temp_status == FFC_TEMP_STATUS__WARM) {
				if ((chip->icharging * -1) < (chip->limits.ffc2_warm_fastchg_ma - chip->limits.ffc2_warm_exit_step_ma)) {
					warm_counts ++;
					if (warm_counts >= FFC_CURRENT_COUNTS) {
						oppo_chg_ffc_variable_reset(chip);
						oppo_chg_turn_on_charging(chip);
						return true;
					}
				} else {
					warm_counts = 0;
				}
			} else {
				warm_counts = normal_counts = 0;
				oppo_chg_ffc_variable_reset(chip);
				oppo_chg_turn_on_charging(chip);
				return true;
			}
		}

		return false;
	}

	vffc1_counts = vffc2_counts = warm_counts = normal_counts = 0;
	return true;
}

static bool oppo_chg_check_vbatt_is_full_by_sw(struct oppo_chg_chip *chip)
{
        bool ret_sw = false;
        bool ret_hw = false;

        if (!chip->check_batt_full_by_sw) {
                return false;
        }

		ret_sw = oppo_chg_check_sw_full(chip);

		ret_hw = oppo_chg_check_hw_full(chip);

        if (ret_sw == true || ret_hw == true) {
                charger_xlog_printk(CHG_LOG_CRTI, "[BATTERY] Battery full by sw[%s] !!\n", (ret_sw == true) ? "S" : "H");
                return true;
        } else {
                return false;
        }
}

#define FULL_DELAY_COUNTS                4
#define DOD0_COUNTS                		10*60/5

static void oppo_chg_check_status_full(struct oppo_chg_chip *chip)
{
        int is_batt_full = 0;
        static int fastchg_present_wait_count = 0;

        if (chip->chg_ctrl_by_vooc) {
		if (oppo_vooc_get_fastchg_ing() == true && oppo_vooc_get_fast_chg_type() != CHARGER_SUBTYPE_FASTCHG_VOOC)
			return;
        } else {
		if (oppo_vooc_get_fastchg_ing() == true) {
			return;
        	}
        }
        if (oppo_vooc_get_allow_reading() == false) {
                is_batt_full = 0;
                fastchg_present_wait_count = 0;
        } else {
                if (((oppo_vooc_get_fastchg_to_normal()== true) || (oppo_vooc_get_fastchg_to_warm() == true))
                        && (fastchg_present_wait_count <= FULL_DELAY_COUNTS)) {
                        is_batt_full = 0;
                        fastchg_present_wait_count++;
                        if (fastchg_present_wait_count == FULL_DELAY_COUNTS && chip->chg_ops->get_charging_enable() == false
                                && chip->charging_state != CHARGING_STATUS_FULL && chip->charging_state != CHARGING_STATUS_FAIL) {
							if (chip->ffc_support && chip->ffc_temp_status != FFC_TEMP_STATUS__HIGH
									&& chip->ffc_temp_status != FFC_TEMP_STATUS__LOW) {
								if (chip->vbatt_num == 2) {
									oppo_chg_turn_on_ffc2(chip);
								} else {
									oppo_chg_turn_on_ffc1(chip);
								}
							} else {
								oppo_chg_turn_on_charging(chip);
							}
                        }
                } else {
                        is_batt_full = chip->chg_ops->read_full();
                        fastchg_present_wait_count = 0;
                }
        }

		if (oppo_chg_check_ffc_status(chip) == false)
				return ;

        if ((is_batt_full == 1) || (chip->charging_state == CHARGING_STATUS_FULL) || oppo_chg_check_vbatt_is_full_by_sw(chip)) {
                oppo_chg_full_action(chip);
                if (chip->tbatt_status == BATTERY_STATUS__LITTLE_COLD_TEMP || chip->tbatt_status == BATTERY_STATUS__COOL_TEMP
                                || chip->tbatt_status == BATTERY_STATUS__LITTLE_COOL_TEMP || chip->tbatt_status == BATTERY_STATUS__NORMAL) {
                        oppo_gauge_set_batt_full(true);
                }

				if(chip->vbatt_num == 2) {
					chip->dod0_counts ++;
					if(chip->dod0_counts == DOD0_COUNTS)
					{
						oppo_gauge_update_battery_dod0();
						chip->dod0_counts = DOD0_COUNTS + 1;

						//chip->in_rechging = true;
						//oppo_chg_voter_charging_start(chip, CHG_STOP_VOTER__FULL);/*now rechging!*/
						charger_xlog_printk(CHG_LOG_CRTI, "oppo_chg_check_status_full,dod0_counts = %d\n",chip->dod0_counts);
					}
				}

        } else if (chip->charging_state == CHARGING_STATUS_FAIL) {
                oppo_chg_fail_action(chip);
        } else {
                chip->charging_state = CHARGING_STATUS_CCCV;
        }

}

static void oppo_chg_kpoc_power_off_check(struct oppo_chg_chip *chip)
{
#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
        if (chip->boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT || chip->boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {       /*vbus < 2.5V*/
                if ((chip->chg_ops->check_chrdet_status() == false) && (chip->charger_volt < 2500)) {
                        if ((oppo_vooc_get_fastchg_to_normal() == false) && (oppo_vooc_get_fastchg_to_warm() == false)
                                && (oppo_vooc_get_adapter_update_status() != ADAPTER_FW_NEED_UPDATE)
                                && (oppo_vooc_get_btb_temp_over() == false)) {
                                charger_xlog_printk(CHG_LOG_CRTI, "[pmic_thread_kthread]Unplug Charger/USB In Kernel Power Off Charging Mode Shutdown OS!\n");
                                chip->chg_ops->set_power_off();
                        }
                }
        }
#endif
}

static void oppo_chg_print_log(struct oppo_chg_chip *chip)
{
/* wenbin.liu@SW.Bsp.Driver, 2016/02/29  Add for log tag*/
        charger_xlog_printk(CHG_LOG_CRTI, " CHGR[ %d / %d / %d / %d / %d ], BAT[ %d / %d / %d / %d / %d / %d ], GAUGE[ %d / %d / %d / %d / %d / %d / %d / %d ], "
                "STATUS[ 0x%x / %d / %d / %d / %d / 0x%x ], OTHER[ %d / %d / %d / %d / %d/ %d ][%d / %d / %d]\n",
        chip->charger_exist, chip->charger_type, chip->charger_volt, chip->prop_status, chip->boot_mode,
        chip->batt_exist, chip->batt_full, chip->chging_on, chip->in_rechging, chip->charging_state, chip->total_time,
        chip->temperature, chip->batt_volt, chip->batt_volt_min, chip->icharging, chip->soc, chip->ui_soc, chip->soc_load, chip->batt_rm,
        chip->vbatt_over, chip->chging_over_time, chip->vchg_status, chip->tbatt_status, chip->stop_voter, chip->notify_code,
        chip->otg_switch, chip->mmi_chg, chip->boot_reason, chip->boot_mode, chip->chargerid_volt, chip->chargerid_volt_got,chip->ap_temp, chip->usb_temp_r, chip->usb_temp_l);


#ifdef CONFIG_OPPO_EMMC_LOG
/*Jingchun.Wang@BSP.Kernel.Debug, 2016/12/21,*/
/*add for emmc log*/
        kernel_emmclog_print(" CHGR[ %d / %d / %d / %d / %d ], BAT[ %d / %d / %d / %d / %d / %d ], GAUGE[ %d / %d / %d / %d / %d / %d / %d / %d ], "
                "STATUS[ 0x%x / %d / %d / %d / %d / 0x%x ], OTHER[ %d / %d / %d / %d / %d/ %d ][%d]\n",
        chip->charger_exist, chip->charger_type, chip->charger_volt, chip->prop_status, chip->boot_mode,
        chip->batt_exist, chip->batt_full, chip->chging_on, chip->in_rechging, chip->charging_state, chip->total_time,
        chip->temperature, chip->batt_volt, chip->batt_volt_min, chip->icharging, chip->soc, chip->ui_soc, chip->soc_load, chip->batt_rm,
        chip->vbatt_over, chip->chging_over_time, chip->vchg_status, chip->tbatt_status, chip->stop_voter, chip->notify_code,
        chip->otg_switch, chip->mmi_chg, chip->boot_reason, chip->boot_mode, chip->chargerid_volt, chip->chargerid_volt_got,chip->ap_temp);
#endif /*CONFIG_OPPO_EMMC_LOG*/

#ifdef CONFIG_OPPO_CHARGER_MTK
        if (chip->charger_type == POWER_SUPPLY_TYPE_USB_DCP)
                oppo_vooc_print_log();
#endif
}
#define CHARGER_ABNORMAL_DETECT_TIME 24
static void oppo_chg_critical_log(struct oppo_chg_chip *chip)
{
        static int chg_abnormal_count = 0;

        if (chip->charger_exist) {
                if (chip->stop_voter == 0 && chip->charger_type == POWER_SUPPLY_TYPE_USB_DCP
                        && chip->soc <= 75 && chip->icharging >= -20) {
                        chg_abnormal_count++;
                        if (chg_abnormal_count >= CHARGER_ABNORMAL_DETECT_TIME) {
                                chg_abnormal_count = CHARGER_ABNORMAL_DETECT_TIME;
                                charger_abnormal_log = CRITICAL_LOG_UNABLE_CHARGING;
                        }
                        charger_xlog_printk(CHG_LOG_CRTI, " unable charging, count=%d\n", chg_abnormal_count);
                } else {
                                chg_abnormal_count = 0;
                }
                if ((chip->stop_voter & CHG_STOP_VOTER__BATTTEMP_ABNORMAL) == CHG_STOP_VOTER__BATTTEMP_ABNORMAL) {
                        charger_abnormal_log = CRITICAL_LOG_BATTTEMP_ABNORMAL;
                } else if ((chip->stop_voter & CHG_STOP_VOTER__VCHG_ABNORMAL) == CHG_STOP_VOTER__VCHG_ABNORMAL) {
                        charger_abnormal_log = CRITICAL_LOG_VCHG_ABNORMAL;
                } else if ((chip->stop_voter & CHG_STOP_VOTER__VBAT_TOO_HIGH) == CHG_STOP_VOTER__VBAT_TOO_HIGH) {
                        charger_abnormal_log = CRITICAL_LOG_VBAT_TOO_HIGH;
                } else if ((chip->stop_voter & CHG_STOP_VOTER__MAX_CHGING_TIME) == CHG_STOP_VOTER__MAX_CHGING_TIME) {
                        charger_abnormal_log = CRITICAL_LOG_CHARGING_OVER_TIME;
                } else {
                        /*do nothing*/
                }
        } else {
                charger_abnormal_log = 0;
        }
}

static void oppo_chg_other_thing(struct oppo_chg_chip *chip)
{
		if (oppo_vooc_get_fastchg_started() == false) {
                chip->chg_ops->kick_wdt();
                chip->chg_ops->dump_registers();
#ifdef ODM_WT_EDIT
				//Junbo.Guo@ODM_WT.BSP.CHG, 2019/11/11, Modify for subcharger
				if(chip->is_double_charger_support){
					chip->sub_chg_ops->kick_wdt();
                	chip->sub_chg_ops->dump_registers();
				}
#endif
        }
        if (chip->charger_exist) {
                chip->total_time += OPPO_CHG_UPDATE_INTERVAL_SEC;
        }
        oppo_chg_print_log(chip);
        oppo_chg_critical_log(chip);
}

#define IBATT_COUNT        10
static void oppo_chg_ibatt_check_and_set(struct oppo_chg_chip *chip)
{
        static int average_current = 0;
        static int ibatt_count = 0;
        static int current_adapt = 0;
        static int pre_tbatt_status = BATTERY_STATUS__INVALID;
        static int fail_count = 0;
        bool set_current_flag = false;
        int recharge_volt = 0;
        int current_limit = 0;
        int current_init = 0;
        int threshold = 0;
        int current_step = 0;

        if ((chip->chg_ops->need_to_check_ibatt && chip->chg_ops->need_to_check_ibatt() == false)
                        || !chip->chg_ops->need_to_check_ibatt) {
                return;
        }

        if (!chip->charger_exist || (oppo_vooc_get_fastchg_started() == true)) {
                current_adapt = 0;
                ibatt_count = 0;
                average_current = 0;
                fail_count = 0;
                return;
        }

        if (oppo_short_c_batt_is_prohibit_chg(chip))
                return;

        if (current_adapt == 0) {
                if (chip->tbatt_status == BATTERY_STATUS__LITTLE_COLD_TEMP) {
                        current_adapt = chip->limits.temp_little_cold_fastchg_current_ma;
                        pre_tbatt_status = BATTERY_STATUS__LITTLE_COLD_TEMP;
                } else if (chip->tbatt_status == BATTERY_STATUS__WARM_TEMP) {
                        current_adapt = chip->limits.temp_warm_fastchg_current_ma;
                        pre_tbatt_status = BATTERY_STATUS__WARM_TEMP;
                } else if (chip->tbatt_status == BATTERY_STATUS__COLD_TEMP) {
                        current_adapt = chip->limits.temp_cold_fastchg_current_ma;
                        pre_tbatt_status = BATTERY_STATUS__COLD_TEMP;
                } else if (chip->tbatt_status == BATTERY_STATUS__NORMAL) {
                        current_adapt = chip->limits.temp_normal_fastchg_current_ma;
                        pre_tbatt_status = BATTERY_STATUS__NORMAL;
                } else if (chip->tbatt_status == BATTERY_STATUS__COOL_TEMP) {
                		//if ( chip->batt_volt > (chip->vbatt_num * 4180)) {
                		if ( chip->batt_volt > 4180) {
                                current_adapt = chip->limits.temp_cool_fastchg_current_ma_low;
                        } else {
                                current_adapt = chip->limits.temp_cool_fastchg_current_ma_high;
                        }
                        pre_tbatt_status = BATTERY_STATUS__COOL_TEMP;
                } else if (chip->tbatt_status == BATTERY_STATUS__LITTLE_COOL_TEMP) {
                        current_adapt = chip->limits.temp_little_cool_fastchg_current_ma;
                        pre_tbatt_status = BATTERY_STATUS__LITTLE_COOL_TEMP;
                }
        }

        if (chip->tbatt_status != pre_tbatt_status) {
                current_adapt = 0;
                ibatt_count = 0;
                average_current = 0;
                fail_count = 0;
                return;
        }

        if (chip->tbatt_status == BATTERY_STATUS__LITTLE_COLD_TEMP) {
                recharge_volt = chip->limits.temp_little_cold_vfloat_mv - chip->limits.recharge_mv;
                current_init = chip->limits.temp_little_cold_fastchg_current_ma;
                current_limit = chip->batt_capacity_mah * 15 / 100;
                threshold = 50;
        } else if (chip->tbatt_status == BATTERY_STATUS__WARM_TEMP) {
                recharge_volt = chip->limits.temp_warm_vfloat_mv - chip->limits.recharge_mv;
                current_init = chip->limits.temp_warm_fastchg_current_ma;
                current_limit = chip->batt_capacity_mah * 25 / 100;
                threshold = 50;
        } else if (chip->tbatt_status == BATTERY_STATUS__COLD_TEMP) {
                recharge_volt = chip->limits.temp_cold_vfloat_mv - chip->limits.recharge_mv;
                current_init = chip->limits.temp_cold_fastchg_current_ma;
                current_limit = 350;
                threshold = 50;
        } else if (chip->tbatt_status == BATTERY_STATUS__NORMAL) {
                recharge_volt = chip->limits.temp_normal_vfloat_mv- chip->limits.recharge_mv;
                current_init = chip->limits.temp_normal_fastchg_current_ma;
                if (chip->vooc_project) {
                        current_limit = 2000;
                } else {
                        current_limit = chip->batt_capacity_mah * 65 / 100;
                }
                threshold = 70;
        } else if (chip->tbatt_status == BATTERY_STATUS__COOL_TEMP) {
                recharge_volt = chip->limits.temp_cool_vfloat_mv - chip->limits.recharge_mv;
                if (vbatt_higherthan_4180mv) {
                                current_init = chip->limits.temp_cool_fastchg_current_ma_low;
                                current_limit = chip->batt_capacity_mah * 15 / 100;
                } else {
                                current_init = chip->limits.temp_cool_fastchg_current_ma_high;
                                current_limit = chip->batt_capacity_mah * 25 / 100;
                }
                threshold = 50;
        } else if (chip->tbatt_status == BATTERY_STATUS__LITTLE_COOL_TEMP) {
                recharge_volt = chip->limits.temp_little_cool_vfloat_mv - chip->limits.recharge_mv;
                current_init = chip->limits.temp_little_cool_fastchg_current_ma;
                current_limit = chip->batt_capacity_mah * 45 / 100;
                threshold = 70;
        }

        if (chip->batt_volt > recharge_volt || chip->led_on) {
                ibatt_count = 0;
                average_current = 0;
                fail_count = 0;
                return;
        }

        if (oppo_vooc_get_allow_reading() == true) {
                current_step = chip->chg_ops->get_chg_current_step();
        } else {
                current_adapt = 0;
                ibatt_count = 0;
                average_current = 0;
                fail_count = 0;
                return;
        }

        if (chip->icharging < 0) {
                ibatt_count++;
                average_current = average_current + chip->icharging;
        }

        /*charge current larger than limit*/
        if ((-1 * chip->icharging) > current_limit) {
                if (current_adapt > current_init) {
                        current_adapt = current_init;
                } else {
                        current_adapt -= 2 * current_step;
                }
                set_current_flag = true;
                fail_count++;
        } else if (ibatt_count == IBATT_COUNT) {
                average_current = -1 * average_current / ibatt_count;
                threshold += fail_count * current_step;
                if (average_current < current_limit - threshold) {
                        current_adapt += current_step;
                        set_current_flag = true;
                } else {
                        ibatt_count = 0;
                        average_current = 0;
                }
        }

        if (set_current_flag == true) {
                if (current_adapt > (current_limit + 100)) {
                        current_adapt = current_limit + 100;
                } else if (current_adapt < 103) {/*(512*20%)*/
                        current_adapt = 103;
                }
                charger_xlog_printk(CHG_LOG_CRTI, "charging_current_write_fast[%d] step[%d]\n", current_adapt, current_step);
                chip->chg_ops->charging_current_write_fast(current_adapt);
                ibatt_count = 0;
                average_current = 0;
        }
}

static void oppo_chg_pd_config(struct oppo_chg_chip *chip)
{

	//static int pd_chging = false;
	int ret = 0;

	if (chip->charger_type != POWER_SUPPLY_TYPE_USB_DCP) {
		chip->pd_chging = false;
		return;
	}

	if (!chip->chg_ops->oppo_chg_pd_setup || !chip->chg_ops->oppo_chg_get_pd_type)
		return;

	if (chip->pd_chging == false && chip->chg_ops->oppo_chg_get_pd_type() == true) {
		chip->pd_chging = true;
		ret = chip->chg_ops->oppo_chg_pd_setup();
		if (ret >= 0) {
			chip->limits.temp_little_cool_fastchg_current_ma = chip->limits.pd_temp_little_cool_fastchg_current_ma;
			chip->limits.temp_normal_fastchg_current_ma = chip->limits.pd_temp_normal_fastchg_current_ma;
			chip->limits.temp_little_cold_fastchg_current_ma_high = chip->limits.pd_temp_little_cold_fastchg_current_ma_high;
			chip->limits.temp_little_cold_fastchg_current_ma_low = chip->limits.pd_temp_little_cold_fastchg_current_ma_low;
			chip->limits.temp_cool_fastchg_current_ma_high = chip->limits.pd_temp_cool_fastchg_current_ma_high;
			chip->limits.temp_cool_fastchg_current_ma_low = chip->limits.pd_temp_cool_fastchg_current_ma_low;
			chip->limits.temp_warm_fastchg_current_ma = chip->limits.pd_temp_warm_fastchg_current_ma;

			chip->limits.input_current_charger_ma = chip->limits.pd_input_current_charger_ma;
			oppo_chg_set_charging_current(chip);
			oppo_chg_set_input_current_limit(chip);
		}
	} 
}
static void oppo_chg_pdqc_to_normal(struct oppo_chg_chip *chip)
{
	int ret = 0;
	static int pdqc_9v = false;

	if (!chip->chg_ops->get_charger_subtype)
		return;

	if (!chip->chg_ops->oppo_chg_pd_setup || !chip->chg_ops->set_qc_config)
		return;

	if (chip->limits.vbatt_pdqc_to_5v_thr < 0)
		return;

	if (chip->charger_volt > 7500) {
		pdqc_9v = true;
	} else {
		pdqc_9v = false;
	}
	if (chip->chg_ops->get_charger_subtype() == CHARGER_SUBTYPE_PD) {
		if (chip->batt_volt > chip->limits.vbatt_pdqc_to_5v_thr && pdqc_9v == true&&chip->ui_soc>=85&&chip->icharging > -1000) {
			ret = chip->chg_ops->oppo_chg_pd_setup();
			if (ret >= 0) {
				pdqc_9v = false;
				chip->limits.temp_normal_fastchg_current_ma = chip->limits.default_temp_normal_fastchg_current_ma;
				chip->limits.temp_little_cool_fastchg_current_ma = chip->limits.default_temp_little_cool_fastchg_current_ma;
				chip->limits.temp_little_cold_fastchg_current_ma_high = chip->limits.default_temp_little_cold_fastchg_current_ma_high;
				chip->limits.temp_little_cold_fastchg_current_ma_low = chip->limits.default_temp_little_cold_fastchg_current_ma_low;
				chip->limits.temp_cool_fastchg_current_ma_high = chip->limits.default_temp_cool_fastchg_current_ma_high;
				chip->limits.temp_cool_fastchg_current_ma_low = chip->limits.default_temp_cool_fastchg_current_ma_low;
				chip->limits.temp_warm_fastchg_current_ma = chip->limits.default_temp_warm_fastchg_current_ma;

				chip->limits.input_current_charger_ma = chip->limits.default_input_current_charger_ma;
				oppo_chg_set_charging_current(chip);
				oppo_chg_set_input_current_limit(chip);
			}
		}
	} else if (chip->chg_ops->get_charger_subtype() == CHARGER_SUBTYPE_QC) {
		if (chip->batt_volt > chip->limits.vbatt_pdqc_to_5v_thr && pdqc_9v == true&&chip->ui_soc>=85&&chip->icharging > -1000) {
			ret = chip->chg_ops->set_qc_config();
			if (ret >= 0) {
				pdqc_9v = false;
				chip->limits.temp_normal_fastchg_current_ma = chip->limits.default_temp_normal_fastchg_current_ma;
				chip->limits.temp_little_cool_fastchg_current_ma = chip->limits.default_temp_little_cool_fastchg_current_ma;
				chip->limits.temp_little_cold_fastchg_current_ma_high = chip->limits.default_temp_little_cold_fastchg_current_ma_high;
				chip->limits.temp_little_cold_fastchg_current_ma_low = chip->limits.default_temp_little_cold_fastchg_current_ma_low;
				chip->limits.temp_cool_fastchg_current_ma_high = chip->limits.default_temp_cool_fastchg_current_ma_high;
				chip->limits.temp_cool_fastchg_current_ma_low = chip->limits.default_temp_cool_fastchg_current_ma_low;
				chip->limits.temp_warm_fastchg_current_ma = chip->limits.default_temp_warm_fastchg_current_ma;

				chip->limits.input_current_charger_ma = chip->limits.default_input_current_charger_ma;
				oppo_chg_set_charging_current(chip);
				oppo_chg_set_input_current_limit(chip);
			}
		} 
	}
}
static void oppo_chg_qc_config(struct oppo_chg_chip *chip)
{
	//static int qc_chging = false;
	int ret = 0;
    int charger_type;
	
    charger_type = chip->chg_ops->get_charger_type();
	if (charger_type != POWER_SUPPLY_TYPE_USB_DCP) {
		chip->qc_chging = false;
		return;
	}

	if (!chip->chg_ops->set_qc_config || !chip->chg_ops->get_charger_subtype)
		return;
	chg_err("charger_type[%d], subtype[%d]\n", charger_type, chip->chg_ops->get_charger_subtype());
	if (chip->qc_chging == false &&chip->chg_ops->get_charger_subtype() == CHARGER_SUBTYPE_QC) {
		chip->qc_chging = true;
		ret = chip->chg_ops->set_qc_config();
		if (ret >= 0) {
			chg_err("QC  config success");
			chip->limits.temp_little_cool_fastchg_current_ma = chip->limits.qc_temp_little_cool_fastchg_current_ma;
			chip->limits.temp_normal_fastchg_current_ma = chip->limits.qc_temp_normal_fastchg_current_ma;
			chip->limits.temp_little_cold_fastchg_current_ma_high = chip->limits.qc_temp_little_cold_fastchg_current_ma_high;
			chip->limits.temp_little_cold_fastchg_current_ma_low = chip->limits.qc_temp_little_cold_fastchg_current_ma_low;
			chip->limits.temp_cool_fastchg_current_ma_high = chip->limits.qc_temp_cool_fastchg_current_ma_high;
			chip->limits.temp_cool_fastchg_current_ma_low = chip->limits.qc_temp_cool_fastchg_current_ma_low;
			chip->limits.temp_warm_fastchg_current_ma = chip->limits.qc_temp_warm_fastchg_current_ma;

			chip->limits.input_current_charger_ma = chip->limits.qc_input_current_charger_ma;
			oppo_chg_set_input_current_limit(chip);
			oppo_chg_set_charging_current(chip);
		}
	} 	
}

#ifdef ODM_WT_EDIT
//Junbo.Guo@ODM_WT.BSP.CHG, 2019/11/11, Modify for pe20
void oppo_pe20_check(struct oppo_chg_chip *chip)
{
	static int pe20_check_count=0;
	static int plug_flag=0;
	struct timespec diff;
	int charger_type;
	
	if (!chip->chg_ops->check_chrdet_status || !chip->chg_ops->oppo_chg_get_pe20_type
		||!chip->chg_ops->oppo_chg_reset_pe20||!chip->chg_ops->get_charger_subtype)
		return;
		
	charger_type = chip->chg_ops->get_charger_type();
	//chg_err("charger_type[%d], subtype[%d],count[%d]\n", charger_type, chip->chg_ops->get_charger_subtype(),pe20_check_count);
	if (chip->chg_ops->check_chrdet_status()){
		if(charger_type==POWER_SUPPLY_TYPE_USB_DCP){
			if((pe20_check_count<1)&&(chip->chg_ops->get_charger_subtype()==CHARGER_SUBTYPE_DEFAULT)){
			
				get_monotonic_boottime(&chip->ptime[1]);
				diff = timespec_sub(chip->ptime[1], chip->ptime[0]);
				if(diff.tv_sec < 2){
					chg_err("oppo_pe20_check:timer:%ld:%ld\n",diff.tv_sec,diff.tv_nsec);
					return;
				}
				
				if(chip->chg_ops->oppo_chg_get_pe20_type()== false){
					pe20_check_count++;
				}else{
					plug_flag=1;
					chg_err("oppo_pe20_check:ok\n");
				}
				oppo_chg_set_charging_current(chip);
				oppo_chg_set_input_current_limit(chip);
			}
		}
	}else{
		pe20_check_count=0;
		
		if(plug_flag){
			plug_flag=0;
			chg_err("oppo_pe20_check:reset\n");
			chip->chg_ops->oppo_chg_reset_pe20();
		}
	}
}

static void oppo_chg_pe20_config(struct oppo_chg_chip *chip)
{
	int ret = 0;
	int charger_type;
	
	charger_type = chip->chg_ops->get_charger_type();
	if (charger_type != POWER_SUPPLY_TYPE_USB_DCP) {
		return;
	}
	
	if (!chip->chg_ops->check_chrdet_status || !chip->chg_ops->oppo_chg_get_pe20_type
		||!chip->chg_ops->oppo_chg_reset_pe20||!chip->chg_ops->get_charger_subtype)
		return;

	if (chip->chg_ops->get_charger_subtype() == CHARGER_SUBTYPE_PE20) {
		ret = chip->chg_ops->oppo_chg_pe20_setup();
		if (ret >= 0) {
			chip->limits.temp_normal_fastchg_current_ma = chip->limits.pe20_temp_normal_fastchg_current_ma;
			chip->limits.temp_little_cool_fastchg_current_ma = chip->limits.pe20_temp_little_cool_fastchg_current_ma;
			chip->limits.temp_little_cold_fastchg_current_ma_high = chip->limits.pe20_temp_little_cold_fastchg_current_ma_high;
			chip->limits.temp_little_cold_fastchg_current_ma_low = chip->limits.pe20_temp_little_cold_fastchg_current_ma_low;
			chip->limits.temp_cool_fastchg_current_ma_high = chip->limits.pe20_temp_cool_fastchg_current_ma_high;
			chip->limits.temp_cool_fastchg_current_ma_low = chip->limits.pe20_temp_cool_fastchg_current_ma_low;
			chip->limits.temp_warm_fastchg_current_ma = chip->limits.pe20_temp_warm_fastchg_current_ma;
			
			chip->limits.input_current_charger_ma = chip->limits.pe20_input_current_charger_ma;
			oppo_chg_set_charging_current(chip);
			oppo_chg_set_input_current_limit(chip);
		}
	} 
}

static void oppo_chg_pe20_to_normal(struct oppo_chg_chip *chip)
{
	int ret = 0;
	
	if (!chip->chg_ops->get_charger_subtype)
		return;

	if (!chip->chg_ops->check_chrdet_status || !chip->chg_ops->oppo_chg_get_pe20_type
		||!chip->chg_ops->oppo_chg_reset_pe20||!chip->chg_ops->get_charger_subtype)
		return;

	if ((chip->batt_volt <= chip->limits.vbatt_pdqc_to_5v_thr)&&(chip->ui_soc <= 85)&&(chip->icharging < -1000))
		return;
	
	if (chip->chg_ops->get_charger_subtype() == CHARGER_SUBTYPE_PE20) {
				chip->limits.temp_normal_fastchg_current_ma = chip->limits.default_temp_normal_fastchg_current_ma;
				chip->limits.temp_little_cool_fastchg_current_ma = chip->limits.default_temp_little_cool_fastchg_current_ma;
				chip->limits.temp_little_cold_fastchg_current_ma_high = chip->limits.default_temp_little_cold_fastchg_current_ma_high;
				chip->limits.temp_little_cold_fastchg_current_ma_low = chip->limits.default_temp_little_cold_fastchg_current_ma_low;
				chip->limits.temp_cool_fastchg_current_ma_high = chip->limits.default_temp_cool_fastchg_current_ma_high;
				chip->limits.temp_cool_fastchg_current_ma_low = chip->limits.default_temp_cool_fastchg_current_ma_low;
				chip->limits.temp_warm_fastchg_current_ma = chip->limits.default_temp_warm_fastchg_current_ma;

				chip->limits.input_current_charger_ma = chip->limits.default_input_current_charger_ma;
				oppo_chg_set_charging_current(chip);
				oppo_chg_set_input_current_limit(chip);
	} 
}

static int __ref FAST_charger_thread_main(void *data)
{
        struct oppo_chg_chip *chip = data;
        struct cred *new;
        int rc = 0;

        new = prepare_creds();
		if (!new) {
                chg_err("init err\n");
				rc = -1;
				return rc;
        }
        new->fsuid = new->euid = KUIDT_INIT(1000);
        commit_creds(new);

        while (!kthread_should_stop()) {
                set_current_state(TASK_RUNNING);
				if (chip->charger_exist) {
					oppo_chg_aicl_check(chip);
					oppo_chg_protection_check(chip);
					oppo_chg_check_status_full(chip);
					oppo_chg_battery_notify_check(chip);
					oppo_chg_get_chargerid_voltage(chip);
					oppo_chg_fast_switch_check(chip);
					oppo_chg_chargerid_switch_check(chip);
                }
				oppo_pe20_check(chip);
				oppo_chg_pd_config(chip);
				oppo_chg_qc_config(chip);
				oppo_chg_pe20_config(chip);
				oppo_chg_pdqc_to_normal(chip);
				oppo_chg_pe20_to_normal(chip);
				oppo_chg_ibatt_check_and_set(chip);
                set_current_state(TASK_UNINTERRUPTIBLE);
                schedule();
        }
        return rc;
}

#endif

static void oppo_chg_update_work(struct work_struct *work)
{
        struct delayed_work *dwork = to_delayed_work(work);
        struct oppo_chg_chip *chip = container_of(dwork, struct oppo_chg_chip, update_work);

        oppo_charger_detect_check(chip);
        oppo_chg_get_battery_data(chip);

#ifndef ODM_WT_EDIT
//Junbo.Guo@ODM_WT.BSP.CHG, 2019/11/11, Modify for pe20	
        if (chip->charger_exist) {
                oppo_chg_aicl_check(chip);
                oppo_chg_protection_check(chip);
                oppo_chg_check_status_full(chip);
				oppo_chg_battery_notify_check(chip);
                oppo_chg_get_chargerid_voltage(chip);
                oppo_chg_fast_switch_check(chip);
                oppo_chg_chargerid_switch_check(chip);
        }
		

        oppo_chg_pd_config(chip);
        oppo_chg_qc_config(chip);
        oppo_chg_pdqc_to_normal(chip);
		
        oppo_chg_ibatt_check_and_set(chip);
#endif
        /* oppo_chg_short_c_battery_check(chip); */
        wake_up_process(chip->shortc_thread);
#ifdef ODM_WT_EDIT
//Junbo.Guo@ODM_WT.BSP.CHG, 2019/11/11, Modify for pe20		
		wake_up_process(chip->FAST_charger_thread);
#endif		
        oppo_chg_battery_update_status(chip);

        oppo_chg_kpoc_power_off_check(chip);

        oppo_chg_other_thing(chip);

        /* run again after interval */
        schedule_delayed_work(&chip->update_work, OPPO_CHG_UPDATE_INTERVAL);
}

bool oppo_chg_wake_update_work(void)
{
        int shedule_work = 0;

        if (!g_charger_chip) {
                chg_err(" g_charger_chip NULL,return\n");
                return true;
        }
        shedule_work = mod_delayed_work(system_wq, &g_charger_chip->update_work, 0);

        return true;
}

void oppo_chg_kick_wdt(void)
{
        if (!g_charger_chip) {
                return;
        }
        if (oppo_vooc_get_allow_reading() == true) {
                g_charger_chip->chg_ops->kick_wdt();
        }
}
void oppo_chg_disable_charge(void)
{
        if (!g_charger_chip) {
                return;
        }
        if (oppo_vooc_get_allow_reading() == true) {
                g_charger_chip->chg_ops->charging_disable();
        }
}

void oppo_chg_unsuspend_charger(void)
{
        if (!g_charger_chip) {
                return;
        }
        if (oppo_vooc_get_allow_reading() == true) {
                g_charger_chip->chg_ops->charger_unsuspend();
        }
}

int oppo_chg_get_batt_volt(void)
{
        if (!g_charger_chip) {
                return 4000;
        } else {
                return g_charger_chip->batt_volt;
        }
}

int oppo_chg_get_ui_soc(void)
{
        if (!g_charger_chip) {
                return 50;
        } else {
                return g_charger_chip->ui_soc;
        }
}

int oppo_chg_get_soc(void)
{
        if (!g_charger_chip) {
                return 50;
        } else {
                return g_charger_chip->soc;
        }
}

void oppo_chg_soc_update_when_resume(unsigned long sleep_tm_sec)
{
        if (!g_charger_chip) {
                return;
        }
        g_charger_chip->sleep_tm_sec = sleep_tm_sec;
        g_charger_chip->soc = oppo_gauge_get_batt_soc();
        oppo_chg_update_ui_soc(g_charger_chip);
}

void oppo_chg_soc_update(void)
{
        if (!g_charger_chip) {
                return;
        }
        oppo_chg_update_ui_soc(g_charger_chip);
}

int oppo_chg_get_chg_type(void)
{
        if (!g_charger_chip) {
                return POWER_SUPPLY_TYPE_UNKNOWN;
        } else {
                return g_charger_chip->charger_type;
        }
}

int oppo_chg_get_chg_temperature(void)
{
        if (!g_charger_chip) {
                return 250;
        } else {
                return g_charger_chip->temperature;
        }
}

int oppo_chg_get_notify_flag(void)
{
        if (!g_charger_chip) {
                return 0;
        } else {
                return g_charger_chip->notify_flag;
        }
}
int oppo_is_vooc_project(void)
{
		if (!g_charger_chip) {
			return 0;
		} else {
			return g_charger_chip->vooc_project;
		}
}
int oppo_chg_show_vooc_logo_ornot(void)
{
        if (!g_charger_chip) {
                return 0;
        }
	 if (g_charger_chip->chg_ctrl_by_vooc) {
		if (oppo_vooc_get_fastchg_started() == true && oppo_vooc_get_fast_chg_type() == CHARGER_SUBTYPE_FASTCHG_VOOC) {
			chg_err("show_vooc_logo_ornot by vooc\n");
			if (g_charger_chip->prop_status != POWER_SUPPLY_STATUS_FULL &&
                        	(g_charger_chip->stop_voter == CHG_STOP_VOTER__FULL || g_charger_chip->stop_voter == CHG_STOP_VOTER_NONE)) {
                        	//chg_err(":1\n");
                      	return 1;
                	} else {
                		//chg_err(":0\n");
                      	return 0;
                	}
		}	
	 }
	 
        if (oppo_vooc_get_fastchg_started()) {
                return 1;
        } else if (oppo_vooc_get_fastchg_to_normal() == true
                || oppo_vooc_get_fastchg_to_warm() == true
                || oppo_vooc_get_fastchg_dummy_started() == true
                || oppo_vooc_get_adapter_update_status() == ADAPTER_FW_NEED_UPDATE) {
                if (g_charger_chip->prop_status != POWER_SUPPLY_STATUS_FULL &&
                        (g_charger_chip->stop_voter == CHG_STOP_VOTER__FULL || g_charger_chip->stop_voter == CHG_STOP_VOTER_NONE)) {
                        return 1;
                } else {
                        return 0;
                }
        } else {
                return 0;
        }
}

bool get_otg_switch(void)
{
        if (!g_charger_chip) {
                return 0;
        } else {
                return g_charger_chip->otg_switch;
        }
}

bool oppo_chg_get_otg_online(void)
{
        if (!g_charger_chip) {
                return 0;
        } else {
                return g_charger_chip->otg_online;
        }
}

void oppo_chg_set_otg_online(bool online)
{
        if (g_charger_chip) {
                g_charger_chip->otg_online = online;
				chg_debug("set otg online:%d",online);
        }
}

bool oppo_chg_get_batt_full(void)
{
        if (!g_charger_chip) {
                return 0;
        } else {
                return g_charger_chip->batt_full;
        }
}

bool oppo_chg_get_rechging_status(void)
{
        if (!g_charger_chip) {
                return 0;
        } else {
                return g_charger_chip->in_rechging;
        }
}


bool oppo_chg_check_chip_is_null(void)
{
        if (!g_charger_chip) {
                return true;
        } else {
                return false;
        }
}

void oppo_chg_set_charger_type_unknown(void)
{
        if (g_charger_chip) {
                g_charger_chip->charger_type = POWER_SUPPLY_TYPE_UNKNOWN;
        }
}

int oppo_chg_get_charger_voltage(void)
{
        if (!g_charger_chip) {
                return -500;
        } else {
                return g_charger_chip->chg_ops->get_charger_volt();
        }
}

int oppo_chg_get_charger_exist(void)
{
        if (!g_charger_chip) {
                return -1;
        } else {
                return g_charger_chip->charger_exist;
        }
}


void oppo_chg_set_chargerid_switch_val(int value)
{
        if (g_charger_chip && g_charger_chip->chg_ops->set_chargerid_switch_val) {
                g_charger_chip->chg_ops->set_chargerid_switch_val(value);
        }
}

void oppo_chg_clear_chargerid_info(void)
{
        if (g_charger_chip && g_charger_chip->chg_ops->set_chargerid_switch_val) {
                g_charger_chip->chargerid_volt = 0;
                g_charger_chip->chargerid_volt_got = false;
        }
}

int oppo_is_rf_ftm_mode(void)
{
	int boot_mode = get_boot_mode();
#ifdef CONFIG_OPPO_CHARGER_MTK
	if (boot_mode == META_BOOT || boot_mode == FACTORY_BOOT
		|| boot_mode == ADVMETA_BOOT || boot_mode == ATE_FACTORY_BOOT){
		chg_debug(" boot_mode:%d, return\n",boot_mode);
		return true;
	} else {
		chg_debug(" boot_mode:%d, return false\n",boot_mode);
		return false;
	}
#else
	if(boot_mode == MSM_BOOT_MODE__RF || boot_mode == MSM_BOOT_MODE__WLAN || boot_mode == MSM_BOOT_MODE__FACTORY){
		chg_debug(" boot_mode:%d, return\n",boot_mode);
		return true;
	} else {
		chg_debug(" boot_mode:%d, return false\n",boot_mode);
		return false;
	}
#endif
}

#ifdef CONFIG_OPPO_CHARGER_MTK
/*
int get_oppo_short_check_fast_to_normal(void)
{
    if (!g_charger_chip) {
            return 0;
    } else {
            return g_charger_chip->short_check_fast_to_normal_flag;
    }

}
*/
int oppo_get_prop_status(void)
{
        if (!g_charger_chip) {
                return 0;
        } else {
                return g_charger_chip->prop_status;
        }
}

#endif

#define OPPO_TBATT_HIGH_PWROFF_COUNT            (18)
#define OPPO_TBATT_EMERGENCY_PWROFF_COUNT        (6)
DECLARE_WAIT_QUEUE_HEAD(oppo_tbatt_pwroff_wq);
static int oppo_tbatt_power_off_kthread(void *arg)
{
        int over_temp_count = 0, emergency_count = 0;
        int batt_temp = 0;
        //struct oppo_chg_chip *chip = (struct oppo_chg_chip *)arg;

        struct sched_param param = {.sched_priority = MAX_RT_PRIO-1};
        sched_setscheduler(current, SCHED_FIFO, &param);
        tbatt_pwroff_enable = 1;

        while (!kthread_should_stop()) {
                schedule_timeout_interruptible(round_jiffies_relative(msecs_to_jiffies(5*1000)));
                //chg_err(" tbatt_pwroff_enable:%d over_temp_count[%d] start\n", tbatt_pwroff_enable, over_temp_count);
                if (!tbatt_pwroff_enable) {
                        emergency_count = 0;
                        over_temp_count = 0;
                        wait_event_interruptible(oppo_tbatt_pwroff_wq, tbatt_pwroff_enable == 1);
                }

                if (oppo_vooc_get_fastchg_started() == true) {
                        batt_temp = oppo_gauge_get_prev_batt_temperature();
                } else {
                        batt_temp = oppo_gauge_get_batt_temperature();
                }

                if (batt_temp > OPCHG_PWROFF_EMERGENCY_BATT_TEMP) {
                        emergency_count++;
                        chg_err(" emergency_count:%d \n", emergency_count);
                } else {
                        emergency_count = 0;
                }
                if (batt_temp > OPCHG_PWROFF_HIGH_BATT_TEMP) {
                        over_temp_count++;
                        chg_err("over_temp_count[%d] \n", over_temp_count);
                } else {
                        over_temp_count = 0;
                }
                //chg_err("over_temp_count[%d], chip->temperature[%d]\n", over_temp_count, batt_temp);

                if (over_temp_count >= OPPO_TBATT_HIGH_PWROFF_COUNT
                    || emergency_count >= OPPO_TBATT_EMERGENCY_PWROFF_COUNT) {
                        chg_err("ERROR: battery temperature is too high, goto power off\n");
                        ///msleep(1000);
                        machine_power_off();
                }
        }
        return 0;
}

int oppo_tbatt_power_off_task_init(struct oppo_chg_chip *chip)
{
        if (!chip) {
                return -1;
        }
		chip->tbatt_pwroff_task = kthread_create(oppo_tbatt_power_off_kthread, chip, "tbatt_pwroff");
		if(chip->tbatt_pwroff_task){
			    wake_up_process(chip->tbatt_pwroff_task);
		}else{
                chg_err("ERROR: chip->tbatt_pwroff_task creat fail\n");
                return -1;
		}
        return 0;
}

void oppo_tbatt_power_off_task_wakeup(void)
{

        wake_up_interruptible(&oppo_tbatt_pwroff_wq);
        return;
}

