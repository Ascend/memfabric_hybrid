/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MF_HYBRID_HYBM_GVM_SYMBOL_GET_H
#define MF_HYBRID_HYBM_GVM_SYMBOL_GET_H

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/dma-mapping.h>

int gvm_symbol_get(void);
void gvm_symbol_put(void);

int drv_davinci_register_sub_module(const char *module_name, const struct file_operations *ops);
int drv_ascend_unregister_sub_module(const char *module_name);

struct data_input_info {
    void *data;
    u32 data_len;
    u32 in_len;
    u32 out_len;
    u32 msg_mode;
};

enum devdrv_s2s_msg_type {
    DEVDRV_S2S_MSG_DEVMM = 0,
    DEVDRV_S2S_MSG_TRSDRV = 1,
    DEVDRV_S2S_MSG_TEST = 2,
    DEVDRV_S2S_MSG_TYPE_MAX
};

#define DEVDRV_S2S_SYNC_MODE 1
#define DEVDRV_S2S_TO_HOST   0x1

typedef int (*s2s_msg_recv)(u32 local_devid, u32 sdid, struct data_input_info *data_info);
int devdrv_register_s2s_msg_proc_func(enum devdrv_s2s_msg_type msg_type, s2s_msg_recv func);
int devdrv_unregister_s2s_msg_proc_func(enum devdrv_s2s_msg_type msg_type);
int devdrv_s2s_msg_send(u32 devid, u32 sdid, enum devdrv_s2s_msg_type msg_type, u32 direction,
                        struct data_input_info *data_info);
int devdrv_common_msg_send(u32 devid, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len, int msg_type);

struct spod_info {
    unsigned int sdid;
    unsigned int scale_type;
    unsigned int super_pod_id;
    unsigned int server_id;
    unsigned int reserve[8];
};

struct sdid_parse_info {
    unsigned int server_id;
    unsigned int chip_id;
    unsigned int die_id;
    unsigned int udevid;
    unsigned int reserve[8];
};

#define UDEVID_BIT_LEN 16

int dbl_get_spod_info(unsigned int udevid, struct spod_info *s);
int dbl_parse_sdid(unsigned int sdid, struct sdid_parse_info *s);
struct device *uda_get_device(u32 udevid);

void *obmm_alloc(int numaid, unsigned long len, int flags);
void obmm_free(void *p, int flags);

dma_addr_t devdrv_dma_map_page(struct device *dev, struct page *page,
                               size_t offset, size_t size, enum dma_data_direction dir);

#endif // MF_HYBRID_HYBM_GVM_SYMBOL_GET_H
