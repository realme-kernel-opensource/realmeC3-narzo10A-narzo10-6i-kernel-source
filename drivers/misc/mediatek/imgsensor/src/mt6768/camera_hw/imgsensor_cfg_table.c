/*
 * Copyright (C) 2017 MediaTek Inc.
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

#include "kd_imgsensor.h"

#include "regulator/regulator.h"
#include "gpio/gpio.h"
/*#include "mt6306/mt6306.h"*/
#include "mclk/mclk.h"



#include "imgsensor_cfg_table.h"

enum IMGSENSOR_RETURN
	(*hw_open[IMGSENSOR_HW_ID_MAX_NUM])(struct IMGSENSOR_HW_DEVICE **) = {
	imgsensor_hw_regulator_open,
	imgsensor_hw_gpio_open,
	/*imgsensor_hw_mt6306_open,*/
	imgsensor_hw_mclk_open
};

struct IMGSENSOR_HW_CFG imgsensor_custom_config[] = {
	{
		IMGSENSOR_SENSOR_IDX_MAIN,
		IMGSENSOR_I2C_DEV_0,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
#ifdef ODM_WT_EDIT
//XingYu.Liu@ODM_WT.CAMERA.Driver.2019/10/9,Add for camera bring up
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_AVDD},
			/*xiaojun.Pu@ODM_WT.CAMERA.Driver.2019/10/18,add for 12M camera af driver*/
			{IMGSENSOR_HW_ID_REGULATOR,IMGSENSOR_HW_PIN_AFVDD},
#endif /* ODM_WT_EDIT */
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_SUB,
		IMGSENSOR_I2C_DEV_1,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
#ifdef ODM_WT_EDIT
//XingYu.Liu@ODM_WT.CAMERA.Driver.2019/10/9,Add for camera bring up
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
#endif /* ODM_WT_EDIT */
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
#ifdef ODM_WT_EDIT
//XingYu.Liu@ODM_WT.CAMERA.Driver.2019/10/9,Add for camera bring up
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_MIPI_SWITCH_EN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL},
#endif /* ODM_WT_EDIT */
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_MAIN2,
		IMGSENSOR_I2C_DEV_2,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
#ifdef ODM_WT_EDIT
//XingYu.Liu@ODM_WT.CAMERA.Driver.2019/10/9,Add for camera bring up
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AVDD},
#endif /* ODM_WT_EDIT */
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
#ifdef ODM_WT_EDIT
//XingYu.Liu@ODM_WT.CAMERA.Driver.2019/10/9,Add for camera bring up
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_MIPI_SWITCH_EN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL},
#endif /* ODM_WT_EDIT */
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_SUB2,
		IMGSENSOR_I2C_DEV_1,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_MAIN3,
		IMGSENSOR_I2C_DEV_2,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
#ifdef ODM_WT_EDIT
//XingYu.Liu@ODM_WT.CAMERA.Driver.2019/10/9,Add for camera bring up
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AVDD},
#endif /* ODM_WT_EDIT */
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},

	{IMGSENSOR_SENSOR_IDX_NONE}
};

struct IMGSENSOR_HW_POWER_SEQ platform_power_sequence[] = {
#ifdef MIPI_SWITCH
	{
		PLATFORM_POWER_SEQ_NAME,
		{
			{
				IMGSENSOR_HW_PIN_MIPI_SWITCH_EN,
				IMGSENSOR_HW_PIN_STATE_LEVEL_0,
				0,
				IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH,
				0
			},
			{
				IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL,
				IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH,
				0,
				IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH,
				0
			},
		},
		IMGSENSOR_SENSOR_IDX_SUB,
	},
	{
		PLATFORM_POWER_SEQ_NAME,
		{
			{
				IMGSENSOR_HW_PIN_MIPI_SWITCH_EN,
				IMGSENSOR_HW_PIN_STATE_LEVEL_0,
				0,
				IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH,
				0
			},
			{
				IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL,
				IMGSENSOR_HW_PIN_STATE_LEVEL_0,
				0,
				IMGSENSOR_HW_PIN_STATE_LEVEL_0,
				0
			},
		},
		IMGSENSOR_SENSOR_IDX_MAIN2,
	},
#endif

	{NULL}
};

/* Legacy design */
struct IMGSENSOR_HW_POWER_SEQ sensor_power_sequence[] = {
#ifdef ODM_WT_EDIT
	/* Zhen.Quan@Camera.Driver, 2019/11/04, modify camera power sequence */
#if defined(MONET_HLT_DEPTH_OV02A1B)
	{SENSOR_DRVNAME_MONET_HLT_DEPTH_OV02A1B,
		{
			{RST, Vol_High, 1,Vol_Low,1},
			{DOVDD, Vol_1800, 5,Vol_Low,5},
			{AVDD, Vol_2800, 5,Vol_Low,5},
			{RST, Vol_High, 1,Vol_High,1},
			{SensorMCLK, Vol_High, 5,Vol_Low,5},
			{RST, Vol_Low, 5,Vol_Low,5},
		},
	},
#endif
#if defined(MONET_HLT_MACRO_GC2375H)
	{SENSOR_DRVNAME_MONET_HLT_MACRO_GC2375H,
		{
			{RST, Vol_High, 1,Vol_Low,1},
			{DOVDD, Vol_1800, 5,Vol_Low,5},
			{AVDD, Vol_2800, 5,Vol_Low,5},
			{SensorMCLK, Vol_High, 5,Vol_Low,5},
			{RST, Vol_Low, 5,Vol_High,5},
		},
	},
#endif
#if defined(MONETX_HLT_MACRO_GC2375H)
	{SENSOR_DRVNAME_MONETX_HLT_MACRO_GC2375H,
		{
			{RST, Vol_High, 1,Vol_Low,1},
			{DOVDD, Vol_1800, 5,Vol_Low,5},
			{AVDD, Vol_2800, 5,Vol_Low,5},
			{SensorMCLK, Vol_High, 5,Vol_Low,5},
			{RST, Vol_Low, 5,Vol_High,5},
		},
	},
#endif
#if defined(MONETD_CXT_DEPTH_GC2375H)
	{SENSOR_DRVNAME_MONETD_CXT_DEPTH_GC2375H,
		{
			{RST, Vol_High, 1,Vol_Low,1},
			{DOVDD, Vol_1800, 5,Vol_Low,5},
			{AVDD, Vol_2800, 5,Vol_Low,5},
			{SensorMCLK, Vol_High, 5,Vol_Low,5},
			{RST, Vol_Low, 5,Vol_High,5},
		},
	},
#endif
#if defined(MONETD_LH_DEPTH_GC2375H)
	{SENSOR_DRVNAME_MONETD_LH_DEPTH_GC2375H,
		{
			{RST, Vol_High, 1,Vol_Low,1},
			{DOVDD, Vol_1800, 5,Vol_Low,5},
			{AVDD, Vol_2800, 5,Vol_Low,5},
			{SensorMCLK, Vol_High, 5,Vol_Low,5},
			{RST, Vol_Low, 5,Vol_High,5},
		},
	},
#endif
#if defined(MONET_LH_MACRO_GC2375H)
    {SENSOR_DRVNAME_MONET_LH_MACRO_GC2375H,
		{
			{RST, Vol_High, 1,Vol_Low,1},
			{DOVDD, Vol_1800, 5,Vol_Low,5},
			{AVDD, Vol_2800, 5,Vol_Low,5},
			{SensorMCLK, Vol_High, 5,Vol_Low,5},
			{RST, Vol_Low, 5,Vol_High,5},
		},
	},
#endif
#if defined(MONETX_HLT_DEPTH_OV02A1B)
	{SENSOR_DRVNAME_MONETX_HLT_DEPTH_OV02A1B,
		{
			{RST, Vol_High, 1,Vol_High,1},
			{DOVDD, Vol_1800, 5,Vol_Low,5},
			{AVDD, Vol_2800, 5,Vol_Low,5},
			{RST, Vol_High, 1,Vol_High,1},
			{SensorMCLK, Vol_High, 5,Vol_Low,1},
			{RST, Vol_Low, 5,Vol_Low,5},
		},
	},
#endif
#if defined(MONET_HLT_FRONT_GC5035)
	{SENSOR_DRVNAME_MONET_HLT_FRONT_GC5035,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 3},
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1200, 1},
			{AVDD, Vol_2800, 1},
			{RST, Vol_High, 1},
			{PDN, Vol_High, 3},
			{SensorMCLK, Vol_High, 3},
		},
	},
#endif
#if defined(MONET_TXD_FRONT_HI556)
	{SENSOR_DRVNAME_MONET_TXD_FRONT_HI556,
		{
			{SensorMCLK, Vol_High, 5},
			{AVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1200, 0},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 3},
			{RST, Vol_Low, 5},
			{RST, Vol_High, 10},
		},
	},
#endif
#if defined(MONET_TRULY_MAIN_OV12A10)
	{
		SENSOR_DRVNAME_MONET_TRULY_MAIN_OV12A10,
		{
			{RST, Vol_Low, 0},
			{AVDD, Vol_2800, 1},
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_High, 1},
			/*xiaojun.Pu@ODM_WT.CAMERA.Driver.2019/10/18,add for 12M camera af driver*/
			{AFVDD, Vol_2800, 1},
			{RST, Vol_High, 1},
                        {SensorMCLK, Vol_High, 0},
		},
	},
#endif
	//Duilin.Qin@ODM_WT.CAMERA.Driver.2019/11/20,Add for camera bring up
#if defined(MONETD_TRULY_MAIN_OV12A10)
        {
                SENSOR_DRVNAME_MONETD_TRULY_MAIN_OV12A10,
                {
                        {RST, Vol_Low, 3},
                        {AVDD, Vol_2800, 1},
                        {DOVDD, Vol_1800, 1},
                        {DVDD, Vol_High, 1},
                        /*xiaojun.Pu@ODM_WT.CAMERA.Driver.2019/10/18,add for 12M camera af driver*/
                        {AFVDD, Vol_2800, 1},
                        {RST, Vol_High, 5},
                        {SensorMCLK, Vol_High, 0},
                },
        },
#endif
        //Duilin.Qin@ODM_WT.CAMERA.Driver.2019/11/20,Add for camera bring up

/*xiaojun.Pu@ODM_WT.CAMERA.Driver.2019/12/19,add for customer supply 5M front camera r*/
#if defined(MONET_HLT_CUSTFRONT_GC5035)
	{
	     SENSOR_DRVNAME_MONET_HLT_CUSTFRONT_GC5035,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 3},
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1200, 1},
			{AVDD, Vol_2800, 1},
			{RST, Vol_High, 1},
			{PDN, Vol_High, 3},
			{SensorMCLK, Vol_High, 3},
		},
	},
#endif
#if defined(MONET_HLT_FRONT_GC5035B)
	{
	     SENSOR_DRVNAME_MONET_HLT_FRONT_GC5035B,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 3},
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1200, 1},
			{AVDD, Vol_2800, 1},
			{RST, Vol_High, 1},
			{PDN, Vol_High, 3},
			{SensorMCLK, Vol_High, 3},
		},
	},
#endif
#if defined(MONETX_TRULY_MAIN_S5KGM1SP)
	{
		SENSOR_DRVNAME_MONETX_TRULY_MAIN_S5KGM1SP,
		{
			{RST, Vol_Low, 1},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AFVDD, Vol_2800, 5},
			{RST, Vol_High, 2},
			{SensorMCLK, Vol_High, 0},
		},
	},
#endif
#if defined(MONETX_OFILM_FRONT_OV16A1Q)
	{
		SENSOR_DRVNAME_MONETX_OFILM_FRONT_OV16A1Q,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 3},
			{AVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1200, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 5},
			{SensorMCLK, Vol_High, 3},
		},
	},
#endif
#if defined(MONETX_HLT_MACRO_GC2385)
	{
		SENSOR_DRVNAME_MONETX_HLT_MACRO_GC2385,
		{
			{RST, Vol_Low, 1, Vol_Low, 1},
			{DOVDD, Vol_1800, 5, Vol_Low, 5},
			{AVDD, Vol_2800, 5, Vol_Low, 5},
			{SensorMCLK, Vol_High, 3, Vol_Low, 5},
			{RST, Vol_High, 5, Vol_Low, 5},
		},
	},
#endif
#if defined(MONETX_SHENGTAI_WIDE_OV8856)
	{SENSOR_DRVNAME_MONETX_SHENGTAI_WIDE_OV8856,
		{
			{PDN, Vol_Low, 1},
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1200, 1},
			{PDN, Vol_High, 1},
			{RST, Vol_High, 5},
			{SensorMCLK, Vol_High, 2},
		},
	},
#endif
#else
#if defined(IMX519_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX519_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{AVDD, Vol_2800, 0},
			{AFVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{DOVDD, Vol_1800, 1},
			{SensorMCLK, Vol_High, 5},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 8}
		},
	},
#endif
#if defined(IMX398_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX398_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1100, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 1},
		},
	},
#endif
#if defined(IMX350_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX350_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{AVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1200, 5},
			{SensorMCLK, Vol_High, 5},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(OV23850_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV23850_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 2},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 5},
		},
	},
#endif
#if defined(IMX386_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX386_MIPI_RAW,
		{
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1100, 0},
			{DOVDD, Vol_1800, 0},
			{AFVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High, 1},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 5},
		},
	},
#endif
#if defined(IMX386_MIPI_MONO)
	{
		SENSOR_DRVNAME_IMX386_MIPI_MONO,
		{
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1100, 0},
			{DOVDD, Vol_1800, 0},
			{AFVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High, 1},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 5},
		},
	},
#endif

#if defined(IMX338_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX338_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2500, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1100, 0},
			{AFVDD, Vol_2800, 0},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 1}
		},
	},
#endif
#if defined(S5K4E6_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K4E6_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2900, 0},
			{DVDD, Vol_1200, 2},
			{AFVDD, Vol_2800, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(S5K3P8SP_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K3P8SP_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1000, 0},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 4},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0},
		},
	},
#endif
#if defined(S5K2T7SP_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K2T7SP_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1000, 0},
			{SensorMCLK, Vol_High, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 2},

		},
	},
#endif
#if defined(IMX230_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX230_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{AVDD, Vol_2500, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1100, 0},
			{AFVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 10}
		},
	},
#endif
#if defined(S5K3M2_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K3M2_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 4},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(S5K3P3SX_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K3P3SX_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 4},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(S5K5E2YA_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K5E2YA_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 4},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(S5K4ECGX_MIPI_YUV)
	{
		SENSOR_DRVNAME_S5K4ECGX_MIPI_YUV,
		{
			{DVDD, Vol_1200, 1},
			{AVDD, Vol_2800, 1},
			{DOVDD, Vol_1800, 1},
			{AFVDD, Vol_2800, 0},
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(OV16880_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV16880_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1200, 5},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 1},
			{RST, Vol_High, 2}
		},
	},
#endif
#if defined(S5K2P7_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K2P7_MIPI_RAW,
		{
			{PDN, Vol_Low, 1},
			{RST, Vol_Low, 1},
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1000, 1},
			{DOVDD, Vol_1800, 1},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0},
		},
	},
#endif
#if defined(S5K2P8_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K2P8_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 4},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(IMX258_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX258_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(IMX258_MIPI_MONO)
	{
		SENSOR_DRVNAME_IMX258_MIPI_MONO,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(IMX377_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX377_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(OV8858_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV8858_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1200, 5},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 1},
			{RST, Vol_High, 2}
		},
	},
#endif
#if defined(OV8856_MIPI_RAW)
	{SENSOR_DRVNAME_OV8856_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 2},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 5},
		},
	},
#endif
#if defined(S5K2X8_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K2X8_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(IMX214_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX214_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1000, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 1}
		},
	},
#endif
#if defined(IMX214_MIPI_MONO)
	{
		SENSOR_DRVNAME_IMX214_MIPI_MONO,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1000, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 1}
		},
	},
#endif
#if defined(S5K3L8_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K3L8_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(IMX362_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX362_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 10},
			{DOVDD, Vol_1800, 10},
			{DVDD, Vol_1200, 10},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(S5K2L7_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K2L7_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1000, 0},
			{AFVDD, Vol_2800, 3},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(IMX318_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX318_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 10},
			{DOVDD, Vol_1800, 10},
			{DVDD, Vol_1200, 10},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(OV8865_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV8865_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 5},
			{RST, Vol_Low, 5},
			{DOVDD, Vol_1800, 5},
			{AVDD, Vol_2800, 5},
			{DVDD, Vol_1200, 5},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_High, 5},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(IMX219_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX219_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 10},
			{DOVDD, Vol_1800, 10},
			{DVDD, Vol_1000, 10},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(S5K3M3_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K3M3_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1000, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 2}
		},
	},
#endif
#if defined(OV5670_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV5670_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 5},
			{RST, Vol_Low, 5},
			{DOVDD, Vol_1800, 5},
			{AVDD, Vol_2800, 5},
			{DVDD, Vol_1200, 5},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_High, 5},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(OV5670_MIPI_RAW_2)
	{
		SENSOR_DRVNAME_OV5670_MIPI_RAW_2,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 5},
			{RST, Vol_Low, 5},
			{DOVDD, Vol_1800, 5},
			{AVDD, Vol_2800, 5},
			{DVDD, Vol_1200, 5},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_High, 5},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(OV20880_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV20880_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{RST, Vol_Low, 1},
			{AVDD, Vol_2800, 1},
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1100, 1},
			{RST, Vol_High, 5}
		},
	},
#endif
#endif /* ODM_WT_EDIT */
	/* add new sensor before this line */
	{NULL,},
};

