/**********************************************************************************
* Copyright (c)  2008-2015  Guangdong OPPO Mobile Comm Corp., Ltd
* VENDOR_EDIT
* Description: Charger IC management module for charger system framework.
*                  Manage all charger IC and define abstarct function flow.
* Version   : 1.0
* Date      : 2015-06-22
* Author    : fanhui@PhoneSW.BSP
*           : Fanhong.Kong@ProDrv.CHG
* ------------------------------ Revision History: --------------------------------
* <version>         <date>              <author>                      <desc>
* Revision 1.0    2015-06-22      fanhui@PhoneSW.BSP          Created for new architecture
* Revision 1.0    2015-06-22      Fanhong.Kong@ProDrv.CHG     Created for new architecture
***********************************************************************************/

#ifndef _OPPO_CHARGER_H_
#define _OPPO_CHARGER_H_

#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/version.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0))
#include <linux/wakelock.h>
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#elif CONFIG_FB
#include <linux/notifier.h>
#include <linux/fb.h>
#ifdef CONFIG_DRM_MSM
#include <linux/msm_drm_notify.h>
#endif
#endif

#ifdef CONFIG_OPPO_CHARGER_MTK
#include <linux/i2c.h>
//#include <mt-plat/battery_meter.h>
#include <mt-plat/mtk_boot.h>
#ifdef CONFIG_OPPO_CHARGER_MTK6779
#include "charger_ic/oppo_battery_mtk6779.h"
#endif
#else /* CONFIG_OPPO_CHARGER_MTK */
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0))
#include <linux/qpnp/qpnp-adc.h>
#include <linux/msm_bcl.h>
#endif
#include <soc/oppo/boot_mode.h>
#ifdef CONFIG_OPPO_MSM8953N_CHARGER
#include "charger_ic/oppo_battery_msm8953_N.h"
#elif defined CONFIG_OPPO_MSM8953_CHARGER
#include "charger_ic/oppo_battery_msm8953.h"
#elif defined CONFIG_OPPO_MSM8998_CHARGER
#include "charger_ic/oppo_battery_msm8998.h"
#elif defined CONFIG_OPPO_MSM8998O_CHARGER
#include "charger_ic/oppo_battery_msm8998_O.h"
#elif defined CONFIG_OPPO_SDM845_CHARGER
#include "charger_ic/oppo_battery_sdm845.h"
#elif defined CONFIG_OPPO_SDM670_CHARGER
#include "charger_ic/oppo_battery_sdm670.h"
#elif defined CONFIG_OPPO_SDM670P_CHARGER
#include "charger_ic/oppo_battery_sdm670P.h"
#elif defined CONFIG_OPPO_SM8150_CHARGER
#include "charger_ic/oppo_battery_msm8150.h"
#elif defined CONFIG_OPPO_SM8150_PRO_CHARGER
#include "charger_ic/oppo_battery_msm8150_pro.h"
#elif defined CONFIG_OPPO_SM6125_CHARGER
#include "charger_ic/oppo_battery_sm6125P.h"
#elif defined CONFIG_OPPO_SM7150_CHARGER
#include "charger_ic/oppo_battery_sm7150_P.h"
#elif defined CONFIG_OPPO_SDM670Q_CHARGER
#include "charger_ic/oppo_battery_sdm670Q.h"
#else /* CONFIG_OPPO_MSM8953_CHARGER */
#include "charger_ic/oppo_battery_msm8976.h"
#endif /* CONFIG_OPPO_MSM8953_CHARGER */
#endif /* CONFIG_OPPO_CHARGER_MTK */

#define CHG_LOG_CRTI 1
#define CHG_LOG_FULL 2

#define OPCHG_PWROFF_HIGH_BATT_TEMP		770
#define OPCHG_PWROFF_EMERGENCY_BATT_TEMP	850

#define OPCHG_INPUT_CURRENT_LIMIT_CHARGER_MA	2000
#define OPCHG_INPUT_CURRENT_LIMIT_USB_MA	500
#define OPCHG_INPUT_CURRENT_LIMIT_CDP_MA	1500
#define OPCHG_INPUT_CURRENT_LIMIT_LED_MA	1200
#define OPCHG_INPUT_CURRENT_LIMIT_CAMERA_MA	1000
#define OPCHG_INPUT_CURRENT_LIMIT_CALLING_MA	1200
#ifdef ODM_WT_EDIT
//Junbo.Guo@ODM_WT.BSP.CHG, 2019/11/11, Modify for QC
#define OPCHG_FAST_CHG_MAX_MA			3000
#else
#define OPCHG_FAST_CHG_MAX_MA			2000
#endif

#define FEATURE_PRINT_CHGR_LOG
#define FEATURE_PRINT_BAT_LOG
#define FEATURE_PRINT_GAUGE_LOG
#define FEATURE_PRINT_STATUS_LOG
/*#define FEATURE_PRINT_OTHER_LOG*/
#define FEATURE_PRINT_VOTE_LOG
#define FEATURE_PRINT_ICHGING_LOG
#define FEATURE_VBAT_PROTECT

#define NOTIFY_CHARGER_OVER_VOL			1
#define NOTIFY_CHARGER_LOW_VOL			2
#define NOTIFY_BAT_OVER_TEMP			3
#define NOTIFY_BAT_LOW_TEMP			4
#define NOTIFY_BAT_NOT_CONNECT			5
#define NOTIFY_BAT_OVER_VOL			6
#define NOTIFY_BAT_FULL				7
#define NOTIFY_CHGING_CURRENT			8
#define NOTIFY_CHGING_OVERTIME			9
#define NOTIFY_BAT_FULL_PRE_HIGH_TEMP		10
#define NOTIFY_BAT_FULL_PRE_LOW_TEMP		11
#define NOTIFY_BAT_FULL_THIRD_BATTERY		14
#define NOTIFY_SHORT_C_BAT_CV_ERR_CODE1		15
#define NOTIFY_SHORT_C_BAT_FULL_ERR_CODE2	16
#define NOTIFY_SHORT_C_BAT_FULL_ERR_CODE3	17
#define NOTIFY_SHORT_C_BAT_DYNAMIC_ERR_CODE4	18
#define NOTIFY_SHORT_C_BAT_DYNAMIC_ERR_CODE5	19
#define	NOTIFY_CHARGER_TERMINAL			20

#define chg_debug(fmt, ...) \
        printk(KERN_NOTICE "[OPPO_CHG][%s]"fmt, __func__, ##__VA_ARGS__)

#define chg_err(fmt, ...) \
        printk(KERN_ERR "[OPPO_CHG][%s]"fmt, __func__, ##__VA_ARGS__)

typedef enum {
        CHG_NONE = 0,
        CHG_DISABLE,
        CHG_SUSPEND,
}OPPO_CHG_DISABLE_STATUS;

typedef enum {
        CHG_STOP_VOTER_NONE                                =        0,
        CHG_STOP_VOTER__BATTTEMP_ABNORMAL                  =        (1 << 0),
        CHG_STOP_VOTER__VCHG_ABNORMAL                      =        (1 << 1),
        CHG_STOP_VOTER__VBAT_TOO_HIGH                      =        (1 << 2),
        CHG_STOP_VOTER__MAX_CHGING_TIME                    =        (1 << 3),
        CHG_STOP_VOTER__FULL                               =        (1 << 4),
}OPPO_CHG_STOP_VOTER;

typedef enum {
        CHARGER_STATUS__GOOD,
        CHARGER_STATUS__VOL_HIGH,
        CHARGER_STATUS__VOL_LOW,
        CHARGER_STATUS__INVALID
}OPPO_CHG_VCHG_STATUS;

typedef enum {
        BATTERY_STATUS__NORMAL = 0,                      /*16C~44C*/
        BATTERY_STATUS__REMOVED,                         /*<-20C*/
        BATTERY_STATUS__LOW_TEMP,                        /*<-2C*/
        BATTERY_STATUS__HIGH_TEMP,                       /*>53C*/
        BATTERY_STATUS__COLD_TEMP,                       /*-2C~0C*/
        BATTERY_STATUS__LITTLE_COLD_TEMP,                /*0C~5C*/
        BATTERY_STATUS__COOL_TEMP,                       /*5C~12C*/
        BATTERY_STATUS__LITTLE_COOL_TEMP,                /*12C~16C*/
        BATTERY_STATUS__WARM_TEMP,                       /*44C~53C*/
        BATTERY_STATUS__INVALID
}
OPPO_CHG_TBATT_STATUS;

typedef enum {
        LED_TEMP_STATUS__NORMAL = 0,                      /*<=35c*/
        LED_TEMP_STATUS__WARM,                         	  /*>35 && <=37C*/
        LED_TEMP_STATUS__HIGH,                            /*>37C*/
}
OPPO_CHG_TLED_STATUS;

typedef enum {
        FFC_TEMP_STATUS__NORMAL = 0,                      /*<=35c*/
        FFC_TEMP_STATUS__WARM,                         	  /*>35 && <=40C*/
        FFC_TEMP_STATUS__HIGH,                            /*>40C*/
        FFC_TEMP_STATUS__LOW,					/*<16C*/
}
OPPO_CHG_FFC_TEMP_STATUS;

typedef enum {
        CRITICAL_LOG_NORMAL = 0,
        CRITICAL_LOG_UNABLE_CHARGING,
        CRITICAL_LOG_BATTTEMP_ABNORMAL,
        CRITICAL_LOG_VCHG_ABNORMAL,
        CRITICAL_LOG_VBAT_TOO_HIGH,
        CRITICAL_LOG_CHARGING_OVER_TIME,
        CRITICAL_LOG_VOOC_WATCHDOG,
        CRITICAL_LOG_VOOC_BAD_CONNECTED,
        CRITICAL_LOG_VOOC_BTB
}OPPO_CHG_CRITICAL_LOG;

typedef enum {
        CHARGING_STATUS_CCCV                =        0X01,
        CHARGING_STATUS_FULL                =        0X02,
        CHARGING_STATUS_FAIL                =        0X03,
}OPPO_CHG_CHARGING_STATUS;

typedef enum {
        CHARGER_SUBTYPE_DEFAULT = 0,
        CHARGER_SUBTYPE_FASTCHG_VOOC,
        CHARGER_SUBTYPE_FASTCHG_SVOOC,
        CHARGER_SUBTYPE_PD,
        CHARGER_SUBTYPE_QC,
//#ifdef ODM_WT_EDIT
//Junbo.Guo@ODM_WT.BSP.CHG, 2019/11/11, Modify for pe20
        CHARGER_SUBTYPE_PE20,
//#endif
}OPPO_CHARGER_SUBTYPE;

#ifdef ODM_WT_EDIT
//Sidong.Zhao@ODM_WT.BSP.CHG, 2019/11/27, ap thermal machine
typedef enum {
        AP_TEMP_BELOW_T0 = 0,
        AP_TEMP_T0_TO_T1,
        AP_TEMP_T1_TO_T2,
        AP_TEMP_T2_TO_T3,
        AP_TEMP_ABOVE_T3
}OPPO_CHG_AP_TEMP_STAT;

typedef enum {
        BATT_TEMP_EXTEND_BELOW_T0 = 0,
        BATT_TEMP_EXTEND_T0_TO_T1,
        BATT_TEMP_EXTEND_T1_TO_T2,
        BATT_TEMP_EXTEND_T2_TO_T3,
        BATT_TEMP_EXTEND_T3_TO_T4,
        BATT_TEMP_EXTEND_T4_TO_T5,
        BATT_TEMP_EXTEND_ABOVE_T5
}OPPO_CHG_BATT_TEMP_EXTEND_STAT;
#endif /*ODM_WT_EDIT*/

typedef enum {
        VOOC_TEMP_STATUS__NORMAL = 0,                      /*<=34c*/
        VOOC_TEMP_STATUS__WARM,                         	  /*>34 && <=38C*/
        VOOC_TEMP_STATUS__HIGH,                            /*>38 && <=45C*/
}OPPO_CHG_TBAT_VOOC_STATUS;

struct tbatt_anti_shake{
        int cold_bound;
        int little_cold_bound;
        int cool_bound;
        int little_cool_bound;
        int normal_bound;
        int warm_bound;
        int hot_bound;
        int overtemp_bound;
};

struct oppo_chg_limits {
        int input_current_cdp_ma;
        int input_current_charger_ma;
        int pd_input_current_charger_ma;
        int qc_input_current_charger_ma;
        int input_current_usb_ma;
        int input_current_camera_ma;
        int input_current_calling_ma;
        int input_current_led_ma;

        int vooc_high_bat_decidegc;                /*>=45C*/
        int input_current_vooc_ma_high;
        int vooc_warm_bat_decidegc;                /*38C*/
        int input_current_vooc_ma_warm;
        int vooc_normal_bat_decidegc;                /*34C*/
        int input_current_vooc_ma_normal;          /*<34c*/
        int charger_current_vooc_ma_normal;
        int iterm_ma;
        bool iterm_disabled;
        int recharge_mv;

        int removed_bat_decidegc; /*-19C*/

        int cold_bat_decidegc;  /*-3C*/
        int temp_cold_vfloat_mv;
        int temp_cold_fastchg_current_ma;

        int little_cold_bat_decidegc;  /*0C*/
        int temp_little_cold_vfloat_mv;
        int temp_little_cold_fastchg_current_ma;
        int temp_little_cold_fastchg_current_ma_high;
        int temp_little_cold_fastchg_current_ma_low;
        int pd_temp_little_cold_fastchg_current_ma_high;
        int pd_temp_little_cold_fastchg_current_ma_low;
        int qc_temp_little_cold_fastchg_current_ma_high;
        int qc_temp_little_cold_fastchg_current_ma_low;

        int cool_bat_decidegc;        /*5C*/
        int temp_cool_vfloat_mv;
        int temp_cool_fastchg_current_ma_high;
        int temp_cool_fastchg_current_ma_low;
        int pd_temp_cool_fastchg_current_ma_high;
        int pd_temp_cool_fastchg_current_ma_low;
        int qc_temp_cool_fastchg_current_ma_high;
        int qc_temp_cool_fastchg_current_ma_low;

        int little_cool_bat_decidegc;        /*12C*/
        int temp_little_cool_vfloat_mv;
        int temp_little_cool_fastchg_current_ma;
        int pd_temp_little_cool_fastchg_current_ma;
        int qc_temp_little_cool_fastchg_current_ma;

        int normal_bat_decidegc;        /*16C*/
        int temp_normal_fastchg_current_ma;
        int pd_temp_normal_fastchg_current_ma;
        int qc_temp_normal_fastchg_current_ma;
        int temp_normal_vfloat_mv;

        int warm_bat_decidegc;                /*45C*/
        int temp_warm_vfloat_mv;
        int temp_warm_fastchg_current_ma;
        int temp_warm_fastchg_current_ma_led_on;
        int pd_temp_warm_fastchg_current_ma;
        int qc_temp_warm_fastchg_current_ma;

        int hot_bat_decidegc;                /*53C*/
        int non_standard_vfloat_mv;
        int non_standard_fastchg_current_ma;
        int max_chg_time_sec;
        int charger_hv_thr;
        int charger_recv_thr;
        int charger_lv_thr;
        int vbatt_full_thr;
        int vbatt_hv_thr;
#ifdef ODM_WT_EDIT
/*Sidong.Zhao@ODM_WT.BSP.CHG 2019/11/11,add charge parameter*/
		int temp_cold_usbchg_current_ma;/*-2-0*/
		int temp_little_cold_usbchg_current_ma_high;/*0-5*/
		int temp_little_cold_usbchg_current_ma_low;
		int temp_little_cold_usbchg_current_ma;
		int temp_cool_usbchg_current_ma_high;/*5-12*/
		int temp_cool_usbchg_current_ma_low;
		int temp_little_cool_usbchg_current_ma;/*12-16*/
		int temp_normal_usbchg_current_ma;/*16-44*/
		int temp_warm_usbchg_current_ma;/*44-53*/
#endif

#ifdef ODM_WT_EDIT
		//Junbo.Guo@ODM_WT.BSP.CHG, 2019/11/11, Modify for pe20
		int pe20_temp_normal_fastchg_current_ma;
		int pe20_temp_little_cool_fastchg_current_ma;
		int pe20_temp_little_cold_fastchg_current_ma_high;
		int pe20_temp_little_cold_fastchg_current_ma_low;
		int pe20_temp_cool_fastchg_current_ma_high;
		int pe20_temp_cool_fastchg_current_ma_low;
		int pe20_temp_warm_fastchg_current_ma;
		int pe20_input_current_charger_ma;
#endif

#ifdef ODM_WT_EDIT
/*Sidong.Zhao@ODM_WT.BSP.CHG 2019/11/27,AP thermal machine*/
	int ap_temp_t0;
	int ap_temp_t0_minus_x;
	int ap_temp_t1;
	int ap_temp_t1_minus_x;
	int ap_temp_t2;
	int ap_temp_t2_minus_x;
	int ap_temp_t3;
	int ap_temp_t3_minus_x;

	int ap_temp_below_t0_ichg;
	int ap_temp_t0_to_t1_ichg;
	int ap_temp_t1_to_t2_ichg;
	int ap_temp_t2_to_t3_ichg;
	int ap_temp_above_t3_ichg;

	int	batt_temp_extend_t0_to_t1_ichg;
	int	batt_temp_extend_t1_to_t2_ichg_part1;
	int	batt_temp_extend_t1_to_t2_ichg_part2;
	int	batt_temp_extend_t2_to_t3_ichg;
	int	batt_temp_extend_t3_to_t4_ichg;
	int	batt_temp_extend_t4_to_t5_ichg;
	int	batt_temp_extend_t4_to_t5_ichg2;
	int	batt_temp_extend_t4_to_t5_ichg3;

	int charge_current_calling_ma;
#endif /*ODM_WT_EDIT*/
        int vfloat_step_mv;  
        int vfloat_sw_set;
        int vfloat_over_counts;

        int non_standard_vfloat_sw_limit;		/*sw full*/
        int cold_vfloat_sw_limit;
        int little_cold_vfloat_sw_limit;
        int cool_vfloat_sw_limit;
        int little_cool_vfloat_sw_limit;
        int normal_vfloat_sw_limit;
        int warm_vfloat_sw_limit;

        int led_high_bat_decidegc;                /*>=37C*/
        int led_high_bat_decidegc_antishake;
        int input_current_led_ma_high;		
        int led_warm_bat_decidegc;                /*35C*/
        int led_warm_bat_decidegc_antishake;
        int input_current_led_ma_warm;		
        int input_current_led_ma_normal;          /*<35c*/



        int short_c_bat_vfloat_mv;
        int short_c_bat_fastchg_current_ma;
        int short_c_bat_vfloat_sw_limit;

        bool sw_vfloat_over_protect_enable;		/*vfloat over check*/
        int non_standard_vfloat_over_sw_limit;
        int cold_vfloat_over_sw_limit;
        int little_cold_vfloat_over_sw_limit;
        int cool_vfloat_over_sw_limit;
        int little_cool_vfloat_over_sw_limit;
        int normal_vfloat_over_sw_limit;
        int warm_vfloat_over_sw_limit;
        int normal_vterm_hw_inc;
        int non_normal_vterm_hw_inc;

        int vbatt_pdqc_to_5v_thr;
		
        int ff1_normal_fastchg_ma;
        int ff1_warm_fastchg_ma;
        int ff1_exit_step_ma;					  /*<=35C,700ma*/
        int ff1_warm_exit_step_ma;
        int ffc2_temp_low_decidegc;		/*<16C*/
        int ffc2_temp_high_decidegc;                /*>=40C*/
        int ffc2_warm_fastchg_ma;					/*35~40C,750ma	*/
        int ffc2_temp_warm_decidegc;                /*35C*/
        int ffc2_normal_fastchg_ma;					  /*<=35C,700ma*/
        int ffc2_exit_step_ma;					  /*<=35C,700ma*/
        int ffc2_warm_exit_step_ma;

        int ffc1_normal_vfloat_sw_limit;				//4.45V
        int ffc2_normal_vfloat_sw_limit;
        int ffc_temp_normal_vfloat_mv;				//4.5v
        int ffc_normal_vfloat_over_sw_limit;		//4.5V
        int default_iterm_ma;						/*16~45 default value*/
        int default_temp_normal_fastchg_current_ma;
        int default_normal_vfloat_sw_limit;
        int default_temp_normal_vfloat_mv;
        int default_normal_vfloat_over_sw_limit;

        int default_temp_little_cool_fastchg_current_ma;  //12 ~ 16
        int default_little_cool_vfloat_sw_limit;
        int default_temp_little_cool_vfloat_mv;
        int default_little_cool_vfloat_over_sw_limit;

        int default_temp_little_cold_fastchg_current_ma_high;  //0 ~ 5
        int default_temp_little_cold_fastchg_current_ma_low;
        int default_temp_cool_fastchg_current_ma_high;  // 5 ~ 12
        int default_temp_cool_fastchg_current_ma_low;
        int default_temp_warm_fastchg_current_ma; //44 ~ 53

        int default_input_current_charger_ma;
};

struct battery_data{
        int  BAT_STATUS;
        int  BAT_HEALTH;
        int  BAT_PRESENT;
        int  BAT_TECHNOLOGY;
        int  BAT_CAPACITY;
    /* Add for Battery Service*/
        int  BAT_batt_vol;
        int  BAT_batt_temp;
	
    /* Add for EM */
        int  BAT_TemperatureR;
        int  BAT_TempBattVoltage;
        int  BAT_InstatVolt;
        int  BAT_BatteryAverageCurrent;
        int  BAT_BatterySenseVoltage;
        int  BAT_ISenseVoltage;
        int  BAT_ChargerVoltage;
        int  battery_request_poweroff;//low battery in sleep
        int  fastcharger;
        int  charge_technology;
    /* Dual battery */
        int  BAT_MMI_CHG;//for MMI_CHG_TEST
        int  BAT_FCC;
        int  BAT_SOH;
        int  BAT_CC;
};

struct normalchg_gpio_pinctrl {
	int			chargerid_switch_gpio;
	int			usbid_gpio;
	int			usbid_irq;
	int			ship_gpio;
	int			shortc_gpio;
	int			dischg_gpio;
	struct pinctrl		*pinctrl;
	struct pinctrl_state	*chargerid_switch_active;
	struct pinctrl_state	*chargerid_switch_sleep;
	struct pinctrl_state	*chargerid_switch_default;
	struct pinctrl_state	*usbid_active;
	struct pinctrl_state	*usbid_sleep;
	struct pinctrl_state	*ship_active;
	struct pinctrl_state	*ship_sleep;
	struct pinctrl_state	*shortc_active;
	struct pinctrl_state	*charger_gpio_as_output1;
	struct pinctrl_state	*charger_gpio_as_output2;
	struct pinctrl_state	*dischg_enable;
	struct pinctrl_state	*dischg_disable;
	struct pinctrl_state	*usb_temp_adc;
	struct pinctrl_state	*usb_temp_adc_suspend;
	struct pinctrl_state	*uart_bias_disable;
	struct pinctrl_state	*uart_pull_down;
	struct pinctrl_state	*chargerid_adc_default;
};


struct short_c_batt_data {
        int short_c_bat_cv_mv;

        int batt_chging_cycle_threshold;
        int batt_chging_cycles;

        int cv_timer1;
        int full_timer2;
        int full_timer3;
        int full_timer4;
        int full_timer5;

        int cool_temp_rbatt;
        int little_cool_temp_rbatt;
        int normal_temp_rbatt;

        int full_delta_vbatt1_mv;
        int full_delta_vbatt2_mv;

        int ex2_lower_ibatt_ma;
        int ex2_low_ibatt_ma;
        int ex2_high_ibatt_ma;
        int ex2_lower_ibatt_count;
        int ex2_low_ibatt_count;
        int ex2_high_ibatt_count;

        int dyna1_low_avg_dv_mv;
        int dyna1_high_avg_dv_mv;
        int dyna1_delta_dv_mv;
        int dyna2_low_avg_dv_mv;
        int dyna2_high_avg_dv_mv;
        int dyna2_delta_dv_mv;

        int is_recheck_on;
        int is_switch_on;
        int is_feature_sw_on;
        int is_feature_hw_on;

		u8 ic_volt_threshold;
		bool ic_short_otp_st;

        int err_code;
        int update_change;
        bool in_idle;
        bool cv_satus;
        bool disable_rechg;
        bool limit_chg;
        bool limit_rechg;

        bool shortc_gpio_status;
};

struct oppo_chg_chip {
        struct i2c_client        *client;
        struct device           *dev;
        const struct oppo_chg_operations *chg_ops;
#ifdef ODM_WT_EDIT
//Junbo.Guo@ODM_WT.BSP.CHG, 2019/11/11, Modify for subcharger 
		const struct oppo_chg_operations *sub_chg_ops;
		bool  is_double_charger_support;
#endif

        struct power_supply        *ac_psy;

    	struct power_supply_desc 	ac_psd;
		struct power_supply_config 	ac_cfg;
    	struct power_supply_desc 	usb_psd;
		struct power_supply_config 	usb_cfg;
    	struct power_supply_desc 	battery_psd;
        

		struct power_supply			*usb_psy;

#ifndef CONFIG_OPPO_CHARGER_MTK
        struct qcom_pmic			pmic_spmi;
#endif
#ifdef CONFIG_OPPO_CHARGER_MTK_CHGIC
        struct mtk_pmic				chgic_mtk;
#endif

        struct power_supply        *batt_psy;
/*        struct battery_data battery_main        */
        struct delayed_work        update_work;
        struct delayed_work        mmi_adapter_in_work;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0))
        struct wake_lock           suspend_lock;
#else
        struct wakeup_source	   *suspend_ws;
#endif

        struct oppo_chg_limits    limits;
        struct tbatt_anti_shake   anti_shake_bound;
        struct short_c_batt_data  short_c_batt;

        bool                charger_exist;
        int                 charger_type;
        int                 charger_volt;
        int                 charger_volt_pre;

	int	sw_full_count;

        int                 temperature;
        int                 offset_temp;
        int                 batt_volt;
		int					vbatt_num;
		int         		batt_volt_max;
		int         		batt_volt_min;
		int         		vbatt_power_off;
		int         		vbatt_soc_1;
		
        int                 icharging;
        int                 soc;
        int                 ui_soc;
        int                 smooth_soc;
        int                 soc_load;
        bool                authenticate;
        int                 batt_fcc;
        int                 batt_cc;
        int                 batt_soh;
        int                 batt_rm;
        int                 batt_capacity_mah;
        int                 tbatt_pre_shake;

        bool                batt_exist;
        bool                batt_full;
        bool                chging_on;
        bool                in_rechging;
        int                 charging_state;
        int                 total_time;
        unsigned long 		sleep_tm_sec;

        bool                vbatt_over;
        bool                chging_over_time;
        int                 vchg_status;
        int                 tbatt_status;
        int                 prop_status;
        int                 stop_voter;
        int                 notify_code;
        int                 notify_flag;
#ifdef ODM_WT_EDIT
/*Sidong.Zhao@ODM_WT.BSP.CHG 2019/11/14,add for thermal detection*/
        int                ap_temp;
#endif /*ODM_WT_EDIT*/

	bool                led_on;
	bool                led_on_change;
	bool                led_temp_change;
	int                 led_temp_status;

	bool 				vooc_temp_change;
	int					vooc_temp_status;
        bool                camera_on;
        bool                calling_on;

        bool                ac_online;
#ifdef         CONFIG_OPPO_CHARGER_MTK
        bool                usb_online;
#endif
        bool                otg_online;
        bool                otg_switch;
        bool                ui_otg_switch;	
        int                 mmi_chg;
        int                 unwakelock_chg;
		int					stop_chg;
        int                 mmi_fastchg;
        int                 boot_reason;
        int                 boot_mode;
        int                 vooc_project;
        bool                suspend_after_full;
        bool                check_batt_full_by_sw;
        bool                external_gauge;
        bool                chg_ctrl_by_lcd;
        bool                chg_ctrl_by_lcd_default;
        bool                chg_ctrl_by_camera;
		bool				bq25890h_flag;
        bool                chg_ctrl_by_calling;
        bool                chg_ctrl_by_vooc;
        bool                chg_ctrl_by_vooc_default;
        bool                fg_bcl_poll;
#ifdef CONFIG_FB
        struct notifier_block chg_fb_notify;
#endif
        struct normalchg_gpio_pinctrl        normalchg_gpio;
        int                 chargerid_volt;
        bool                chargerid_volt_got;
        int                 enable_shipmode;
        int                 dod0_counts;
		bool				ffc_support;
		bool				fastchg_to_ffc;
		int					fastchg_ffc_status;
		int 				ffc_temp_status;
		bool                allow_swtich_to_fastchg;
        bool                platform_fg_flag;
        struct task_struct        *shortc_thread;
        int                 usbtemp_volt;
        int                 usb_temp;
	 int                 usbtemp_volt_l;
	 int			usbtemp_volt_r;
	 int			usb_temp_l;
	 int			usb_temp_r;
        struct task_struct *tbatt_pwroff_task;
#ifdef ODM_WT_EDIT
//Junbo.Guo@ODM_WT.BSP.CHG, 2019/11/11, Modify for pe20
	struct task_struct		  *FAST_charger_thread;
	struct timespec ptime[2];
	bool pd_chging;
	bool qc_chging;
#endif
#ifdef ODM_WT_EDIT
//Sidong.Zhao@ODM_WT.BSP.CHG, 2019/11/27, ap thermal machine
	OPPO_CHG_AP_TEMP_STAT ap_sm;
	OPPO_CHG_BATT_TEMP_EXTEND_STAT jeita_sm;
#endif /*ODM_WT_EDIT*/
};



struct oppo_chg_operations {
        void (*dump_registers)(void);
        int (*kick_wdt)(void);
        int (*hardware_init)(void);
        int (*charging_current_write_fast)(int cur);
        int (*input_current_ctrl_by_vooc_write)(int cur);
        void (*set_aicl_point)(int vbatt);
        int (*input_current_write)(int cur);
        int (*float_voltage_write)(int cur);
        int (*term_current_set)(int cur);
        int (*charging_enable)(void);
        int (*charging_disable)(void);
        int (*get_charging_enable)(void);
        int (*charger_suspend)(void);
        int (*charger_unsuspend)(void);
        int (*set_rechg_vol)(int vol);
        int (*reset_charger)(void);
        int (*read_full)(void);
        int (*otg_enable)(void);
        int (*otg_disable)(void);
        int (*set_charging_term_disable)(void);
        bool (*check_charger_resume)(void);

        int (*get_charger_type)(void);
        int (*get_charger_volt)(void);
        int (*get_charger_current)(void);
        int (*get_chargerid_volt)(void);
        void (*set_chargerid_switch_val)(int value);
        int (*get_chargerid_switch_val)(void);
        bool (*check_chrdet_status)(void);
        int (*get_boot_mode)(void);
        int (*get_boot_reason)(void);
        int (*get_instant_vbatt)(void);
        int (*get_rtc_soc)(void);
        int (*set_rtc_soc)(int val);
        void (*set_power_off)(void);
        void (*usb_connect)(void);
        void (*usb_disconnect)(void);
#ifndef CONFIG_OPPO_CHARGER_MTK
        int (*get_aicl_ma)(void);
        void(*rerun_aicl)(void);
        int (*tlim_en)(bool);
        int (*set_system_temp_level)(int);
        int(*otg_pulse_skip_disable)(enum skip_reason, bool);
        int(*set_dp_dm)(int);
        int(*calc_flash_current)(void);
#endif
        int (*get_chg_current_step)(void);
        bool (*need_to_check_ibatt)(void);
#ifdef CONFIG_OPPO_RTC_DET_SUPPORT
        int (*check_rtc_reset)(void);
#endif /* CONFIG_OPPO_RTC_DET_SUPPORT */
        int (*get_dyna_aicl_result) (void);
        bool (*get_shortc_hw_gpio_status)(void);
		void (*check_is_iindpm_mode) (void);
        bool (*oppo_chg_get_pd_type) (void);
        int (*oppo_chg_pd_setup) (void);
	int (*get_charger_subtype)(void);
	int (*set_qc_config)(void);
	int (*enable_qc_detect)(void);
#ifdef ODM_WT_EDIT
//Junbo.Guo@ODM_WT.BSP.CHG, 2019/11/11, Modify for pe20
	int (*oppo_chg_get_pe20_type)(void);
	int (*oppo_chg_pe20_setup)(void);
	int (*oppo_chg_reset_pe20)(void);
	int (*oppo_chg_set_high_vbus)(bool en);
	int (*oppo_chg_set_hz_mode)(bool en);
#endif
};


/*********************************************
 * power_supply usb/ac/battery functions
 **********************************************/


extern int oppo_usb_get_property(struct power_supply *psy,
        enum power_supply_property psp,
        union power_supply_propval *val);
extern int oppo_usb_property_is_writeable(struct power_supply *psy,
        enum power_supply_property psp);
extern int oppo_usb_set_property(struct power_supply *psy,
        enum power_supply_property psp,
        const union power_supply_propval *val);
extern int oppo_ac_get_property(struct power_supply *psy,
        enum power_supply_property psp,
        union power_supply_propval *val);
extern int oppo_battery_property_is_writeable(struct power_supply *psy,
        enum power_supply_property psp);
extern int oppo_battery_set_property(struct power_supply *psy,
        enum power_supply_property psp,
        const union power_supply_propval *val);
extern int oppo_battery_get_property(struct power_supply *psy,
        enum power_supply_property psp,
        union power_supply_propval *val);



/*********************************************
 * oppo_chg_init - initialize oppo_chg_chip
 * @chip: pointer to the oppo_chg_chip
 * @clinet: i2c client of the chip
 *
 * Returns: 0 - success; -1/errno - failed
 **********************************************/
int oppo_chg_parse_svooc_dt(struct oppo_chg_chip *chip);
int oppo_chg_parse_charger_dt(struct oppo_chg_chip *chip);

int oppo_chg_init(struct oppo_chg_chip *chip);
void oppo_charger_detect_check(struct oppo_chg_chip *chip);
int oppo_chg_get_prop_batt_health(struct oppo_chg_chip *chip);

bool oppo_chg_wake_update_work(void);
void oppo_chg_soc_update_when_resume(unsigned long sleep_tm_sec);
void oppo_chg_soc_update(void);
int oppo_chg_get_batt_volt(void);

int oppo_chg_get_ui_soc(void);
int oppo_chg_get_soc(void);
int oppo_chg_get_chg_temperature(void);

void oppo_chg_kick_wdt(void);
void oppo_chg_disable_charge(void);
void oppo_chg_unsuspend_charger(void);

int oppo_chg_get_chg_type(void);

int oppo_chg_get_notify_flag(void);
int oppo_is_vooc_project(void);

int oppo_chg_show_vooc_logo_ornot(void);

bool get_otg_switch(void);

#ifdef CONFIG_OPPO_CHARGER_MTK
bool oppo_chg_get_otg_online(void);
void oppo_chg_set_otg_online(bool online);
#endif

bool oppo_chg_get_batt_full(void);
bool oppo_chg_get_rechging_status(void);

bool oppo_chg_check_chip_is_null(void);
void oppo_chg_set_charger_type_unknown(void);
int oppo_chg_get_charger_voltage(void);
int oppo_chg_get_charger_exist(void);
void oppo_chg_set_chargerid_switch_val(int value);
void oppo_chg_turn_on_charging(struct oppo_chg_chip *chip);
#ifdef ODM_WT_EDIT
/*Sidong.Zhao@ODM_WT.BSP.CHG 2019/11/15,declear function*/
	void oppo_chg_turn_off_charging(struct oppo_chg_chip *chip);
#endif /*ODM_WT_EDIT*/
void oppo_chg_clear_chargerid_info(void);
#ifndef CONFIG_OPPO_CHARGER_MTK
void oppo_chg_variables_reset(struct oppo_chg_chip *chip, bool in);
void oppo_chg_external_power_changed(struct power_supply *psy);
#endif
int oppo_is_rf_ftm_mode(void);
//huangtongfeng@BSP.CHG.Basic, 2017/01/13, add for kpoc charging param.
int oppo_get_charger_chip_st(void);
void oppo_chg_set_allow_switch_to_fastchg(bool allow);
int oppo_tbatt_power_off_task_init(struct oppo_chg_chip *chip);
void oppo_tbatt_power_off_task_wakeup(void);
#endif /*_OPPO_CHARGER_H_*/
