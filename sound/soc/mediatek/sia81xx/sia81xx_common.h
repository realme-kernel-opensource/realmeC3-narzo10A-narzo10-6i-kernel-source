/*
 * Copyright (C) 2018, SI-IN, Yun Shi (yun.shi@si-in.com).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#ifndef _SIA81XX_COMMOMN_H
#define _SIA81XX_COMMOMN_H

#define SIA81XX_DRIVER_VERSION			("1.1.2")
#define SIA81XX_MAX_CHANNEL_SUPPORT		(8)

enum {
	AUDIO_SCENE_PLAYBACK = 0,
	AUDIO_SCENE_VOICE,
	AUDIO_SCENE_NUM
};

enum {
	CHIP_TYPE_SIA8101 = 0,
	CHIP_TYPE_SIA8108,
	CHIP_TYPE_SIA8109,
	CHIP_TYPE_UNKNOWN
};


#endif /* _SIA81XX_COMMOMN_H */

