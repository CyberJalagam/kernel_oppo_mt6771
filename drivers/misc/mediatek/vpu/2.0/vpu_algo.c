/*
 * Copyright (C) 2016 MediaTek Inc.
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

#include <linux/slab.h>
#include <linux/string.h>
#include "vpu_algo.h"

/* list of vlist_type(struct vpu_algo) */
static struct list_head vpu_algo_pool[MTK_VPU_CORE];
static uint32_t prop_info_data_length;
static int debug_algo_id;

const size_t g_vpu_prop_type_size[VPU_NUM_PROP_TYPES] = {
	[VPU_PROP_TYPE_CHAR]     = sizeof(char),
	[VPU_PROP_TYPE_INT32]    = sizeof(int32_t),
	[VPU_PROP_TYPE_FLOAT]    = sizeof(int32_t),
	[VPU_PROP_TYPE_INT64]    = sizeof(int64_t),
	[VPU_PROP_TYPE_DOUBLE]   = sizeof(int64_t)
};

const char *g_vpu_prop_type_names[VPU_NUM_PROP_TYPES] = {
	[VPU_PROP_TYPE_CHAR]     = "char",
	[VPU_PROP_TYPE_INT32]    = "int32",
	[VPU_PROP_TYPE_FLOAT]    = "float",
	[VPU_PROP_TYPE_INT64]    = "int64",
	[VPU_PROP_TYPE_DOUBLE]   = "double"
};

const char *g_vpu_port_usage_names[VPU_NUM_PORT_USAGES] = {
	[VPU_PORT_USAGE_IMAGE]     = "image",
	[VPU_PORT_USAGE_DATA]      = "data",
};

const char *g_vpu_port_dir_names[VPU_NUM_PORT_DIRS] = {
	[VPU_PORT_DIR_IN]       = "in",
	[VPU_PORT_DIR_OUT]      = "out",
	[VPU_PORT_DIR_IN_OUT]   = "in-out",
};

struct vpu_prop_desc g_vpu_prop_descs[VPU_NUM_PROPS] = {
#define INS_PROP(id, name, type, count, access) \
	{ VPU_PROP_ ## id, VPU_PROP_TYPE_ ## type, VPU_PROP_ACCESS_ ## access, 0, count, name }

	INS_PROP(RESERVED, "reserved", INT32, 256, RDONLY),
#undef INS_PROP
};

int vpu_init_algo(struct vpu_device *vpu_device)
{
	uint16_t i = 0;
	uint32_t offset = 0;
	uint32_t prop_data_length;
	struct vpu_prop_desc *prop_desc;

	for (i = 0; i < MTK_VPU_CORE; i++)
		INIT_LIST_HEAD(&(vpu_algo_pool[i]));

	for (i = 0; i < VPU_NUM_PROPS; i++) {
		prop_desc = &g_vpu_prop_descs[i];
		prop_desc->offset = offset;
		prop_data_length = prop_desc->count * g_vpu_prop_type_size[prop_desc->type];
		offset += prop_data_length;
	}
	/* the total length = last offset + last data length */
	prop_info_data_length = offset;

	return 0;
}

int vpu_add_algo_to_pool(int core, struct vpu_algo *algo)
{
	LOG_DBG("[vpu] vpu_add_algo_to_pool +\n");
	list_add_tail(vlist_link(algo, struct vpu_algo), &vpu_algo_pool[core]);
	return 0;
}

int vpu_find_algo_by_id(int core, vpu_id_t id, struct vpu_algo **ralgo)
{
	struct vpu_algo *algo;
	struct list_head *head;
	char *name;

	if (id < 1)
		goto err;

	list_for_each(head, &vpu_algo_pool[core])
	{
		algo = vlist_node_of(head, struct vpu_algo);
		if (algo->id[core] == id) {
			*ralgo = algo;
			return 0;
		}
	}

	if (vpu_get_name_of_algo(core, id, &name))
		goto err;

	if (vpu_create_algo(core, name, &algo, false) == 0) {
		vpu_add_algo_to_pool(core, algo);
		*ralgo = algo;
		return 0;
	}

err:
	*ralgo = NULL;
	return -ENOENT;
}

int vpu_find_algo_by_name(int core, char *name, struct vpu_algo **ralgo, bool needload)
{
	struct vpu_algo *algo;
	struct list_head *head;

	LOG_DBG("[vpu] vpu_find_algo_by_name +\n");

	if (name == NULL)
		goto err;

	list_for_each(head, &vpu_algo_pool[core])
	{
		algo = vlist_node_of(head, struct vpu_algo);
		if (!strcmp(name, algo->name)) {
			*ralgo = algo;
			return 0;
		}
	}


	if (vpu_create_algo(core, name, &algo, needload) == 0) {
		vpu_add_algo_to_pool(core, algo);
		*ralgo = algo;
		return 0;
	}

err:
	*ralgo = NULL;
	return -ENOENT;
}

int vpu_get_algo_id_by_name(int core, char *name)
{
	struct vpu_algo *algo;
	int algo_id = -1;
	int ret = 0;

	if (name == NULL)
		goto out;

	LOG_DBG("vpu_get_algo_id_by_name core:%d\n", core);
	ret = vpu_find_algo_by_name(core, name, &algo, false);
	LOG_DBG("ret:%d\n", ret);
	CHECK_RET("vpu_find_algo_by_name fail, name=%s\n", name);
	algo_id = algo->id[core];
	LOG_DBG("vpu(%d)_get algo_id:%d\n", core, algo_id);
	return algo_id;

out:
	return -1;
}

static int vpu_calc_prop_offset(struct vpu_prop_desc *descs, uint32_t count,
		uint32_t *length) {

	struct vpu_prop_desc *prop_desc;
	uint32_t offset = 0;
	uint32_t alignment = 1;
	uint32_t i, tmp;
	size_t type_size;

	/* get the alignment of struct packing */
	for (i = 0; i < count; i++) {
		prop_desc = &descs[i];
		type_size = g_vpu_prop_type_size[prop_desc->type];

		if (alignment < type_size)
			alignment = type_size;
	}

	/* calculate every prop's offset  */
	for (i = 0; i < count; i++) {
		prop_desc = &descs[i];
		type_size = g_vpu_prop_type_size[prop_desc->type];

		/* align offset with next data type */
		tmp = offset % type_size;
		if (tmp)
			offset += type_size - tmp;

		/* padding if the remainder is not enough */
		tmp = alignment - offset % alignment;
		if (tmp < type_size)
			offset += tmp;

		prop_desc->offset = offset;
		offset += prop_desc->count * type_size;
	}
	*length = offset;

	return 0;
}

int vpu_create_algo(int core, char *name, struct vpu_algo **ralgo, bool needload)
{
	int ret, id, length;
	unsigned int mva;
	struct vpu_algo *algo = NULL;

	LOG_DBG("[vpu] vpu_create_algo + (%d)\n", needload);

	ret = vpu_get_entry_of_algo(core, name, &id, &mva, &length);
	CHECK_RET("algo(%s) is not existed in image files!\n", name);

	ret = vpu_alloc_algo(&algo);
	CHECK_RET("vpu_alloc_algo failed!\n");

	strlcpy(algo->name, name, sizeof(vpu_name_t));
	algo->id[core] = id;
	algo->bin_ptr = mva;
	algo->bin_length = length;
	LOG_DBG("[vpu] vpu_hw_load_algo done, (%d/0x%lx/0x%x)\n", id, (unsigned long)mva, length);
	LOG_DBG("[vpu] vpu_hw_load_algo done, (%d/0x%lx/0x%x)\n",
		algo->id[core], (unsigned long)(algo->bin_ptr), algo->bin_length);

	if (needload) {
		ret = vpu_hw_load_algo(core, algo);
		CHECK_RET("vpu_hw_load_algo failed!\n");
		LOG_DBG("[vpu_%d] vpu_hw_load_algo done\n", core);
		ret = vpu_hw_get_algo_info(core, algo);
		CHECK_RET("vpu_hw_get_algo_info failed!\n");
		LOG_DBG("[vpu_%d] vpu_hw_get_algo_info done\n", core);
		ret = vpu_calc_prop_offset(algo->info_descs, algo->info_desc_count, &algo->info_length);
		CHECK_RET("vpu_calc_prop_offset[info] failed!\n");
		LOG_DBG("[vpu_%d] vpu_calc_prop_offset done,algo->info_length(0x%x)\n", core, algo->info_length);
		ret = vpu_calc_prop_offset(algo->sett_descs, algo->sett_desc_count, &algo->sett_length);
		CHECK_RET("vpu_calc_prop_offset[sett] failed!\n");
		LOG_DBG("[vpu_%d] vpu_calc_prop_offset done,algo->sett_length(0x%x)\n", core, algo->sett_length);
	}
	*ralgo = algo;
	return 0;

out:
	*ralgo = NULL;
	if (algo != NULL)
		vpu_free_algo(algo);
	return ret;
}

int vpu_alloc_algo(struct vpu_algo **ralgo)
{
	struct vpu_algo *algo;

	LOG_DBG("[vpu] vpu_alloc_algo +\n");
	algo = kzalloc(sizeof(vlist_type(struct vpu_algo)) + prop_info_data_length, GFP_KERNEL);
	if (algo == NULL) {
		LOG_ERR("vpu_alloc_algo(), algo=0x%p\n", algo);
		return -ENOMEM;
	}
	algo->info_ptr = (uintptr_t) algo + sizeof(vlist_type(struct vpu_algo));
	algo->info_length = prop_info_data_length;

	INIT_LIST_HEAD(vlist_link(algo, struct vpu_algo));
	*ralgo = algo;
	return 0;
}


int vpu_free_algo(struct vpu_algo *algo)
{
	if (algo == NULL)
		return 0;

	kfree(algo);
	return 0;
}

int vpu_alloc_request(struct vpu_request **rreq)
{
	struct vpu_request *req;

	req = kzalloc(sizeof(vlist_type(struct vpu_request)), GFP_KERNEL);
	if (req == NULL) {
		LOG_ERR("vpu_alloc_request(), node=0x%p\n", req);
		return -ENOMEM;
	}

	*rreq = req;

	return 0;
}

int vpu_free_request(struct vpu_request *req)
{
	if (req != NULL)
		kfree(req);
	return 0;
}

int vpu_dump_algo(struct seq_file *s)
{
	struct vpu_algo *debug_algo;
	struct vpu_prop_desc *prop_desc;
	struct vpu_port *port;
	char *debug_algo_name;
	uint32_t i, j;
	uint32_t data_length;
	char line_buffer[24 + 1] = {0};
	unsigned char *info_data;
	int debug_core = 0;

	if (vpu_get_name_of_algo(debug_core, debug_algo_id, &debug_algo_name)) {
		vpu_print_seq(s, "vpu_get_name_of_algo fail, id=%d\n", debug_algo_id);
		goto err;
	}

	if (vpu_create_algo(debug_core, debug_algo_name, &debug_algo, true) == 0) {
		vpu_print_seq(s, "[Algo: id=%d name=%s, address=0x%llx, length=0x%x]\n",
					  debug_algo_id, debug_algo->name,
					  debug_algo->bin_ptr, debug_algo->bin_length);

#define LINE_BAR "  +-----+---------------+-------+-------+\n"
		vpu_print_seq(s, LINE_BAR);
		vpu_print_seq(s, "  |%-5s|%-15s|%-7s|%-7s|\n", "Port", "Name", "Dir", "Usage");
		vpu_print_seq(s, LINE_BAR);

		for (i = 0; i < debug_algo->port_count; i++) {
			port = &debug_algo->ports[i];
			vpu_print_seq(s, "  |%-5d|%-15s|%-7s|%-7s|\n",
						  port->id, port->name,
						  g_vpu_port_dir_names[port->dir],
						  g_vpu_port_usage_names[port->usage]);
		}
		vpu_print_seq(s, LINE_BAR);
		vpu_print_seq(s, "\n");
#undef LINE_BAR

#define LINE_BAR "  +-----+---------------+-------+-------+------------------------------+\n"
		if (debug_algo->info_desc_count) {
			vpu_print_seq(s, LINE_BAR);
			vpu_print_seq(s, "  |%-5s|%-15s|%-7s|%-7s|%-30s|\n", "Info", "Name", "Type", "Count", "Value");
			vpu_print_seq(s, LINE_BAR);
		}

		for (i = 0; i < debug_algo->info_desc_count; i++) {
			prop_desc = &debug_algo->info_descs[i];
			data_length = prop_desc->count * g_vpu_prop_type_size[prop_desc->type];

			vpu_print_seq(s, "  |%-5d|%-15s|%-7s|%-7d|%04XH ",
						  prop_desc->id,
						  prop_desc->name,
						  g_vpu_prop_type_names[prop_desc->type],
						  prop_desc->count,
						  0);

			info_data = (unsigned char *) ((uintptr_t)debug_algo->info_ptr + prop_desc->offset);
			memset(line_buffer, ' ', 24);
			for (j = 0; j < data_length; j++, info_data++) {
				int pos = j % 8;

				if (j && pos == 0) {
					vpu_print_seq(s, "  |%-5s|%-15s|%-7s|%-7s|%04XH ",
								  "", "", "", "", j);
				}
				sprintf(line_buffer + pos * 3, "%02X", *info_data);
				line_buffer[pos * 3 + 2] = ' ';
				if (pos == 7 || j + 1 == data_length)
					vpu_print_seq(s, "%s|\n", line_buffer);
			}
		}

		if (debug_algo->info_desc_count) {
			vpu_print_seq(s, LINE_BAR);
			vpu_print_seq(s, "\n");
		}
#undef LINE_BAR

#define LINE_BAR "  +-----+---------------+-------+-------+-------+\n"
		if (debug_algo->sett_desc_count < 1)
			goto out;

		vpu_print_seq(s, LINE_BAR);
		vpu_print_seq(s, "  |%-5s|%-15s|%-7s|%-7s|%-7s|\n", "Sett", "Name", "Offset", "Type", "Count");
		vpu_print_seq(s, LINE_BAR);
		for (i = 0; i < debug_algo->sett_desc_count; i++) {
			prop_desc = &debug_algo->sett_descs[i];
			data_length = prop_desc->count * g_vpu_prop_type_size[prop_desc->type];

			vpu_print_seq(s, "  |%-5d|%-15s|%-7d|%-7s|%-7d|\n",
						  prop_desc->id,
						  prop_desc->name,
						  prop_desc->offset,
						  g_vpu_prop_type_names[prop_desc->type],
						  prop_desc->count);
		}
		vpu_print_seq(s, LINE_BAR);
		vpu_print_seq(s, "\n");
#undef LINE_BAR
out:
		vpu_free_algo(debug_algo);
		vpu_debug_func_core_state(debug_core, VCT_IDLE);
		vpu_shut_down(debug_core);
	} else {
		vpu_print_seq(s, "create algo fail, id=%d\n", debug_algo_id);
		LOG_ERR("create algo fail\n");
	}
err:
	return 0;
}

int vpu_set_algo_parameter(uint8_t param, int argc, int *args)
{
	int ret = 0;

	switch (param) {
	case VPU_DEBUG_ALGO_PARAM_DUMP_ALGO:
		ret = (argc == 1) ? 0 : -EINVAL;
		CHECK_RET("invalid argument, expected:1, received:%d\n", argc);

		debug_algo_id = args[0];

		break;
	default:
		LOG_ERR("unsupport the power parameter:%d\n", param);
		break;
	}
out:
	return ret;
}

