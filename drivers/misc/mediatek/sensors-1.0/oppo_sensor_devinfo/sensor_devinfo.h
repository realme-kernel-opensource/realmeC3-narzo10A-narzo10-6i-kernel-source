/******************************************************************
** Copyright (C), 2004-2017, OPPO Mobile Comm Corp., Ltd.
** VENDOR_EDIT
** File: - oppo_sensor_devinfo.c
** Description: Source file for sensor device infomation registered in scp.
** Version: 1.0
** Date : 2017/12/12
** Author: Fei.Mo@PSW.BSP.Sensor
**
** --------------------------- Revision History: ---------------------
* <version>	<date>		<author>              		<desc>
* Revision 1.0      2017/12/12        Fei.Mo@PSW.BSP.Sensor   	Created,need to sync acp info
*******************************************************************/
#ifndef SENSOR_DEVINFO_H
#define SENSOR_DEVINFO_H

enum {
    ALSPS_UNKNOW = 0,
    ALSPS_TMD2725,
    ALSPS_APSD9922,
    ALSPS_STK3335,
    ALSPS_UNION_TSL2540_STK3331,
    ALSPS_MULT_APDS9925_STK3332,
    ALSPS_STK3331,
    ALSPS_STK2232,
};

enum {
    GSENSOR_UNKNOW = 0,
    GSENSOR_LIS3DH,
    GSENSOR_LSM6DS3,
    GSENSOR_BMI160,
    GSENSOR_LIS2HH12,
    GSENSOR_LSM6DSM,

};

enum {
    MAG_UNKNOW = 0,
    MAG_AKM09911,
    MAG_MMC3530,
    MAG_MMC5603,
    MAG_MXG4300,
};

enum {
    GYRO_UNKNOW = 0,
    GYRO_LSM6DS3,
    GYRO_BMI160,
    GYRO_LSM6DSM,
};

/*
 * @sensortye ,defined in hwmsensor.h, ID_ACCELEROMETER ,etc
 * return sensor name , for gsensor ,GSENSOR_LIS3DH,etc
*/
extern int get_sensor_name(int sensortye);

/*
 * @sensortye ,defined in hwmsensor.h, ID_ACCELEROMETER ,etc
 * @ data,  sensor calibration return , for light sensor , return gain
*/
extern int get_sensor_parameter(int sensortye ,int * data);

/*
 * @sensortye ,defined in hwmsensor.h, ID_ACCELEROMETER ,etc
 * @ data ,sensor calibration need to update , for light sensor , update gain
*/
extern int update_sensor_parameter(int sensortye ,int * data);
#endif
