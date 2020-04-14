/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef _CAM_CAL_DATA_H
#define _CAM_CAL_DATA_H

#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

#ifdef ODM_WT_EDIT
/* Zhen.Quan@Camera.Driver, 2019/10/17, add for [otp bringup] */
struct stCAM_CAL_DATAINFO_STRUCT {
	u32 sensorID;
	u32 deviceID; /* MAIN = 0x01, SUB  = 0x02, MAIN_2 = 0x04,SUB2 = 0x08, MAIN3 =0x10*/
	u32 dataLength;
	u32 sensorVendorid;
	u8  vendorPos;
	u8  sensorPos;
	u8  vendorByte;
	u8  *dataBuffer;
};

typedef enum{
	MODULE_ITEM = 0,
	AWB_H_ITEM,
	AWB_M_ITEM,
	AWB_L_ITEM,
	AWB_H_SRC_ITEM,
	AWB_M_SRC_ITEM,
	AWB_L_SRC_ITEM,
	AF_ITEM,
	BARCODE_ITEM,
	LSC_QUA_ITEM,
	LSC_MTK_ITEM,
	PDAF_QUA_GM_ITEM,
	PDAF_QUA_DCC_ITEM,
	PDAF_MTK_S1_ITEM,
	PDAF_MTK_S2_ITEM,
	DUALCAM_ITEM,
	CROSS_TALK_ITEM,
	DPC_ITEM,
	LDC_ITEM,
	MAX_ITEM,
}stCAM_CAL_CHECKSUM_ITEM;

struct stCAM_CAL_CHECKSUM_STRUCT{
	stCAM_CAL_CHECKSUM_ITEM item;
	u32 startAdress;
	u32 endAdress;
	u32 flagAdrees;
	u32 checksumAdress;
	u8  validFlag;
};
#endif /* ODM_WT_EDIT */

struct stCAM_CAL_INFO_STRUCT {
	u32 u4Offset;
	u32 u4Length;
	u32 sensorID;
	/*
	 * MAIN = 0x01,
	 * SUB  = 0x02,
	 * MAIN_2 = 0x04,
	 * SUB_2 = 0x08,
	 * MAIN_3 = 0x10,
	 */
	u32 deviceID;
	u8 *pu1Params;
};

#ifdef CONFIG_COMPAT

struct COMPAT_stCAM_CAL_INFO_STRUCT {
	u32 u4Offset;
	u32 u4Length;
	u32 sensorID;
	u32 deviceID;
	compat_uptr_t pu1Params;
};
#endif

#endif/*_CAM_CAL_DATA_H*/
