/***********************************************************
** Copyright (C), 2008-2019, OPPO Mobile Comm Corp., Ltd.
** ODM_WT_EDIT
** File: - imgsensor_read_eeprom.h
** Description: source  for read otp data
**
** Version: 1.0
** Date : 2019/10/18
** Author: Zhen.Quan@Camera.Dev
**
** ------------------------------- Revision History: -------------------------------
**     <author>        <data>        <version >        <desc>
**     Zhen.Quan      2019/10/18     1.0               source  for read otp data
**
****************************************************************/


#ifndef __IMGSENSOR_READ_EEPROM_H__
#define __IMGSENSOR_READ_EEPROM_H__

#include "kd_camera_typedef.h"
#include "cam_cal_define.h"
#include "kd_imgsensor.h"
/*Zhen.Quan@Camera.Driver, 2019/10/24, modify for build defined but not used*/
//monet
static struct stCAM_CAL_DATAINFO_STRUCT monet_truly_main_ov12a10_eeprom_data __attribute((unused))={
	.sensorID= OV12A10_SENSOR_ID,
	.deviceID = 1,
	.dataLength = 0x1628,
	.sensorVendorid = 0x0002002F,
	.vendorPos = 0,
	.sensorPos = 6,
	.vendorByte = 2,
	.dataBuffer = NULL,
};
static struct stCAM_CAL_CHECKSUM_STRUCT monet_truly_main_ov12a10_checksum[MAX_ITEM + 1] __attribute((unused))=
{
	{MODULE_ITEM,0x0000,0x000B,0x000C,0x000D,0x01},
	{AWB_H_ITEM,0x0010,0x001F,0x0020,0x0021,0x01},
	{AWB_M_ITEM,0x0022,0x0031,0x0032,0x0033,0x01},
	{AWB_L_ITEM,0x0034,0x0043,0x0044,0x0045,0x01},
	{AWB_H_SRC_ITEM,0x0060,0x0063,0x0064,0x0065,0x01},
	{AWB_M_SRC_ITEM,0x0066,0x0069,0x006A,0x006B,0x01},
	{AWB_L_SRC_ITEM,0x006C,0x006F,0x0070,0x0071,0x01},
	{AF_ITEM,0x0090,0x0095,0x0096,0x0097,0x01},
	{BARCODE_ITEM,0x00B0,0x00C0,0x00C1,0x00C2,0x01},
	{LSC_QUA_ITEM,0x00D0,0x00525,0x0526,0x0527,0x01},
	{LSC_MTK_ITEM,0x0530,0x0C7B,0x0C7C,0x0C7D,0x01},
	{PDAF_QUA_GM_ITEM,0x0C90,0x1009,0x100A,0x100B,0x01},
	{PDAF_QUA_DCC_ITEM,0x1010,0x1075,0x1076,0x1077,0x01},
	{PDAF_MTK_S1_ITEM,0x1080,0x126F,0x1270,0x1271,0x01},
	{PDAF_MTK_S2_ITEM,0x1280,0x15F5,0x15F6,0x15F7,0x01},
	{DUALCAM_ITEM,0x1620,0x1626,0x1626,0x1627,0x01},
	{MAX_ITEM,0xFFFF,0xFFFF,0xFFFF,0x0021,0x01},  // this line must haved
};
static struct stCAM_CAL_DATAINFO_STRUCT monetd_truly_main_ov12a10_eeprom_data __attribute((unused))={
        .sensorID= MONETD_TRULY_OV12A10_SENSOR_ID,
        .deviceID = 1,
        .dataLength = 0x1628,
        .sensorVendorid = 0x0002002F,
        .vendorPos = 0,
        .sensorPos = 6,
        .vendorByte = 2,
        .dataBuffer = NULL,
};
static struct stCAM_CAL_CHECKSUM_STRUCT monetd_truly_main_ov12a10_checksum[MAX_ITEM + 1] __attribute((unused))=
{
        {MODULE_ITEM,0x0000,0x000B,0x000C,0x000D,0x01},
        {AWB_H_ITEM,0x0010,0x001F,0x0020,0x0021,0x01},
        {AWB_M_ITEM,0x0022,0x0031,0x0032,0x0033,0x01},
        {AWB_L_ITEM,0x0034,0x0043,0x0044,0x0045,0x01},
        {AWB_H_SRC_ITEM,0x0060,0x0063,0x0064,0x0065,0x01},
        {AWB_M_SRC_ITEM,0x0066,0x0069,0x006A,0x006B,0x01},
        {AWB_L_SRC_ITEM,0x006C,0x006F,0x0070,0x0071,0x01},
        {AF_ITEM,0x0090,0x0095,0x0096,0x0097,0x01},
        {BARCODE_ITEM,0x00B0,0x00C0,0x00C1,0x00C2,0x01},
        {LSC_QUA_ITEM,0x00D0,0x00525,0x0526,0x0527,0x01},
        {LSC_MTK_ITEM,0x0530,0x0C7B,0x0C7C,0x0C7D,0x01},
        {PDAF_QUA_GM_ITEM,0x0C90,0x1009,0x100A,0x100B,0x01},
        {PDAF_QUA_DCC_ITEM,0x1010,0x1075,0x1076,0x1077,0x01},
        {PDAF_MTK_S1_ITEM,0x1080,0x126F,0x1270,0x1271,0x01},
        {PDAF_MTK_S2_ITEM,0x1280,0x15F5,0x15F6,0x15F7,0x01},
        {DUALCAM_ITEM,0x1620,0x1626,0x1626,0x1627,0x01},
        {MAX_ITEM,0xFFFF,0xFFFF,0xFFFF,0x0021,0x01},  // this line must haved
};

static struct stCAM_CAL_DATAINFO_STRUCT monet_hlt_front_gc5035_eeprom_data __attribute((unused))={
	.sensorID= GC5035_SENSOR_ID,
	.deviceID = 2,
	.dataLength = 0x0776,
	.sensorVendorid = 0x00100074,
	.vendorPos = 1,
	.sensorPos = 5,
	.vendorByte = 1,
	.dataBuffer = NULL,
};
static struct stCAM_CAL_CHECKSUM_STRUCT monet_hlt_front_gc5035_checksum[MAX_ITEM + 1] __attribute((unused))=
{
	{MODULE_ITEM,0x0000,0x0008,0x0000,0x0009,0x55},
	{AWB_H_ITEM,0x000A,0x0012,0x000A,0x0013,0x55},
	{AWB_M_ITEM,0x0014,0x001C,0x0014,0x001D,0x55},
	{AWB_L_ITEM,0x001E,0x0026,0x001E,0x0027,0x55},
	{LSC_MTK_ITEM,0x0028,0x0774,0x0028,0x0775,0x55},
	{MAX_ITEM,0xFFFF,0xFFFF,0xFFFF,0x0021,0x01},  // this line must haved
};
static struct stCAM_CAL_DATAINFO_STRUCT monet_hlt_custfront_gc5035_eeprom_data __attribute((unused))={
	.sensorID= MONET_GC5035_SENSOR_ID,
	.deviceID = 2,
	.dataLength = 0x1400,
	.sensorVendorid = 0x00090074,
	.vendorPos = 0,
	.sensorPos = 6,
	.vendorByte = 2,
	.dataBuffer = NULL,
};
static struct stCAM_CAL_CHECKSUM_STRUCT monet_hlt_custfront_gc5035_checksum[MAX_ITEM + 1] __attribute((unused))=
{
	{MODULE_ITEM,0x0000,0x001B,0x0042,0x0043,0x01},
	{AWB_H_ITEM,0x0000,0x001B,0x0042,0x0043,0x01},
	{AWB_M_ITEM,0x0054,0x0063,0x0068,0x0069,0x01},
	{AWB_L_ITEM,0x0032,0x0041,0x0046,0x0047,0x01},
	{LSC_MTK_ITEM,0x0C00,0x134B,0x134C,0x134D,0x01},
	{MAX_ITEM,0xFFFF,0xFFFF,0xFFFF,0x0021,0x01},  // this line must haved
};
static struct stCAM_CAL_DATAINFO_STRUCT monet_hlt_front_gc5035b_eeprom_data __attribute((unused))={
	.sensorID= MONET_GC5035B_SENSOR_ID,
	.deviceID = 2,
	.dataLength = 0x0776,
	.sensorVendorid = 0x00100075,
	.vendorPos = 1,
	.sensorPos = 5,
	.vendorByte = 1,
	.dataBuffer = NULL,
};
static struct stCAM_CAL_CHECKSUM_STRUCT monet_hlt_front_gc5035b_checksum[MAX_ITEM + 1] __attribute((unused))=
{
	{MODULE_ITEM,0x0000,0x0008,0x0000,0x0009,0x55},
	{AWB_H_ITEM,0x000A,0x0012,0x000A,0x0013,0x55},
	{AWB_M_ITEM,0x0014,0x001C,0x0014,0x001D,0x55},
	{AWB_L_ITEM,0x001E,0x0026,0x001E,0x0027,0x55},
	{LSC_MTK_ITEM,0x0028,0x0774,0x0028,0x0775,0x55},
	{MAX_ITEM,0xFFFF,0xFFFF,0xFFFF,0x0021,0x01},  // this line must haved

};
static struct stCAM_CAL_DATAINFO_STRUCT monet_hlt_macro_gc2375h_eeprom_data __attribute((unused))={
	.sensorID= GC2375H_SENSOR_ID,
	.deviceID = 0x10,
	.dataLength = 0x0776,
	.sensorVendorid = 0x00100005,/* Zhen.Quan@Camera.Driver, 2019/10/18, add for [otp bringup] */
	.vendorPos = 1,//module id
	.sensorPos = 2,//position
	.vendorByte = 1,
	.dataBuffer = NULL,
};
static struct stCAM_CAL_CHECKSUM_STRUCT monet_hlt_macro_gc2375h_checksum[MAX_ITEM + 1] __attribute((unused))=
{
	{MODULE_ITEM,0x0000,0x0008,0x0000,0x0009,0x55},
	{AWB_H_ITEM,0x000A,0x0012,0x000A,0x0013,0x55},
	{LSC_MTK_ITEM,0x0014,0x0760,0x0014,0x0761,0x55},
	{MAX_ITEM,0xFFFF,0xFFFF,0xFFFF,0x0021,0x01},  // this line must haved
};
/* Zhen.Quan@Camera.Driver, 2019/11/4, add for [otp bringup] */
static struct stCAM_CAL_DATAINFO_STRUCT monet_lh_macro_gc2375h_eeprom_data __attribute((unused))={
	.sensorID= MONET_LH_MACRO_GC2375H_SENSOR_ID,
	.deviceID = 0x10,
	.dataLength = 0x0776,
	.sensorVendorid = 0x001A0005,
	.vendorPos = 1,//module id
	.sensorPos = 2,//position
	.vendorByte = 1,
	.dataBuffer = NULL,
};
static struct stCAM_CAL_CHECKSUM_STRUCT monet_lh_macro_gc2375h_checksum[MAX_ITEM + 1] __attribute((unused))=
{
	{MODULE_ITEM,0x0000,0x0008,0x0000,0x0009,0x55},
	{AWB_H_ITEM,0x000A,0x0012,0x000A,0x0013,0x55},
	{LSC_MTK_ITEM,0x0014,0x0760,0x0014,0x0761,0x55},
	{MAX_ITEM,0xFFFF,0xFFFF,0xFFFF,0x0021,0x01},  // this line must haved
};
//monet-d
static struct stCAM_CAL_DATAINFO_STRUCT monetd_lh_macro_gc2375h_eeprom_data __attribute((unused))={
	.sensorID= MONETD_LH_DEPTH_GC2375H_SENSOR_ID,
	.deviceID = 0x10,
	.dataLength = 0x0776,
	.sensorVendorid = 0x001A0005,
	.vendorPos = 1,//module id
	.sensorPos = 2,//position
	.vendorByte = 1,
	.dataBuffer = NULL,
};
static struct stCAM_CAL_CHECKSUM_STRUCT monetd_lh_macro_gc2375h_checksum[MAX_ITEM + 1] __attribute((unused))=
{
	{MODULE_ITEM,0x0000,0x0008,0x0000,0x0009,0x55},
	{AWB_H_ITEM,0x000A,0x0012,0x000A,0x0013,0x55},
	{LSC_MTK_ITEM,0x0014,0x0760,0x0014,0x0761,0x55},
	{MAX_ITEM,0xFFFF,0xFFFF,0xFFFF,0x0021,0x01},  // this line must haved
};
//monet-x
static struct stCAM_CAL_DATAINFO_STRUCT monetx_truly_main_s5kgm1sp_eeprom_data __attribute((unused))={
	.sensorID= S5KGM1SP_SENSOR_ID,
	.deviceID = 1,
	.dataLength = 0x1632,
	.sensorVendorid = 0x0002006B,
	.vendorPos = 0,
	.sensorPos = 6,
	.vendorByte = 2,
	.dataBuffer = NULL,
};
static struct stCAM_CAL_CHECKSUM_STRUCT monetx_truly_main_s5kgm1sp_hecksum[MAX_ITEM + 1] __attribute((unused))=
{
	{MODULE_ITEM,0x0000,0x000B,0x000C,0x000D,0x01},
	{AWB_H_ITEM,0x0010,0x001F,0x0020,0x0021,0x01},
	{AWB_M_ITEM,0x0022,0x0031,0x0032,0x0033,0x01},
	{AWB_L_ITEM,0x0034,0x0043,0x0044,0x0045,0x01},
	{AWB_H_SRC_ITEM,0x0060,0x0063,0x0064,0x0065,0x01},
	{AWB_M_SRC_ITEM,0x0066,0x0069,0x006A,0x006B,0x01},
	{AWB_L_SRC_ITEM,0x006C,0x006F,0x0070,0x0071,0x01},
	{AF_ITEM,0x0090,0x0095,0x0096,0x0097,0x01},
	{BARCODE_ITEM,0x00B0,0x00C0,0x00C1,0x00C2,0x01},
	{LSC_QUA_ITEM,0x00D0,0x00525,0x0526,0x0527,0x01},
	{LSC_MTK_ITEM,0x0530,0x0C7B,0x0C7C,0x0C7D,0x01},
	{PDAF_QUA_GM_ITEM,0x0C90,0x1009,0x100A,0x100B,0x01},
	{PDAF_QUA_DCC_ITEM,0x1010,0x1075,0x1076,0x1077,0x01},
	{PDAF_MTK_S1_ITEM,0x1080,0x126F,0x1270,0x1271,0x01},
	{PDAF_MTK_S2_ITEM,0x1280,0x1615,0x1616,0x1617,0x01},
	{DUALCAM_ITEM,0x1620,0x162F,0x1630,0x1631,0x01},
	{MAX_ITEM,0xFFFF,0xFFFF,0xFFFF,0x0021,0x01},  // this line must haved
};

static struct stCAM_CAL_DATAINFO_STRUCT monetx_ofilm_front_ov16a1q_eeprom_data __attribute((unused))={
	.sensorID= OV16A1Q_SENSOR_ID,
	.deviceID = 2,
	.dataLength = 0x1682,
	.sensorVendorid = 0x00060094,
	.vendorPos = 0,
	.sensorPos = 6,
	.vendorByte = 2,
	.dataBuffer = NULL,
};
static struct stCAM_CAL_CHECKSUM_STRUCT monetx_ofilm_front_ov16a1q_checksum[MAX_ITEM + 1] __attribute((unused))=
{
	{MODULE_ITEM,0x0000,0x000B,0x000C,0x000D,0x01},
	{AWB_H_ITEM,0x0010,0x001F,0x0020,0x0021,0x01},
	{AWB_M_ITEM,0x0022,0x0031,0x0032,0x0033,0x01},
	{AWB_L_ITEM,0x0034,0x0043,0x0044,0x0045,0x01},
	{AWB_H_SRC_ITEM,0x0060,0x0063,0x0064,0x0065,0x01},
	{AWB_M_SRC_ITEM,0x0066,0x0069,0x006A,0x006B,0x01},
	{AWB_L_SRC_ITEM,0x006C,0x006F,0x0070,0x0071,0x01},
	{BARCODE_ITEM,0x00B0,0x00C0,0x00C1,0x00C2,0x01},
	{LSC_QUA_ITEM,0x00D0,0x00525,0x0526,0x0527,0x01},
	{LSC_MTK_ITEM,0x0530,0x0C7B,0x0C7C,0x0C7D,0x01},
	{CROSS_TALK_ITEM,0x0C90,0x0EE7,0x0EE8,0x0EE9,0x01},
	{DPC_ITEM,0x0FF0,0x167F,0x1680,0x1681,0x01},
	{MAX_ITEM,0xFFFF,0xFFFF,0xFFFF,0x0021,0x01},  // this line must haved
};

static struct stCAM_CAL_DATAINFO_STRUCT monetx_shengtai_wide_ov8856_eeprom_data __attribute((unused))={
	.sensorID= OV8856_SENSOR_ID,
	.deviceID = 4,
	.dataLength = 0x1a20,
	.sensorVendorid = 0x00070025,
	.vendorPos = 0,
	.sensorPos = 6,
	.vendorByte = 2,
	.dataBuffer = NULL,
};
static struct stCAM_CAL_CHECKSUM_STRUCT monetx_shengtai_wide_ov8856_checksum[MAX_ITEM + 1] __attribute((unused))=
{
	{MODULE_ITEM,0x0000,0x000B,0x000C,0x000D,0x01},
	{AWB_H_ITEM,0x0010,0x001F,0x0020,0x0021,0x01},
	{AWB_M_ITEM,0x0022,0x0031,0x0032,0x0033,0x01},
	{AWB_L_ITEM,0x0034,0x0043,0x0044,0x0045,0x01},
	{AWB_H_SRC_ITEM,0x0060,0x0063,0x0064,0x0065,0x01},
	{AWB_M_SRC_ITEM,0x0066,0x0069,0x006A,0x006B,0x01},
	{AWB_L_SRC_ITEM,0x006C,0x006F,0x0070,0x0071,0x01},
	{BARCODE_ITEM,0x00B0,0x00C0,0x00C1,0x00C2,0x01},
	{LSC_QUA_ITEM,0x00D0,0x00525,0x0526,0x0527,0x01},
	{LSC_MTK_ITEM,0x0530,0x0C7B,0x0C7C,0x0C7D,0x01},
	{DUALCAM_ITEM,0x0C90,0x0C93,0x0C94,0x0C95,0x01},
	{LDC_ITEM,0x0CA0,0x1A10,0x1A11,0x1A12,0x01},
	{MAX_ITEM,0xFFFF,0xFFFF,0xFFFF,0x0021,0x01},  // this line must haved
};
/* Zhen.Quan@Camera.Driver, 2019/11/7, add for [otp bringup] */
static struct stCAM_CAL_DATAINFO_STRUCT monetx_hlt_macro_gc2375h_eeprom_data __attribute((unused))={
	.sensorID= MONETX_HLT_MACRO_GC2375H_SENSOR_ID,
	.deviceID = 0x10,
	.dataLength = 0x1F00,
	.sensorVendorid = 0x00090071,
	.vendorPos = 0,//module id
	.sensorPos = 6,//position
	.vendorByte = 2,
	.dataBuffer = NULL,
};
static struct stCAM_CAL_CHECKSUM_STRUCT monetx_hlt_macro_gc2375h_checksum[MAX_ITEM + 1] __attribute((unused))=
{
	{MODULE_ITEM,0x0000,0x000B,0x000C,0x000D,0x01},
	{AWB_H_ITEM,0x000E,0x001D,0x001E,0x001F,0x01},
	{AWB_M_ITEM,0x0020,0x002F,0x0030,0x0031,0x01},
	{AWB_L_ITEM,0x0032,0x0041,0x0042,0x0043,0x01},
	{AWB_H_SRC_ITEM,0x0044,0x0047,0x0048,0x0049,0x01},
	{AWB_M_SRC_ITEM,0x004A,0x004D,0x004E,0x004F,0x01},
	{AWB_L_SRC_ITEM,0x0050,0x0053,0x0054,0x0055,0x01},
	{BARCODE_ITEM,0x00E0,0x00F0,0x00F1,0x00F2,0x01},
	{LSC_QUA_ITEM,0x0200,0x0654,0x0656,0x0657,0x01},
	/* Tian.Tian@Camera.Driver, 2019/11/17, add for [otp bringup] */
	{LSC_MTK_ITEM,0x0C00,0x134B,0x134C,0x134D,0x01},
	{MAX_ITEM,0xFFFF,0xFFFF,0xFFFF,0x0021,0x01},  // this line must haved
};
/* Tian.Tian@Camera.Driver, 2019/11/17, add for [otp bringup] */
static struct stCAM_CAL_DATAINFO_STRUCT monetx_hlt_macro_gc2385_eeprom_data __attribute((unused))={
	.sensorID= MONETX_HLT_MACRO_GC2385_SENSOR_ID,
	.deviceID = 0x10,
	.dataLength = 0x1F00,
	.sensorVendorid = 0x0009007b,
	.vendorPos = 0,//module id
	.sensorPos = 6,//position
	.vendorByte = 2,
	.dataBuffer = NULL,
};
static struct stCAM_CAL_CHECKSUM_STRUCT monetx_hlt_macro_gc2385_checksum[MAX_ITEM + 1] __attribute((unused))=
{
	{MODULE_ITEM,0x0000,0x000B,0x000C,0x000D,0x01},
	{AWB_H_ITEM,0x000E,0x001D,0x001E,0x001F,0x01},
	{AWB_M_ITEM,0x0020,0x002F,0x0030,0x0031,0x01},
	{AWB_L_ITEM,0x0032,0x0041,0x0042,0x0043,0x01},
	{AWB_H_SRC_ITEM,0x0044,0x0047,0x0048,0x0049,0x01},
	{AWB_M_SRC_ITEM,0x004A,0x004D,0x004E,0x004F,0x01},
	{AWB_L_SRC_ITEM,0x0050,0x0053,0x0054,0x0055,0x01},
	{BARCODE_ITEM,0x00E0,0x00F0,0x00F1,0x00F2,0x01},
	{LSC_QUA_ITEM,0x0200,0x0654,0x0656,0x0657,0x01},
	{LSC_MTK_ITEM,0x0C00,0x134B,0x134C,0x134D,0x01},
	{MAX_ITEM,0xFFFF,0xFFFF,0xFFFF,0x0021,0x01},  // this line must haved
};

static  __attribute((unused)) char* board_monetx[20]={
	"S98675AA1","S98675BA1","S98675CA1",
 	"S98675DA1","S98675EA1","S98675FA1",
	"S98675GA1","S98675HA1","S98675JA1",
	"S98675KA1","S98675LA1","S98675MA1",
	"S98675NA1","S98675PA1","S98675QA1",
	"S98675RA1","NULL"
};

static __attribute((unused)) char* board_monet[20]  ={
	"S98670AA1","S98670BA1","S98670CA1",
	"S98670GA1","S98670HA1","S98670JA1",
	"S98670KA1","S98670LA1","S98670MA1",
	"S98670NA1","S98670PA1","S98670WA1",
	"S986703A1","S986704A1","NULL"
};

static __attribute((unused)) char* board_monetd[20] ={
	"S98670DA1","S98670EA1","S98670FA1",
	"S98670QA1","S98670RA1","S98670SA1",
	"S986701A1","NULL"
};

extern int imgSensorReadEepromData(struct stCAM_CAL_DATAINFO_STRUCT* pData,
	struct stCAM_CAL_CHECKSUM_STRUCT* checkData);
extern int imgSensorSetEepromData(struct stCAM_CAL_DATAINFO_STRUCT* pData);
extern char *saved_command_line;
static char* boardid_get(void)
{
	char *p, *q;
	char* s1="not found";
	char board_id[HARDWARE_MAX_ITEM_LONGTH];
	p = strstr(saved_command_line, "board_id=");
	if(!p) {
		printk("board_id not found in cmdline\n");
		return s1;
	}

	if ((p - saved_command_line) > strlen(saved_command_line + 1))
		return s1;

	p += strlen("board_id=");
	q = p;
	while (*q != ' ' && *q != '\0')
		q++;

	memset(board_id, 0, sizeof(board_id));
	strncpy(board_id, p, (int)(q - p));
	board_id[q - p + 1]='\0';
	s1 = board_id;
	//printk("board_id found in cmdline : %s\n", board_id);

	return s1;
}
static __attribute((unused)) kal_bool check_board_id(char **boardid_arr){
	int i = 0;
	char *board_id = NULL;
	board_id = boardid_get();
	printk("get board_id=%s\n",board_id);
	for (i = 0;strcmp(boardid_arr[i],"NULL");i++){
		if(!strcmp(boardid_arr[i], board_id)){
			printk("get board_id i=%d\n",i);
			break;
		}
	}
	if (!strcmp(boardid_arr[i],"NULL")){
		return KAL_FALSE;
	}
	return KAL_TRUE;
}
static __attribute((unused)) kal_bool check_otp_data(struct stCAM_CAL_DATAINFO_STRUCT* pData, struct stCAM_CAL_CHECKSUM_STRUCT* checkData,UINT32 *sensor_id){
	int size = 0;
	size = imgSensorReadEepromData(pData, checkData);
	if(size != pData->dataLength){
		pr_err("get eeprom data failed\n");
		*sensor_id = 0xFFFFFFFF;
		return KAL_FALSE;
	} else {
		pr_err("get eeprom data success\n");
	}
	imgSensorSetEepromData(pData);
	return KAL_TRUE;
}


#endif

