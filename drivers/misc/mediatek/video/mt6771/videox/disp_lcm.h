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

#ifndef _DISP_LCM_H_
#define _DISP_LCM_H_

#include "lcm_drv.h"

#define MAX_LCM_NUMBER	2

extern unsigned char lcm_name_list[][128];
extern struct LCM_DRIVER lcm_common_drv;

struct disp_lcm_handle {
	struct LCM_PARAMS *params;
	struct LCM_DRIVER *drv;
	enum LCM_INTERFACE_ID lcm_if_id;
	int module;
	int is_inited;
	unsigned int lcm_original_width;
	unsigned int lcm_original_height;
	int index;
};

/* these 2 variables are defined in mt65xx_lcm_list.c */
extern struct LCM_DRIVER *lcm_driver_list[];
extern unsigned int lcm_count;

struct disp_lcm_handle *disp_lcm_probe(char *plcm_name,
				       enum LCM_INTERFACE_ID lcm_id,
				       int is_lcm_inited);
struct disp_lcm_handle *disp_ext_lcm_probe(char *plcm_name,
					   enum LCM_INTERFACE_ID lcm_id,
					   int is_lcm_inited);
int disp_lcm_init(struct disp_lcm_handle *plcm, int force);
struct LCM_PARAMS *disp_lcm_get_params(struct disp_lcm_handle *plcm);
enum LCM_INTERFACE_ID disp_lcm_get_interface_id(struct disp_lcm_handle *plcm);
int disp_lcm_update(struct disp_lcm_handle *plcm, int x, int y,
		    int w, int h, int force);
int disp_lcm_esd_check(struct disp_lcm_handle *plcm);
int disp_lcm_esd_recover(struct disp_lcm_handle *plcm);
int disp_lcm_suspend(struct disp_lcm_handle *plcm);
int disp_lcm_resume(struct disp_lcm_handle *plcm);
#ifdef VENDOR_EDIT
/* YongPeng.Yi@PSW.MM.Display.LCD.Stability, 2018/12/10, add for Lcd tm td4330 */
int disp_lcm_init_code(struct disp_lcm_handle *plcm);
#endif /*VENDOR_EDIT*/
int disp_lcm_is_support_adjust_fps(struct disp_lcm_handle *plcm);
int disp_lcm_adjust_fps(void *cmdq, struct disp_lcm_handle *plcm, int fps);
int disp_lcm_set_backlight(struct disp_lcm_handle *plcm, void *handle,
			   int level);
int disp_lcm_read_fb(struct disp_lcm_handle *plcm);
int disp_lcm_ioctl(struct disp_lcm_handle *plcm, enum LCM_IOCTL ioctl,
		   unsigned int arg);
int disp_lcm_is_video_mode(struct disp_lcm_handle *plcm);
int disp_lcm_is_inited(struct disp_lcm_handle *plcm);
unsigned int disp_lcm_ATA(struct disp_lcm_handle *plcm);
void *disp_lcm_switch_mode(struct disp_lcm_handle *plcm, int mode);
int disp_lcm_set_lcm_cmd(struct disp_lcm_handle *plcm, void *cmdq_handle,
			 unsigned int *lcm_cmd, unsigned int *lcm_count,
			 unsigned int *lcm_value);

int disp_lcm_is_partial_support(struct disp_lcm_handle *plcm);
int disp_lcm_validate_roi(struct disp_lcm_handle *plcm, int *x, int *y,
			  int *w, int *h);
int disp_lcm_aod(struct disp_lcm_handle *plcm, int enter);

#ifdef VENDOR_EDIT
/*
* Ling.Guo@PSW.MM.Display.LCD.Stability, 2019/02/14,
* modify for support aod state.
*/
int disp_lcm_aod_from_display_on(struct disp_lcm_handle *plcm);
int disp_lcm_set_aod_mode(struct disp_lcm_handle *plcm, void *handle, unsigned int mode);
int mtk_disp_lcm_set_hbm(bool en, struct disp_lcm_handle *plcm, void *qhandle);
int disp_lcm_get_hbm_state(struct disp_lcm_handle *plcm);
int disp_lcm_get_hbm_wait(struct disp_lcm_handle *plcm);
int disp_lcm_set_hbm_wait(bool wait, struct disp_lcm_handle *plcm);
unsigned int disp_lcm_get_hbm_time(bool en, struct disp_lcm_handle *plcm);
/* Yongpeng.Yi@PSW.MultiMedia.Display.LCD.Machine, 2018/09/10, Add for Porting cabc interface */
int disp_lcm_oppo_set_lcm_cabc_cmd(struct disp_lcm_handle *plcm, void *handle, unsigned int level);
/* liping-m@PSW.MM.Display.LCD.Stability, 2018/07/21,
* add power seq api for ulps
*/
int disp_lcm_poweron_before_ulps(struct disp_lcm_handle *plcm);
int disp_lcm_poweroff_after_ulps(struct disp_lcm_handle *plcm);
/*
* Yongpeng.Yi@PSW.MM.Display.LCD.Stability, 2018/01/16,
* add for samsung lcd hbm node
*/
int disp_lcm_set_hbm(struct disp_lcm_handle *plcm, void *handle, unsigned int hbm_level);
/*
* Yongpeng.Yi@PSW.MM.Display.LCD.Feature, 2018/09/26,
* add for Aod feature
*/
int disp_lcm_aod_doze_resume(struct disp_lcm_handle *plcm);
#endif /* VENDOR_EDIT */
#endif /* _DISP_LCM_H_ */
