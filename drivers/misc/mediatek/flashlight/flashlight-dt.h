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

#ifndef _FLASHLIGHT_DT_H
#define _FLASHLIGHT_DT_H

#ifdef VENDOR_EDIT
/*Feng.Hu@Camera.Driver 20171121 add for mp3331 flash driver IC*/
#define MP3331_DTNAME "mediatek,flashlights_mp3331"
#define AW3642_DTNAME     "mediatek,flashlights_aw3642"
#define AW3642_DTNAME_I2C "mediatek,strobe_main_2"

#endif

#define DUMMY_GPIO_DTNAME "mediatek,flashlights_dummy_gpio"
#define DUMMY_DTNAME      "mediatek,flashlights_dummy"
#define DUMMY_DTNAME_I2C  "mediatek,flashlights_dummy_i2c"
#define LED191_DTNAME     "mediatek,flashlights_led191"
#define LM3642_DTNAME     "mediatek,flashlights_lm3642"
#define LM3642_DTNAME_I2C "mediatek,strobe_main"
#define LM3643_DTNAME     "mediatek,flashlights_lm3643"
#define LM3643_DTNAME_I2C "mediatek,strobe_main"
#define LM3644_DTNAME     "mediatek,flashlights_lm3644"
#define LM3644_DTNAME_I2C "mediatek,strobe_main"
#ifdef ODM_WT_EDIT
//XingYu.Liu@ODM_WT.CAMERA.Driver.2019/10/9,Add for flashlight bring up
#define MONET_DTNAME     "mediatek,flashlights_monet"
#define MONET_DTNAME_I2C "mediatek,strobe_main"
#endif /* ODM_WT_EDIT */
#define MT6336_DTNAME     "mediatek,flashlights_mt6336"
#define MT6370_DTNAME     "mediatek,flashlights_mt6370"
#define MT6360_DTNAME     "mediatek,flashlights_mt6360"
#define RT4505_DTNAME     "mediatek,flashlights_rt4505"
#define RT4505_DTNAME_I2C "mediatek,strobe_main"
#define RT5081_DTNAME     "mediatek,flashlights_rt5081"

#endif /* _FLASHLIGHT_DT_H */
