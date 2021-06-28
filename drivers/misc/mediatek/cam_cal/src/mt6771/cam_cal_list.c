/*
 * Copyright (C) 2018 MediaTek Inc.
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
#include <linux/kernel.h>
#include "cam_cal_list.h"
#include "eeprom_i2c_common_driver.h"
#include "eeprom_i2c_custom_driver.h"
#include "kd_imgsensor.h"

#ifndef VENDOR_EDIT
#define VENDOR_EDIT
#endif

struct stCAM_CAL_LIST_STRUCT g_camCalList[] = {
	/*Below is commom sensor */
        #ifdef VENDOR_EDIT
        /*Xi.Chen@RM.Camera 20190309 modify for sensor eeprom*/
        {OV13855_SENSOR_ID, 0xA0, Common_read_region},
        {S5K4H7_SENSOR_ID, 0x20, Common_read_region},
        {IMX576_SENSOR_ID, 0xA8, Common_read_region},
        {S5K3L6_SENSOR_ID, 0xA0, Common_read_region},
	/*Longyuan.Yang@Camera.Driver 20190111 modify for sensor eeprom*/
	{IMX398_SENSOR_ID, 0xA0, Common_read_region},
	{GC2375H_SENSOR_ID, 0xA0, Common_read_region},
	/*Caohua.Lin@Camera.Driver 20181023 modify for sensor eeprom*/
	{IMX476_SENSOR_ID, 0xA2, Common_read_region},
	{IMX519_SENSOR_ID, 0xA0, Common_read_region},
	{IMX519_SENSOR_ID, 0xA0, Common_read_region},
	{S5KGM1SP_SENSOR_ID, 0xA0, Common_read_region},
	{S5K3P9SP_SENSOR_ID, 0xA8, Common_read_region},
	{GC5035_SENSOR_ID, 0xA0, Common_read_region},
	#else
	{IMX338_SENSOR_ID, 0xA0, Common_read_region},
	{S5K4E6_SENSOR_ID, 0xA8, Common_read_region},
	{IMX386_SENSOR_ID, 0xA0, Common_read_region},
	{S5K3M3_SENSOR_ID, 0xA0, Common_read_region},
	{S5K2L7_SENSOR_ID, 0xA0, Common_read_region},
	{IMX318_SENSOR_ID, 0xA0, Common_read_region},
	{OV8858_SENSOR_ID, 0xA8, Common_read_region},
	{IMX350_SENSOR_ID, 0xA0, Common_read_region},
	{S5K3P8SX_SENSOR_ID, 0xA0, Common_read_region},
	{IMX386_MONO_SENSOR_ID, 0xA0, Common_read_region},
	{IMX576_SENSOR_ID, 0xA2, Common_read_region},
	/*99*/
	{IMX258_SENSOR_ID, 0xA0, Common_read_region},
	{IMX258_MONO_SENSOR_ID, 0xA0, Common_read_region},
	/*97*/
	{OV23850_SENSOR_ID, 0xA0, Common_read_region},
	{OV23850_SENSOR_ID, 0xA8, Common_read_region},
	{S5K3M2_SENSOR_ID, 0xA0, Common_read_region},
	/*39*/
	{OV13870_SENSOR_ID, 0xA8, Common_read_region},
	{OV8856_SENSOR_ID, 0xA0, Common_read_region},
	/*55*/
	{S5K2P8_SENSOR_ID, 0xA2, Common_read_region},
	{S5K2P8_SENSOR_ID, 0xA0, Common_read_region},
	{OV8858_SENSOR_ID, 0xA2, Common_read_region},
	/* Others */
	{S5K2X8_SENSOR_ID, 0xA0, Common_read_region},
	{IMX376_SENSOR_ID, 0xA2, Common_read_region},
	{IMX214_SENSOR_ID, 0xA0, Common_read_region},
	{IMX214_MONO_SENSOR_ID, 0xA0, Common_read_region},
	#endif
	/*  ADD before this line */
	{0, 0, 0}	/*end of list */
};

unsigned int cam_cal_get_sensor_list(
	struct stCAM_CAL_LIST_STRUCT **ppCamcalList)
{
	if (ppCamcalList == NULL)
		return 1;

	*ppCamcalList = &g_camCalList[0];
	return 0;
}


