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

#ifndef __MTK_BOOT_COMMON_H__
#define __MTK_BOOT_COMMON_H__

/* boot type definitions */
enum boot_mode_t {
	NORMAL_BOOT = 0,
	META_BOOT = 1,
	RECOVERY_BOOT = 2,
	SW_REBOOT = 3,
	FACTORY_BOOT = 4,
	ADVMETA_BOOT = 5,
	ATE_FACTORY_BOOT = 6,
	ALARM_BOOT = 7,
	KERNEL_POWER_OFF_CHARGING_BOOT = 8,
	LOW_POWER_OFF_CHARGING_BOOT = 9,
	DONGLE_BOOT = 10,
#ifdef VENDOR_EDIT
/* Bin.Li@EXP.BSP.bootloader.bootflow, 2017/05/24, Add for oppo boot mode */
	OPPO_SAU_BOOT = 11,
	SILENCE_BOOT = 12,
/* xiaofan.yang@PSW.TECH.AgingTest, 2019/09/09,Add for factory agingtest */
	AGING_BOOT = 998,
	SAFE_BOOT = 999,
#endif /* VENDOR_EDIT */
	UNKNOWN_BOOT
};


#ifdef ODM_WT_EDIT
/* Hui.Yuan@ODM_WT.system.Kernel.Boot, 2019/10/17, Add for oppo_boot_mode */

#ifdef VENDOR_EDIT
/* Bin.Li@EXP.BSP.bootloader.bootflow, 2017/05/24, Add for oppo boot mode */

typedef enum
{
	OPPO_NORMAL_BOOT = 0,
	OPPO_SILENCE_BOOT = 1,
	OPPO_SAFE_BOOT = 2,
	/* xiaofan.yang@PSW.TECH.AgingTest, 2019/09/09,Add for factory agingtest */
	OPPO_AGING_BOOT = 3,
	OPPO_UNKNOWN_BOOT
}OPPO_BOOTMODE;
extern OPPO_BOOTMODE oppo_boot_mode;
#endif /* VENDOR_EDIT */
#endif /* ODM_WT_EDIT */

/* for boot type usage */
#define BOOTDEV_NAND            (0)
#define BOOTDEV_SDMMC           (1)
#define BOOTDEV_UFS             (2)

#define BOOT_DEV_NAME           "BOOT"
#define BOOT_SYSFS              "boot"
#define BOOT_MODE_SYSFS_ATTR    "boot_mode"
#define BOOT_TYPE_SYSFS_ATTR    "boot_type"

extern enum boot_mode_t get_boot_mode(void);
extern unsigned int get_boot_type(void);
extern bool is_meta_mode(void);
extern bool is_advanced_meta_mode(void);
extern void set_boot_mode(unsigned int bm);

#endif
