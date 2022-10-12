/*
 * Copyright (C) 2017 MediaTek Inc.
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
/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 OV16A1Qmipiraw_sensor.c
 *
 * Project:
 * --------
 *	 ALPS MT6758
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
/*#include <asm/atomic.h>*/

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
/*xiaojun.Pu@Camera.Driver, 2019/10/15, add for [add hardware_info for factory]*/
#include <linux/hardware_info.h>
#include "ov16a1qmipiraw_Sensor.h"
/* Zhen.Quan@Camera.Driver, 2019/10/17, add for [otp bringup] */
#include "imgsensor_read_eeprom.h"
#define ENABLE_OV16A1Q_OTP 1

#define PFX "OV16A1Q"

#define LOG_INF(fmt, args...)   pr_debug(PFX "[%s] " fmt, __func__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = OV16A1Q_SENSOR_ID,		/*record sensor id defined in Kd_imgsensor.h*/
	.checksum_value = 0xb1893b4f,		/*checksum value for Camera Auto Test*/
	.pre = {
		.pclk = 100000000,				/*record different mode's pclk*/
		.linelength  = 1700,				/*record different mode's linelength*/
		.framelength = 1960,			/*record different mode's framelength*/
		.startx = 0,					/*record different mode's startx of grabwindow*/
		.starty = 0,					/*record different mode's starty of grabwindow*/
		.grabwindow_width  = 2328,		/*record different mode's width of grabwindow*/
		.grabwindow_height = 1748,		/*record different mode's height of grabwindow*/
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 19,	//19,23,65,85,120 若点亮后有数据可以尝试修改这个值，此四个值为推荐修改值
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 302400000,
	},
	.cap = {
		.pclk = 100000000,
		.linelength = 1700,	//850,
		.framelength = 1960, //3920,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2328,
		.grabwindow_height = 1748,
		.mipi_data_lp2hs_settle_dc = 19,	//19,23,65,85,120 若点亮后有数据可以尝试修改这个值，此四个值为推荐修改值
		.max_framerate = 300,	//30.0120fps
		.mipi_pixel_rate = 302400000,
	},
	.cap1 = {							/*capture for 15fps*/
		.pclk = 100000000,
		.linelength  = 1700,
		.framelength = 3920,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 2328,
		.grabwindow_height = 1748,
		.mipi_data_lp2hs_settle_dc = 19,
		.max_framerate = 150,
		.mipi_pixel_rate = 302400000,
	},
	.normal_video = { /* cap*/
		.pclk = 100000000,			/* XiaoSong.Xue@Camera.Driver, 2019/11/28, add for update ov16a1q driver */
		.linelength = 1700, 		/* XiaoSong.Xue@Camera.Driver, 2019/11/28, add for update ov16a1q driver */
		.framelength = 1960, 		/* XiaoSong.Xue@Camera.Driver, 2019/11/28, add for update ov16a1q driver */
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2328,	/* XiaoSong.Xue@Camera.Driver, 2019/11/28, add for update ov16a1q driver */
		.grabwindow_height = 1748,	/* XiaoSong.Xue@Camera.Driver, 2019/11/28, add for update ov16a1q driver */
		.mipi_data_lp2hs_settle_dc = 19,	/* XiaoSong.Xue@Camera.Driver, 2019/11/28, add for update ov16a1q driver */
		.max_framerate = 300,
		.mipi_pixel_rate = 302400000,
	},
	.hs_video = {
		.pclk = 100000000,
		.linelength = 1700,	//850,
		.framelength = 1960, //3920,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2328,
		.grabwindow_height = 1748,
		.mipi_data_lp2hs_settle_dc = 19,	//19,23,65,85,120 若点亮后有数据可以尝试修改这个值，此四个值为推荐修改值
		.max_framerate = 300,	//30.0120fps
		.mipi_pixel_rate = 302400000,
	},
	.slim_video = {/*pre*/
		.pclk = 100000000,
		.linelength = 1700,	//850,
		.framelength = 1960, //3920,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2328,
		.grabwindow_height = 1748,
		.mipi_data_lp2hs_settle_dc = 19,	//19,23,65,85,120 若点亮后有数据可以尝试修改这个值，此四个值为推荐修改值
		.max_framerate = 300,	//30.0120fps
		.mipi_pixel_rate = 302400000,
	},
	/* Tian.Tian@Camera.Driver, 2019/11/10, add for remosaic start */
	.custom1 = {
		.pclk = 100000000,
		.linelength = 850,
		.framelength = 3920,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4656,
		.grabwindow_height = 3496,
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		.max_framerate = 300,
		.mipi_pixel_rate = 580800000,
	},
	.custom2 = {
		.pclk = 100000000,
		.linelength = 850,
		.framelength = 3920,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4656,
		.grabwindow_height = 3496,
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		.max_framerate = 300,
		.mipi_pixel_rate = 580800000,
	},
	.custom3 = {
		.pclk = 100000000,
		.linelength = 850,
		.framelength = 3920,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4656,
		.grabwindow_height = 3496,
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		.max_framerate = 300,
		.mipi_pixel_rate = 580800000,
	},
	/* Tian.Tian@Camera.Driver, 2019/11/10, add for remosaic end */
	.margin = 8,			/*sensor framelength & shutter margin*/
	.min_shutter = 8,		/*min shutter*/
	.max_frame_length = 0x7ffe,/*max framelength by sensor register's limitation*/
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,/*isp gain delay frame for AE cycle*/
	.ihdr_support = 0,	  /*1, support; 0,not support*/
	.ihdr_le_firstline = 0,  /*1,le first ; 0, se first*/
	/* Tian.Tian@Camera.Driver, 2019/11/10, add for remosaic */
	.sensor_mode_num = 8,	  /*support sensor mode num ,don't support Slow motion*/
	.cap_delay_frame = 3,		/*enter capture delay frame num*/
	.pre_delay_frame = 3,		/*enter preview delay frame num*/
	.video_delay_frame = 3,		/*enter video delay frame num*/
	.hs_video_delay_frame = 3,	/*enter high speed video  delay frame num*/
	.slim_video_delay_frame = 3,/*enter slim video delay frame num*/
	.custom3_delay_frame = 3, /* Tian.Tian@Camera.Driver, 2019/11/10, add for remosaic */
	/*Duilin.Qin@ODM_WT.Camera.Driver.493476, 2019/10/21, modify mclk driving current */
	.isp_driving_current = ISP_DRIVING_2MA, /*mclk driving current*/
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,/*Sensor_interface_type*/
	.mipi_sensor_type = MIPI_OPHY_NCSI2, /*0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2*/
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_MANUAL,/*0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL*/
	/* Tian.Tian@Camera.Driver, 2019/11/10, add for remosaic */
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,/*sensor output first pixel color*/
	.mclk = 24,/*mclk value, suggest 24 or 26 for 24Mhz or 26Mhz*/
	.mipi_lane_num = SENSOR_MIPI_4_LANE,/*mipi lane num*/
	.i2c_addr_table = {0x20, 0xff},/*record sensor support all write id addr, only supprt 4must end with 0xff*/
};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,				/*mirrorflip information*/
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x4C00,					/*current shutter*/
	.gain = 0x200,						/*current gain*/
	.dummy_pixel = 0,					/*current dummypixel*/
	.dummy_line = 0,					/*current dummyline*/
	.current_fps = 300,  /*full size current fps : 24fps for PIP, 30fps for Normal or ZSD*/
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,/*current scenario id*/
	.ihdr_en = 0, /*sensor need support LE, SE with HDR feature*/
	.i2c_write_id = 0x20,/*record current sensor's i2c write id*/
};


/* Sensor output window information*/
/* Tian.Tian@Camera.Driver, 2019/11/10, add for remosaic */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[10] = {
{ 4656, 3496, 0, 0, 4656, 3496, 2328, 1748,  0, 0,  2328, 1748, 0, 0, 2328, 1748 }, // Preview
{ 4656, 3496, 0, 0, 4656, 3496, 2328, 1748,  0, 0,  2328, 1748, 0, 0, 2328, 1748 },//capture
{ 4656, 3496, 0, 0, 4656, 3496, 2328, 1748,  0, 0,  2328, 1748, 0, 0, 2328, 1748 },//normal-video
{ 4656, 3496, 0, 0, 4656, 3496, 2328, 1748,  0, 0,  2328, 1748, 0, 0, 2328, 1748 },/*hs-video*/
{ 4656, 3496, 0, 0, 4656, 3496, 2328, 1748,  0, 0,  2328, 1748, 0, 0, 2328, 1748 },/*slim-video*/
{ 4656, 3496, 0, 0, 4656, 3496, 4656, 3496,  0, 0,  4656, 3496, 0, 0, 4656, 3496 }, //custom3
};

#define SET_STREAMING_TEST 0
#define MULTI_WRITE 1

#if MULTI_WRITE
#define I2C_BUFFER_LEN 255
#else
#define I2C_BUFFER_LEN 3
#endif

static kal_uint16 ov16a1q_table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend, IDX;
	kal_uint16 addr = 0, addr_last = 0, data;

	tosend = 0;
	IDX = 0;
	while (len > IDX) {
		addr = para[IDX];

		{
			puSendCmd[tosend++] = (char)(addr >> 8);
			puSendCmd[tosend++] = (char)(addr & 0xFF);
			data = para[IDX + 1];
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
			addr_last = addr;

		}
#if MULTI_WRITE
		/* Write when remain buffer size is less than 3 bytes or reach end of data */
		if ((I2C_BUFFER_LEN - tosend) < 3 || IDX == len || addr != addr_last) {
			iBurstWriteReg_multi(puSendCmd, tosend, imgsensor.i2c_write_id, 3,
				 imgsensor_info.i2c_speed);
			tosend = 0;
		}
#else
		iWriteRegI2C(puSendCmd, 3, imgsensor.i2c_write_id);
		tosend = 0;

#endif
	}
	return 0;
}

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d\n", imgsensor.dummy_line, imgsensor.dummy_pixel);
	write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
	write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
	write_cmos_sensor(0x380c, imgsensor.line_length >> 8);
	write_cmos_sensor(0x380d, imgsensor.line_length & 0xFF);
}	/*	set_dummy  */


static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length;

	LOG_INF("framerate = %d, min framelength should enable?\n", framerate);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length =
		(frame_length > imgsensor.min_frame_length) ? frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}	/*	set_max_framerate  */




static void set_shutter_frame_length(kal_uint16 shutter, kal_uint16 frame_length)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	spin_lock(&imgsensor_drv_lock);
	/*Change frame time*/
	dummy_line = frame_length - imgsensor.frame_length;
	imgsensor.frame_length = imgsensor.frame_length + dummy_line;
	imgsensor.min_frame_length = imgsensor.frame_length;

	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	if (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
		shutter = (imgsensor_info.max_frame_length - imgsensor_info.margin);

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
		/* Extend frame length*/
		write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
		}
	} else {
		/* Extend frame length*/
		 write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
		 write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
	}

	/* Update Shutter*/
	write_cmos_sensor(0x3502, (shutter) & 0xFF);
	write_cmos_sensor(0x3501, (shutter >> 8) & 0xFF);
	//write_cmos_sensor(0x3500, (shutter >> 12) & 0x0F);

	LOG_INF("Add for N3D! shutterlzl =%d, framelength =%d\n", shutter, imgsensor.frame_length);

}

/*************************************************************************
* FUNCTION
*	set_shutter
*
* DESCRIPTION
*	This function set e-shutter of sensor to change exposure time.
*
* PARAMETERS
*	iShutter : exposured lines
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void set_shutter(kal_uint16 shutter)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	/* 0x3500, 0x3501, 0x3502 will increase VBLANK to get exposure larger than frame exposure */
	/* AE doesn't update sensor gain at capture mode, thus extra exposure lines must be updated here. */
	/* OV Recommend Solution*/
	/* if shutter bigger than frame_length, should extend frame length first*/
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	if (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
		shutter = (imgsensor_info.max_frame_length - imgsensor_info.margin);

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
		/* Extend frame length*/
		write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
		}
	} else {
		/* Extend frame length*/
		write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
	}

	/* Update Shutter*/
	write_cmos_sensor(0x3502, (shutter) & 0xFF);
	write_cmos_sensor(0x3501, (shutter >> 8) & 0xFF);
	//write_cmos_sensor(0x3500, (shutter >> 12) & 0x0F);

	LOG_INF("Exit! shutter =%d, framelength =%d\n", shutter, imgsensor.frame_length);

}

/*************************************************************************
* FUNCTION
*	set_gain
*
* DESCRIPTION
*	This function is to set global gain to sensor.
*
* PARAMETERS
*	iGain : sensor global gain(base: 0x40)
*
* RETURNS
*	the actually gain set to sensor.
*
* GLOBALS AFFECTED
*
*************************************************************************/
/*HuangMiao@ODM_WT.Camera.Driver, 2019/11/05, ADD gain2reg */
static kal_uint16 gain2reg(const kal_uint16 gain)//check out
{
	kal_uint16 iReg = 0x0000;

	//platform 1xgain = 64, sensor driver 1*gain = 0x80
	iReg = gain*256/BASEGAIN;

	if (iReg < 0x100)	 // sensor 1xGain
		iReg = 0X100;
	if (iReg > 0xf80)	 // sensor 15.5xGain
		iReg = 0Xf80;

	return iReg;
}

static kal_uint16 set_gain(kal_uint16 gain)//check out
{
	kal_uint16 reg_gain;
	unsigned long flags;

	reg_gain = gain2reg(gain);
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.gain = reg_gain;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);
	write_cmos_sensor(0x03508, (reg_gain >> 8));
	write_cmos_sensor(0x03509, (reg_gain&0xff));

	return gain;
}

#if 0
static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain) //for OV8856 only
{
	LOG_INF("le:0x%x, se:0x%x, gain:0x%x\n", le, se, gain);
	if (imgsensor.ihdr_en) {
		spin_lock(&imgsensor_drv_lock);
			if (le > imgsensor.min_frame_length - imgsensor_info.margin)
				imgsensor.frame_length = le + imgsensor_info.margin;
			else
				imgsensor.frame_length = imgsensor.min_frame_length;
			if (imgsensor.frame_length > imgsensor_info.max_frame_length)
				imgsensor.frame_length = imgsensor_info.max_frame_length;
			spin_unlock(&imgsensor_drv_lock);
			if (le < imgsensor_info.min_shutter)
				le = imgsensor_info.min_shutter;
			if (se < imgsensor_info.min_shutter)
				se = imgsensor_info.min_shutter;
		/* Extend frame length first*/
		write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
		write_cmos_sensor(0x3502, (le << 4) & 0xFF);
		write_cmos_sensor(0x3501, (le >> 4) & 0xFF);
		write_cmos_sensor(0x3500, (le >> 12) & 0x0F);
		write_cmos_sensor(0x3512, (se << 4) & 0xFF);
		write_cmos_sensor(0x3511, (se >> 4) & 0xFF);
		write_cmos_sensor(0x3510, (se >> 12) & 0x0F);
		set_gain(gain);
	}
}
#endif

#if 0
static void set_mirror_flip(kal_uint8 image_mirror) //for OV8856 only
{
	LOG_INF("image_mirror = %d\n", image_mirror);

	/********************************************************
	   *
	   *   0x3820[2] ISP Vertical flip
	   *   0x3820[1] Sensor Vertical flip
	   *
	   *   0x3821[2] ISP Horizontal mirror
	   *   0x3821[1] Sensor Horizontal mirror
	   *
	   *   ISP and Sensor flip or mirror register bit should be the same!!
	   *
	   ********************************************************/

	switch (image_mirror) {
	case IMAGE_NORMAL:
			write_cmos_sensor(0x3820, ((read_cmos_sensor(0x3820) & 0xB9) | 0x00));
			write_cmos_sensor(0x3821, ((read_cmos_sensor(0x3821) & 0xF9) | 0x06));
			write_cmos_sensor(0x502e, ((read_cmos_sensor(0x502e) & 0xFC) | 0x03));
			write_cmos_sensor(0x5001, ((read_cmos_sensor(0x5001) & 0xFB) | 0x00));
			write_cmos_sensor(0x5004, ((read_cmos_sensor(0x5004) & 0xFB) | 0x04));
			write_cmos_sensor(0x376b, 0x30);

			break;
	case IMAGE_H_MIRROR:
			write_cmos_sensor(0x3820, ((read_cmos_sensor(0x3820) & 0xB9) | 0x00));
			write_cmos_sensor(0x3821, ((read_cmos_sensor(0x3821) & 0xF9) | 0x00));
			write_cmos_sensor(0x502e, ((read_cmos_sensor(0x502e) & 0xFC) | 0x03));
			write_cmos_sensor(0x5001, ((read_cmos_sensor(0x5001) & 0xFB) | 0x00));
			write_cmos_sensor(0x5004, ((read_cmos_sensor(0x5004) & 0xFB) | 0x00));
			write_cmos_sensor(0x376b, 0x30);
			break;
	case IMAGE_V_MIRROR:
			write_cmos_sensor(0x3820, ((read_cmos_sensor(0x3820) & 0xB9) | 0x46));
			write_cmos_sensor(0x3821, ((read_cmos_sensor(0x3821) & 0xF9) | 0x06));
			write_cmos_sensor(0x502e, ((read_cmos_sensor(0x502e) & 0xFC) | 0x00));
			write_cmos_sensor(0x5001, ((read_cmos_sensor(0x5001) & 0xFB) | 0x04));
			write_cmos_sensor(0x5004, ((read_cmos_sensor(0x5004) & 0xFB) | 0x04));
			write_cmos_sensor(0x376b, 0x36);
			break;
	case IMAGE_HV_MIRROR:
			write_cmos_sensor(0x3820, ((read_cmos_sensor(0x3820) & 0xB9) | 0x46));
			write_cmos_sensor(0x3821, ((read_cmos_sensor(0x3821) & 0xF9) | 0x00));
			write_cmos_sensor(0x502e, ((read_cmos_sensor(0x502e) & 0xFC) | 0x00));
			write_cmos_sensor(0x5001, ((read_cmos_sensor(0x5001) & 0xFB) | 0x04));
			write_cmos_sensor(0x5004, ((read_cmos_sensor(0x5004) & 0xFB) | 0x00));
			write_cmos_sensor(0x376b, 0x36);
			break;
	default:
			LOG_INF("Error image_mirror setting\n");
	}

}
#endif
/*************************************************************************
* FUNCTION
*	night_mode
*
* DESCRIPTION
*	This function night mode of sensor.
*
* PARAMETERS
*	bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/
}	/*	night_mode	*/

/*************************************************************************
* FUNCTION
*	sensor_init
*
* DESCRIPTION
*	Sensor init
*
* PARAMETERS
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/

kal_uint16 addr_data_pair_init_ov16a1q[] = {
	0x0103, 0x01,
	0x0102, 0x00,
	0x0301, 0x48,
	0x0302, 0x31,
	0x0303, 0x04,
	0x0305, 0xc2,
	0x0306, 0x00,
	0x0320, 0x02,
	0x0323, 0x04,
	0x0326, 0xd8,
	0x0327, 0x0b,
	0x0329, 0x01,
	0x0343, 0x04,
	0x0344, 0x01,
	0x0345, 0x2c,
	0x0346, 0xc0,
	0x034a, 0x07,
	0x300e, 0x22,
	0x3012, 0x41,
	0x3016, 0xd2,
	0x3018, 0x70,
	0x301e, 0x98,
	0x3025, 0x03,
	0x3026, 0x10,
	0x3027, 0x08,
	0x3102, 0x00,
	0x3400, 0x04,
	0x3406, 0x04,
	0x3408, 0x04,
	0x3421, 0x09,
	0x3422, 0x20,
	0x3423, 0x15,
	0x3424, 0x40,
	0x3425, 0x14,
	0x3426, 0x04,
	0x3504, 0x08,
	0x3508, 0x01,
	0x3509, 0x00,
	0x350a, 0x01,
	0x350b, 0x00,
	0x350c, 0x00,
	0x3548, 0x01,
	0x3549, 0x00,
	0x354a, 0x01,
	0x354b, 0x00,
	0x354c, 0x00,
	0x3600, 0xff,
	0x3602, 0x42,
	0x3603, 0x7b,
	0x3608, 0x9b,
	0x360a, 0x69,
	0x360b, 0x53,
	0x3618, 0xc0,
	0x361a, 0x8b,
	0x361d, 0x20,
	0x361e, 0x30,
	0x361f, 0x01,
	0x3620, 0x89,
	0x3624, 0x8f,
	0x3629, 0x09,
	0x362e, 0x50,
	0x3631, 0xe2,
	0x3632, 0xe2,
	0x3634, 0x10,
	0x3635, 0x10,
	0x3636, 0x10,
	0x3639, 0xa6,
	0x363a, 0xaa,
	0x363b, 0x0c,
	0x363c, 0x16,
	0x363d, 0x29,
	0x363e, 0x4f,
	0x3642, 0xa8,
	0x3652, 0x00,
	0x3653, 0x00,
	0x3654, 0x8a,
	0x3656, 0x0c,
	0x3657, 0x8e,
	0x3660, 0x80,
	0x3663, 0x00,
	0x3664, 0x00,
	0x3668, 0x05,
	0x3669, 0x05,
	0x370d, 0x10,
	0x370e, 0x05,
	0x370f, 0x10,
	0x3711, 0x01,
	0x3712, 0x09,
	0x3713, 0x40,
	0x3714, 0xe4,
	0x3716, 0x04,
	0x3717, 0x01,
	0x3718, 0x02,
	0x3719, 0x01,
	0x371a, 0x02,
	0x371b, 0x02,
	0x371c, 0x01,
	0x371d, 0x02,
	0x371e, 0x12,
	0x371f, 0x02,
	0x3720, 0x14,
	0x3721, 0x12,
	0x3722, 0x44,
	0x3723, 0x60,
	0x372f, 0x34,
	0x3726, 0x21,
	0x37d0, 0x02,
	0x37d1, 0x10,
	0x37db, 0x08,
	0x3808, 0x12,
	0x3809, 0x30,
	0x380a, 0x0d,
	0x380b, 0xa8,
	0x380c, 0x03,
	0x380d, 0x52,
	0x380e, 0x0f,
	0x380f, 0x51,
	0x3814, 0x11,
	0x3815, 0x11,
	0x3820, 0x00,
	0x3821, 0x06,
	0x3822, 0x00,
	0x3823, 0x04,
	0x3837, 0x10,
	0x383c, 0x34,
	0x383d, 0xff,
	0x383e, 0x0d,
	0x383f, 0x22,
	0x3857, 0x00,
	0x388f, 0x00,
	0x3890, 0x00,
	0x3891, 0x00,
	0x3d81, 0x10,
	0x3d83, 0x0c,
	0x3d84, 0x00,
	0x3d85, 0x1b,
	0x3d88, 0x00,
	0x3d89, 0x00,
	0x3d8a, 0x00,
	0x3d8b, 0x01,
	0x3d8c, 0x77,
	0x3d8d, 0xa0,
	0x3f00, 0x02,
	0x3f0c, 0x07,
	0x3f0d, 0x2f,
	0x4012, 0x0d,
	0x4015, 0x04,
	0x4016, 0x1b,
	0x4017, 0x04,
	0x4018, 0x0b,
	0x401b, 0x1f,
	0x401e, 0x01,
	0x401f, 0x38,
	0x4500, 0x20,
	0x4501, 0x6a,
	0x4502, 0xb4,
	0x4586, 0x00,
	0x4588, 0x02,
	0x4640, 0x01,
	0x4641, 0x04,
	0x4643, 0x00,
	0x4645, 0x03,
	0x4806, 0x40,
	0x480e, 0x00,
	0x4815, 0x2b,
	0x481b, 0x3c,
	0x4833, 0x18,
	0x4837, 0x08,
	0x484b, 0x07,
	0x4850, 0x41,
	0x4860, 0x00,
	0x4861, 0xec,
	0x4864, 0x00,
	0x4883, 0x00,
	0x4888, 0x10,
	0x4a00, 0x10,
	0x4e00, 0x00,
	0x4e01, 0x04,
	0x4e02, 0x01,
	0x4e03, 0x00,
	0x4e04, 0x08,
	0x4e05, 0x04,
	0x4e06, 0x00,
	0x4e07, 0x13,
	0x4e08, 0x01,
	0x4e09, 0x00,
	0x4e0a, 0x15,
	0x4e0b, 0x0e,
	0x4e0c, 0x00,
	0x4e0d, 0x17,
	0x4e0e, 0x07,
	0x4e0f, 0x00,
	0x4e10, 0x19,
	0x4e11, 0x06,
	0x4e12, 0x00,
	0x4e13, 0x1b,
	0x4e14, 0x08,
	0x4e15, 0x00,
	0x4e16, 0x1f,
	0x4e17, 0x08,
	0x4e18, 0x00,
	0x4e19, 0x21,
	0x4e1a, 0x0e,
	0x4e1b, 0x00,
	0x4e1c, 0x2d,
	0x4e1d, 0x30,
	0x4e1e, 0x00,
	0x4e1f, 0x6a,
	0x4e20, 0x05,
	0x4e21, 0x00,
	0x4e22, 0x6c,
	0x4e23, 0x05,
	0x4e24, 0x00,
	0x4e25, 0x6e,
	0x4e26, 0x39,
	0x4e27, 0x00,
	0x4e28, 0x7a,
	0x4e29, 0x6d,
	0x4e2a, 0x00,
	0x4e2b, 0x00,
	0x4e2c, 0x00,
	0x4e2d, 0x00,
	0x4e2e, 0x00,
	0x4e2f, 0x00,
	0x4e30, 0x00,
	0x4e31, 0x00,
	0x4e32, 0x00,
	0x4e33, 0x00,
	0x4e34, 0x00,
	0x4e35, 0x00,
	0x4e36, 0x00,
	0x4e37, 0x00,
	0x4e38, 0x00,
	0x4e39, 0x00,
	0x4e3a, 0x00,
	0x4e3b, 0x00,
	0x4e3c, 0x00,
	0x4e3d, 0x00,
	0x4e3e, 0x00,
	0x4e3f, 0x00,
	0x4e40, 0x00,
	0x4e41, 0x00,
	0x4e42, 0x00,
	0x4e43, 0x00,
	0x4e44, 0x00,
	0x4e45, 0x00,
	0x4e46, 0x00,
	0x4e47, 0x00,
	0x4e48, 0x00,
	0x4e49, 0x00,
	0x4e4a, 0x00,
	0x4e4b, 0x00,
	0x4e4c, 0x00,
	0x4e4d, 0x00,
	0x4e4e, 0x00,
	0x4e4f, 0x00,
	0x4e50, 0x00,
	0x4e51, 0x00,
	0x4e52, 0x00,
	0x4e53, 0x00,
	0x4e54, 0x00,
	0x4e55, 0x00,
	0x4e56, 0x00,
	0x4e57, 0x00,
	0x4e58, 0x00,
	0x4e59, 0x00,
	0x4e5a, 0x00,
	0x4e5b, 0x00,
	0x4e5c, 0x00,
	0x4e5d, 0x00,
	0x4e5e, 0x00,
	0x4e5f, 0x00,
	0x4e60, 0x00,
	0x4e61, 0x00,
	0x4e62, 0x00,
	0x4e63, 0x00,
	0x4e64, 0x00,
	0x4e65, 0x00,
	0x4e66, 0x00,
	0x4e67, 0x00,
	0x4e68, 0x00,
	0x4e69, 0x00,
	0x4e6a, 0x00,
	0x4e6b, 0x00,
	0x4e6c, 0x00,
	0x4e6d, 0x00,
	0x4e6e, 0x00,
	0x4e6f, 0x00,
	0x4e70, 0x00,
	0x4e71, 0x00,
	0x4e72, 0x00,
	0x4e73, 0x00,
	0x4e74, 0x00,
	0x4e75, 0x00,
	0x4e76, 0x00,
	0x4e77, 0x00,
	0x4e78, 0x1c,
	0x4e79, 0x1e,
	0x4e7a, 0x00,
	0x4e7b, 0x00,
	0x4e7c, 0x2c,
	0x4e7d, 0x2f,
	0x4e7e, 0x79,
	0x4e7f, 0x7b,
	0x4e80, 0x0a,
	0x4e81, 0x31,
	0x4e82, 0x66,
	0x4e83, 0x81,
	0x4e84, 0x03,
	0x4e85, 0x40,
	0x4e86, 0x02,
	0x4e87, 0x09,
	0x4e88, 0x43,
	0x4e89, 0x53,
	0x4e8a, 0x32,
	0x4e8b, 0x67,
	0x4e8c, 0x05,
	0x4e8d, 0x83,
	0x4e8e, 0x00,
	0x4e8f, 0x00,
	0x4e90, 0x00,
	0x4e91, 0x00,
	0x4e92, 0x00,
	0x4e93, 0x00,
	0x4e94, 0x00,
	0x4e95, 0x00,
	0x4e96, 0x00,
	0x4e97, 0x00,
	0x4e98, 0x00,
	0x4e99, 0x00,
	0x4e9a, 0x00,
	0x4e9b, 0x00,
	0x4e9c, 0x00,
	0x4e9d, 0x00,
	0x4e9e, 0x00,
	0x4e9f, 0x00,
	0x4ea0, 0x00,
	0x4ea1, 0x00,
	0x4ea2, 0x00,
	0x4ea3, 0x00,
	0x4ea4, 0x00,
	0x4ea5, 0x00,
	0x4ea6, 0x1e,
	0x4ea7, 0x20,
	0x4ea8, 0x32,
	0x4ea9, 0x6d,
	0x4eaa, 0x18,
	0x4eab, 0x7f,
	0x4eac, 0x00,
	0x4ead, 0x00,
	0x4eae, 0x7c,
	0x4eaf, 0x07,
	0x4eb0, 0x7c,
	0x4eb1, 0x07,
	0x4eb2, 0x07,
	0x4eb3, 0x1c,
	0x4eb4, 0x07,
	0x4eb5, 0x1c,
	0x4eb6, 0x07,
	0x4eb7, 0x1c,
	0x4eb8, 0x07,
	0x4eb9, 0x1c,
	0x4eba, 0x07,
	0x4ebb, 0x14,
	0x4ebc, 0x07,
	0x4ebd, 0x1c,
	0x4ebe, 0x07,
	0x4ebf, 0x1c,
	0x4ec0, 0x07,
	0x4ec1, 0x1c,
	0x4ec2, 0x07,
	0x4ec3, 0x1c,
	0x4ec4, 0x2c,
	0x4ec5, 0x2f,
	0x4ec6, 0x79,
	0x4ec7, 0x7b,
	0x4ec8, 0x7c,
	0x4ec9, 0x07,
	0x4eca, 0x7c,
	0x4ecb, 0x07,
	0x4ecc, 0x00,
	0x4ecd, 0x00,
	0x4ece, 0x07,
	0x4ecf, 0x31,
	0x4ed0, 0x69,
	0x4ed1, 0x7f,
	0x4ed2, 0x67,
	0x4ed3, 0x00,
	0x4ed4, 0x00,
	0x4ed5, 0x00,
	0x4ed6, 0x7c,
	0x4ed7, 0x07,
	0x4ed8, 0x7c,
	0x4ed9, 0x07,
	0x4eda, 0x33,
	0x4edb, 0x7f,
	0x4edc, 0x00,
	0x4edd, 0x16,
	0x4ede, 0x00,
	0x4edf, 0x00,
	0x4ee0, 0x32,
	0x4ee1, 0x70,
	0x4ee2, 0x01,
	0x4ee3, 0x30,
	0x4ee4, 0x22,
	0x4ee5, 0x28,
	0x4ee6, 0x6f,
	0x4ee7, 0x75,
	0x4ee8, 0x00,
	0x4ee9, 0x00,
	0x4eea, 0x30,
	0x4eeb, 0x7f,
	0x4eec, 0x00,
	0x4eed, 0x00,
	0x4eee, 0x00,
	0x4eef, 0x00,
	0x4ef0, 0x69,
	0x4ef1, 0x7f,
	0x4ef2, 0x07,
	0x4ef3, 0x30,
	0x4ef4, 0x32,
	0x4ef5, 0x09,
	0x4ef6, 0x7d,
	0x4ef7, 0x65,
	0x4ef8, 0x00,
	0x4ef9, 0x00,
	0x4efa, 0x00,
	0x4efb, 0x00,
	0x4efc, 0x7f,
	0x4efd, 0x09,
	0x4efe, 0x7f,
	0x4eff, 0x09,
	0x4f00, 0x1e,
	0x4f01, 0x7c,
	0x4f02, 0x7f,
	0x4f03, 0x09,
	0x4f04, 0x7f,
	0x4f05, 0x0b,
	0x4f06, 0x7c,
	0x4f07, 0x02,
	0x4f08, 0x7c,
	0x4f09, 0x02,
	0x4f0a, 0x32,
	0x4f0b, 0x64,
	0x4f0c, 0x32,
	0x4f0d, 0x64,
	0x4f0e, 0x32,
	0x4f0f, 0x64,
	0x4f10, 0x32,
	0x4f11, 0x64,
	0x4f12, 0x31,
	0x4f13, 0x4f,
	0x4f14, 0x83,
	0x4f15, 0x84,
	0x4f16, 0x63,
	0x4f17, 0x64,
	0x4f18, 0x83,
	0x4f19, 0x84,
	0x4f1a, 0x31,
	0x4f1b, 0x32,
	0x4f1c, 0x7b,
	0x4f1d, 0x7c,
	0x4f1e, 0x2f,
	0x4f1f, 0x30,
	0x4f20, 0x30,
	0x4f21, 0x69,
	0x4d06, 0x08,
	0x5000, 0x01,
	0x5001, 0x40,
	0x5002, 0x53,
	0x5003, 0x42,
	0x5005, 0x00,
	0x5038, 0x00,
	0x5081, 0x00,
	0x5180, 0x00,
	0x5181, 0x10,
	0x5182, 0x07,
	0x5183, 0x8f,
	0x5820, 0xc5,
	0x5854, 0x00,
	0x58cb, 0x03,
	0x5bd0, 0x15,
	0x5bd1, 0x02,
	0x5c0e, 0x11,
	0x5c11, 0x00,
	0x5c16, 0x02,
	0x5c17, 0x01,
	0x5c1a, 0x04,
	0x5c1b, 0x03,
	0x5c21, 0x10,
	0x5c22, 0x10,
	0x5c23, 0x04,
	0x5c24, 0x0c,
	0x5c25, 0x04,
	0x5c26, 0x0c,
	0x5c27, 0x04,
	0x5c28, 0x0c,
	0x5c29, 0x04,
	0x5c2a, 0x0c,
	0x5c2b, 0x01,
	0x5c2c, 0x01,
	0x5c2e, 0x08,
	0x5c30, 0x04,
	0x5c35, 0x03,
	0x5c36, 0x03,
	0x5c37, 0x03,
	0x5c38, 0x03,
	0x5d00, 0xff,
	0x5d01, 0x0f,
	0x5d02, 0x80,
	0x5d03, 0x44,
	0x5d05, 0xfc,
	0x5d06, 0x0b,
	0x5d08, 0x10,
	0x5d09, 0x10,
	0x5d0a, 0x04,
	0x5d0b, 0x0c,
	0x5d0c, 0x04,
	0x5d0d, 0x0c,
	0x5d0e, 0x04,
	0x5d0f, 0x0c,
	0x5d10, 0x04,
	0x5d11, 0x0c,
	0x5d12, 0x01,
	0x5d13, 0x01,
	0x5d15, 0x10,
	0x5d16, 0x10,
	0x5d17, 0x10,
	0x5d18, 0x10,
	0x5d1a, 0x10,
	0x5d1b, 0x10,
	0x5d1c, 0x10,
	0x5d1d, 0x10,
	0x5d1e, 0x04,
	0x5d1f, 0x04,
	0x5d20, 0x04,
	0x5d27, 0x64,
	0x5d28, 0xc8,
	0x5d29, 0x96,
	0x5d2a, 0xff,
	0x5d2b, 0xc8,
	0x5d2c, 0xff,
	0x5d2d, 0x04,
	0x5d34, 0x00,
	0x5d35, 0x08,
	0x5d36, 0x00,
	0x5d37, 0x04,
	0x5d4a, 0x00,
	0x5d4c, 0x00,
};

static void sensor_init(void)
{
	LOG_INF("v3 E\n");
#if 1	/* MULTI_WRITE */
	ov16a1q_table_write_cmos_sensor(addr_data_pair_init_ov16a1q,
			   sizeof(addr_data_pair_init_ov16a1q) / sizeof(kal_uint16));
#endif

#if SET_STREAMING_TEST
		//write_cmos_sensor(0x0100, 0x01);
#endif
}	/*	sensor_init  */

/*************************************************************************
* FUNCTION
*	preview_setting
*
* DESCRIPTION
*	Sensor preview
*
* PARAMETERS
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
kal_uint16 addr_data_pair_preview_ov16a1q[] = {
	0x0305,0x7a,
	0x0307,0x01,
	0x4837,0x15,
	0x0329,0x01,
	0x0344,0x01,
	0x0345,0x2c,
	0x034a,0x07,
	0x3608,0x75,
	0x360a,0x69,
	0x361a,0x8b,
	0x361e,0x30,
	0x3639,0xa6,
	0x363a,0xaa,
	0x3642,0xa8,
	0x3654,0x8a,
	0x3656,0x0c,
	0x3663,0x01,
	0x370e,0xa5,
	0x3712,0x59,
	0x3713,0x40,
	0x3714,0xe4,
	0x37d0,0x02,
	0x37d1,0x10,
	0x37db,0x04,
	0x3808,0x09,
	0x3809,0x18,
	0x380a,0x06,
	0x380b,0xd4,/*improve the normal luminance differences new add for set back to default*/
	0x380c,0x06, //0x03,/*Zhen.Quan@Camera.Driver, 2019/11/14, modify setting for oppo engineer preview*/
	0x380d,0xa4, //0x52,
	0x380e,0x07, //0x0f,
	0x380f,0xa8, //0x50,
	0x3814,0x11,
	0x3815,0x22,
	0x3820,0x01,
	0x3821,0x06,
	0x3822,0x01,
	0x383c,0x22,
	0x383f,0x33,
	0x4015,0x02,
	0x4016,0x0d,
	0x4017,0x00,
	0x4018,0x07,
	0x401b,0x1f,
	0x401f,0x38,
	0x4500,0x20,
	0x4501,0x6f,
	0x4502,0xe4,
	0x4e05,0x04,
	0x4e11,0x06,
	0x4e1d,0x30,
	0x4e26,0x39,
	0x4e29,0x6d,
	0x5000,0x29,
	0x5001,0xc2,
	0x5003,0xc2,
	0x5820,0xc0,
	0x5854,0x01,
	0x5bd0,0x19,
	0x5c0e,0x11,
	0x5c11,0x01,
	0x5c16,0x02,
	0x5c17,0x00,
	0x5c1a,0x00,
	0x5c1b,0x00,
	0x5c21,0x10,
	0x5c22,0x08,
	0x5c23,0x04,
	0x5c24,0x0c,
	0x5c25,0x04,
	0x5c26,0x0c,
	0x5c27,0x02,
	0x5c28,0x06,
	0x5c29,0x02,
	0x5c2a,0x06,
	0x5c2b,0x01,
	0x5c2c,0x00,
	0x5d01,0x07,
	0x5d08,0x08,
	0x5d09,0x08,
	0x5d0a,0x02,
	0x5d0b,0x06,
	0x5d0c,0x02,
	0x5d0d,0x06,
	0x5d0e,0x02,
	0x5d0f,0x06,
	0x5d10,0x02,
	0x5d11,0x06,
	0x5d12,0x00,
	0x5d13,0x00,
	0x3500,0x00,
	0x3501,0x0f,
	0x3502,0x48,
	0x3508,0x01,
	0x3509,0x00,
};
static void preview_setting(void)
{
	LOG_INF("E\n");
#if SET_STREAMING_TEST
		//write_cmos_sensor(0x0100, 0x00);
#endif
	ov16a1q_table_write_cmos_sensor(addr_data_pair_preview_ov16a1q,
				   sizeof(addr_data_pair_preview_ov16a1q) / sizeof(kal_uint16));

#if SET_STREAMING_TEST
		//write_cmos_sensor(0x0100, 0x01);
#endif
}	/*	preview_setting  */
/*************************************************************************
* FUNCTION
*	Capture
*
* DESCRIPTION
*	Sensor capture
*
* PARAMETERS
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
kal_uint16 addr_data_pair_capture_30fps_ov16a1q[] = {
	0x0305,0x7a,
	0x0307,0x01,
	0x4837,0x15,
	0x0329,0x01,
	0x0344,0x01,
	0x0345,0x2c,
	0x034a,0x07,
	0x3608,0x75,
	0x360a,0x69,
	0x361a,0x8b,
	0x361e,0x30,
	0x3639,0xa6,
	0x363a,0xaa,
	0x3642,0xa8,
	0x3654,0x8a,
	0x3656,0x0c,
	0x3663,0x01,
	0x370e,0xa5,
	0x3712,0x59,
	0x3713,0x40,
	0x3714,0xe4,
	0x37d0,0x02,
	0x37d1,0x10,
	0x37db,0x04,
	0x3808,0x09,
	0x3809,0x18,
	0x380a,0x06,
	0x380b,0xd4,/*improve the normal luminance differences new add for set back to default*/
	0x380c,0x06, //0x03,
	0x380d,0xa4, //0x52,
	0x380e,0x07, //0x0f,
	0x380f,0xa8, //0x50,
	0x3814,0x11,
	0x3815,0x22,
	0x3820,0x01,
	0x3821,0x06,
	0x3822,0x01,
	0x383c,0x22,
	0x383f,0x33,
	0x4015,0x02,
	0x4016,0x0d,
	0x4017,0x00,
	0x4018,0x07,
	0x401b,0x1f,
	0x401f,0x38,
	0x4500,0x20,
	0x4501,0x6f,
	0x4502,0xe4,
	0x4e05,0x04,
	0x4e11,0x06,
	0x4e1d,0x30,
	0x4e26,0x39,
	0x4e29,0x6d,
	0x5000,0x29,
	0x5001,0xc2,
	0x5003,0xc2,
	0x5820,0xc0,
	0x5854,0x01,
	0x5bd0,0x19,
	0x5c0e,0x11,
	0x5c11,0x01,
	0x5c16,0x02,
	0x5c17,0x00,
	0x5c1a,0x00,
	0x5c1b,0x00,
	0x5c21,0x10,
	0x5c22,0x08,
	0x5c23,0x04,
	0x5c24,0x0c,
	0x5c25,0x04,
	0x5c26,0x0c,
	0x5c27,0x02,
	0x5c28,0x06,
	0x5c29,0x02,
	0x5c2a,0x06,
	0x5c2b,0x01,
	0x5c2c,0x00,
	0x5d01,0x07,
	0x5d08,0x08,
	0x5d09,0x08,
	0x5d0a,0x02,
	0x5d0b,0x06,
	0x5d0c,0x02,
	0x5d0d,0x06,
	0x5d0e,0x02,
	0x5d0f,0x06,
	0x5d10,0x02,
	0x5d11,0x06,
	0x5d12,0x00,
	0x5d13,0x00,
	0x3500,0x00,
	0x3501,0x0f,
	0x3502,0x48,
	0x3508,0x01,
	0x3509,0x00,
};

kal_uint16 addr_data_pair_capture_15fps_ov16a1q[] = {
	0x0305,0x7a,
	0x0307,0x01,
	0x4837,0x15,
	0x0329,0x01,
	0x0344,0x01,
	0x0345,0x2c,
	0x034a,0x07,
	0x3608,0x75,
	0x360a,0x69,
	0x361a,0x8b,
	0x361e,0x30,
	0x3639,0xa6,
	0x363a,0xaa,
	0x3642,0xa8,
	0x3654,0x8a,
	0x3656,0x0c,
	0x3663,0x01,
	0x370e,0xa5,
	0x3712,0x59,
	0x3713,0x40,
	0x3714,0xe4,
	0x37d0,0x02,
	0x37d1,0x10,
	0x37db,0x04,
	0x3808,0x09,
	0x3809,0x18,
	0x380a,0x06,
	0x380b,0xd4,/*improve the normal luminance differences new add for set back to default*/
	0x380c,0x06, //0x03,
	0x380d,0xa4, //0x52,
	0x380e,0x0f, //0x0f,
	0x380f,0x50, //0x50,
	0x3814,0x11,
	0x3815,0x22,
	0x3820,0x01,
	0x3821,0x06,
	0x3822,0x01,
	0x383c,0x22,
	0x383f,0x33,
	0x4015,0x02,
	0x4016,0x0d,
	0x4017,0x00,
	0x4018,0x07,
	0x401b,0x1f,
	0x401f,0x38,
	0x4500,0x20,
	0x4501,0x6f,
	0x4502,0xe4,
	0x4e05,0x04,
	0x4e11,0x06,
	0x4e1d,0x30,
	0x4e26,0x39,
	0x4e29,0x6d,
	0x5000,0x29,
	0x5001,0xc2,
	0x5003,0xc2,
	0x5820,0xc0,
	0x5854,0x01,
	0x5bd0,0x19,
	0x5c0e,0x11,
	0x5c11,0x01,
	0x5c16,0x02,
	0x5c17,0x00,
	0x5c1a,0x00,
	0x5c1b,0x00,
	0x5c21,0x10,
	0x5c22,0x08,
	0x5c23,0x04,
	0x5c24,0x0c,
	0x5c25,0x04,
	0x5c26,0x0c,
	0x5c27,0x02,
	0x5c28,0x06,
	0x5c29,0x02,
	0x5c2a,0x06,
	0x5c2b,0x01,
	0x5c2c,0x00,
	0x5d01,0x07,
	0x5d08,0x08,
	0x5d09,0x08,
	0x5d0a,0x02,
	0x5d0b,0x06,
	0x5d0c,0x02,
	0x5d0d,0x06,
	0x5d0e,0x02,
	0x5d0f,0x06,
	0x5d10,0x02,
	0x5d11,0x06,
	0x5d12,0x00,
	0x5d13,0x00,
	0x3500,0x00,
	0x3501,0x0f,
	0x3502,0x48,
	0x3508,0x01,
	0x3509,0x00,
};

static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("E! currefps:%d\n", currefps);
#if SET_STREAMING_TEST
		//write_cmos_sensor(0x0100, 0x00);
#endif
	if (currefps == 150) {
		ov16a1q_table_write_cmos_sensor(addr_data_pair_capture_15fps_ov16a1q,
					       sizeof(addr_data_pair_capture_15fps_ov16a1q) /
					       sizeof(kal_uint16));
	} else {
		ov16a1q_table_write_cmos_sensor(addr_data_pair_capture_30fps_ov16a1q,
					       sizeof(addr_data_pair_capture_30fps_ov16a1q) /
					       sizeof(kal_uint16));
	}
#if SET_STREAMING_TEST
		//write_cmos_sensor(0x0100, 0x01);
#endif
}


/*************************************************************************
* FUNCTION
*	Video
*
* DESCRIPTION
*	Sensor video
*
* PARAMETERS
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/


kal_uint16 addr_data_pair_vga_setting_120fps_ov16a1q[] = {
	0x0305,0x7a,
	0x0307,0x01,
	0x4837,0x15,
	0x0329,0x01,
	0x0344,0x01,
	0x0345,0x2c,
	0x034a,0x07,
	0x3608,0x75,
	0x360a,0x69,
	0x361a,0x8b,
	0x361e,0x30,
	0x3639,0xa6,
	0x363a,0xaa,
	0x3642,0xa8,
	0x3654,0x8a,
	0x3656,0x0c,
	0x3663,0x01,
	0x370e,0xa5,
	0x3712,0x59,
	0x3713,0x40,
	0x3714,0xe4,
	0x37d0,0x02,
	0x37d1,0x10,
	0x37db,0x04,
	0x3808,0x09,
	0x3809,0x18,
	0x380a,0x06,
	0x380b,0xd4,/*improve the normal luminance differences new add for set back to default*/
	0x380c,0x06, //0x03,/*Zhen.Quan@Camera.Driver, 2019/11/14, modify setting for oppo engineer preview*/
	0x380d,0xa4, //0x52,
	0x380e,0x07, //0x0f,
	0x380f,0xa8, //0x50,
	0x3814,0x11,
	0x3815,0x22,
	0x3820,0x01,
	0x3821,0x06,
	0x3822,0x01,
	0x383c,0x22,
	0x383f,0x33,
	0x4015,0x02,
	0x4016,0x0d,
	0x4017,0x00,
	0x4018,0x07,
	0x401b,0x1f,
	0x401f,0x38,
	0x4500,0x20,
	0x4501,0x6f,
	0x4502,0xe4,
	0x4e05,0x04,
	0x4e11,0x06,
	0x4e1d,0x30,
	0x4e26,0x39,
	0x4e29,0x6d,
	0x5000,0x29,
	0x5001,0xc2,
	0x5003,0xc2,
	0x5820,0xc0,
	0x5854,0x01,
	0x5bd0,0x19,
	0x5c0e,0x11,
	0x5c11,0x01,
	0x5c16,0x02,
	0x5c17,0x00,
	0x5c1a,0x00,
	0x5c1b,0x00,
	0x5c21,0x10,
	0x5c22,0x08,
	0x5c23,0x04,
	0x5c24,0x0c,
	0x5c25,0x04,
	0x5c26,0x0c,
	0x5c27,0x02,
	0x5c28,0x06,
	0x5c29,0x02,
	0x5c2a,0x06,
	0x5c2b,0x01,
	0x5c2c,0x00,
	0x5d01,0x07,
	0x5d08,0x08,
	0x5d09,0x08,
	0x5d0a,0x02,
	0x5d0b,0x06,
	0x5d0c,0x02,
	0x5d0d,0x06,
	0x5d0e,0x02,
	0x5d0f,0x06,
	0x5d10,0x02,
	0x5d11,0x06,
	0x5d12,0x00,
	0x5d13,0x00,
	0x3500,0x00,
	0x3501,0x0f,
	0x3502,0x48,
	0x3508,0x01,
	0x3509,0x00,
};

static void vga_setting_120fps(void)
{
#if SET_STREAMING_TEST
		write_cmos_sensor(0x0100, 0x01);
#endif

	ov16a1q_table_write_cmos_sensor(addr_data_pair_vga_setting_120fps_ov16a1q,
				   sizeof(addr_data_pair_vga_setting_120fps_ov16a1q) /
				   sizeof(kal_uint16));

#if SET_STREAMING_TEST
		write_cmos_sensor(0x0100, 0x01);
#endif
}

static void hs_video_setting(void)
{
	LOG_INF("E\n");
	vga_setting_120fps();
}

static void slim_video_setting(void)
{
	LOG_INF("E\n");
	preview_setting();
}

/* Tian.Tian@Camera.Driver, 2019/11/10, add for remosaic start */
kal_uint16 addr_data_pair_capture_custom3_ov16a1q[] = {
	0x0305, 0x6b,
	0x0307, 0x00,
	0x4837, 0x0b,
	0x0329, 0x01,
	0x0344, 0x01,
	0x0345, 0x2c,
	0x034a, 0x07,
	0x3608, 0x9b,
	0x360a, 0x69,
	0x361a, 0x8b,
	0x361e, 0x30,
	0x3639, 0xa6,
	0x363a, 0xaa,
	0x3642, 0xa8,
	0x3654, 0x8a,
	0x3656, 0x0c,
	0x3663, 0x00,
	0x370e, 0x05,
	0x3712, 0x09,
	0x3713, 0x40,
	0x3714, 0xe4,
	0x37d0, 0x02,
	0x37d1, 0x10,
	0x37db, 0x08,
	0x3808, 0x12,
	0x3809, 0x30,
	0x380a, 0x0d,
	0x380b, 0xa8,
	0x380c, 0x03,
	0x380d, 0x52,
	0x380e, 0x0f,
	0x380f, 0x50,
	0x3814, 0x11,
	0x3815, 0x11,
	0x3820, 0x00,
	0x3821, 0x06,
	0x3822, 0x00,
	0x383c, 0x34,
	0x383f, 0x22,
	0x4015, 0x04,
	0x4016, 0x1b,
	0x4017, 0x04,
	0x4018, 0x0b,
	0x401b, 0x1f,
	0x401f, 0x38,
	0x4500, 0x20,
	0x4501, 0x6a,
	0x4502, 0xb4,
	0x4e05, 0x04,
	0x4e11, 0x06,
	0x4e1d, 0x30,
	0x4e26, 0x39,
	0x4e29, 0x6d,
	0x5000, 0x01,
	0x5001, 0x40,
	0x5003, 0x42,
	0x5820, 0xc5,
	0x5854, 0x00,
	0x5bd0, 0x15,
	0x5c0e, 0x11,
	0x5c11, 0x00,
	0x5c16, 0x02,
	0x5c17, 0x01,
	0x5c1a, 0x04,
	0x5c1b, 0x03,
	0x5c21, 0x10,
	0x5c22, 0x10,
	0x5c23, 0x04,
	0x5c24, 0x0c,
	0x5c25, 0x04,
	0x5c26, 0x0c,
	0x5c27, 0x04,
	0x5c28, 0x0c,
	0x5c29, 0x04,
	0x5c2a, 0x0c,
	0x5c2b, 0x01,
	0x5c2c, 0x01,
	0x5d01, 0x0f,
	0x5d08, 0x10,
	0x5d09, 0x10,
	0x5d0a, 0x04,
	0x5d0b, 0x0c,
	0x5d0c, 0x04,
	0x5d0d, 0x0c,
	0x5d0e, 0x04,
	0x5d0f, 0x0c,
	0x5d10, 0x04,
	0x5d11, 0x0c,
	0x5d12, 0x01,
	0x5d13, 0x01,
	0x3500, 0x00,
	0x3501, 0x0f,
	0x3502, 0x48,
	0x3508, 0x01,
    0x3509, 0x00,
};

static void custom3_setting(void)
{
#if SET_STREAMING_TEST
	write_cmos_sensor(0x0100, 0x01);
#endif
	LOG_INF("E\n");
	ov16a1q_table_write_cmos_sensor(addr_data_pair_capture_custom3_ov16a1q,
				   sizeof(addr_data_pair_capture_custom3_ov16a1q) /
				   sizeof(kal_uint16));
	LOG_INF("X\n");
#if SET_STREAMING_TEST
	write_cmos_sensor(0x0100, 0x01);
#endif
}
/* Tian.Tian@Camera.Driver, 2019/11/10, add for remosaic end */

/*************************************************************************
* FUNCTION
*	get_imgsensor_id
*
* DESCRIPTION
*	This function get the sensor ID
*
* PARAMETERS
*	*sensorID : return the sensor ID
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	/*sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address*/
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = ((read_cmos_sensor(0x300B) << 8) | read_cmos_sensor(0x300C));
			if (*sensor_id == imgsensor_info.sensor_id) {
				/* Zhen.Quan@Camera.Driver, 2019/10/17, add for [otp bringup] */
#if ENABLE_OV16A1Q_OTP
				if(!check_otp_data(&monetx_ofilm_front_ov16a1q_eeprom_data, monetx_ofilm_front_ov16a1q_checksum, sensor_id)){
					break;
				} else {
					/*xiaojun.Pu@Camera.Driver, 2019/10/18, add for [add hardware_info for factory]*/
					hardwareinfo_set_prop(HARDWARE_FRONT_CAM_MOUDULE_ID, "Ofilm");
				}
#endif
				printk("i2c write id: 0x%x, sensor id: 0x%x, ver = 0x%x<0xB1=r2a,0xB0=r1a>\n",
					imgsensor.i2c_write_id, *sensor_id, read_cmos_sensor(0x302A));
				return ERROR_NONE;
			}
			printk("Read sensor id fail,write_id:0x%x, id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		/* if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF */
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
*	open
*
* DESCRIPTION
*	This function initialize the registers of CMOS sensor
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 open(void)
{
	/*const kal_uint8 i2c_addr[] = {IMGSENSOR_WRITE_ID_1, IMGSENSOR_WRITE_ID_2};*/
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint16 sensor_id = 0;

	LOG_INF("PLATFORM:Vison,MIPI 4LANE\n");
	LOG_INF("read_cmos_sensor(0x302A): 0x%x\n", read_cmos_sensor(0x302A));
	/*sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address*/
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = ((read_cmos_sensor(0x300B) << 8) | read_cmos_sensor(0x300C));
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
				break;
			}
			LOG_INF("Read sensor id fail, write: 0x%x, sensor: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}
	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;
	/* initail sequence write in  */
	sensor_init();
	//mdelay(10);
	#ifdef OV16A1QR1AOTP
	/*	LOG_INF("Apply the sensor OTP\n");
	 *	struct otp_struct *otp_ptr = (struct otp_struct *)kzalloc(sizeof(struct otp_struct), GFP_KERNEL);
	 *	read_otp(otp_ptr);
	 *	apply_otp(otp_ptr);
	 *	kfree(otp_ptr);
	 */
	#endif
	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.shutter = 0x2D00;
	imgsensor.gain = 0x100;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = 0;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}	/*	open  */



/*************************************************************************
* FUNCTION
*	close
*
* DESCRIPTION
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 close(void)
{
	LOG_INF("E\n");

	/*No Need to implement this function*/
	return ERROR_NONE;
}	/*	close  */


/*************************************************************************
* FUNCTION
* preview
*
* DESCRIPTION
*	This function start the sensor preview.
*
* PARAMETERS
*	*image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	/*imgsensor.video_mode = KAL_FALSE;*/
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	//set_mirror_flip(imgsensor.mirror);
	//mdelay(10);
	return ERROR_NONE;
}	/*	preview   */

/*************************************************************************
* FUNCTION
*	capture
*
* DESCRIPTION
*	This function setup the CMOS sensor in capture MY_OUTPUT mode
*
* PARAMETERS
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
						  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {/*15fps*/
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_INF("Warning: current_fps %d fps is not support, so use cap1's setting: %d fps!\n",
				imgsensor.current_fps, imgsensor_info.cap1.max_framerate/10);
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps);
	//mdelay(10);

	if (imgsensor.test_pattern == KAL_TRUE) {
		/*write_cmos_sensor(0x5002,0x00);*/
		write_cmos_sensor(0x5000, (read_cmos_sensor(0x5000)&0xBF)|0x00);
	}
	//set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}	/* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	/*imgsensor.current_fps = 300;*/
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	capture_setting(imgsensor.current_fps);
	//set_mirror_flip(imgsensor.mirror);
	//mdelay(10);
	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	/*imgsensor.video_mode = KAL_TRUE;*/
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/*imgsensor.current_fps = 300;*/
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	//set_mirror_flip(imgsensor.mirror);
	//mdelay(10);

	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	/*imgsensor.video_mode = KAL_TRUE;*/
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/*imgsensor.current_fps = 300;*/
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	//set_mirror_flip(imgsensor.mirror);
	//mdelay(10);

	return ERROR_NONE;
}	/*	slim_video	 */

/* Tian.Tian@Camera.Driver, 2019/11/10, add for remosaic start */
static kal_uint32 custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
						  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM3;
	imgsensor.pclk = imgsensor_info.custom3.pclk;
	imgsensor.line_length = imgsensor_info.custom3.linelength;
	imgsensor.frame_length = imgsensor_info.custom3.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	custom3_setting();
	//mdelay(10);

	if (imgsensor.test_pattern == KAL_TRUE) {
		/*write_cmos_sensor(0x5002,0x00);*/
		write_cmos_sensor(0x5000, (read_cmos_sensor(0x5000)&0xBF)|0x00);
	}

	//set_mirror_flip(imgsensor.mirror);
	LOG_INF("X\n");
	return ERROR_NONE;
}	/* custom3() */
/* Tian.Tian@Camera.Driver, 2019/11/10, add for remosaic end */

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	LOG_INF("E\n");
	sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;
	sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;
	sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;
	sensor_resolution->SensorHighSpeedVideoWidth	 = imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight	 = imgsensor_info.hs_video.grabwindow_height;
	sensor_resolution->SensorSlimVideoWidth	 = imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight	 = imgsensor_info.slim_video.grabwindow_height;
	/* Tian.Tian@Camera.Driver, 2019/11/10, add for remosaic start */
	sensor_resolution->SensorCustom3Width = imgsensor_info.custom3.grabwindow_width;
	sensor_resolution->SensorCustom3Height = imgsensor_info.custom3.grabwindow_height;
	/* Tian.Tian@Camera.Driver, 2019/11/10, add for remosaic end */
	return ERROR_NONE;
}	/*	get_resolution	*/

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
					  MSDK_SENSOR_INFO_STRUCT *sensor_info,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; /* inverse with datasheet*/
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */
	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;
	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;
	/* Tian.Tian@Camera.Driver, 2019/11/10, add for remosaic */
	sensor_info->Custom3DelayFrame = imgsensor_info.custom3_delay_frame;
	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */
	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;  /* 0 is default 1x*/
	sensor_info->SensorHightSampling = 0;	/* 0 is default 1x */
	sensor_info->SensorPacketECCOrder = 1;

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
				imgsensor_info.pre.mipi_data_lp2hs_settle_dc;

			break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
				imgsensor_info.cap.mipi_data_lp2hs_settle_dc;

			break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
				imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;

			break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
				imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;

			break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
			sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
				imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;
			break;
	/* Tian.Tian@Camera.Driver, 2019/11/10, add for remosaic start */
	case MSDK_SCENARIO_ID_CUSTOM3:
			sensor_info->SensorGrabStartX = imgsensor_info.custom3.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.custom3.starty;
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
				imgsensor_info.custom3.mipi_data_lp2hs_settle_dc;
			break;
	/* Tian.Tian@Camera.Driver, 2019/11/10, add for remosaic end */
	default:
			sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
				imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
			break;
	}
	return ERROR_NONE;
}	/*	get_info  */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			preview(image_window, sensor_config_data);
			break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			capture(image_window, sensor_config_data);
			break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			normal_video(image_window, sensor_config_data);
			break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			hs_video(image_window, sensor_config_data);
			break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
			slim_video(image_window, sensor_config_data);
			break;
	/* Tian.Tian@Camera.Driver, 2019/11/10, add for remosaic start */
	case MSDK_SCENARIO_ID_CUSTOM3:
			custom3(image_window, sensor_config_data);
			break;
	/* Tian.Tian@Camera.Driver, 2019/11/10, add for remosaic end */
	default:
			LOG_INF("Error ScenarioId setting");
			preview(image_window, sensor_config_data);
			return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_INF("framerate = %d\n ", framerate);
	/* SetVideoMode Function should fix framerate*/
	if (framerate == 0)
		/* Dynamic frame rate*/
		return ERROR_NONE;
	spin_lock(&imgsensor_drv_lock);
	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 296;
	else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = framerate;
	spin_unlock(&imgsensor_drv_lock);
	set_max_framerate(imgsensor.current_fps, 1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable) /*enable auto flicker	  */
		imgsensor.autoflicker_en = KAL_TRUE;
	else /*Cancel Auto flick*/
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
			spin_lock(&imgsensor_drv_lock);
			if (frame_length > imgsensor_info.pre.framelength)
				imgsensor.dummy_line = (frame_length - imgsensor_info.pre.framelength);
			else
				imgsensor.dummy_line = 0;
			imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			if (framerate == 0)
				return ERROR_NONE;
			frame_length = imgsensor_info.normal_video.pclk / framerate * 10;
			frame_length /= imgsensor_info.normal_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			if (frame_length > imgsensor_info.normal_video.framelength)
				imgsensor.dummy_line = (frame_length - imgsensor_info.normal_video.framelength);
			else
				imgsensor.dummy_line = 0;
			imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			if (frame_length > imgsensor_info.cap.framelength)
				imgsensor.dummy_line = (frame_length - imgsensor_info.cap.framelength);
			else
				imgsensor.dummy_line = 0;
			imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			frame_length = imgsensor_info.hs_video.pclk / framerate * 10;
			frame_length /= imgsensor_info.hs_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			if (frame_length > imgsensor_info.hs_video.framelength)
				imgsensor.dummy_line = (frame_length - imgsensor_info.hs_video.framelength);
			else
				imgsensor.dummy_line = 0;
			imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
			frame_length = imgsensor_info.slim_video.pclk / framerate * 10;
			frame_length /= imgsensor_info.slim_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			if (frame_length > imgsensor_info.slim_video.framelength)
				imgsensor.dummy_line = (frame_length - imgsensor_info.slim_video.framelength);
			else
				imgsensor.dummy_line = 0;
			imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			break;
	/* Tian.Tian@Camera.Driver, 2019/11/10, add for remosaic start */
	case MSDK_SCENARIO_ID_CUSTOM3:
			frame_length = imgsensor_info.custom3.pclk / framerate * 10 / imgsensor_info.custom3.linelength;
			spin_lock(&imgsensor_drv_lock);
			if (frame_length > imgsensor_info.custom3.framelength)
				imgsensor.dummy_line = (frame_length - imgsensor_info.custom3.framelength);
			else
				imgsensor.dummy_line = 0;
			imgsensor.frame_length = imgsensor_info.custom3.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			break;
	/* Tian.Tian@Camera.Driver, 2019/11/10, add for remosaic end */
	default:  /*coding with  preview scenario by default*/
			frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
			spin_lock(&imgsensor_drv_lock);
			if (frame_length > imgsensor_info.pre.framelength)
				imgsensor.dummy_line = (frame_length - imgsensor_info.pre.framelength);
			else
				imgsensor.dummy_line = 0;
			imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			LOG_INF("error scenario_id = %d, we use preview scenario\n", scenario_id);
			break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	LOG_INF("scenario_id = %d\n", scenario_id);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*framerate = imgsensor_info.pre.max_framerate;
			break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*framerate = imgsensor_info.normal_video.max_framerate;
			break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*framerate = imgsensor_info.cap.max_framerate;
			break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*framerate = imgsensor_info.hs_video.max_framerate;
			break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*framerate = imgsensor_info.slim_video.max_framerate;
			break;
	/* Tian.Tian@Camera.Driver, 2019/11/10, add for remosaic start */
	case MSDK_SCENARIO_ID_CUSTOM3:
			*framerate = imgsensor_info.custom3.max_framerate;
			break;
	/* Tian.Tian@Camera.Driver, 2019/11/10, add for remosaic end */
	default:
			break;
	}
	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);

	if (enable) {
		write_cmos_sensor(0x5000, 0x57);
		write_cmos_sensor(0x5001, 0x02);
		write_cmos_sensor(0x5e00, 0x80);
	} else {
		write_cmos_sensor(0x5000, 0x77);
		write_cmos_sensor(0x5001, 0x0a);
		write_cmos_sensor(0x5e00, 0x00);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable)
		write_cmos_sensor(0x0100, 0X01);
	else
		write_cmos_sensor(0x0100, 0x00);
	//mdelay(30);
	return ERROR_NONE;
}

/* Tian.Tian@Camera.Driver, 2019/11/10, add for remosaic start
#define EEPROM_READ_ID  0xA8
static void read_eeprom(int offset, char *data, kal_uint32 size)
{
	int i = 0, addr = offset;
	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	for (i = 0; i < size; i++) {
		pu_send_cmd[0] = (char)(addr >> 8);
		pu_send_cmd[1] = (char)(addr & 0xFF);
		iReadRegI2C(pu_send_cmd, 2, &data[i], 1, EEPROM_READ_ID);

		addr++;
	}
}
*/

/* Tian.Tian@Camera.Driver, 2019/11/10, add for remosaic end */

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
	UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	uintptr_t tmp;
	unsigned long long *feature_data = (unsigned long long *) feature_para;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data = (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;
        /*Yang.Guo@Camera.Driver , 20200224, add for ITS--sensor_fusion*/
        MUINT32 offset1 = 0;

	LOG_INF("feature_id = %d\n", feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.cap.pclk;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.normal_video.pclk;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.hs_video.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom1.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom2.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom3.pclk;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.slim_video.pclk;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.pclk;
			break;
		}
		break;
        /*Yang.Guo@Camera.Driver , 20200224, add for ITS--sensor_fusion start*/
        case SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE:
			LOG_INF("Front SENOSR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE\n");
			offset1 = 1730000;
			LOG_INF("Front offset1=%d, offset1=0x%x \n", offset1, offset1);
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = offset1;
               break;
        /*Yang.Guo@Camera.Driver , 20200224, add for ITS--sensor_fusion end*/
	case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.cap.framelength << 16)
				+ imgsensor_info.cap.linelength;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.normal_video.framelength << 16)
				+ imgsensor_info.normal_video.linelength;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.hs_video.framelength << 16)
				+ imgsensor_info.hs_video.linelength;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.slim_video.framelength << 16)
				+ imgsensor_info.slim_video.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom1.framelength << 16)
				+ imgsensor_info.custom1.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom2.framelength << 16)
				+ imgsensor_info.custom2.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom3.framelength << 16)
				+ imgsensor_info.custom3.linelength;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		night_mode((BOOL) (*feature_data));
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData = read_cmos_sensor(sensor_reg_data->RegAddr);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode(*feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		get_imgsensor_id(feature_return_para_32);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode((BOOL)*feature_data_16, *(feature_data_16+1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM)*feature_data,
			*(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
			(MUINT32 *)(uintptr_t)(*(feature_data+1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: /*for factory mode auto testing*/
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("current fps :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		LOG_INF("ihdr enable :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_en = (UINT8)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (UINT32)*feature_data);
		tmp = (uintptr_t)(*(feature_data+1));

		wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)tmp;

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[1],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[2],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[3],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[4],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		/* Tian.Tian@Camera.Driver, 2019/11/10, add for remosaic start */
		case MSDK_SCENARIO_ID_CUSTOM3:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[5],
				sizeof(struct  SENSOR_WINSIZE_INFO_STRUCT));
		break;
		/* Tian.Tian@Camera.Driver, 2019/11/10, add for remosaic end */
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[0],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data+1),
			(UINT16)*(feature_data+2));
		//ihdr_write_shutter_gain((UINT16)*feature_data, (UINT16)*(feature_data+1), (UINT16)*(feature_data+2));
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length((UINT16) (*feature_data), (UINT16) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		LOG_INF("This sensor can't support temperature get\n");
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n", *feature_data);
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
	{
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.cap.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom1.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom2.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom3.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			/*Tian.Tian@ODM_WT.CAMERA.Hal 2678995 on 2020/02/24, modify for cts testMultiCameraRelease*/
			//*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			//	= imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
	}
	break;
	/* Tian.Tian@Camera.Driver, 2019/11/10, add for remosaic start */
	case SENSOR_FEATURE_GET_4CELL_DATA:/*get 4 cell data from eeprom*/
	{
		int type = (kal_uint16)(*feature_data);
		char *data = (char *)(*(feature_data+1));
		char *databuffer = NULL;
		if ((NULL != monetx_ofilm_front_ov16a1q_eeprom_data.dataBuffer) &&
			monetx_ofilm_front_ov16a1q_eeprom_data.dataLength == 0x1682) {
			databuffer = monetx_ofilm_front_ov16a1q_eeprom_data.dataBuffer;

			/*only copy Cross Talk calibration data*/
			if (type == FOUR_CELL_CAL_TYPE_XTALK_CAL) {
				//read_eeprom(0xC90, &data[2], 600);
				data[0] = 88;
				data[1] = 2;
				memcpy(data + 2,databuffer + 0xC90,600);
				LOG_INF("read Cross Talk calibration data size= %d %d %d %d,%d,,%d\n",
					data[0],data[1],data[2], data[3],data[4],data[5]);
			} else if (type == FOUR_CELL_CAL_TYPE_DPC) {
				//read_eeprom(0xF00, &data[2], 1920);
				data[0] = 128;
				data[1] = 7;
				memcpy(data + 2,databuffer + 0xF00,1920);
				LOG_INF("read DPC Talk calibration data size= %d %d %d %d,%d,,%d\n",
					data[0],data[1],data[2], data[3],data[4],data[5]);
				}
			}
		break;
	}
	/* Tian.Tian@Camera.Driver, 2019/11/10, add for remosaic end */
	default:
		break;
	}

	return ERROR_NONE;
}    /*    feature_control()  */


static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};


UINT32 OV16A1Q_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}	/*	OV16A1Q_MIPI_RAW_SensorInit	*/
