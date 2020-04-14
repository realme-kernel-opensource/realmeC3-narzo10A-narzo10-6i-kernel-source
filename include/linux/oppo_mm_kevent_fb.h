/***************************************************************
** Copyright (C),  2018,  OPPO Mobile Comm Corp.,  Ltd
** VENDOR_EDIT
** File : oppo_mm_kevent_fb.h
** Description : MM kevent fb data
** Version : 1.0
** Date : 2018/12/03
** Author : Ling.Guo@PSW.MM.Display.Stability
**
** ------------------------------- Revision History: -----------
**  <author>        <data>        <version >        <desc>
**   Guo.Ling          2018/12/03        1.0           Build this moudle
******************************************************************/
#ifndef _OPPO_MM_KEVENT_FB_
#define _OPPO_MM_KEVENT_FB_

enum OPPO_MM_DIRVER_FB_EVENT_ID {
	OPPO_MM_DIRVER_FB_EVENT_ID_ESD = 401,
	OPPO_MM_DIRVER_FB_EVENT_ID_VSYNC,
	OPPO_MM_DIRVER_FB_EVENT_ID_HBM,
	OPPO_MM_DIRVER_FB_EVENT_ID_FFLSET,
	OPPO_MM_DIRVER_FB_EVENT_ID_MTK_CMDQ,
	OPPO_MM_DIRVER_FB_EVENT_ID_MTK_UNDERFLOW,
	OPPO_MM_DIRVER_FB_EVENT_ID_MTK_FENCE,
	OPPO_MM_DIRVER_FB_EVENT_ID_MTK_GPU_JS,
	OPPO_MM_DIRVER_FB_EVENT_ID_MTK_GPU_SOFT_RESET,
	OPPO_MM_DIRVER_FB_EVENT_ID_MTK_GPU_MMU_FAULT,
	OPPO_MM_DIRVER_FB_EVENT_ID_MTK_GPU_FAULT,
	OPPO_MM_DIRVER_FB_EVENT_ID_AUDIO = 801,
};

enum OPPO_MM_DIRVER_FB_EVENT_MODULE {
	OPPO_MM_DIRVER_FB_EVENT_MODULE_DISPLAY = 0,
	OPPO_MM_DIRVER_FB_EVENT_MODULE_AUDIO
};

enum OPPO_MM_DIRVER_FB_EVENT_REPORTLEVEL {
	OPPO_MM_DIRVER_FB_EVENT_REPORTLEVEL_LOW = 0,
	OPPO_MM_DIRVER_FB_EVENT_REPORTLEVEL_HIGH
};

int upload_mm_kevent_fb_data(enum OPPO_MM_DIRVER_FB_EVENT_MODULE module, unsigned char *payload);

#endif /* _OPPO_MM_KEVENT_FB_ */

