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

#include "mclk.h"
#ifndef VENDOR_EDIT
#define VENDOR_EDIT
#endif

#ifdef VENDOR_EDIT
/*Feng.Hu@Camera.Driver 20171213 add for evb and t0 board both work well*/
#include<soc/oppo/oppo_project.h>
#endif

struct MCLK_PINCTRL_NAMES mclk_pinctrl_list[IMGSENSOR_SENSOR_IDX_MAX_NUM][MCLK_STATE_MAX_NUM] = {
	{{"cam1_mclk_off"}, {"cam1_mclk_on"} },
	{{"cam2_mclk_off"}, {"cam2_mclk_on"} },
	{{"cam0_mclk_off"}, {"cam0_mclk_on"} },
	{{"cam3_mclk_off"}, {"cam3_mclk_on"} },
};

#ifdef VENDOR_EDIT
struct MCLK_PINCTRL_NAMES mclk_pinctrl_list_17197[IMGSENSOR_SENSOR_IDX_MAX_NUM][MCLK_STATE_MAX_NUM] = {
	{{"cam0_mclk_off"}, {"cam0_mclk_on"} },
	{{"cam2_mclk_off"}, {"cam2_mclk_on"} },
	{{"cam1_mclk_off"}, {"cam1_mclk_on"} },
	{{"cam3_mclk_off"}, {"cam3_mclk_on"} },
	/*Xiaoyang.Huang@RM.Camera add for 18611 board,20190304*/
	{{"cam4_mclk_off"}, {"cam4_mclk_on"} }
};
#endif

static struct mclk mclk_instance;

static enum IMGSENSOR_RETURN mclk_init(
	void *pinstance,
	struct IMGSENSOR_HW_DEVICE_COMMON *pcommon)
{
	struct mclk            *pinst         = (struct mclk *)pinstance;
	enum   IMGSENSOR_RETURN ret           = IMGSENSOR_RETURN_SUCCESS;
	int i;

	pinst->pmclk_mutex = &pcommon->pinctrl_mutex;

	pinst->ppinctrl = devm_pinctrl_get(&pcommon->pplatform_device->dev);
	if (IS_ERR(pinst->ppinctrl)) {
		PK_PR_ERR("%s : Cannot find camera pinctrl!\n", __func__);
		ret = IMGSENSOR_RETURN_ERROR;
	}

	#ifdef VENDOR_EDIT
	/*Feng.Hu@Camera.Driver 20171213 add for evb and t0 board both work well*/
	PK_PR_ERR("15:13 ophf mclk_init enter\n");
	#ifdef VENDOR_EDIT
	/*Xiaoyang.Huang@RM.Camera add for 18611 board,20190304*/
	if (is_project(OPPO_17197) || is_project(OPPO_18011) ||is_project(OPPO_18311) || is_project(OPPO_18611)) {
	#else
	if (is_project(OPPO_17197) || is_project(OPPO_18011) ||is_project(OPPO_18311) ) {
	#endif
		PK_PR_ERR("ophf 17197/18011/18311 board\n");
	for (i = IMGSENSOR_SENSOR_IDX_MIN_NUM; i < IMGSENSOR_SENSOR_IDX_MAX_NUM; i++) {
		if (mclk_pinctrl_list_17197[i][MCLK_STATE_DISABLE].ppinctrl_names)
			pinst->ppinctrl_state[i][MCLK_STATE_DISABLE] =
				pinctrl_lookup_state(pinst->ppinctrl,
				mclk_pinctrl_list_17197[i][MCLK_STATE_DISABLE].ppinctrl_names);

		if (pinst->ppinctrl_state[i][MCLK_STATE_DISABLE] != NULL &&
			!IS_ERR(pinst->ppinctrl_state[i][MCLK_STATE_DISABLE]))
			pinctrl_select_state(pinst->ppinctrl, pinst->ppinctrl_state[i][MCLK_STATE_DISABLE]);
		else {
			PK_PR_ERR("%s : 1 pinctrl err, %s\n",
				__func__, mclk_pinctrl_list_17197[i][MCLK_STATE_ENABLE].ppinctrl_names);

			ret = IMGSENSOR_RETURN_ERROR;
		}

		if (mclk_pinctrl_list_17197[i][MCLK_STATE_ENABLE].ppinctrl_names)
			pinst->ppinctrl_state[i][MCLK_STATE_ENABLE] =
				pinctrl_lookup_state(pinst->ppinctrl,
				mclk_pinctrl_list_17197[i][MCLK_STATE_ENABLE].ppinctrl_names);
		if (pinst->ppinctrl_state[i][MCLK_STATE_ENABLE] != NULL ||
			IS_ERR(pinst->ppinctrl_state[i][MCLK_STATE_ENABLE])) {
			PK_PR_ERR("%s : 2 pinctrl err, %s\n", __func__,
				mclk_pinctrl_list_17197[i][MCLK_STATE_ENABLE].ppinctrl_names);

			ret = IMGSENSOR_RETURN_ERROR;
		}

	}
	} else {
	for (i = IMGSENSOR_SENSOR_IDX_MIN_NUM; i < IMGSENSOR_SENSOR_IDX_MAX_NUM; i++) {
		if (mclk_pinctrl_list[i][MCLK_STATE_DISABLE].ppinctrl_names)
			pinst->ppinctrl_state[i][MCLK_STATE_DISABLE] =
				pinctrl_lookup_state(pinst->ppinctrl,
				mclk_pinctrl_list[i][MCLK_STATE_DISABLE].ppinctrl_names);

		if (pinst->ppinctrl_state[i][MCLK_STATE_DISABLE] != NULL &&
			!IS_ERR(pinst->ppinctrl_state[i][MCLK_STATE_DISABLE]))
			pinctrl_select_state(pinst->ppinctrl, pinst->ppinctrl_state[i][MCLK_STATE_DISABLE]);
		else {
			PK_PR_ERR("%s :3  pinctrl err, %s\n",
				__func__, mclk_pinctrl_list[i][MCLK_STATE_ENABLE].ppinctrl_names);

			ret = IMGSENSOR_RETURN_ERROR;
		}

		if (mclk_pinctrl_list[i][MCLK_STATE_ENABLE].ppinctrl_names)
			pinst->ppinctrl_state[i][MCLK_STATE_ENABLE] =
				pinctrl_lookup_state(pinst->ppinctrl,
				mclk_pinctrl_list[i][MCLK_STATE_ENABLE].ppinctrl_names);
		if (pinst->ppinctrl_state[i][MCLK_STATE_ENABLE] != NULL ||
			IS_ERR(pinst->ppinctrl_state[i][MCLK_STATE_ENABLE])) {
			PK_PR_ERR("%s :4 pinctrl err, %s\n", __func__,
				mclk_pinctrl_list[i][MCLK_STATE_ENABLE].ppinctrl_names);

			ret = IMGSENSOR_RETURN_ERROR;
		}

	}
	}
	#else
	for (i = IMGSENSOR_SENSOR_IDX_MIN_NUM; i < IMGSENSOR_SENSOR_IDX_MAX_NUM; i++) {
		if (mclk_pinctrl_list[i][MCLK_STATE_DISABLE].ppinctrl_names)
			pinst->ppinctrl_state[i][MCLK_STATE_DISABLE] =
				pinctrl_lookup_state(pinst->ppinctrl,
				mclk_pinctrl_list[i][MCLK_STATE_DISABLE].ppinctrl_names);

		if (pinst->ppinctrl_state[i][MCLK_STATE_DISABLE] != NULL &&
			!IS_ERR(pinst->ppinctrl_state[i][MCLK_STATE_DISABLE]))
			pinctrl_select_state(pinst->ppinctrl, pinst->ppinctrl_state[i][MCLK_STATE_DISABLE]);
		else {
			PK_PR_ERR("%s :5 pinctrl err, %s\n",
				__func__, mclk_pinctrl_list[i][MCLK_STATE_ENABLE].ppinctrl_names);

			ret = IMGSENSOR_RETURN_ERROR;
		}

		if (mclk_pinctrl_list[i][MCLK_STATE_ENABLE].ppinctrl_names)
			pinst->ppinctrl_state[i][MCLK_STATE_ENABLE] =
				pinctrl_lookup_state(pinst->ppinctrl,
				mclk_pinctrl_list[i][MCLK_STATE_ENABLE].ppinctrl_names);
		if (pinst->ppinctrl_state[i][MCLK_STATE_ENABLE] != NULL ||
			IS_ERR(pinst->ppinctrl_state[i][MCLK_STATE_ENABLE])) {
			PK_PR_ERR("%s :6 pinctrl err, %s\n", __func__,
				mclk_pinctrl_list[i][MCLK_STATE_ENABLE].ppinctrl_names);

			ret = IMGSENSOR_RETURN_ERROR;
		}

	}
	#endif

	return ret;
}

static enum IMGSENSOR_RETURN mclk_release(void *pinstance)
{
	int i;
	struct mclk *pinst = (struct mclk *)pinstance;

	for (i = IMGSENSOR_SENSOR_IDX_MIN_NUM; i < IMGSENSOR_SENSOR_IDX_MAX_NUM; i++) {
		if (pinst->ppinctrl_state[i][MCLK_STATE_DISABLE] != NULL &&
			!IS_ERR(pinst->ppinctrl_state[i][MCLK_STATE_DISABLE]))
			pinctrl_select_state(pinst->ppinctrl, pinst->ppinctrl_state[i][MCLK_STATE_DISABLE]);
	}
	return IMGSENSOR_RETURN_SUCCESS;
}

static enum IMGSENSOR_RETURN mclk_set(
	void *pinstance,
	enum IMGSENSOR_SENSOR_IDX   sensor_idx,
	enum IMGSENSOR_HW_PIN       pin,
	enum IMGSENSOR_HW_PIN_STATE pin_state)
{
	struct mclk *pinst = (struct mclk *)pinstance;
	struct pinctrl_state *ppinctrl_state;
	enum   IMGSENSOR_RETURN ret = IMGSENSOR_RETURN_SUCCESS;
	enum MCLK_STATE state_index = MCLK_STATE_DISABLE;

	PK_DBG("%s : sensor_idx %d mclk_set pinctrl, PinIdx %d, Val %d\n",
		__func__, sensor_idx, pin, pin_state);

	if (pin_state < IMGSENSOR_HW_PIN_STATE_LEVEL_0 ||
	    pin_state > IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH) {
		ret = IMGSENSOR_RETURN_ERROR;
	} else {
		state_index = (pin_state > IMGSENSOR_HW_PIN_STATE_LEVEL_0) ? MCLK_STATE_ENABLE : MCLK_STATE_DISABLE;

		ppinctrl_state = pinst->ppinctrl_state[sensor_idx][state_index];

		mutex_lock(pinst->pmclk_mutex);

		if (!IS_ERR(ppinctrl_state))
			pinctrl_select_state(pinst->ppinctrl, ppinctrl_state);
		else
			PK_PR_ERR("%s : sensor_idx %d mclk_set pinctrl, PinIdx %d, Val %d\n",
			__func__, sensor_idx, pin, pin_state);

		mutex_unlock(pinst->pmclk_mutex);
	}
	return ret;
}

static struct IMGSENSOR_HW_DEVICE device = {
	.id        = IMGSENSOR_HW_ID_MCLK,
	.pinstance = (void *)&mclk_instance,
	.init      = mclk_init,
	.set       = mclk_set,
	.release   = mclk_release,
};

enum IMGSENSOR_RETURN imgsensor_hw_mclk_open(
	struct IMGSENSOR_HW_DEVICE **pdevice)
{
	*pdevice = &device;
	return IMGSENSOR_RETURN_SUCCESS;
}

