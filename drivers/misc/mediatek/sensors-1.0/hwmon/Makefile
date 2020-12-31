ifeq ($(CONFIG_MTK_SENSOR_SUPPORT),y)
obj-y += hwmsen/
obj-y += sensor_dts/
obj-y += sensor_attributes/
obj-y += sensor_event/
obj-y += sensor_performance/
#ifdef VENDOR_EDIT
obj-y += sensor_list/
#endif /* VENDOR_EDIT */
else
obj-n := hwmsen/
endif
