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
 
/*****************************************************************************
 *
 * Filename:
 * ---------
 *    mtk_battery.c
 *
 * Project:
 * --------
 *   Android_Software
 *
 * Description:
 * ------------
 * This Module defines functions of the Anroid Battery service for
 * updating the battery status
 *
 * Author:
 * -------
 * Weiching Lin
 *
 ****************************************************************************/
#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/wait.h>		/* For wait queue*/
#include <linux/sched.h>	/* For wait queue*/
#include <linux/kthread.h>	/* For Kthread_run */
#include <linux/platform_device.h>	/* platform device */
#include <linux/time.h>

#include <linux/netlink.h>	/* netlink */
#include <linux/kernel.h>
#include <linux/socket.h>	/* netlink */
#include <linux/skbuff.h>	/* netlink */
#include <net/sock.h>		/* netlink */
#include <linux/cdev.h>		/* cdev */

#include <linux/err.h>	/* IS_ERR, PTR_ERR */
#include <linux/reboot.h>	/*kernel_power_off*/
#include <linux/proc_fs.h>
#include <linux/of_fdt.h>	/*of_dt API*/
#include <linux/vmalloc.h>


#include <linux/math64.h> /*div_s64*/

#include <mt-plat/aee.h>
#include <mt-plat/charger_type.h>
#include <mt-plat/mtk_charger.h>
#include <mt-plat/mtk_battery.h>
#include <mt-plat/mtk_boot.h>

#include <mtk_gauge_class.h>
#include "mtk_battery_internal.h"
#include <mach/mtk_battery_property.h>
#include <mtk_gauge_time_service.h>
#include <mt-plat/upmu_common.h>
#include <pmic_lbat_service.h>
#ifdef VENDOR_EDIT
/* Qiao.Hu@EXP.BSP.CHG.basic, 2017/07/20, Add for charger */
#include <soc/oppo/device_info.h>
#include <soc/oppo/oppo_project.h>
#include <linux/gpio.h>
#include "../../oppo/oppo_gauge.h"
extern bool is_vooc_project(void);

#endif  /*VENDOR_EDIT*/
#ifdef VENDOR_EDIT
/* Qiao.Hu@EXP.BSP.BaseDrv.CHG.Basic, 2017/08/08, Add for charger */
int fgauge_is_start = 0;
#endif /* VENDOR_EDIT */
#ifdef VENDOR_EDIT
//PengNan@BSP.CHG.basic,2017/07/27, customizing the battery NTC,modify the resistor.
#define RBAT_PULL_DOWN_R 24000
#endif /*VENDOR_EDIT*/




/* ============================================================ */
/* define */
/* ============================================================ */
#define NETLINK_FGD 26


/************ adc_cali *******************/
#define ADC_CALI_DEVNAME "MT_pmic_adc_cali"
#define TEST_ADC_CALI_PRINT _IO('k', 0)
#define SET_ADC_CALI_Slop _IOW('k', 1, int)
#define SET_ADC_CALI_Offset _IOW('k', 2, int)
#define SET_ADC_CALI_Cal _IOW('k', 3, int)
#define ADC_CHANNEL_READ _IOW('k', 4, int)
#define BAT_STATUS_READ _IOW('k', 5, int)
#define Set_Charger_Current _IOW('k', 6, int)
/* add for meta tool----------------------------------------- */
#define Get_META_BAT_VOL _IOW('k', 10, int)
#define Get_META_BAT_SOC _IOW('k', 11, int)
#define Get_META_BAT_CAR_TUNE_VALUE _IOW('k', 12, int)
#define Set_META_BAT_CAR_TUNE_VALUE _IOW('k', 13, int)
#define Set_BAT_DISABLE_NAFG _IOW('k', 14, int)
#define Set_CARTUNE_TO_KERNEL _IOW('k', 15, int)
/* add for meta tool----------------------------------------- */
#ifdef VENDOR_EDIT
/* Qiao.Hu@EXP.BSP.BaseDrv.CHG.Basic, 2017/08/08, Add for charger */
#define Get_FakeOff_Param _IOW('k', 7, int)
#define Turn_Off_Charging _IOW('k', 9, int)
extern int oppo_chg_get_ui_soc(void);
extern int oppo_chg_get_notify_flag(void);
extern int oppo_chg_show_vooc_logo_ornot(void);
extern bool pmic_chrdet_status(void);
extern int oppo_get_prop_status(void);
extern bool oppo_chg_check_chip_is_null(void);
extern signed int battery_get_bat_voltage(void);


#endif
static struct class *adc_cali_class;
static int adc_cali_major;
static dev_t adc_cali_devno;
static struct cdev *adc_cali_cdev;

static int adc_cali_slop[14] = {
	1000, 1000, 1000, 1000, 1000, 1000,
	1000, 1000, 1000, 1000, 1000, 1000,
	1000, 1000
};
static int adc_cali_offset[14] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static int adc_cali_cal[1] = { 0 };
static int battery_in_data[1] = { 0 };
static int battery_out_data[1] = { 0 };
static bool g_ADC_Cali;
#ifndef VENDOR_EDIT
/* ChaoYing.Chen@EXP.BSP.CHG.basic, 2017/06/17, Remove for charge driver */
static enum power_supply_property battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_TEMP,
};
#endif
/* weak function */
int __attribute__ ((weak))
	do_ptim_gauge(
	bool isSuspend, unsigned int *bat, signed int *cur, bool *is_charging)
{
	return 0;
}

int __attribute__ ((weak))
	get_rac(void)
{
	return 0;
}

int __attribute__ ((weak))
	get_imix(void)
{
	return 0;
}

void __attribute__ ((weak))
	battery_dump_nag(void)
{
}

enum charger_type __attribute__ ((weak))
	mt_get_charger_type(void)
{
	return CHARGER_UNKNOWN;
}

int __attribute__ ((weak))
	charger_manager_get_zcv(
	struct charger_consumer *consumer, int idx, u32 *uV)
{
	return 0;
}

struct charger_consumer __attribute__ ((weak))
	*charger_manager_get_by_name(struct device *dev,
	const char *supply_name)
{
	return NULL;
}

int __attribute__ ((weak))
	register_charger_manager_notifier(struct charger_consumer *consumer,
	struct notifier_block *nb)
{
	return 0;
}

/* weak function end */


bool is_battery_init_done(void)
{
	return gm.is_probe_done;
}

bool is_recovery_mode(void)
{
	int boot_mode = get_boot_mode();

	if (is_fg_disabled())
		return false;

	bm_debug("mtk_battery boot mode =%d\n", boot_mode);
	if (boot_mode == RECOVERY_BOOT) {
		gm.log_level = BMLOG_DEBUG_LEVEL;
		fg_cust_data.daemon_log_level = BMLOG_DEBUG_LEVEL;
		return true;
	}

	return false;
}

bool is_fg_disabled(void)
{
	return gm.disableGM30;
}



int register_battery_notifier(struct notifier_block *nb)
{
	int ret = 0;

	mutex_lock(&gm.notify_mutex);
	ret = srcu_notifier_chain_register(&gm.gm_notify, nb);
	mutex_unlock(&gm.notify_mutex);

	return ret;
}

int unregister_battery_notifier(struct notifier_block *nb)
{
	int ret = 0;

	mutex_lock(&gm.notify_mutex);
	ret = srcu_notifier_chain_unregister(&gm.gm_notify, nb);
	mutex_unlock(&gm.notify_mutex);

	return ret;
}

int battery_notifier(int event)
{
	return srcu_notifier_call_chain(&gm.gm_notify, event, NULL);
}

/* ============================================================ */
/* functions for fg hal*/
/* ============================================================ */
void set_hw_ocv_unreliable(bool _flag_unreliable)
{
	gm.hw_status.flag_hw_ocv_unreliable
		= _flag_unreliable;
}


/* ============================================================ */
/* functions */
/* ============================================================ */

static void disable_fg(void)
{

	int fgv;

	fgv = gauge_get_hw_version();

	if (fgv >= GAUGE_HW_V1000
	&& fgv < GAUGE_HW_V2000) {
		en_intr_VBATON_UNDET(0);
	}

	pmic_enable_interrupt(FG_BAT1_INT_L_NO, 0, "GM30");
	pmic_enable_interrupt(FG_BAT1_INT_H_NO, 0, "GM30");

	pmic_enable_interrupt(FG_BAT0_INT_L_NO, 0, "GM30");
	pmic_enable_interrupt(FG_BAT0_INT_H_NO, 0, "GM30");

	pmic_enable_interrupt(FG_N_CHARGE_L_NO, 0, "GM30");

	pmic_enable_interrupt(FG_IAVG_H_NO, 0, "GM30");
	pmic_enable_interrupt(FG_IAVG_L_NO, 0, "GM30");

	pmic_enable_interrupt(FG_ZCV_NO, 0, "GM30");

	pmic_enable_interrupt(FG_BAT_PLUGOUT_NO, 0, "GM30");
	pmic_enable_interrupt(FG_RG_INT_EN_NAG_C_DLTV, 0, "GM30");

	pmic_enable_interrupt(FG_RG_INT_EN_BAT_TEMP_H, 0, "GM30");
	pmic_enable_interrupt(FG_RG_INT_EN_BAT_TEMP_L, 0, "GM30");

	pmic_enable_interrupt(FG_RG_INT_EN_BAT2_H, 0, "GM30");
	pmic_enable_interrupt(FG_RG_INT_EN_BAT2_L, 0, "GM30");
	gm.disableGM30 = 1;
	gm.ui_soc = 50;
}

bool fg_interrupt_check(void)
{
	if (is_fg_disabled()) {
		disable_fg();
		return false;
	}
	return true;
}

signed int battery_meter_get_tempR(signed int dwVolt)
{

	int TRes;

	TRes = 0;

	if (is_fg_disabled())
		return 0;

	TRes = (gm.rbat.rbat_pull_up_r * dwVolt) /
		(gm.rbat.rbat_pull_up_volt - dwVolt);
	return TRes;
}

signed int battery_meter_get_tempV(void)
{

	int val = 0;

	if (is_fg_disabled())
		return 0;

	val = pmic_get_v_bat_temp();
	return val;
}

signed int battery_meter_get_VSense(void)
{
	if (is_fg_disabled())
		return 0;
	else
		return pmic_get_ibus();
}

void battery_update_psd(struct battery_data *bat_data)
{
	bat_data->BAT_batt_vol = battery_get_bat_voltage();
	bat_data->BAT_batt_temp = battery_get_bat_temperature();
}
#ifndef VENDOR_EDIT
/* ChaoYing.Chen@EXP.BSP.CHG.basic, 2017/06/17, Remove for charge driver */
static int battery_get_property(struct power_supply *psy,
	enum power_supply_property psp,
	union power_supply_propval *val)
{
	int ret = 0;
	int fgcurrent = 0;
	bool b_ischarging = 0;

	struct battery_data *data =
		container_of(psy->desc, struct battery_data, psd);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = data->BAT_STATUS;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = data->BAT_HEALTH;/* do not change before*/
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = data->BAT_PRESENT;/* do not change before*/
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = data->BAT_TECHNOLOGY;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (gm.fixed_uisoc != 0xffff)
			val->intval = gm.fixed_uisoc;
		else
			val->intval = data->BAT_CAPACITY;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		b_ischarging = gauge_get_current(&fgcurrent);
		if (b_ischarging == false)
			fgcurrent = 0 - fgcurrent;

		val->intval = fgcurrent * 100;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = gm.soc;
		/* using soc as charge_counter */
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = data->BAT_batt_vol * 1000;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = data->BAT_batt_temp * 10;
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* battery_data initialization */
struct battery_data battery_main = {
	.psd = {
		.name = "battery",
		.type = POWER_SUPPLY_TYPE_BATTERY,
		.properties = battery_props,
		.num_properties = ARRAY_SIZE(battery_props),
		.get_property = battery_get_property,
		},

	.BAT_STATUS = POWER_SUPPLY_STATUS_DISCHARGING,
	.BAT_HEALTH = POWER_SUPPLY_HEALTH_GOOD,
	.BAT_PRESENT = 1,
	.BAT_TECHNOLOGY = POWER_SUPPLY_TECHNOLOGY_LION,
	.BAT_CAPACITY = -1,
	.BAT_batt_vol = 0,
	.BAT_batt_temp = 0,
};

void evb_battery_init(void)
{
	battery_main.BAT_STATUS = POWER_SUPPLY_STATUS_FULL;
	battery_main.BAT_HEALTH = POWER_SUPPLY_HEALTH_GOOD;
	battery_main.BAT_PRESENT = 1;
	battery_main.BAT_TECHNOLOGY = POWER_SUPPLY_TECHNOLOGY_LION;
	battery_main.BAT_CAPACITY = 100;
	battery_main.BAT_batt_vol = 4200;
	battery_main.BAT_batt_temp = 22;
}

void battery_update(struct battery_data *bat_data)
{
	struct power_supply *bat_psy = bat_data->psy;

	battery_update_psd(&battery_main);
	bat_data->BAT_TECHNOLOGY = POWER_SUPPLY_TECHNOLOGY_LION;
	bat_data->BAT_HEALTH = POWER_SUPPLY_HEALTH_GOOD;
	bat_data->BAT_PRESENT = 1;

#if defined(CONFIG_MTK_DISABLE_GAUGE)
	return;
#endif

	if (is_fg_disabled())
		bat_data->BAT_CAPACITY = 50;

	power_supply_changed(bat_psy);
}
#endif /* VENDOR_EDIT */

bool is_kernel_power_off_charging(void)
{
	int boot_mode = get_boot_mode();

	if (boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT
	    || boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
		return true;
	}
	return false;
}

int bat_get_debug_level(void)
{
	return gm.log_level;
}

/* ============================================================ */
/* function prototype */
/* ============================================================ */
static void nl_send_to_user(u32 pid, int seq, struct fgd_nl_msg_t *reply_msg);

struct fuel_gauge_custom_data fg_cust_data;
struct fuel_gauge_table_custom_data fg_table_cust_data;


/* ============================================================ */
/* extern function */
/* ============================================================ */
static void proc_dump_log(struct seq_file *m)
{
	seq_printf(m, "subcmd:%d para1:%d\n",
		gm.proc_subcmd, gm.proc_subcmd_para1);
	seq_printf(m, "%s\n", gm.proc_log);
}

static void proc_dump_dtsi(struct seq_file *m)
{
	int i;

	seq_puts(m, "********** dump DTSI **********\n");
	seq_printf(m, "DIFFERENCE_FULLOCV_ITH = %d\n",
		fg_cust_data.difference_fullocv_ith);
	seq_printf(m, "Q_MAX_SYS_VOLTAGE_BAT = %d\n",
		fg_cust_data.q_max_sys_voltage);

	seq_printf(m, "SHUTDOWN_1_TIME = %d\n",
		fg_cust_data.shutdown_1_time);
	seq_printf(m, "KEEP_100_PERCENT = %d\n",
		fg_cust_data.keep_100_percent);
	seq_printf(m, "R_FG_VALUE = %d\n",
		fg_cust_data.r_fg_value);

	seq_printf(m, "Active Table :%d\n",
		fg_table_cust_data.active_table_number);

	for (i = 0; i < fg_table_cust_data.active_table_number; i++)
		seq_printf(m, "TEMPERATURE_%d = %d\n",
			i,
			fg_table_cust_data.fg_profile[i].temperature);

	for (i = 0; i < fg_table_cust_data.active_table_number; i++)
		seq_printf(m, "g_FG_PSEUDO100_%d = %d\n",
			i,
			fg_table_cust_data.fg_profile[i].pseudo100);


	seq_printf(m, "EMBEDDED_SEL = %d\n", fg_cust_data.embedded_sel);
	seq_printf(m, "PMIC_SHUTDOWN_CURRENT = %d\n",
		fg_cust_data.pmic_shutdown_current);
	seq_printf(m, "FG_METER_RESISTANCE = %d\n",
		fg_cust_data.fg_meter_resistance);
	seq_printf(m, "CAR_TUNE_VALUE = %d\n", fg_cust_data.car_tune_value);

	seq_printf(m, "pl_two_sec_reboot = %d\n", gm.pl_two_sec_reboot);
#ifdef SHUTDOWN_CONDITION_LOW_BAT_VOLT
	seq_puts(m, "SHUTDOWN_CONDITION_LOW_BAT_VOLT = 1\n");
	seq_printf(m, "lbat_def: %d %d %d\n",
		VBAT2_DET_VOLTAGE1, VBAT2_DET_VOLTAGE2, VBAT2_DET_VOLTAGE3);
	seq_printf(m, "lbat: %d %d %d\n", fg_cust_data.vbat2_det_voltage1,
		fg_cust_data.vbat2_det_voltage2,
		fg_cust_data.vbat2_det_voltage3);
#else
	seq_puts(m, "SHUTDOWN_CONDITION_LOW_BAT_VOLT = 0\n");
#endif

	seq_printf(m, "multi_temp_gauge0 = %d\n",
	fg_cust_data.multi_temp_gauge0);

	seq_printf(m, "hw_version = %d\n", gauge_get_hw_version());

}

static void dump_daemon_table(struct seq_file *m)
{
	int i, j;
	struct FUELGAUGE_PROFILE_STRUCT *ptr;
	struct fuel_gauge_table_custom_data *ptable2;
	struct fuel_gauge_table *pfgt;

	ptable2 = &gm.fg_data.fg_table_cust_data;

	for (j = 0; j < fg_table_cust_data.active_table_number; j++) {
		pfgt = &gm.fg_data.fg_table_cust_data.fg_profile[j];
		seq_printf(m, "daemon table idx:%d size:%d\n",
			j,
			pfgt->size);

		seq_printf(m,
			"tmp:%d qmax:%d %d pseudo1:%d pseudo100:%d\n",
			pfgt->temperature,
			pfgt->q_max,
			pfgt->q_max_h_current,
			pfgt->pseudo1,
			pfgt->pseudo100);

		seq_printf(m,
			"pmic_min_vol:%d pon_iboot:%d qmax_sys_vol:%d sd_hl_zcv:%d\n",
			pfgt->pmic_min_vol,
			pfgt->pon_iboot,
			pfgt->qmax_sys_vol,
			pfgt->shutdown_hl_zcv);

		seq_puts(m, "idx: maH, voltage, R1, R2, percentage\n");
		ptr = &ptable2->fg_profile[j].fg_profile[0];
		for (i = 0; i < 100; i++) {
			seq_printf(m, "%d: %d %d %d %d %d\n",
				i,
				ptr[i].mah,
				ptr[i].voltage,
				ptr[i].resistance,
				ptr[i].resistance2,
				ptr[i].percentage);
		}
	}

	seq_printf(m, "\ndaemon table idx:tmp0 size:%d\n",
		gm.fg_data.fg_table_cust_data.fg_profile_temperature_0_size);
	seq_puts(m, "idx: maH, voltage, R1, R2, percentage\n");
	ptr = &gm.fg_data.fg_table_cust_data.fg_profile_temperature_0[0];
	for (i = 0; i < 100; i++) {
		seq_printf(m, "%d: %d %d %d %d %d\n",
			i,
			ptr[i].mah,
			ptr[i].voltage,
			ptr[i].resistance,
			ptr[i].resistance2,
			ptr[i].percentage);

	}

	seq_printf(m, "\ndaemon table idx:tmp1 size:%d\n",
		gm.fg_data.fg_table_cust_data.fg_profile_temperature_1_size);
	seq_puts(m, "idx: maH, voltage, R, R2, percentage\n");
	ptr = &gm.fg_data.fg_table_cust_data.fg_profile_temperature_1[0];
	for (i = 0; i < 100; i++) {
		seq_printf(m, "%d: %d %d %d %d %d\n",
			i,
			ptr[i].mah,
			ptr[i].voltage,
			ptr[i].resistance,
			ptr[i].resistance2,
			ptr[i].percentage);
	}

}

static void dump_kernel_table(struct seq_file *m)
{
	int i, j;
	struct FUELGAUGE_PROFILE_STRUCT *ptr;
	struct fuel_gauge_table_custom_data *ptable1;
	struct fuel_gauge_table *pfgt;

	ptable1 = &fg_table_cust_data;

	seq_printf(m, "tables no:%d table size:%d\n",
		fg_table_cust_data.active_table_number,
		fg_table_cust_data.fg_profile[0].size);

	for (j = 0; j < fg_table_cust_data.active_table_number; j++) {
		pfgt = &ptable1->fg_profile[j];
		ptr = &ptable1->fg_profile[j].fg_profile[0];
		seq_printf(m, "table idx:%d size:%d\n",
			j,
			pfgt->size);

		seq_printf(m,
			"tmp:%d qmax:%d %d pseudo1:%d pseudo100:%d\n",
			pfgt->temperature,
			pfgt->q_max,
			pfgt->q_max_h_current,
			pfgt->pseudo1,
			pfgt->pseudo100);

		seq_printf(m,
			"pmic_min_vol:%d pon_iboot:%d qmax_sys_vol:%d sd_hl_zcv:%d\n",
			pfgt->pmic_min_vol,
			pfgt->pon_iboot,
			pfgt->qmax_sys_vol,
			pfgt->shutdown_hl_zcv);

		seq_puts(m, "idx: maH, voltage, R1, R2, percentage\n");
		for (i = 0; i < 100; i++) {
			seq_printf(m, "%d: %d %d %d %d %d\n",
				i,
				ptr[i].mah,
				ptr[i].voltage,
				ptr[i].resistance,
				ptr[i].resistance2,
				ptr[i].percentage);
		}
	}

	seq_puts(m, "\n");

	if (is_recovery_mode() == false) {
		dump_daemon_table(m);
	} else {

		ptr = &ptable1->fg_profile_temperature_1[0];
		seq_puts(m, "tmp1 idx: maH, voltage, R1, R2, percentage\n");
		for (i = 0; i < 100; i++) {
			seq_printf(m, "%d: %d %d %d %d %d\n",
				i,
				ptr[i].mah,
				ptr[i].voltage,
				ptr[i].resistance,
				ptr[i].resistance2,
				ptr[i].percentage);
		}
	}

}


static int proc_dump_log_show(struct seq_file *m, void *v)
{
	int i;

	seq_puts(m, "********** Gauge Dump **********\n");

	seq_puts(m, "Command Table list\n");
	seq_puts(m, "0: dump dtsi\n");
	seq_puts(m, "1: dump v-mode table\n");
	seq_puts(m, "101: dump gauge hw register\n");
	seq_puts(m, "102: kernel table\n");
	seq_puts(m, "103: send CHR FULL\n");
	seq_puts(m, "104: disable NAFG interrupt\n");
	seq_printf(m, "current command:%d\n", gm.proc_cmd_id);

	switch (gm.proc_cmd_id) {
	case 0:
		proc_dump_dtsi(m);
		break;
	case 1:
	case 2:
	case 3:
	case 4:
		wakeup_fg_algo_cmd(
			FG_INTR_KERNEL_CMD, FG_KERNEL_CMD_DUMP_LOG,
			gm.proc_cmd_id);
		for (i = 0; i < 5; i++) {
			msleep(500);
			if (gm.proc_subcmd_para1 == 1)
				break;
		}
		proc_dump_log(m);
		break;
	case 101:
		gauge_dev_dump(gm.gdev, m);
		break;
	case 102:
		dump_kernel_table(m);
		break;
	case 103:
		wakeup_fg_algo(FG_INTR_CHR_FULL);
		break;
	case 104:
		gauge_set_nag_en(false);
		gm.disable_nafg_int = true;
		break;
	default:
		seq_printf(m, "do not support command:%d\n", gm.proc_cmd_id);
		break;
	}



	/*battery_dump_info(m);*/

	return 0;
}

static ssize_t proc_write(
	struct file *file, const char __user *buffer,
	size_t count, loff_t *f_pos)
{
	int cmd = 0;
	char num[10];

	memset(num, 0, 10);

	if (!count)
		return 0;

	if (count > (sizeof(num) - 1))
		return -EINVAL;

	if (copy_from_user(num, buffer, count))
		return -EFAULT;

	if (kstrtoint(num, 10, &cmd) == 0)
		gm.proc_cmd_id = cmd;
	else {
		gm.proc_cmd_id = 0;
		return -EFAULT;
	}

	bm_err("proc_write success %d\n", cmd);
	return count;
}


static int proc_dump_log_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_dump_log_show, NULL);
}

static const struct file_operations battery_dump_log_proc_fops = {
	.open = proc_dump_log_open,
	.read = seq_read,
	.llseek	= seq_lseek,
	.write = proc_write,
};

void battery_debug_init(void)
{
	struct proc_dir_entry *battery_dir;

	battery_dir = proc_mkdir("battery", NULL);
	if (!battery_dir) {
		bm_err("fail to mkdir /proc/battery\n");
		return;
	}

	proc_create("dump_log", 0644,
		battery_dir, &battery_dump_log_proc_fops);
}

static ssize_t show_Battery_Temperature(
	struct device *dev, struct device_attribute *attr,
					       char *buf)
{
#ifndef VENDOR_EDIT
    /* ChaoYing.Chen@EXP.BSP.CHG.basic, 2017/06/17, Remove for charge driver */
	bm_err("show_Battery_Temperature: %d %d\n",
		battery_main.BAT_batt_temp, gm.fixed_bat_tmp);
#endif /* VENDOR_EDIT */

	return sprintf(buf, "%d\n", gm.fixed_bat_tmp);
}

static ssize_t store_Battery_Temperature(
	struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	signed int temp;

	if (kstrtoint(buf, 10, &temp) == 0) {

		gm.fixed_bat_tmp = temp;
		if (gm.fixed_bat_tmp == 0xffff)
			fg_bat_temp_int_internal();
		else {
			gauge_dev_enable_battery_tmp_lt_interrupt(
				gm.gdev, 0, 0);
			gauge_dev_enable_battery_tmp_ht_interrupt(
				gm.gdev, 0, 0);
			wakeup_fg_algo(FG_INTR_BAT_TMP_C_HT);
			wakeup_fg_algo(FG_INTR_BAT_TMP_HT);
		}
#ifndef VENDOR_EDIT
/* ChaoYing.Chen@EXP.BSP.CHG.basic, 2017/06/05, Remove for charge driver */

		battery_main.BAT_batt_temp = force_get_tbat(true);
		bm_err(
			"store_Battery_Temperature: fixed_bat_tmp:%d ,tmp:%d!\n",
			temp, battery_main.BAT_batt_temp);
		battery_update(&battery_main);
#endif /* VENDOR_EDIT */
	} else {
		bm_err("store_Battery_Temperature: format error!\n");
	}
	return size;
}

static DEVICE_ATTR(Battery_Temperature, 0664, show_Battery_Temperature,
		   store_Battery_Temperature);

static ssize_t show_UI_SOC(
	struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	bm_err("show_UI_SOC: %d %d\n",
		gm.ui_soc, gm.fixed_uisoc);
	return sprintf(buf, "%d\n", gm.fixed_uisoc);
}

static ssize_t store_UI_SOC(
	struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	signed int temp;

	if (kstrtoint(buf, 10, &temp) == 0) {

		gm.fixed_uisoc = temp;

		bm_err("store_UI_SOC: %d %d\n",
			gm.ui_soc, gm.fixed_uisoc);
#ifndef VENDOR_EDIT
/* ChaoYing.Chen@EXP.BSP.CHG.basic, 2017/06/05, Remove for charge driver */
		battery_update(&battery_main);
#endif
	}

	return size;
}

static DEVICE_ATTR(UI_SOC, 0664, show_UI_SOC,
		   store_UI_SOC);


/* ============================================================ */
/* Internal function */
/* ============================================================ */
void fg_custom_data_check(void)
{
	struct fuel_gauge_custom_data *p;

	p = &fg_cust_data;
	fgauge_get_profile_id();

	bm_err("FGLOG MultiGauge0[%d] BATID[%d] pmic_min_vol[%d,%d,%d,%d,%d]\n",
		p->multi_temp_gauge0, gm.battery_id,
		fg_table_cust_data.fg_profile[0].pmic_min_vol,
		fg_table_cust_data.fg_profile[1].pmic_min_vol,
		fg_table_cust_data.fg_profile[2].pmic_min_vol,
		fg_table_cust_data.fg_profile[3].pmic_min_vol,
		fg_table_cust_data.fg_profile[4].pmic_min_vol);
	bm_err("FGLOG pon_iboot[%d,%d,%d,%d,%d] qmax_sys_vol[%d %d %d %d %d]\n",
		fg_table_cust_data.fg_profile[0].pon_iboot,
		fg_table_cust_data.fg_profile[1].pon_iboot,
		fg_table_cust_data.fg_profile[2].pon_iboot,
		fg_table_cust_data.fg_profile[3].pon_iboot,
		fg_table_cust_data.fg_profile[4].pon_iboot,
		fg_table_cust_data.fg_profile[0].qmax_sys_vol,
		fg_table_cust_data.fg_profile[1].qmax_sys_vol,
		fg_table_cust_data.fg_profile[2].qmax_sys_vol,
		fg_table_cust_data.fg_profile[3].qmax_sys_vol,
		fg_table_cust_data.fg_profile[4].qmax_sys_vol);

}

int interpolation(int i1, int b1, int i2, int b2, int i)
{
	int ret;

	ret = (b2 - b1) * (i - i1) / (i2 - i1) + b1;

	return ret;
}

unsigned int TempConverBattThermistor(int temp)
{
	int RES1 = 0, RES2 = 0;
	int TMP1 = 0, TMP2 = 0;
	int i;
	unsigned int TBatt_R_Value = 0xffff;

	if (temp >= Fg_Temperature_Table[20].BatteryTemp) {
		TBatt_R_Value = Fg_Temperature_Table[20].TemperatureR;
	} else if (temp <= Fg_Temperature_Table[0].BatteryTemp) {
		TBatt_R_Value = Fg_Temperature_Table[0].TemperatureR;
	} else {
		RES1 = Fg_Temperature_Table[0].TemperatureR;
		TMP1 = Fg_Temperature_Table[0].BatteryTemp;

		for (i = 0; i <= 20; i++) {
			if (temp <= Fg_Temperature_Table[i].BatteryTemp) {
				RES2 = Fg_Temperature_Table[i].TemperatureR;
				TMP2 = Fg_Temperature_Table[i].BatteryTemp;
				break;
			}
			{	/* hidden else */
				RES1 = Fg_Temperature_Table[i].TemperatureR;
				TMP1 = Fg_Temperature_Table[i].BatteryTemp;
			}
		}


		TBatt_R_Value = interpolation(TMP1, RES1, TMP2, RES2, temp);
	}

	bm_warn(
		"[TempConverBattThermistor] [%d] %d %d %d %d %d\n",
		TBatt_R_Value, TMP1,
		RES1, TMP2, RES2, temp);

	return TBatt_R_Value;
}

int BattThermistorConverTemp(int Res)
{
	int i = 0;
	int RES1 = 0, RES2 = 0;
	int TBatt_Value = -200, TMP1 = 0, TMP2 = 0;

	if (Res >= Fg_Temperature_Table[0].TemperatureR) {
		TBatt_Value = -40;
	} else if (Res <= Fg_Temperature_Table[26].TemperatureR) {
		TBatt_Value = 90;
	} else {
		RES1 = Fg_Temperature_Table[0].TemperatureR;
		TMP1 = Fg_Temperature_Table[0].BatteryTemp;

		for (i = 0; i <= 26; i++) {
			if (Res >= Fg_Temperature_Table[i].TemperatureR) {
				RES2 = Fg_Temperature_Table[i].TemperatureR;
				TMP2 = Fg_Temperature_Table[i].BatteryTemp;
				break;
			}
			{	/* hidden else */
				RES1 = Fg_Temperature_Table[i].TemperatureR;
				TMP1 = Fg_Temperature_Table[i].BatteryTemp;
			}
		}

		TBatt_Value = (((Res - RES2) * TMP1) +
			((RES1 - Res) * TMP2)) / (RES1 - RES2);
	}
	bm_trace(
		"[BattThermistorConverTemp] %d %d %d %d %d %d\n",
		RES1, RES2, Res, TMP1,
		TMP2, TBatt_Value);

	return TBatt_Value;
}

unsigned int TempToBattVolt(int temp, int update)
{
	unsigned int R_NTC = TempConverBattThermistor(temp);
	long long Vin = 0;
	long long V_IR_comp = 0;
	/*int vbif28 = pmic_get_auxadc_value(AUXADC_LIST_VBIF);*/
	int vbif28 = gm.rbat.rbat_pull_up_volt;
	static int fg_current_temp;
	static bool fg_current_state;
	int fg_r_value = fg_cust_data.r_fg_value;
	int fg_meter_res_value = 0;

	if (NO_BAT_TEMP_COMPENSATE == 0)
		fg_meter_res_value = fg_cust_data.fg_meter_resistance;
	else
		fg_meter_res_value = 0;

#ifdef RBAT_PULL_UP_VOLT_BY_BIF
	vbif28 = pmic_get_vbif28_volt();
#endif
	Vin = (long long)R_NTC * vbif28 * 10;	/* 0.1 mV */

#if defined(__LP64__) || defined(_LP64)
	do_div(Vin, (R_NTC + gm.rbat.rbat_pull_up_r));
#else
	Vin = div_s64(Vin, (R_NTC + gm.rbat.rbat_pull_up_r));
#endif

	if (update == true)
		fg_current_state = gauge_get_current(&fg_current_temp);

	if (fg_current_state == true) {
		V_IR_comp = Vin;
		V_IR_comp +=
			((fg_current_temp *
			(fg_meter_res_value + fg_r_value)) / 10000);
	} else {
		V_IR_comp = Vin;
		V_IR_comp -=
			((fg_current_temp *
			(fg_meter_res_value + fg_r_value)) / 10000);
	}

	bm_notice("[TempToBattVolt] temp %d R_NTC %d V(%lld %lld) I %d CHG %d\n",
		temp, R_NTC, Vin, V_IR_comp, fg_current_temp, fg_current_state);

	return (unsigned int) V_IR_comp;
}

int BattVoltToTemp(int dwVolt, int volt_cali)
{
	long long TRes_temp;
	long long TRes;
	int sBaTTMP = -100;
	int vbif28 = gm.rbat.rbat_pull_up_volt;
	int delta_v;

	TRes_temp = (gm.rbat.rbat_pull_up_r * (long long) dwVolt);
#ifdef RBAT_PULL_UP_VOLT_BY_BIF
	vbif28 = pmic_get_vbif28_volt() + volt_cali;
	delta_v = abs(vbif28 - dwVolt);
	if (delta_v == 0)
		delta_v = 1;

#if defined(__LP64__) || defined(_LP64)
		do_div(TRes_temp, delta_v);
#else
		TRes_temp = div_s64(TRes_temp, delta_v);
#endif

	if (vbif28 > 3000 || vbif28 < 2500)
		bm_err(
			"[RBAT_PULL_UP_VOLT_BY_BIF] vbif28:%d\n",
			pmic_get_vbif28_volt());
#else
	delta_v = abs(gm.rbat.rbat_pull_up_volt - dwVolt);
	if (delta_v == 0)
		delta_v = 1;
#if defined(__LP64__) || defined(_LP64)
	do_div(TRes_temp, delta_v);
#else
	TRes_temp = div_s64(TRes_temp, delta_v);
#endif


#endif

#ifdef RBAT_PULL_DOWN_R
	TRes = (TRes_temp * RBAT_PULL_DOWN_R);

#if defined(__LP64__) || defined(_LP64)
		do_div(TRes, abs(RBAT_PULL_DOWN_R - TRes_temp));
#else
		TRes_temp = div_s64(TRes, abs(RBAT_PULL_DOWN_R - TRes_temp));
#endif

#else
	TRes = TRes_temp;
#endif

	/* convert register to temperature */
	if (!pmic_is_bif_exist())
		sBaTTMP = BattThermistorConverTemp((int)TRes);
	else
		sBaTTMP = BattThermistorConverTemp((int)TRes -
		gm.rbat.bif_ntc_r);

	bm_notice(
		"[BattVoltToTemp] %d %d %d %d\n",
		dwVolt, gm.rbat.rbat_pull_up_r,
		vbif28, volt_cali);
	return sBaTTMP;
}

int force_get_tbat_internal(bool update)
{
	int bat_temperature_volt = 0;
	int bat_temperature_val = 0;
	static int pre_bat_temperature_val = -1;
	int fg_r_value = 0;
	int fg_meter_res_value = 0;
	int fg_current_temp = 0;
	bool fg_current_state = false;
	int bat_temperature_volt_temp = 0;
	int vol_cali = 0;

	static int pre_bat_temperature_volt_temp, pre_bat_temperature_volt;
	static int pre_fg_current_temp;
	static int pre_fg_current_state;
	static int pre_fg_r_value;
	static int pre_bat_temperature_val2;
	static struct timespec pre_time;
	struct timespec ctime, dtime;

	if (is_battery_init_done() == false)
		return 25;

	if (gm.fixed_bat_tmp != 0xffff)
		return gm.fixed_bat_tmp;

	if (get_ec()->fixed_temp_en)
		return get_ec()->fixed_temp_value;

	if (update == true || pre_bat_temperature_val == -1) {
		/* Get V_BAT_Temperature */
		bat_temperature_volt = 2;
		bat_temperature_volt = pmic_get_v_bat_temp();

		if (bat_temperature_volt != 0) {
			fg_r_value = fg_cust_data.r_fg_value;
			if (NO_BAT_TEMP_COMPENSATE == 0)
				fg_meter_res_value =
					fg_cust_data.fg_meter_resistance;
			else
				fg_meter_res_value = 0;


			gauge_dev_get_current(
				gm.gdev, &fg_current_state, &fg_current_temp);
			fg_current_temp = fg_current_temp / 10;

			if (fg_current_state == true) {
				bat_temperature_volt_temp =
					bat_temperature_volt;
				bat_temperature_volt =
				bat_temperature_volt -
				((fg_current_temp *
					(fg_meter_res_value + fg_r_value))
						/ 10000);
				vol_cali =
					-((fg_current_temp *
					(fg_meter_res_value + fg_r_value))
						/ 10000);
			} else {
				bat_temperature_volt_temp =
					bat_temperature_volt;
				bat_temperature_volt =
				bat_temperature_volt +
				((fg_current_temp *
				(fg_meter_res_value + fg_r_value)) / 10000);
				vol_cali =
					((fg_current_temp *
					(fg_meter_res_value + fg_r_value))
					/ 10000);
			}

			bat_temperature_val =
				BattVoltToTemp(bat_temperature_volt, vol_cali);
		}

#ifdef CONFIG_MTK_BIF_SUPPORT
		/*	CHARGING_CMD_GET_BIF_TBAT need fix */
#endif
		bm_notice("[force_get_tbat] %d,%d,%d,%d,%d,%d r:%d %d %d\n",
		bat_temperature_volt_temp, bat_temperature_volt,
		fg_current_state, fg_current_temp,
		fg_r_value, bat_temperature_val,
		fg_meter_res_value, fg_r_value, NO_BAT_TEMP_COMPENSATE);

		if (pre_bat_temperature_val2 == 0) {
			pre_bat_temperature_volt_temp =
				bat_temperature_volt_temp;
			pre_bat_temperature_volt = bat_temperature_volt;
			pre_fg_current_temp = fg_current_temp;
			pre_fg_current_state = fg_current_state;
			pre_fg_r_value = fg_r_value;
			pre_bat_temperature_val2 = bat_temperature_val;
			get_monotonic_boottime(&pre_time);
		} else {
			get_monotonic_boottime(&ctime);
			dtime = timespec_sub(ctime, pre_time);

			if (((dtime.tv_sec <= 20) &&
				(abs(pre_bat_temperature_val2 -
				bat_temperature_val) >= 5)) ||
				bat_temperature_val >= 58) {
				bm_err(
				"[force_get_tbat][err] current:%d,%d,%d,%d,%d,%d pre:%d,%d,%d,%d,%d,%d\n",
					bat_temperature_volt_temp,
					bat_temperature_volt,
					fg_current_state,
					fg_current_temp,
					fg_r_value,
					bat_temperature_val,
					pre_bat_temperature_volt_temp,
					pre_bat_temperature_volt,
					pre_fg_current_state,
					pre_fg_current_temp,
					pre_fg_r_value,
					pre_bat_temperature_val2);
				/*pmic_auxadc_debug(1);*/
#ifndef VENDOR_EDIT
/* Fuchun.Liao@BSP.CHG.Basic 2018/01/20 modify to reduce log */

				WARN_ON(1);
#endif /* VENDOR_EDIT */

			}

			pre_bat_temperature_volt_temp =
				bat_temperature_volt_temp;
			pre_bat_temperature_volt = bat_temperature_volt;
			pre_fg_current_temp = fg_current_temp;
			pre_fg_current_state = fg_current_state;
			pre_fg_r_value = fg_r_value;
			pre_bat_temperature_val2 = bat_temperature_val;
			pre_time = ctime;
			bm_trace(
			"[force_get_tbat] current:%d,%d,%d,%d,%d,%d pre:%d,%d,%d,%d,%d,%d time:%d\n",
			bat_temperature_volt_temp, bat_temperature_volt,
			fg_current_state, fg_current_temp,
			fg_r_value, bat_temperature_val,
			pre_bat_temperature_volt_temp, pre_bat_temperature_volt,
			pre_fg_current_state, pre_fg_current_temp,
			pre_fg_r_value,
			pre_bat_temperature_val2, (int)dtime.tv_sec);
		}
	} else {
		bat_temperature_val = pre_bat_temperature_val;
	}

	return bat_temperature_val;
}

int force_get_tbat(bool update)
{
	int bat_temperature_val = 0;
	int counts = 0;
    int fgv;

	if (is_fg_disabled()) {
		bm_debug("[force_get_tbat] fixed TBAT=25 t\n");
		return 25;
	}

#if defined(FIXED_TBAT_25)
	bm_debug("[force_get_tbat] fixed TBAT=25 t\n");
	return 25;
#else

	bat_temperature_val = force_get_tbat_internal(update);

	while (counts < 5 && bat_temperature_val >= 60) {
		bm_err("[force_get_tbat]over60 count=%d, bat_temp=%d\n",
			counts, bat_temperature_val);
		bat_temperature_val = force_get_tbat_internal(true);
		counts++;
	}

	if (bat_temperature_val <=
		BATTERY_TMP_TO_DISABLE_GM30 && gm.disableGM30 == false) {
		bm_err(
			"battery temperature is too low %d and disable GM3.0\n",
			bat_temperature_val);
		disable_fg();
#ifndef VENDOR_EDIT
/* ChaoYing.Chen@EXP.BSP.CHG.basic, 2017/06/17, Remove for charge driver */
		if (gm.disableGM30 == true)
			battery_main.BAT_CAPACITY = 50;
		battery_update(&battery_main);
#endif /* VENDOR_EDIT */

	}

	if (bat_temperature_val <= BATTERY_TMP_TO_DISABLE_NAFG) {

		fgv = gauge_get_hw_version();
		if (fgv >= GAUGE_HW_V1000
		&& fgv < GAUGE_HW_V2000) {
			en_intr_VBATON_UNDET(0);
		}

		gm.ntc_disable_nafg = true;
		bm_err("[force_get_tbat] ntc_disable_nafg %d %d\n",
			bat_temperature_val,
			DEFAULT_BATTERY_TMP_WHEN_DISABLE_NAFG);

#ifndef VENDOR_EDIT
		/* tongfeng.huang@EXP.BSP.CHG.basic, 2018/01/25, do not return 25 when disconnect ntc pin */
		return DEFAULT_BATTERY_TMP_WHEN_DISABLE_NAFG;
#else
		return bat_temperature_val;
#endif

	}

	gm.ntc_disable_nafg = false;
	return bat_temperature_val;
#endif
}

unsigned int battery_meter_get_fg_time(void)
{
	unsigned int time;

	gauge_dev_get_time(gm.gdev, &time);
	return time;
}

unsigned int battery_meter_enable_time_interrupt(unsigned int sec)
{
	return gauge_dev_enable_time_interrupt(gm.gdev, sec);
}

/* ============================================================ */
/* Internal function */
/* ============================================================ */
void update_fg_dbg_tool_value(void)
{
	/* Todo: backup variables */
}

static void nl_send_to_user(u32 pid, int seq, struct fgd_nl_msg_t *reply_msg)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	/* int size=sizeof(struct fgd_nl_msg_t); */
	int size = reply_msg->fgd_data_len + FGD_NL_MSG_T_HDR_LEN;
	int len = NLMSG_SPACE(size);
	void *data;
	int ret;

	reply_msg->identity = FGD_NL_MAGIC;

	skb = alloc_skb(len, GFP_ATOMIC);
	if (!skb)
		return;

	nlh = nlmsg_put(skb, pid, seq, 0, size, 0);
	data = NLMSG_DATA(nlh);
	memcpy(data, reply_msg, size);
	NETLINK_CB(skb).portid = 0;	/* from kernel */
	NETLINK_CB(skb).dst_group = 0;	/* unicast */

	ret = netlink_unicast(gm.daemo_nl_sk, skb, pid, MSG_DONTWAIT);
	if (ret < 0) {
		bm_err("[Netlink] send failed %d\n", ret);
		return;
	}
	/*bm_debug("[Netlink] reply_user: netlink_unicast- ret=%d\n", ret); */


}

static void nl_data_handler(struct sk_buff *skb)
{
	u32 pid;
	kuid_t uid;
	int seq;
	void *data;
	struct nlmsghdr *nlh;
	struct fgd_nl_msg_t *fgd_msg, *fgd_ret_msg;
	int size = 0;

	nlh = (struct nlmsghdr *)skb->data;
	pid = NETLINK_CREDS(skb)->pid;
	uid = NETLINK_CREDS(skb)->uid;
	seq = nlh->nlmsg_seq;

	data = NLMSG_DATA(nlh);

	fgd_msg = (struct fgd_nl_msg_t *)data;

	if (fgd_msg->identity != FGD_NL_MAGIC) {
		bm_err("[FGERR]not correct MTKFG netlink packet!%d\n",
			fgd_msg->identity);
		return;
	}

	if (gm.g_fgd_pid != pid &&
		fgd_msg->fgd_cmd > FG_DAEMON_CMD_SET_DAEMON_PID) {
		bm_err("drop rev netlink pid:%d:%d  cmd:%d:%d\n",
			pid,
			gm.g_fgd_pid,
			fgd_msg->fgd_cmd,
			FG_DAEMON_CMD_SET_DAEMON_PID);
		return;
	}

	size = fgd_msg->fgd_ret_data_len + FGD_NL_MSG_T_HDR_LEN;

	fgd_ret_msg = vmalloc(size);
	if (!fgd_ret_msg)
		return;

	memset(fgd_ret_msg, 0, size);

	bmd_ctrl_cmd_from_user(data, fgd_ret_msg);
	nl_send_to_user(pid, seq, fgd_ret_msg);

	vfree(fgd_ret_msg);
}

int wakeup_fg_algo(unsigned int flow_state)
{
	update_fg_dbg_tool_value();

	if (gm.disableGM30) {
		bm_err("FG daemon is disabled\n");
		return -1;
	}

	if (is_recovery_mode()) {
		wakeup_fg_algo_recovery(flow_state);
		return 0;
	}

	gm3_log_notify(flow_state);

	if (gm.g_fgd_pid != 0) {
		struct fgd_nl_msg_t *fgd_msg;
		int size = FGD_NL_MSG_T_HDR_LEN + sizeof(flow_state);

		fgd_msg = vmalloc(size);
		if (!fgd_msg) {
/* bm_err("Error: wakeup_fg_algo() vmalloc fail!!!\n"); */
			return -1;
		}

		bm_debug("[wakeup_fg_algo] malloc size=%d pid=%d cmd:%d\n",
			size, gm.g_fgd_pid, flow_state);
		memset(fgd_msg, 0, size);
		fgd_msg->fgd_cmd = FG_DAEMON_CMD_NOTIFY_DAEMON;
		memcpy(fgd_msg->fgd_data, &flow_state, sizeof(flow_state));
		fgd_msg->fgd_data_len += sizeof(flow_state);
		nl_send_to_user(gm.g_fgd_pid, 0, fgd_msg);
		vfree(fgd_msg);
		return 0;
	} else {
		return -1;
	}
}

int wakeup_fg_algo_cmd(unsigned int flow_state, int cmd, int para1)
{
	update_fg_dbg_tool_value();

	if (gm.disableGM30) {
		bm_err("FG daemon is disabled\n");
		return -1;
	}

	if (is_recovery_mode()) {
		wakeup_fg_algo_recovery(flow_state);
		return 0;
	}

	gm3_log_notify(flow_state);

	if (gm.g_fgd_pid != 0) {
		struct fgd_nl_msg_t *fgd_msg;
		int size = FGD_NL_MSG_T_HDR_LEN + sizeof(flow_state);

		fgd_msg = vmalloc(size);
		if (!fgd_msg) {
/* bm_err("Error: wakeup_fg_algo() vmalloc fail!!!\n"); */
			return -1;
		}

		bm_debug(
			"[wakeup_fg_algo] malloc size=%d pid=%d cmd:%d\n",
			size, gm.g_fgd_pid, flow_state);
		memset(fgd_msg, 0, size);
		fgd_msg->fgd_cmd = FG_DAEMON_CMD_NOTIFY_DAEMON;
		fgd_msg->fgd_subcmd = cmd;
		fgd_msg->fgd_subcmd_para1 = para1;
		memcpy(fgd_msg->fgd_data, &flow_state, sizeof(flow_state));
		fgd_msg->fgd_data_len += sizeof(flow_state);
		nl_send_to_user(gm.g_fgd_pid, 0, fgd_msg);
		vfree(fgd_msg);
		return 0;
	} else {
		return -1;
	}
}

int wakeup_fg_algo_atomic(unsigned int flow_state)
{
	update_fg_dbg_tool_value();

	if (gm.disableGM30) {
		bm_err("FG daemon is disabled\n");
		return -1;
	}

	if (is_recovery_mode()) {
		wakeup_fg_algo_recovery(flow_state);
		return 0;
	}

	gm3_log_notify(flow_state);

	if (gm.g_fgd_pid != 0) {
		struct fgd_nl_msg_t *fgd_msg;
		int size = FGD_NL_MSG_T_HDR_LEN + sizeof(flow_state);

		fgd_msg = kmalloc(size, GFP_ATOMIC);
		if (!fgd_msg) {
/* bm_err("Error: wakeup_fg_algo() vmalloc fail!!!\n"); */
			return -1;
		}

		bm_debug(
			"[wakeup_fg_algo] malloc size=%d pid=%d cmd:%d\n",
			size, gm.g_fgd_pid, flow_state);
		memset(fgd_msg, 0, size);
		fgd_msg->fgd_cmd = FG_DAEMON_CMD_NOTIFY_DAEMON;
		memcpy(fgd_msg->fgd_data, &flow_state, sizeof(flow_state));
		fgd_msg->fgd_data_len += sizeof(flow_state);
		nl_send_to_user(gm.g_fgd_pid, 0, fgd_msg);
		kfree(fgd_msg);
		return 0;
	} else {
		return -1;
	}
}


int fg_get_battery_temperature_for_zcv(void)
{
#ifndef VENDOR_EDIT
	/* ChaoYing.Chen@EXP.BSP.CHG.basic, 2017/06/05, Remove for charge driver */
	return battery_main.BAT_batt_temp;
#else /* VENDOR_EDIT */
	return  25;
#endif /* VENDOR_EDIT */

}

int battery_get_charger_zcv(void)
{
	u32 zcv = 0;

	charger_manager_get_zcv(gm.pbat_consumer, MAIN_CHARGER, &zcv);
	return zcv;
}

void fg_ocv_query_soc(int ocv)
{
	if (ocv > 50000 || ocv <= 0)
		return;

	gm.algo_req_ocv = ocv;
	wakeup_fg_algo_cmd(
		FG_INTR_KERNEL_CMD, FG_KERNEL_CMD_REQ_ALGO_DATA,
		ocv);

	bm_trace("[fg_ocv_query_soc] ocv:%d\n", ocv);
}

void exec_BAT_EC(int cmd, int param)
{
	int i;

	bm_err("exe_BAT_EC cmd %d, param %d\n", cmd, param);
	switch (cmd) {
	case 101:
		{
			/* Force Temperature, force_get_tbat*/
			if (param == 0xff) {
				get_ec()->fixed_temp_en = 0;
				get_ec()->fixed_temp_value = 25;
				bm_err("exe_BAT_EC cmd %d, param %d, disable\n",
					cmd, param);

			} else {
				get_ec()->fixed_temp_en = 1;
				if (param >= 100)
					get_ec()->fixed_temp_value =
						0 - (param - 100);
				else
					get_ec()->fixed_temp_value = param;
				bm_err("exe_BAT_EC cmd %d, param %d, enable\n",
					cmd, param);
			}
		}
		break;
	case 102:
		{
			/* force PTIM RAC */
			if (param == 0xff) {
				get_ec()->debug_rac_en = 0;
				get_ec()->debug_rac_value = 0;
				bm_err("exe_BAT_EC cmd %d, param %d, disable\n",
					cmd, param);

			} else {
				get_ec()->debug_rac_en = 1;
				get_ec()->debug_rac_value = param;
				bm_err("exe_BAT_EC cmd %d, param %d, enable\n",
					cmd, param);
			}
		}
		break;
	case 103:
		{
			/* force PTIM V */
			if (param == 0xff) {
				get_ec()->debug_ptim_v_en = 0;
				get_ec()->debug_ptim_v_value = 0;
				bm_err("exe_BAT_EC cmd %d, param %d, disable\n",
					cmd, param);

			} else {
				get_ec()->debug_ptim_v_en = 1;
				get_ec()->debug_ptim_v_value = param;
				bm_err("exe_BAT_EC cmd %d, param %d, enable\n",
					cmd, param);
			}

		}
		break;
	case 104:
		{
			/* force PTIM R_current */
			if (param == 0xff) {
				get_ec()->debug_ptim_r_en = 0;
				get_ec()->debug_ptim_r_value = 0;
				bm_err("exe_BAT_EC cmd %d, param %d, disable\n",
					cmd, param);

			} else {
				get_ec()->debug_ptim_r_en = 1;
				get_ec()->debug_ptim_r_value = param;
				bm_err("exe_BAT_EC cmd %d, param %d, enable\n",
					cmd, param);
			}
		}
		break;
	case 105:
		{
			/* force interrupt trigger */
			switch (param) {
			case 1:
				{
					wakeup_fg_algo(FG_INTR_TIMER_UPDATE);
				}
				break;
			case 4096:
				{
					wakeup_fg_algo(FG_INTR_NAG_C_DLTV);
				}
				break;
			case 8192:
				{
					wakeup_fg_algo(FG_INTR_FG_ZCV);
				}
				break;
			case 32768:
				{
					wakeup_fg_algo(FG_INTR_RESET_NVRAM);
				}
				break;
			case 65536:
				{
					wakeup_fg_algo(FG_INTR_BAT_PLUGOUT);
				}
				break;


			default:
				{

				}
				break;
			}
			bm_err(
				"exe_BAT_EC cmd %d, param %d, force interrupt\n",
				cmd, param);
		}
		break;
	case 106:
		{
			/* force FG Current */
			if (param == 0xff) {
				get_ec()->debug_fg_curr_en = 0;
				get_ec()->debug_fg_curr_value = 0;
				bm_err(
					"exe_BAT_EC cmd %d, param %d, disable\n",
					cmd, param);

			} else {
				get_ec()->debug_fg_curr_en = 1;
				get_ec()->debug_fg_curr_value = param;
				bm_err(
					"exe_BAT_EC cmd %d, param %d, enable\n",
					cmd, param);
			}
		}
		break;
	case 107:
		{
			/* force Battery ID */
			if (param == 0xff) {
				get_ec()->debug_bat_id_en = 0;
				get_ec()->debug_bat_id_value = 0;
				bm_err(
					"exe_BAT_EC cmd %d, param %d, disable\n",
					cmd, param);
			} else {
				get_ec()->debug_bat_id_en = 1;
				get_ec()->debug_bat_id_value = param;
				bm_err(
					"exe_BAT_EC cmd %d, param %d, enable\n",
					cmd, param);
				fg_custom_init_from_header();
			}
		}
		break;
	case 108:
		{
			/* Set D0_C_CUST */
			if (param == 0xff) {
				get_ec()->debug_d0_c_en = 0;
				get_ec()->debug_d0_c_value = 0;
				bm_err(
					"exe_BAT_EC cmd %d, param %d, disable\n",
					cmd, param);

			} else {
				get_ec()->debug_d0_c_en = 1;
				get_ec()->debug_d0_c_value = param;
				bm_err(
					"exe_BAT_EC cmd %d, param %d, enable\n",
					cmd, param);
			}
		}
		break;
	case 109:
		{
			/* Set D0_V_CUST */
			if (param == 0xff) {
				get_ec()->debug_d0_v_en = 0;
				get_ec()->debug_d0_v_value = 0;
				bm_err(
					"exe_BAT_EC cmd %d, param %d, disable\n",
					cmd, param);

			} else {
				get_ec()->debug_d0_v_en = 1;
				get_ec()->debug_d0_v_value = param;
				bm_err(
					"exe_BAT_EC cmd %d, param %d, enable\n",
					cmd, param);
			}
		}
		break;
	case 110:
		{
			/* Set UISOC_CUST */
			if (param == 0xff) {
				get_ec()->debug_uisoc_en = 0;
				get_ec()->debug_uisoc_value = 0;
				bm_err(
					"exe_BAT_EC cmd %d, param %d, disable\n",
					cmd, param);

			} else {
				get_ec()->debug_uisoc_en = 1;
				get_ec()->debug_uisoc_value = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, enable\n",
				cmd, param);
			}
		}
		break;

	case 701:
		{
			fg_cust_data.pseudo1_en = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, pseudo1_en\n",
				cmd, param);
		}
		break;
	case 702:
		{
			fg_cust_data.pseudo100_en = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, pseudo100_en\n",
				cmd, param);
		}
		break;
	case 703:
		{
			fg_cust_data.qmax_sel = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, qmax_sel\n",
				cmd, param);
		}
		break;
	case 704:
		{
			fg_cust_data.iboot_sel = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, iboot_sel\n",
				cmd, param);
		}
		break;
	case 705:
		{
			for (i = 0;
				i < fg_table_cust_data.active_table_number;
				i++) {
				fg_table_cust_data.fg_profile[i].pmic_min_vol =
					param * UNIT_TRANS_10;
			}
			bm_err(
				"exe_BAT_EC cmd %d, param %d, pmic_min_vol\n",
				cmd, param);
		}
		break;
	case 706:
		{
			for (i = 0;
				i < fg_table_cust_data.active_table_number;
				i++) {
				fg_table_cust_data.fg_profile[i].pon_iboot =
				param * UNIT_TRANS_10;
			}
			bm_err("exe_BAT_EC cmd %d, param %d, poweron_system_iboot\n"
				, cmd, param * UNIT_TRANS_10);
		}
		break;
	case 707:
		{
			fg_cust_data.shutdown_system_iboot = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, shutdown_system_iboot\n",
				cmd, param);
		}
		break;
	case 708:
		{
			fg_cust_data.fg_meter_resistance = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, fg_meter_resistance\n",
				cmd, param);
		}
		break;
	case 709:
		{
			fg_cust_data.r_fg_value = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, r_fg_value\n",
				cmd, param);
		}
		break;
	case 710:
		{
			fg_cust_data.q_max_sys_voltage = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, q_max_sys_voltage\n",
				cmd, param);
		}
		break;
	case 711:
		{
			fg_cust_data.loading_1_en = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, loading_1_en\n",
				cmd, param);
		}
		break;
	case 712:
		{
			fg_cust_data.loading_2_en = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, loading_2_en\n",
				cmd, param);
		}
		break;
	case 713:
		{
			fg_cust_data.aging_temp_diff = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, aging_temp_diff\n",
				cmd, param);
		}
		break;
	case 714:
		{
			fg_cust_data.aging1_load_soc = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, aging1_load_soc\n",
				cmd, param);
		}
		break;
	case 715:
		{
			fg_cust_data.aging1_update_soc = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, aging1_update_soc\n",
				cmd, param);
		}
		break;
	case 716:
		{
			fg_cust_data.aging_100_en = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, aging_100_en\n",
				cmd, param);
		}
		break;
	case 717:
		{
			fg_table_cust_data.active_table_number = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, additional_battery_table_en\n",
				cmd, param);
		}
		break;
	case 718:
		{
			fg_cust_data.d0_sel = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, d0_sel\n",
				cmd, param);
		}
		break;
	case 719:
		{
			fg_cust_data.zcv_car_gap_percentage = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, zcv_car_gap_percentage\n",
				cmd, param);
		}
		break;
	case 720:
		{
			fg_cust_data.multi_temp_gauge0 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.multi_temp_gauge0=%d\n",
				cmd, param);
		}
		break;
	case 721:
		{
			fg_custom_data_check();
			bm_err("exe_BAT_EC cmd %d", cmd);
		}
		break;
	case 724:
		{
			fg_table_cust_data.fg_profile[0].pseudo100 =
				UNIT_TRANS_100 * param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.pseudo100_t0=%d\n",
				cmd, param);
		}
		break;
	case 725:
		{
			fg_table_cust_data.fg_profile[1].pseudo100 =
				UNIT_TRANS_100 * param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.pseudo100_t1=%d\n",
				cmd, param);
		}
		break;
	case 726:
		{
			fg_table_cust_data.fg_profile[2].pseudo100 =
				UNIT_TRANS_100 * param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.pseudo100_t2=%d\n",
				cmd, param);
		}
		break;
	case 727:
		{
			fg_table_cust_data.fg_profile[3].pseudo100 =
				UNIT_TRANS_100 * param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.pseudo100_t3=%d\n",
				cmd, param);
		}
		break;
	case 728:
		{
			fg_table_cust_data.fg_profile[4].pseudo100 =
				UNIT_TRANS_100 * param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.pseudo100_t4=%d\n",
				cmd, param);
		}
		break;
	case 729:
		{
			fg_cust_data.keep_100_percent = UNIT_TRANS_100 * param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.keep_100_percent=%d\n",
				cmd, param);
		}
		break;
	case 730:
		{
			fg_cust_data.ui_full_limit_en = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.ui_full_limit_en=%d\n",
				cmd, param);
		}
		break;
	case 731:
		{
			fg_cust_data.ui_full_limit_soc0 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.ui_full_limit_soc0=%d\n",
				cmd, param);
		}
		break;
	case 732:
		{
			fg_cust_data.ui_full_limit_ith0 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.ui_full_limit_ith0=%d\n",
				cmd, param);
		}
		break;
	case 733:
		{
			fg_cust_data.ui_full_limit_soc1 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.ui_full_limit_soc1=%d\n",
				cmd, param);
		}
		break;
	case 734:
		{
			fg_cust_data.ui_full_limit_ith1 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.ui_full_limit_ith1=%d\n",
				cmd, param);
		}
		break;
	case 735:
		{
			fg_cust_data.ui_full_limit_soc2 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.ui_full_limit_soc2=%d\n",
				cmd, param);
		}
		break;
	case 736:
		{
			fg_cust_data.ui_full_limit_ith2 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.ui_full_limit_ith2=%d\n",
				cmd, param);
		}
		break;
	case 737:
		{
			fg_cust_data.ui_full_limit_soc3 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.ui_full_limit_soc3=%d\n",
				cmd, param);
		}
		break;
	case 738:
		{
			fg_cust_data.ui_full_limit_ith3 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.ui_full_limit_ith3=%d\n",
				cmd, param);
		}
		break;
	case 739:
		{
			fg_cust_data.ui_full_limit_soc4 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.ui_full_limit_soc4=%d\n",
				cmd, param);
		}
		break;
	case 740:
		{
			fg_cust_data.ui_full_limit_ith4 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.ui_full_limit_ith4=%d\n",
				cmd, param);
		}
		break;
	case 741:
		{
			fg_cust_data.ui_full_limit_time = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.ui_full_limit_time=%d\n",
				cmd, param);
		}
		break;
	case 743:
		{
			fg_table_cust_data.fg_profile[0].pmic_min_vol = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.pmic_min_vol_t0=%d\n",
				cmd, param);
		}
		break;
	case 744:
		{
			fg_table_cust_data.fg_profile[1].pmic_min_vol = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.pmic_min_vol_t1=%d\n",
				cmd, param);
		}
		break;
	case 745:
		{
			fg_table_cust_data.fg_profile[2].pmic_min_vol = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.pmic_min_vol_t2=%d\n",
				cmd, param);
		}
		break;
	case 746:
		{
			fg_table_cust_data.fg_profile[3].pmic_min_vol = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.pmic_min_vol_t3=%d\n",
				cmd, param);
		}
		break;
	case 747:
		{
			fg_table_cust_data.fg_profile[4].pmic_min_vol = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.pmic_min_vol_t4=%d\n",
				cmd, param);
		}
		break;
	case 748:
		{
			fg_table_cust_data.fg_profile[0].pon_iboot = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.pon_iboot_t0=%d\n",
				cmd, param);
		}
		break;
	case 749:
		{
			fg_table_cust_data.fg_profile[1].pon_iboot = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.pon_iboot_t1=%d\n",
				cmd, param);
		}
		break;
	case 750:
		{
			fg_table_cust_data.fg_profile[2].pon_iboot = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.pon_iboot_t2=%d\n",
				cmd, param);
		}
		break;
	case 751:
		{
			fg_table_cust_data.fg_profile[3].pon_iboot = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.pon_iboot_t3=%d\n",
				cmd, param);
		}
		break;
	case 752:
		{
			fg_table_cust_data.fg_profile[4].pon_iboot = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.pon_iboot_t4=%d\n",
				cmd, param);
		}
		break;
	case 753:
		{
			fg_table_cust_data.fg_profile[0].qmax_sys_vol = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.qmax_sys_vol_t0=%d\n",
				cmd, param);
		}
		break;
	case 754:
		{
			fg_table_cust_data.fg_profile[1].qmax_sys_vol = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.qmax_sys_vol_t1=%d\n",
				cmd, param);
		}
		break;
	case 755:
		{
			fg_table_cust_data.fg_profile[2].qmax_sys_vol = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.qmax_sys_vol_t2=%d\n",
				cmd, param);
		}
		break;
	case 756:
		{
			fg_table_cust_data.fg_profile[3].qmax_sys_vol = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.qmax_sys_vol_t3=%d\n",
				cmd, param);
		}
		break;
	case 757:
		{
			fg_table_cust_data.fg_profile[4].qmax_sys_vol = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.qmax_sys_vol_t4=%d\n",
				cmd, param);
		}
		break;
	case 758:
		{
			fg_cust_data.nafg_ratio = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.nafg_ratio=%d\n",
				cmd, param);
		}
		break;
	case 759:
		{
			fg_cust_data.nafg_ratio_en = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.nafg_ratio_en=%d\n",
				cmd, param);
		}
		break;
	case 760:
		{
			fg_cust_data.nafg_ratio_tmp_thr = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.nafg_ratio_tmp_thr=%d\n",
				cmd, param);
		}
		break;
	case 761:
		{
			wakeup_fg_algo(FG_INTR_BAT_CYCLE);
			bm_err(
				"exe_BAT_EC cmd %d, bat_cycle intr\n", cmd);
		}
		break;
	case 762:
		{
			fg_cust_data.difference_fgc_fgv_th1 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.difference_fgc_fgv_th1=%d\n",
				cmd, param);
		}
		break;
	case 763:
		{
			fg_cust_data.difference_fgc_fgv_th2 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.difference_fgc_fgv_th1=%d\n",
				cmd, param);
		}
		break;
	case 764:
		{
			fg_cust_data.difference_fgc_fgv_th3 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.difference_fgc_fgv_th3=%d\n",
				cmd, param);
		}
		break;
	case 765:
		{
			fg_cust_data.difference_fullocv_ith = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.difference_fullocv_ith=%d\n",
				cmd, param);
		}
		break;
	case 766:
		{
			fg_cust_data.difference_fgc_fgv_th_soc1 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.difference_fgc_fgv_th_soc1=%d\n",
				cmd, param);
		}
		break;
	case 767:
		{
			fg_cust_data.difference_fgc_fgv_th_soc2 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.difference_fgc_fgv_th_soc2=%d\n",
				cmd, param);
		}
		break;
	case 768:
		{
			fg_cust_data.embedded_sel = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.embedded_sel=%d\n",
				cmd, param);
		}
		break;
	case 769:
		{
			fg_cust_data.car_tune_value = param * UNIT_TRANS_10;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.car_tune_value=%d\n",
				cmd, param * UNIT_TRANS_10);
		}
		break;
	case 770:
		{
			fg_cust_data.shutdown_1_time = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.shutdown_1_time=%d\n",
				cmd, param);
		}
		break;
	case 771:
		{
			fg_cust_data.tnew_told_pon_diff = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.tnew_told_pon_diff=%d\n",
				cmd, param);
		}
		break;
	case 772:
		{
			fg_cust_data.tnew_told_pon_diff2 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.tnew_told_pon_diff2=%d\n",
				cmd, param);
		}
		break;
	case 773:
		{
			fg_cust_data.swocv_oldocv_diff = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.swocv_oldocv_diff=%d\n",
				cmd, param);
		}
		break;
	case 774:
		{
			fg_cust_data.hwocv_oldocv_diff = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.hwocv_oldocv_diff=%d\n",
				cmd, param);
		}
		break;
	case 775:
		{
			fg_cust_data.hwocv_swocv_diff = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.hwocv_swocv_diff=%d\n",
				cmd, param);
		}
		break;
	case 776:
		{
			get_ec()->debug_kill_daemontest = param;
			bm_err(
				"exe_BAT_EC cmd %d, debug_kill_daemontest=%d\n",
				cmd, param);
		}
		break;
	case 777:
		{
			fg_cust_data.swocv_oldocv_diff_emb = param;
			bm_err("exe_BAT_EC cmd %d, swocv_oldocv_diff_emb=%d\n",
				cmd, param);
		}
		break;
	case 778:
		{
			fg_cust_data.vir_oldocv_diff_emb_lt = param;
			bm_err(
				"exe_BAT_EC cmd %d, vir_oldocv_diff_emb_lt=%d\n",
				cmd, param);
		}
		break;
	case 779:
		{
			fg_cust_data.vir_oldocv_diff_emb_tmp = param;
			bm_err(
				"exe_BAT_EC cmd %d, vir_oldocv_diff_emb_tmp=%d\n",
				cmd, param);
		}
		break;
	case 780:
		{
			fg_cust_data.vir_oldocv_diff_emb = param;
			bm_err("exe_BAT_EC cmd %d, vir_oldocv_diff_emb=%d\n",
				cmd, param);
		}
		break;
	case 781:
		{
			get_ec()->debug_kill_daemontest = 1;
			fg_cust_data.dod_init_sel = 12;
			bm_err("exe_BAT_EC cmd %d,force goto dod_init12=%d\n",
				cmd, param);
		}
		break;
	case 782:
		{
			fg_cust_data.charge_pseudo_full_level = param;
			bm_err(
				"exe_BAT_EC cmd %d,fg_cust_data.charge_pseudo_full_level=%d\n",
				cmd, param);
		}
		break;
	case 783:
		{
			fg_cust_data.full_tracking_bat_int2_multiply = param;
			bm_err(
				"exe_BAT_EC cmd %d,fg_cust_data.full_tracking_bat_int2_multiply=%d\n",
				cmd, param);
		}
		break;
	case 784:
		{
			bm_err(
				"exe_BAT_EC cmd %d,DAEMON_PID=%d\n",
				cmd, gm.g_fgd_pid);
		}
		break;
	case 785:
		{
			gm.bat_cycle_thr = param;
			bm_err(
				"exe_BAT_EC cmd %d,thr=%d\n",
				cmd, gm.bat_cycle_thr);
		}
		break;
	case 786:
		{
			gm.bat_cycle_ncar = param;

			bm_err(
				"exe_BAT_EC cmd %d,thr=%d\n",
				cmd, gm.bat_cycle_ncar);
		}
		break;
	case 787:
		{
			fg_ocv_query_soc(param);
			bm_err(
				"exe_BAT_EC cmd %d,[fg_ocv_query_soc]ocv=%d\n",
				cmd, param);
		}
		break;


	default:
		bm_err(
			"exe_BAT_EC cmd %d, param %d, default\n",
			cmd, param);
		break;
	}

}

static ssize_t show_FG_daemon_disable(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	bm_trace("[FG] show FG disable : %d\n", gm.disableGM30);
	return sprintf(buf, "%d\n", gm.disableGM30);
}

static ssize_t store_FG_daemon_disable(
	struct device *dev, struct device_attribute *attr,
					const char *buf, size_t size)
{
	bm_err("[disable FG daemon]\n");
	disable_fg();
#ifndef VENDOR_EDIT
/* ChaoYing.Chen@EXP.BSP.CHG.basic, 2017/06/05, Remove for charge driver */
	if (gm.disableGM30 == true)
		battery_main.BAT_CAPACITY = 50;
	battery_update(&battery_main);
#endif /* VENDOR_EDIT */

	return size;
}
static DEVICE_ATTR(
	FG_daemon_disable, 0664,
	show_FG_daemon_disable, store_FG_daemon_disable);

static ssize_t show_FG_meter_resistance(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	bm_trace(
		"[FG] show fg_meter_resistance : %d\n",
		fg_cust_data.fg_meter_resistance);
	return sprintf(buf, "%d\n", fg_cust_data.fg_meter_resistance);
}

static ssize_t store_FG_meter_resistance(
	struct device *dev, struct device_attribute *attr,
					const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret;

	bm_err("[store_FG_meter_resistance]\n");

	if (buf != NULL && size != 0) {
		bm_err("[store_FG_meter_resistance] buf is %s\n", buf);
		ret = kstrtoul(buf, 10, &val);
		if (val < 0) {
			bm_err(
				"[store_FG_meter_resistance] val is %d ??\n",
				(int)val);
			val = 0;
		} else
			fg_cust_data.fg_meter_resistance = val;

		bm_err(
			"[store_FG_meter_resistance] FG_meter_resistance = %d\n",
			(int)val);
	}

	return size;

}
static DEVICE_ATTR(
	FG_meter_resistance, 0664,
	show_FG_meter_resistance, store_FG_meter_resistance);

static ssize_t show_FG_nafg_disable(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	bm_trace("[FG] show nafg disable : %d\n", gm.cmd_disable_nafg);
	return sprintf(buf, "%d\n", gm.cmd_disable_nafg);
}

static ssize_t store_FG_nafg_disable(
	struct device *dev, struct device_attribute *attr,
					const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret;

	bm_err("[store_FG_nafg_disable]\n");

	if (buf != NULL && size != 0) {
		bm_err("[store_FG_nafg_disable] buf is %s\n", buf);
		ret = kstrtoul(buf, 10, &val);
		if (val < 0) {
			bm_err(
				"[store_FG_nafg_disable] val is %d ??\n",
				(int)val);
			val = 0;
		}

		if (val == 0)
			gm.cmd_disable_nafg = false;
		else
			gm.cmd_disable_nafg = true;

		wakeup_fg_algo_cmd(
			FG_INTR_KERNEL_CMD, FG_KERNEL_CMD_DISABLE_NAFG, val);

		bm_err(
			"[store_FG_nafg_disable] FG_nafg_disable = %d\n",
			(int)val);
	}


	return size;
}
static DEVICE_ATTR(
	disable_nafg, 0664,
	show_FG_nafg_disable, store_FG_nafg_disable);

static ssize_t show_uisoc_update_type(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	bm_trace("[FG] show_uisoc_update_type : %d\n",
		fg_cust_data.uisoc_update_type);
	return sprintf(buf, "%d\n", fg_cust_data.uisoc_update_type);
}

static ssize_t store_uisoc_update_type(
	struct device *dev, struct device_attribute *attr,
					const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret;

	bm_err("[store_uisoc_update_type]\n");

	if (buf != NULL && size != 0) {
		bm_err("[store_uisoc_update_type] buf is %s\n", buf);
		ret = kstrtoul(buf, 10, &val);
		if (val < 0) {
			bm_err(
				"[store_uisoc_update_type] val is %d ??\n",
				(int)val);
			val = 0;
		}

		if (val >= 0 && val <= 2) {
			fg_cust_data.uisoc_update_type = val;
			wakeup_fg_algo_cmd(
				FG_INTR_KERNEL_CMD,
				FG_KERNEL_CMD_UISOC_UPDATE_TYPE,
				val);
			bm_err(
				"[store_uisoc_update_type] type = %d\n",
				(int)val);
		} else
			bm_err(
			"[store_uisoc_update_type] invalid type:%d\n",
			(int)val);
	}

	return size;
}
static DEVICE_ATTR(uisoc_update_type, 0664,
	show_uisoc_update_type, store_uisoc_update_type);

static ssize_t show_FG_daemon_log_level(
	struct device *dev, struct device_attribute *attr,
					char *buf)
{
	static int loglevel_count;

	loglevel_count++;
	if (loglevel_count % 5 == 0)
		bm_err(
		"[FG] show FG_daemon_log_level : %d\n",
		fg_cust_data.daemon_log_level);

	return sprintf(buf, "%d\n", fg_cust_data.daemon_log_level);
}

static ssize_t store_FG_daemon_log_level(
	struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret;

	bm_err("[FG_daemon_log_level]\n");

	if (buf != NULL && size != 0) {
		bm_err("[FG_daemon_log_level] buf is %s\n", buf);
		ret = kstrtoul(buf, 10, &val);
		if (val < 0) {
			bm_err(
				"[FG_daemon_log_level] val is %d ??\n",
				(int)val);
			val = 0;
		}

		if (val < 10 && val >= 0) {
			fg_cust_data.daemon_log_level = val;
			wakeup_fg_algo_cmd(
				FG_INTR_KERNEL_CMD,
				FG_KERNEL_CMD_CHANG_LOGLEVEL,
				val
			);

			gm.log_level = val;
		}
		if (val >= 7) {
			gtimer_set_log_level(3);
			gauge_coulomb_set_log_level(3);
		} else {
			gtimer_set_log_level(0);
			gauge_coulomb_set_log_level(0);
		}

		bm_err(
			"[FG_daemon_log_level]fg_cust_data.daemon_log_level=%d\n",
			fg_cust_data.daemon_log_level);
	}
	return size;
}
static DEVICE_ATTR(FG_daemon_log_level, 0664,
	show_FG_daemon_log_level, store_FG_daemon_log_level);

static ssize_t show_shutdown_cond_enable(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	bm_trace(
		"[FG] show_shutdown_cond_enable : %d\n",
		get_shutdown_cond_flag());
	return sprintf(buf, "%d\n", get_shutdown_cond_flag());
}

static ssize_t store_shutdown_cond_enable(
	struct device *dev, struct device_attribute *attr,
					const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret;

	bm_err("[store_shutdown_cond_enable]\n");
	if (buf != NULL && size != 0) {
		bm_err("[store_shutdown_cond_enable] buf is %s\n", buf);
		ret = kstrtoul(buf, 10, &val);
		if (val < 0) {
			bm_err(
				"[store_shutdown_cond_enable] val is %d ??\n",
				(int)val);
			val = 0;
		}

		set_shutdown_cond_flag(val);

		bm_err(
			"[store_shutdown_cond_enable] shutdown_cond_enabled=%d\n",
			get_shutdown_cond_flag());
	}

	return size;
}
static DEVICE_ATTR(
	shutdown_condition_enable, 0664,
	show_shutdown_cond_enable, store_shutdown_cond_enable);

static ssize_t show_reset_battery_cycle(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	bm_trace("[FG] show_reset_battery_cycle : %d\n",
		gm.is_reset_battery_cycle);
	return sprintf(buf, "%d\n", gm.is_reset_battery_cycle);
}

static ssize_t store_reset_battery_cycle(
	struct device *dev, struct device_attribute *attr,
					const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret;

	bm_err("[store_reset_battery_cycle]\n");
	if (buf != NULL && size != 0) {
		bm_err("[store_reset_battery_cycle] buf is %s\n", buf);
		ret = kstrtoul(buf, 10, &val);
		if (val < 0) {
			bm_err(
				"[store_reset_battery_cycle] val is %d ??\n",
				(int)val);
			val = 0;
		}
		if (val == 0)
			gm.is_reset_battery_cycle = false;
		else {
			gm.is_reset_battery_cycle = true;
			wakeup_fg_algo(FG_INTR_BAT_CYCLE);
		}
		bm_err(
			"[store_reset_battery_cycle] store_reset_battery_cycle=%d\n",
			gm.is_reset_battery_cycle);
	}

	return size;
}
static DEVICE_ATTR(
	reset_battery_cycle, 0664,
	show_reset_battery_cycle, store_reset_battery_cycle);

static ssize_t show_BAT_EC(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	bm_err("show_BAT_EC\n");

	return sprintf(buf, "%d:%d\n", gm.BAT_EC_cmd, gm.BAT_EC_param);
}

static ssize_t store_BAT_EC(
	struct device *dev, struct device_attribute *attr,
	const char *buf, size_t size)
{
	int ret1 = 0, ret2 = 0;
	char cmd_buf[4], param_buf[16];

	bm_err("store_BAT_EC\n");
	cmd_buf[3] = '\0';
	param_buf[15] = '\0';

	if (size < 4 || size > 20) {
		bm_err("store_BAT_EC error, size mismatch:%Zu\n", size);
		return -1;
	}

	if (buf != NULL && size != 0) {
		bm_err("buf is %s and size is %Zu\n", buf, size);
		cmd_buf[0] = buf[0];
		cmd_buf[1] = buf[1];
		cmd_buf[2] = buf[2];
		cmd_buf[3] = '\0';

		if ((size - 4) > 0) {
			strncpy(param_buf, buf + 4, size - 4);
			param_buf[size - 4 - 1] = '\0';
			bm_err("[FG_IT]cmd_buf %s, param_buf %s\n",
				cmd_buf, param_buf);
			ret2 = kstrtouint(param_buf, 10, &gm.BAT_EC_param);
		}

		ret1 = kstrtouint(cmd_buf, 10, &gm.BAT_EC_cmd);

		if (ret1 != 0 || ret2 != 0) {
			bm_err("ERROR! not valid number! %d %d\n",
				ret1, ret2);
			return -1;
		}
		bm_err("CMD is:%d, param:%d\n",
			gm.BAT_EC_cmd, gm.BAT_EC_param);
	}


	exec_BAT_EC(gm.BAT_EC_cmd, gm.BAT_EC_param);

	return size;
}
static DEVICE_ATTR(BAT_EC, 0664, show_BAT_EC, store_BAT_EC);

static ssize_t show_FG_Battery_CurrentConsumption(
struct device *dev, struct device_attribute *attr,
						  char *buf)
{
	int ret_value = 8888;

	ret_value = battery_get_bat_avg_current();
	bm_err("[EM] FG_Battery_CurrentConsumption : %d mA\n", ret_value);
	return sprintf(buf, "%d\n", ret_value);
}

static ssize_t store_FG_Battery_CurrentConsumption(struct device *dev,
	struct device_attribute *attr, const char *buf,
	size_t size)
{
	bm_err("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(
	FG_Battery_CurrentConsumption, 0664, show_FG_Battery_CurrentConsumption,
		   store_FG_Battery_CurrentConsumption);

/* /////////////////////////////////////*/
/* // Create File For EM : Power_On_Voltage */
/* /////////////////////////////////////*/
static ssize_t show_Power_On_Voltage(
struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = 3450;
	bm_err("[EM] Power_On_Voltage : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_Power_On_Voltage(
	struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t size)
{
	bm_err("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(
	Power_On_Voltage, 0664, show_Power_On_Voltage, store_Power_On_Voltage);

/* /////////////////////////////////////////// */
/* // Create File For EM : Power_Off_Voltage */
/* /////////////////////////////////////////// */
static ssize_t show_Power_Off_Voltage(
struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = 3400;
	bm_err("[EM] Power_Off_Voltage : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_Power_Off_Voltage(
	struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t size)
{
	bm_err("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(
	Power_Off_Voltage, 0664,
	show_Power_Off_Voltage, store_Power_Off_Voltage);

#if 1
static int battery_callback(
	struct notifier_block *nb, unsigned long event, void *v)
{
	bm_err("battery_callback:%ld\n", event);
	switch (event) {
	case CHARGER_NOTIFY_EOC:
		{
/* CHARGING FULL */
			notify_fg_chr_full();
		}
		break;
	case CHARGER_NOTIFY_START_CHARGING:
		{
/* START CHARGING */
			if (gauge_get_hw_version() >= GAUGE_HW_V1000 &&
				gauge_get_hw_version() < GAUGE_HW_V2000)
				fg_sw_bat_cycle_accu();
#ifndef VENDOR_EDIT
/* ChaoYing.Chen@EXP.BSP.CHG.basic, 2017/06/05, Remove for charge driver */
			battery_main.BAT_STATUS = POWER_SUPPLY_STATUS_CHARGING;
			battery_update(&battery_main);
#endif /* VENDOR_EDIT */

		}
		break;
	case CHARGER_NOTIFY_STOP_CHARGING:
		{
/* STOP CHARGING */
#ifndef VENDOR_EDIT
/* ChaoYing.Chen@EXP.BSP.CHG.basic, 2017/06/05, Remove for charge driver */
			battery_main.BAT_STATUS =
			POWER_SUPPLY_STATUS_DISCHARGING;
			battery_update(&battery_main);
#endif /* VENDOR_EDIT */
		}
		break;
	case CHARGER_NOTIFY_ERROR:
		{
/* charging enter error state */
#ifndef VENDOR_EDIT
/* ChaoYing.Chen@EXP.BSP.CHG.basic, 2017/06/05, Remove for charge driver */
		battery_main.BAT_STATUS = POWER_SUPPLY_STATUS_DISCHARGING;
		battery_update(&battery_main);
#endif /* VENDOR_EDIT */
		}
		break;
	case CHARGER_NOTIFY_NORMAL:
		{
/* charging leave error state */
#ifndef VENDOR_EDIT
/* ChaoYing.Chen@EXP.BSP.CHG.basic, 2017/06/05, Remove for charge driver */
		battery_main.BAT_STATUS = POWER_SUPPLY_STATUS_CHARGING;
		battery_update(&battery_main);
#endif /* VENDOR_EDIT */

		}
		break;

	default:
		{
		}
		break;
	}

	return NOTIFY_DONE;
}
#endif
/********** adc_cdev*******************/
signed int battery_meter_meta_tool_cali_car_tune(int meta_current)
{
	int cali_car_tune = 0;
	int ret = 0;

	if (meta_current == 0)
		return fg_cust_data.car_tune_value * 10;

	ret = gauge_dev_enable_car_tune_value_calibration(
		gm.gdev, meta_current, &cali_car_tune);

	return cali_car_tune;		/* 1000 base */
}

#if IS_ENABLED(CONFIG_COMPAT)
static long compat_adc_cali_ioctl(
struct file *filp, unsigned int cmd, unsigned long arg)
{
	bm_notice("compat_adc_cali_ioctl 32bit IOCTL, cmd=0x%08x\n", cmd);
	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		bm_err("compat_adc_cali_ioctl file has no f_op or no f_op->unlocked_ioctl.\n");
		return -ENOTTY;
	}

	switch (cmd) {
	case BAT_STATUS_READ:
	case ADC_CHANNEL_READ:
	case Get_META_BAT_VOL:
	case Get_META_BAT_SOC:
	case Get_META_BAT_CAR_TUNE_VALUE:
	case Set_META_BAT_CAR_TUNE_VALUE:
	case Set_BAT_DISABLE_NAFG:
#ifdef VENDOR_EDIT
//huangtongfeng@BSP.CHG.Basic, 2017/12/14, add for kpoc charging param.
    case Get_FakeOff_Param:
    case Turn_Off_Charging:
#endif /*VENDOR_EDIT*/

	case Set_CARTUNE_TO_KERNEL: {
		bm_notice(
			"compat_adc_cali_ioctl send to unlocked_ioctl cmd=0x%08x\n",
			cmd);
		return filp->f_op->unlocked_ioctl(
			filp, cmd,
			(unsigned long)compat_ptr(arg));
	}
		break;
	default:
		bm_err("compat_adc_cali_ioctl unknown IOCTL: 0x%08x\n", cmd);
		break;
	}

	return 0;
}
#endif

static long adc_cali_ioctl(
	struct file *file, unsigned int cmd, unsigned long arg)
{
	int *user_data_addr;
	int *naram_data_addr;
	int i = 0;
	int ret = 0;
	int adc_in_data[2] = { 1, 1 };
	int adc_out_data[2] = { 1, 1 };
	int temp_car_tune;
	int isdisNAFG = 0;
#ifdef VENDOR_EDIT
//huangtongfeng@BSP.CHG.Basic, 2017/01/13, add for kpoc charging param.
    int fakeoff_out_data[5] = {0,0,0,0,0};
#endif /*VENDOR_EDIT*/

	bm_notice("adc_cali_ioctl enter\n");

	mutex_lock(&gm.fg_mutex);

	switch (cmd) {
	case TEST_ADC_CALI_PRINT:
		g_ADC_Cali = false;
		break;

	case SET_ADC_CALI_Slop:
		naram_data_addr = (int *)arg;
		ret = copy_from_user(adc_cali_slop, naram_data_addr, 36);
		g_ADC_Cali = false;
		/* Protection */
		for (i = 0; i < 14; i++) {
			if ((*(adc_cali_slop + i) == 0)
				|| (*(adc_cali_slop + i) == 1))
				*(adc_cali_slop + i) = 1000;
		}
		for (i = 0; i < 14; i++)
			bm_notice("adc_cali_slop[%d] = %d\n", i,
				    *(adc_cali_slop + i));
		bm_notice("**** unlocked_ioctl : SET_ADC_CALI_Slop Done!\n");
		break;

	case SET_ADC_CALI_Offset:
		naram_data_addr = (int *)arg;
		ret = copy_from_user(adc_cali_offset, naram_data_addr, 36);
		g_ADC_Cali = false;
		for (i = 0; i < 14; i++)
			bm_notice("adc_cali_offset[%d] = %d\n", i,
				    *(adc_cali_offset + i));
		bm_notice("**** unlocked_ioctl : SET_ADC_CALI_Offset Done!\n");
		break;

	case SET_ADC_CALI_Cal:
		naram_data_addr = (int *)arg;
		ret = copy_from_user(adc_cali_cal, naram_data_addr, 4);
		g_ADC_Cali = true;
		if (adc_cali_cal[0] == 1)
			g_ADC_Cali = true;
		else
			g_ADC_Cali = false;

		for (i = 0; i < 1; i++)
			bm_notice("adc_cali_cal[%d] = %d\n", i,
				    *(adc_cali_cal + i));
		bm_notice("**** unlocked_ioctl : SET_ADC_CALI_Cal Done!\n");
		break;

	case ADC_CHANNEL_READ:
		/* g_ADC_Cali = KAL_FALSE; *//* 20100508 Infinity */
		user_data_addr = (int *)arg;
		ret = copy_from_user(
				adc_in_data, user_data_addr, 8);

		if (adc_in_data[0] == 0) {
			/* I_SENSE */
			adc_out_data[0] =
				battery_get_bat_voltage() * adc_in_data[1];
		} else if (adc_in_data[0] == 1) {
			/* BAT_SENSE */
			adc_out_data[0] =
				battery_get_bat_voltage() * adc_in_data[1];
		} else if (adc_in_data[0] == 3) {
			/* V_Charger */
			adc_out_data[0] = battery_get_vbus() * adc_in_data[1];
			/* adc_out_data[0] = adc_out_data[0] / 100; */
		} else if (adc_in_data[0] == 30) {
			/* V_Bat_temp magic number */
			adc_out_data[0] =
				battery_get_bat_temperature() * adc_in_data[1];
		} else if (adc_in_data[0] == 66)
			adc_out_data[0] = (battery_get_bat_current()) / 10;
		else {
			bm_notice("unknown channel(%d,%d)\n",
				    adc_in_data[0], adc_in_data[1]);
		}

		if (adc_out_data[0] < 0)
			adc_out_data[1] = 1;	/* failed */
		else
			adc_out_data[1] = 0;	/* success */

		if (adc_in_data[0] == 30)
			adc_out_data[1] = 0;	/* success */

		if (adc_in_data[0] == 66)
			adc_out_data[1] = 0;	/* success */

		ret = copy_to_user(user_data_addr, adc_out_data, 8);
		bm_notice(
			    "**** unlocked_ioctl : Channel %d * %d times = %d\n",
			    adc_in_data[0], adc_in_data[1], adc_out_data[0]);
		break;

	case BAT_STATUS_READ:
		user_data_addr = (int *)arg;
		ret = copy_from_user(battery_in_data, user_data_addr, 4);
		/* [0] is_CAL */
		if (g_ADC_Cali)
			battery_out_data[0] = 1;
		else
			battery_out_data[0] = 0;
		ret = copy_to_user(user_data_addr, battery_out_data, 4);
		bm_notice(
			"unlocked_ioctl : CAL:%d\n", battery_out_data[0]);
		break;
#if 0
	case Set_Charger_Current:	/* For Factory Mode */
		user_data_addr = (int *)arg;
		ret = copy_from_user(charging_level_data, user_data_addr, 4);
		g_ftm_battery_flag = KAL_TRUE;
		if (charging_level_data[0] == 0)
			charging_level_data[0] = CHARGE_CURRENT_70_00_MA;
		else if (charging_level_data[0] == 1)
			charging_level_data[0] = CHARGE_CURRENT_200_00_MA;
		else if (charging_level_data[0] == 2)
			charging_level_data[0] = CHARGE_CURRENT_400_00_MA;
		else if (charging_level_data[0] == 3)
			charging_level_data[0] = CHARGE_CURRENT_450_00_MA;
		else if (charging_level_data[0] == 4)
			charging_level_data[0] = CHARGE_CURRENT_550_00_MA;
		else if (charging_level_data[0] == 5)
			charging_level_data[0] = CHARGE_CURRENT_650_00_MA;
		else if (charging_level_data[0] == 6)
			charging_level_data[0] = CHARGE_CURRENT_700_00_MA;
		else if (charging_level_data[0] == 7)
			charging_level_data[0] = CHARGE_CURRENT_800_00_MA;
		else if (charging_level_data[0] == 8)
			charging_level_data[0] = CHARGE_CURRENT_900_00_MA;
		else if (charging_level_data[0] == 9)
			charging_level_data[0] = CHARGE_CURRENT_1000_00_MA;
		else if (charging_level_data[0] == 10)
			charging_level_data[0] = CHARGE_CURRENT_1100_00_MA;
		else if (charging_level_data[0] == 11)
			charging_level_data[0] = CHARGE_CURRENT_1200_00_MA;
		else if (charging_level_data[0] == 12)
			charging_level_data[0] = CHARGE_CURRENT_1300_00_MA;
		else if (charging_level_data[0] == 13)
			charging_level_data[0] = CHARGE_CURRENT_1400_00_MA;
		else if (charging_level_data[0] == 14)
			charging_level_data[0] = CHARGE_CURRENT_1500_00_MA;
		else if (charging_level_data[0] == 15)
			charging_level_data[0] = CHARGE_CURRENT_1600_00_MA;
		else
			charging_level_data[0] = CHARGE_CURRENT_450_00_MA;
		wake_up_bat();
		bm_notice("**** unlocked_ioctl : set_Charger_Current:%d\n",
			    charging_level_data[0]);
		break;
#endif
		/* add for meta tool------------------------------- */
	case Get_META_BAT_VOL:
		user_data_addr = (int *)arg;
		ret = copy_from_user(adc_in_data, user_data_addr, 8);
		adc_out_data[0] = battery_get_bat_voltage();
		ret = copy_to_user(user_data_addr, adc_out_data, 8);
		bm_notice("**** unlocked_ioctl :Get_META_BAT_VOL Done!\n");
		break;
	case Get_META_BAT_SOC:
		user_data_addr = (int *)arg;
		ret = copy_from_user(adc_in_data, user_data_addr, 8);
		adc_out_data[0] = battery_get_uisoc();
		ret = copy_to_user(user_data_addr, adc_out_data, 8);
		bm_notice("**** unlocked_ioctl :Get_META_BAT_SOC Done!\n");
		break;

	case Get_META_BAT_CAR_TUNE_VALUE:
		user_data_addr = (int *)arg;
		ret = copy_from_user(adc_in_data, user_data_addr, 8);
		adc_out_data[0] = fg_cust_data.car_tune_value;
		bm_err("Get_BAT_CAR_TUNE_VALUE, res=%d\n", adc_out_data[0]);
		ret = copy_to_user(user_data_addr, adc_out_data, 8);
		bm_notice("**** unlocked_ioctl :Get_META_BAT_CAR_TUNE_VALUE Done!\n");
		break;

	case Set_META_BAT_CAR_TUNE_VALUE:
		/* meta tool input: adc_in_data[1] (mA)*/
		user_data_addr = (int *)arg;
		ret = copy_from_user(adc_in_data, user_data_addr, 8);

		/* Send cali_current to hal to calculate car_tune_value*/
		temp_car_tune =
			battery_meter_meta_tool_cali_car_tune(adc_in_data[1]);

		/* return car_tune_value to meta tool in adc_out_data[0] */
		if (temp_car_tune >= 900 && temp_car_tune <= 1100)
			fg_cust_data.car_tune_value = temp_car_tune;
		else
			bm_err("car_tune_value invalid:%d\n",
			temp_car_tune);

		adc_out_data[0] = temp_car_tune;
		ret = copy_to_user(user_data_addr, adc_out_data, 8);
		bm_err("**** unlocked_ioctl Set_BAT_CAR_TUNE_VALUE[%d], result=%d, ret=%d\n",
			adc_in_data[1], adc_out_data[0], ret);

		break;

	case Set_BAT_DISABLE_NAFG:
		user_data_addr = (int *)arg;
		ret = copy_from_user(adc_in_data, user_data_addr, 8);
		isdisNAFG = adc_in_data[1];

		if (isdisNAFG == 1) {
			gm.cmd_disable_nafg = true;
			wakeup_fg_algo_cmd(
				FG_INTR_KERNEL_CMD,
				FG_KERNEL_CMD_DISABLE_NAFG, 1);
		} else if (isdisNAFG == 0) {
			gm.cmd_disable_nafg = false;
			wakeup_fg_algo_cmd(
				FG_INTR_KERNEL_CMD,
				FG_KERNEL_CMD_DISABLE_NAFG, 0);
		}
		bm_debug("unlocked_ioctl Set_BAT_DISABLE_NAFG,isdisNAFG=%d [%d]\n",
			isdisNAFG, adc_in_data[1]);
		break;

		/* add bing meta tool------------------------------- */
	case Set_CARTUNE_TO_KERNEL:
		user_data_addr = (int *)arg;
		ret = copy_from_user(adc_in_data, user_data_addr, 8);
		temp_car_tune = adc_in_data[1];
		if (temp_car_tune > 500 && temp_car_tune < 1500)
			fg_cust_data.car_tune_value = temp_car_tune;

		bm_err("**** unlocked_ioctl Set_CARTUNE_TO_KERNEL[%d,%d], ret=%d\n",
			adc_in_data[0], adc_in_data[1], ret);
		break;
#ifdef VENDOR_EDIT
/************ kpoc_charger *******************/
//huangtongfeng@BSP.CHG.Basic, 2017/01/13, add for kpoc charging param.
    case Get_FakeOff_Param:
        user_data_addr = (int *)arg;
        fakeoff_out_data[0] = oppo_chg_get_ui_soc();
        fakeoff_out_data[1] = oppo_chg_get_notify_flag();
        if(pmic_chrdet_status() == true && oppo_get_prop_status() != 3)
        {
            fakeoff_out_data[2] = 1;
        }
        else
        {
            fakeoff_out_data[2] = 0;
        }
        fakeoff_out_data[3] = oppo_chg_show_vooc_logo_ornot();
#ifdef CONFIG_OPPO_CHARGER_MTK6771
        if (is_vooc_project()) {
            fakeoff_out_data[4] = (oppo_chg_check_chip_is_null() == false? 1:0);
        } else
#endif /* CONFIG_OPPO_CHARGER_MTK6771 */
        {
            if(fgauge_is_start == 1)
                fakeoff_out_data[4] = 2;
            else
                fakeoff_out_data[4] = 0;
        }
        
        ret = copy_to_user(user_data_addr, fakeoff_out_data, 20);
        bm_err("ioctl : Get_FakeOff_Param:ui_soc:%d, g_NotifyFlag:%d,chr_det:%d,fast_chg = %d\n",fakeoff_out_data[0],fakeoff_out_data[1],fakeoff_out_data[2],fakeoff_out_data[3]);
        break;
    case Turn_Off_Charging:
        bm_err("ioctl : Turn_Off_Charging\n");
        break;
#endif /*VEDNOR_EDIT*/

	default:
		bm_err("**** unlocked_ioctl unknown IOCTL: 0x%08x\n", cmd);
		g_ADC_Cali = false;
		break;
	}

	mutex_unlock(&gm.fg_mutex);

	return 0;
}

static int adc_cali_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int adc_cali_release(struct inode *inode, struct file *file)
{
	return 0;
}

static unsigned long bat_node;

static int fb_early_init_dt_get_chosen(
	unsigned long node, const char *uname, int depth, void *data)
{
	if (depth != 1 || (strcmp(uname, "chosen") != 0
		&& strcmp(uname, "chosen@0") != 0))
		return 0;
	bat_node = node;
	return 1;
}


static const struct file_operations adc_cali_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = adc_cali_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = compat_adc_cali_ioctl,
#endif
	.open = adc_cali_open,
	.release = adc_cali_release,
};

#ifdef VENDOR_EDIT

//PengNan@BSP.CHG.Basic, 2017/09/07, add for compatabling two fuelgauge.
extern int oppo_gauge_get_batt_current(void);
extern int oppo_chg_get_soc(void);
extern int oppo_chg_get_ui_soc(void);
extern int oppo_gauge_get_batt_temperature(void);
extern int pmic_get_battery_voltage(void);
extern int gauge_get_average_current(bool *valid);
extern bool gauge_get_current(int *bat_current);
/* ChaoYing.Chen@EXP.BSP.CHG.basic, 2017/05/16, Add for charge driver */
int oppo_battery_get_bat_temperature(void)
{
#ifdef VENDOR_EDIT
    //PengNan@BSP.CHG.Basic, 2017/09/07, add for compatabling two fuelgauge.
    if(is_vooc_project()){
        return oppo_gauge_get_batt_temperature()/10;
    }
#endif /*VENDOR_EDIT*/
   /* TODO */
   if (is_battery_init_done()) {
       return force_get_tbat(true);
   }
   else {
       return -127;
   }

}
bool oppo_get_current_status(int *bat_current)
{
	*bat_current = oppo_gauge_get_batt_current() * 10;
	if(*bat_current >= 0){
		return false;
	} else 
		return true;
}

bool battery_meter_get_battery_current_signed(void)
{
    int curr_val;

	if(is_vooc_project()){
		return oppo_get_current_status(&curr_val);
	} else {
		return gauge_get_current(&curr_val);
	}

}

static int meter_fg_30_get_battery_mvolts(void)
{
	int ret = 0;

	ret = battery_get_bat_voltage();
	return ret;
}

static int meter_fg_30_get_battery_temperature(void)
{
	int ret = 0;

	ret = oppo_battery_get_bat_temperature();
	return ret * 10;
}

static int meter_fg_30_get_batt_remaining_capacity(void)
{
	return -1;
}
/*
signed int oppo_battery_get_bat_soc(void)
{

	if(is_vooc_project()){
		return oppo_chg_get_soc();
	} else {
		return gm.soc;
    }

}
*/
static int meter_fg_30_get_battery_soc(void)
{

    if(is_vooc_project()){
        return oppo_chg_get_ui_soc();
    } else {
        if(gm.init_flag == 1) {
            return gm.ui_soc;
        } else {
            return -1;
        }
    }


}

static int meter_fg_30_get_average_current(void)
{
	int ret = 0;
	bool volid;

	//ret = gauge_get_average_current(&volid);
	volid = gauge_get_current(&ret);
	ret = ret / 10;
	if (battery_meter_get_battery_current_signed() == 1)
		ret = 0 - ret;	/* charging */

	return ret;
}

int meter_fg_30_get_bat_charging_current(void)
{
	return meter_fg_30_get_average_current();
}

static int meter_fg_30_get_battery_fcc(void)
{
	return -1;
}

static int meter_fg_30_get_battery_cc(void)
{
	return -1;
}

static int meter_fg_30_get_battery_soh(void)
{
	return -1;
}

static int meter_fg_30_get_prev_batt_remaining_capacity(void)
{
	return -1;
}

static int meter_fg_30_modify_dod0(void)
{
	return -1;
}

static int meter_fg_30_update_soc_smooth_parameter(void)
{
	return -1;
}
#endif

#ifdef VENDOR_EDIT
/* Qiao.Hu@EXP.BSP.BaseDrv.CHG.Basic, 2017/08/22, Add for charger full status of FG 3.0 */
bool last_full = false;
#ifdef VENDOR_EDIT
/* Qiao.Hu@EXP.BSP.BaseDrv.CHG.Basic, 2017/08/15, Add for charger full status */
extern int notify_battery_full(void);

static void meter_fg_30_set_battery_full(bool full)
{
	if(last_full != full) {
		if (full) {
			if (notify_battery_full()) {
				//BMT_status.bat_full = true;
				last_full = full;
			}
		} else {
			//BMT_status.bat_full = false;
			last_full = full;
		}
	}
}
#else
static void meter_fg_30_set_battery_full(bool full)
{
		if (full) {
			BMT_status.bat_full = true;
		} else {
			BMT_status.bat_full = false;
		}
}
#endif
#ifdef VENDOR_EDIT
/* Qiao.Hu@EXP.BSP.BaseDrv.CHG.Basic, 2017/11/29, Add for charger full status of FG 3.0 */
int oppo_get_rtc_ui_soc(void)
{
	int rtc_ui_soc;
	if (is_vooc_project())
		return 0;
	gauge_dev_get_rtc_ui_soc(gm.gdev, &rtc_ui_soc);
  	printk("oppo_get_rtc_ui_soc =%d\n",rtc_ui_soc);
	return rtc_ui_soc;
}
int oppo_set_rtc_ui_soc(int value)
{
	if (is_vooc_project())
		return 0;
	gauge_dev_set_rtc_ui_soc(gm.gdev, value);
  	printk("oppo_set_rtc_ui_soc =%d\n",value);
	return value;
}
#endif
#ifdef VENDOR_EDIT
/* Qiao.Hu@EXP.BSP.BaseDrv.CHG.Basic,, 2016/08/25  Add for use lk vbatt */
int lk_vbatt;

static int oppo_get_lk_vbatt(char *oppo_vbatt_char)
{
	sscanf(oppo_vbatt_char, "%d", &lk_vbatt);
	printk(KERN_ERR "lk_vbatt=%d\n", lk_vbatt);

	return 1;
}
__setup("vbatt=", oppo_get_lk_vbatt);
#endif /*VENDOR_EDIT*/
#ifdef VENDOR_EDIT
/* Qiao.Hu@EXP.BSP.CHG.basic, 2017/07/20, Add for charger */
extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int* rawdata);
extern int IMM_IsAdcInitReady(void);
enum {
	BAT_TYPE__UNKNOWN,
	BAT_TYPE__SDI_4350mV, //50mV~290mV
	BAT_TYPE__SDI_4400mV, //300mV~520mV
	BAT_TYPE__LG_4350mV, //NO use
	BAT_TYPE__LG_4400mV, //530mV~780mV
	BAT_TYPE__ATL_4350mV, //1110mV~1450mV
	BAT_TYPE__ATL_4400mV, //790mV~1100mV
	BAT_TYPE__TWS_4400mV,
};
#define BAT_ID (3) //AUXIN3
static int battery_type_check(void)
{
	int value = 0;
	int data[4] = {0};
	int i = 0;
	int ret = 0;
	int ret_value = 0;
	int times = 5;
	int channel = BAT_ID;
	int battery_type = BAT_TYPE__UNKNOWN;
	if (IMM_IsAdcInitReady() == 0) {
		printk(KERN_ERR "[battery_type_check]: AUXADC is not ready\n");
		return 0;
	}
	 i = times;
	 while (i--) {
	 	printk("IMM_GetOneChannelValue start\n");
		ret = IMM_GetOneChannelValue(channel, data, &ret_value);
 		printk(KERN_ERR "[battery_type_check]: ret = %d,ret_value[%d]\n", ret,ret_value);
		if (ret == 0) {
			value += ret_value;
		} else {
			 times = times > 1 ? times - 1 : 1;
			 printk(KERN_ERR "[battery_type_check]: ret[%d], times[%d]\n", ret, times);
		 }
	 }
	value = value * 1500 / 4096;
	value = value / times;
	printk(KERN_ERR "[battery_value= %d\n", value);
	if(!is_vooc_project()){
		if (value >= 790 && value <= 1100) {
			battery_type = BAT_TYPE__ATL_4400mV;
			gm.battery_id = 0;
			if(is_project(OPPO_17061)){
				gm.battery_id = 2;
			}
		} else if (value >= 300 && value <= 520) {
			battery_type = BAT_TYPE__SDI_4400mV;
			gm.battery_id = 1;
			if(is_project(OPPO_17061)){
				gm.battery_id = 3;
			}
		} else {
			battery_type = BAT_TYPE__UNKNOWN;
		}
	}
	else {
		if (value >= 50 && value <= 290) {
			battery_type = BAT_TYPE__SDI_4350mV;
			gm.battery_id = 1;
		}
		else {
			battery_type = BAT_TYPE__UNKNOWN;
			gm.battery_id = 0;
		}
	}

	printk(KERN_ERR "[battery_type_check]: adc_value[%d], battery_type[%d],g_fg_battery_id[%d]\n", value, battery_type, gm.battery_id);
	return battery_type;
}

#ifdef CONFIG_OPPO_4400MV_BATTERY_SUPPORT
static bool battery_type_is_4400mv(void)
{
	int battery_type = BAT_TYPE__UNKNOWN;
	int retry_flag = 0;
try_again:
	battery_type = battery_type_check();
	if (battery_type == BAT_TYPE__SDI_4400mV || battery_type == BAT_TYPE__LG_4400mV || battery_type == BAT_TYPE__ATL_4400mV) {
		return true;
	} else {
		if (retry_flag == 0) {
			retry_flag = 1;
			goto try_again;
		}
		if (is_meta_mode() == true) {
			return false;
		} else {
			return false;
		}
	}
}
#else

static bool battery_type_is_4350mv(void)
{
	int battery_type = BAT_TYPE__UNKNOWN;
	int retry_flag = 0;
try_again:
	battery_type = battery_type_check();
	if (battery_type == BAT_TYPE__SDI_4350mV || battery_type == BAT_TYPE__ATL_4350mV) {
		return true;
	} else {
		if (retry_flag == 0) {
			retry_flag = 1;
			goto try_again;
		}
		return false;
	}
}
#endif

bool meter_fg_30_get_battery_authenticate(void)
{
    #ifdef CONFIG_OPPO_4400MV_BATTERY_SUPPORT
	return battery_type_is_4400mv();
    #else
	return battery_type_is_4350mv();
    #endif
}

static void register_battery_devinfo(void)
{
	int ret = 0;
	char *version;
	char *manufacture;
	switch (battery_type_check()) {
		case BAT_TYPE__SDI_4350mV:
			version = "4.35v";
			manufacture = "SDI";
			break;
		case BAT_TYPE__SDI_4400mV:
			version = "4.40v";
			manufacture = "SDI";
			break;
		case BAT_TYPE__LG_4350mV:
			version = "4.35v";
			manufacture = "LG";
			break;
		case BAT_TYPE__LG_4400mV:
			version = "4.40v";
			manufacture = "LG";
			break;
		case BAT_TYPE__ATL_4350mV:
			version = "4.35v";
			manufacture = "ATL";
			break;
		case BAT_TYPE__ATL_4400mV:
			version = "4.40v";
			manufacture = "ATL";
			break;
		default:
			version = "unknown";
			manufacture = "UNKNOWN";
			break;
	}
	ret = register_device_proc("battery", version, manufacture);
	if (ret)
		pr_err("register_battery_devinfo fail\n");
}
#endif  /*VENDOR_EDIT*/

static int average_current_fg_30 = 0;
static void get_average_current(struct work_struct *work);
static struct workqueue_struct *average_current_wq = NULL;
static DECLARE_DELAYED_WORK(get_average_current_wk, get_average_current);
static void get_average_current(struct work_struct *work)
{
	int i = 0;
	int delay_time = 0;
	int fg_current_avg = 0;
	int fg_current_temp = 0;
	bool valid = false;

	average_current_fg_30 = battery_meter_get_battery_current();
	delay_time = 5;

	if (battery_meter_get_battery_current_signed() == 1) {
		for (i = 0; i < 25; i++) {
			fg_current_temp = gauge_get_average_current(&valid);
			fg_current_avg = fg_current_avg + fg_current_temp;
			msleep(20);
		}
		if (i != 0)
			fg_current_avg = fg_current_avg / i;
		average_current_fg_30 = fg_current_avg;
		delay_time = 3;
	}

	queue_delayed_work(average_current_wq,
						&get_average_current_wk,
						msecs_to_jiffies(delay_time * 1000));
}

static struct oppo_gauge_operations battery_meter_fg_30_gauge = {
	.get_battery_mvolts			= meter_fg_30_get_battery_mvolts,
	.get_battery_temperature		= meter_fg_30_get_battery_temperature,
	.get_batt_remaining_capacity	= meter_fg_30_get_batt_remaining_capacity,
	.get_battery_soc				= meter_fg_30_get_battery_soc,
	.get_average_current			= meter_fg_30_get_average_current,
	.get_battery_fcc				= meter_fg_30_get_battery_fcc,
	.get_battery_cc				= meter_fg_30_get_battery_cc,
	.get_battery_soh				= meter_fg_30_get_battery_soh,
	.get_battery_authenticate		= meter_fg_30_get_battery_authenticate,
	.set_battery_full				= meter_fg_30_set_battery_full,
	.get_prev_battery_mvolts		= meter_fg_30_get_battery_mvolts,
	.get_prev_battery_temperature	= meter_fg_30_get_battery_temperature,
	.get_prev_battery_soc			= meter_fg_30_get_battery_soc,
	.get_prev_average_current		= meter_fg_30_get_average_current,
	.get_prev_batt_remaining_capacity	= meter_fg_30_get_prev_batt_remaining_capacity,
	.get_battery_mvolts_2cell_max		= meter_fg_30_get_battery_mvolts,
	.get_battery_mvolts_2cell_min		= meter_fg_30_get_battery_mvolts,
	.get_prev_battery_mvolts_2cell_max	= meter_fg_30_get_battery_mvolts,
	.get_prev_battery_mvolts_2cell_min	= meter_fg_30_get_battery_mvolts,
	.update_battery_dod0				= meter_fg_30_modify_dod0,
	.update_soc_smooth_parameter		= meter_fg_30_update_soc_smooth_parameter,
};
#endif /* VENDOR_EDIT */

/*************************************/
static struct wakeup_source battery_lock;
#ifdef VENDOR_EDIT
/* Qiao.Hu@BSP.BaseDrv.CHG.Basic, 2018/01/13, mtk patch for distinguish fast charging and normal charging fgauge*/
void dis_GM3_SRC_SEL(void)
{
         unsigned int reset_sel;
 
         reset_sel = pmic_get_register_value(PMIC_RG_FGADC_RST_SRC_SEL);
 
         if (reset_sel == 1) {
                   pmic_set_register_value(PMIC_RG_FGADC_RST_SRC_SEL, 0);
                   bm_err("DISABLE GM3! set PMIC_RG_FGADC_RST_SRC_SEL to 0\n");
         }
}
#endif /* VENDOR_EDIT */

static int __init battery_probe(struct platform_device *dev)
{
	int ret_device_file = 0;
	int ret;
	struct class_device *class_dev = NULL;
	const char *fg_swocv_v = NULL;
	char fg_swocv_v_tmp[10];
	int fg_swocv_v_len = 0;
	const char *fg_swocv_i = NULL;
	char fg_swocv_i_tmp[10];
	int fg_swocv_i_len = 0;
	const char *shutdown_time = NULL;
	char shutdown_time_tmp[10];
	int shutdown_time_len = 0;
	const char *boot_voltage = NULL;
	char boot_voltage_tmp[10];
	int boot_voltage_len = 0;
	wakeup_source_init(&battery_lock, "abc wakelock");
	__pm_stay_awake(&battery_lock);

/********* adc_cdev **********/
	ret = alloc_chrdev_region(&adc_cali_devno, 0, 1, ADC_CALI_DEVNAME);
	if (ret)
		bm_notice("Error: Can't Get Major number for adc_cali\n");
	adc_cali_cdev = cdev_alloc();
	adc_cali_cdev->owner = THIS_MODULE;
	adc_cali_cdev->ops = &adc_cali_fops;
	ret = cdev_add(adc_cali_cdev, adc_cali_devno, 1);
	if (ret)
		bm_notice("adc_cali Error: cdev_add\n");
	adc_cali_major = MAJOR(adc_cali_devno);
	adc_cali_class = class_create(THIS_MODULE, ADC_CALI_DEVNAME);
	class_dev = (struct class_device *)device_create(adc_cali_class,
							 NULL,
					adc_cali_devno,
					NULL, ADC_CALI_DEVNAME);
/*****************************/

	mtk_battery_init(dev);
	/* Power supply class */
#if !defined(CONFIG_MTK_DISABLE_GAUGE)
#ifndef VENDOR_EDIT
/* ChaoYing.Chen@EXP.BSP.CHG.basic, 2017/06/17, Remove for charge driver */
	battery_main.psy =
		power_supply_register(
			&(dev->dev), &battery_main.psd, NULL);
	if (IS_ERR(battery_main.psy)) {
		bm_err("[BAT_probe] power_supply_register Battery Fail !!\n");
		ret = PTR_ERR(battery_main.psy);
		return ret;
	}
#endif /* VENDOR_EDIT */
	bm_err("[BAT_probe] power_supply_register Battery Success !!\n");
#endif
	ret = device_create_file(&(dev->dev), &dev_attr_Battery_Temperature);
	ret = device_create_file(&(dev->dev), &dev_attr_UI_SOC);

	/* sysfs node */
	ret_device_file = device_create_file(&(dev->dev),
		&dev_attr_uisoc_update_type);
	ret_device_file = device_create_file(&(dev->dev),
		&dev_attr_disable_nafg);
	ret_device_file = device_create_file(&(dev->dev),
		&dev_attr_FG_meter_resistance);
	ret_device_file = device_create_file(&(dev->dev),
		&dev_attr_FG_daemon_log_level);
	ret_device_file = device_create_file(&(dev->dev),
		&dev_attr_FG_daemon_disable);
	ret_device_file = device_create_file(&(dev->dev),
		&dev_attr_BAT_EC);
	ret_device_file = device_create_file(&(dev->dev),
		&dev_attr_FG_Battery_CurrentConsumption);
	ret_device_file = device_create_file(&(dev->dev),
		&dev_attr_Power_On_Voltage);
	ret_device_file = device_create_file(&(dev->dev),
		&dev_attr_Power_Off_Voltage);
	ret_device_file = device_create_file(&(dev->dev),
		&dev_attr_shutdown_condition_enable);
	ret_device_file = device_create_file(&(dev->dev),
		&dev_attr_reset_battery_cycle);

	if (of_scan_flat_dt(fb_early_init_dt_get_chosen, NULL) > 0)
		fg_swocv_v =
		of_get_flat_dt_prop(
			bat_node, "atag,fg_swocv_v",
			&fg_swocv_v_len);
	if (fg_swocv_v == NULL) {
		bm_err(" fg_swocv_v == NULL len = %d\n", fg_swocv_v_len);
	} else {
		snprintf(fg_swocv_v_tmp, (fg_swocv_v_len + 1),
			"%s", fg_swocv_v);
		ret = kstrtoint(fg_swocv_v_tmp, 10, &gm.ptim_lk_v);
		bm_err(" fg_swocv_v = %s len %d fg_swocv_v_tmp %s ptim_lk_v[%d]\n",
			fg_swocv_v, fg_swocv_v_len,
			fg_swocv_v_tmp, gm.ptim_lk_v);
	}

	if (of_scan_flat_dt(fb_early_init_dt_get_chosen, NULL) > 0)
		fg_swocv_i = of_get_flat_dt_prop(
		bat_node, "atag,fg_swocv_i", &fg_swocv_i_len);
	if (fg_swocv_i == NULL) {
		bm_err(" fg_swocv_i == NULL len = %d\n", fg_swocv_i_len);
	} else {
		snprintf(
			fg_swocv_i_tmp, (fg_swocv_i_len + 1),
			"%s", fg_swocv_i);
		ret = kstrtoint(fg_swocv_i_tmp, 10, &gm.ptim_lk_i);

		bm_err(" fg_swocv_i = %s len %d fg_swocv_i_tmp %s ptim_lk_i[%d]\n",
			fg_swocv_i, fg_swocv_i_len,
			fg_swocv_i_tmp, gm.ptim_lk_i);
	}

	if (of_scan_flat_dt(fb_early_init_dt_get_chosen, NULL) > 0)
		shutdown_time =
			of_get_flat_dt_prop(
				bat_node,
				"atag,shutdown_time",
				&shutdown_time_len);
	if (shutdown_time == NULL) {
		bm_err(" shutdown_time == NULL len = %d\n", shutdown_time_len);
	} else {
		snprintf(shutdown_time_tmp,
			(shutdown_time_len + 1),
			"%s", shutdown_time);
		ret = kstrtoint(shutdown_time_tmp, 10, &gm.pl_shutdown_time);

		bm_err(" shutdown_time = %s len %d shutdown_time_tmp %s pl_shutdown_time[%d]\n",
			shutdown_time, shutdown_time_len,
			shutdown_time_tmp, gm.pl_shutdown_time);
	}

	if (of_scan_flat_dt(fb_early_init_dt_get_chosen, NULL) > 0)
		boot_voltage =
		of_get_flat_dt_prop(
			bat_node, "atag,boot_voltage",
			&boot_voltage_len);
	if (boot_voltage == NULL) {
		bm_err(" boot_voltage == NULL len = %d\n", boot_voltage_len);
	} else {
		snprintf(
			boot_voltage_tmp, (boot_voltage_len + 1),
			"%s", boot_voltage);
		ret = kstrtoint(boot_voltage_tmp, 10, &gm.pl_bat_vol);

		bm_err(
			"boot_voltage=%s len %d boot_voltage_tmp %s pl_bat_vol[%d]\n",
			boot_voltage, boot_voltage_len,
			boot_voltage_tmp, gm.pl_bat_vol);
	}
#if 1
	gm.pbat_consumer = charger_manager_get_by_name(&(dev->dev), "charger");
	if (gm.pbat_consumer != NULL) {
		gm.bat_nb.notifier_call = battery_callback;
		register_charger_manager_notifier(gm.pbat_consumer, &gm.bat_nb);
	}
#endif

	battery_debug_init();
	__pm_relax(&battery_lock);

#ifdef VENDOR_EDIT
/* Jianchao.Shi@BSP.CHG.Basic, 2018/11/20, sjc Modify for charging */
	if (is_vooc_project()) {
		bm_err("disable GM 3.0\n");
		disable_fg();
		dis_GM3_SRC_SEL();
	} else if (is_recovery_mode()) {
		if (!is_vooc_project()) {
			battery_recovery_init();
		}
	}
#else
	if (IS_ENABLED(CONFIG_POWER_EXT) || gm.disable_mtkbattery ||
		IS_ENABLED(CONFIG_MTK_DISABLE_GAUGE)) {
		bm_err("disable GM 3.0\n");
		disable_fg();
	} else if (is_recovery_mode()) {
		if (!is_vooc_project()) {
			battery_recovery_init();
		}
	}
#endif /*VENDOR_EDIT*/

	mtk_battery_last_init(dev);


	gm.is_probe_done = true;

	return 0;
}

void battery_shutdown(struct platform_device *dev)
{
	int fg_coulomb = 0;
	int shut_car_diff = 0;
	int verify_car;

	fg_coulomb = gauge_get_coulomb();
	if (gm.d_saved_car != 0) {
		shut_car_diff = fg_coulomb - gm.d_saved_car;
		gauge_dev_set_info(gm.gdev, GAUGE_SHUTDOWN_CAR, shut_car_diff);
		/* ready for writing to PMIC_RG */
	}
	gauge_dev_get_info(gm.gdev, GAUGE_SHUTDOWN_CAR, &verify_car);

	bm_err("******** battery_shutdown!! car=[o:%d,new:%d,diff:%d v:%d]********\n",
		gm.d_saved_car, fg_coulomb, shut_car_diff, verify_car);

}

static int battery_suspend(struct platform_device *dev, pm_message_t state)
{
	bm_err("******** battery_suspend!! iavg=%d ***GM3 disable:%d %d %d %d***\n",
		gm.hw_status.iavg_intr_flag,
		gm.disableGM30,
		fg_cust_data.disable_nafg,
		gm.ntc_disable_nafg,
		gm.cmd_disable_nafg);
	if (gauge_get_hw_version() >= GAUGE_HW_V2000
		&& gm.hw_status.iavg_intr_flag == 1) {
		pmic_enable_interrupt(FG_IAVG_H_NO, 0, "GM30");
		if (gm.hw_status.iavg_lt > 0)
			pmic_enable_interrupt(FG_IAVG_L_NO, 0, "GM30");
	}
	return 0;
}

static int battery_resume(struct platform_device *dev)
{
	bm_err("******** battery_resume!! iavg=%d ***GM3 disable:%d %d %d %d***\n",
		gm.hw_status.iavg_intr_flag,
		gm.disableGM30,
		fg_cust_data.disable_nafg,
		gm.ntc_disable_nafg,
		gm.cmd_disable_nafg);
	if (gauge_get_hw_version() >=
		GAUGE_HW_V2000
		&& gm.hw_status.iavg_intr_flag == 1) {
		pmic_enable_interrupt(FG_IAVG_H_NO, 1, "GM30");
		if (gm.hw_status.iavg_lt > 0)
			pmic_enable_interrupt(FG_IAVG_L_NO, 1, "GM30");
	}
	/* reset nafg monitor time to avoid suspend for too long case */
	get_monotonic_boottime(&gm.last_nafg_update_time);


	fg_update_sw_iavg();
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mtk_bat_of_match[] = {
	{.compatible = "mediatek,bat_gm30",},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_bat_of_match);
#endif


struct platform_device battery_device = {
	.name = "battery",
	.id = -1,
};

static struct platform_driver battery_driver_probe = {
	.probe = battery_probe,
	.remove = NULL,
	.shutdown = battery_shutdown,
	.suspend = battery_suspend,
	.resume = battery_resume,
	.driver = {
		.name = "battery",
#ifdef CONFIG_OF
		.of_match_table = mtk_bat_of_match,
#endif
	},
};

int __init battery_init(void)
{
	struct netlink_kernel_cfg cfg = {
		.input = nl_data_handler,
	};

	int ret;
#ifdef VENDOR_EDIT
	/* ChaoYing.Chen@EXP.BSP.CHG.basic, 2017/05/16, Add for charge driver */
	struct oppo_gauge_chip *chip = NULL;
#endif /* VENDOR_EDIT */
    printk("battery_init\n");
	gm.daemo_nl_sk = netlink_kernel_create(&init_net, NETLINK_FGD, &cfg);
	bm_err("netlink_kernel_create protol= %d\n", NETLINK_FGD);

	if (gm.daemo_nl_sk == NULL) {
		bm_err("netlink_kernel_create error\n");
		return -1;
	}
	bm_err("netlink_kernel_create ok\n");
#ifdef VENDOR_EDIT
	/* Qiao.Hu@EXP.BSP.CHG.basic, 2017/07/20, Add for charger */
	register_battery_devinfo();
#endif  /*VENDOR_EDIT*/
#ifdef VENDOR_EDIT
    /* ChaoYing.Chen@EXP.BSP.CHG.basic, 2017/05/16, Add for charge driver */
    chip = (struct oppo_gauge_chip*) kzalloc(sizeof(struct oppo_gauge_chip),
                GFP_KERNEL);
    if (!chip) {
        pr_err("oppo_gauge_chip devm_kzalloc failed.\n");
        return -ENOMEM;
    }
/* Qiao.Hu@BSP.BaseDrv.CHG.Basic, 2018/01/01, modefy for fastcharger */
if(!is_vooc_project()) {
    chip->gauge_ops = &battery_meter_fg_30_gauge;
    oppo_gauge_init(chip);
}
#endif /* VENDOR_EDIT */

#ifdef CONFIG_OF
	/* register battery_device by DTS */
#else
	ret = platform_device_register(&battery_device);
#endif

	ret = platform_driver_register(&battery_driver_probe);

	bm_err("[battery_init] Initialization : DONE\n");

	return 0;
}

static void __exit battery_exit(void)
{

}
//late_initcall(battery_init);

module_init(battery_init);
module_exit(battery_exit);

MODULE_AUTHOR("Weiching Lin");
MODULE_DESCRIPTION("Battery Device Driver");
MODULE_LICENSE("GPL");
