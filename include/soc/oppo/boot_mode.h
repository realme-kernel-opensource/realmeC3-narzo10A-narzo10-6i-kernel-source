/************************************************************************************
** File: - android\kernel\arch\arm\mach-msm\include\mach\oppo_boot.h
** VENDOR_EDIT
** Copyright (C), 2008-2012, OPPO Mobile Comm Corp., Ltd
** 
** Description:  
**     change define of boot_mode here for other place to use it
** Version: 1.0 
** --------------------------- Revision History: --------------------------------
** 	<author>	<data>			<desc>
** tong.han@BasicDrv.TP&LCD 11/01/2014 add this file
************************************************************************************/
#ifndef _OPPO_BOOT_H
#define _OPPO_BOOT_H
#include <soc/oppo/boot_mode_types.h>
//#ifdef VENDOR_EDIT
//Ke.Li@ROM.Security, KernelEvent, 2019-9-30, remove the "static" flag for kernel kevent
//extern static int get_boot_mode(void);
extern int get_boot_mode(void);
//#endif /* VENDOR_EDIT */
#ifdef VENDOR_EDIT
//Fuchun.Liao@Mobile.BSP.CHG 2016-01-14 add for charge
extern bool qpnp_is_power_off_charging(void);
#endif
#ifdef VENDOR_EDIT
//PengNan@SW.BSP add for detect charger when reboot 2016-04-22
extern bool qpnp_is_charger_reboot(void);
#endif /*VENDOR_EDIT*/
#endif
