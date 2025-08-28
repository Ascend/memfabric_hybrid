/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "hybm_gvm_symbol_get.h"

#include <linux/module.h>
#include <linux/slab.h>

#include "hybm_gvm_log.h"

struct gvm_symbol_manager {
    int (*davinci_register_func)(const char *module_name, const struct file_operations *ops);
    int (*davinci_unregister_func)(const char *module_name);
    int (*s2s_msg_register_func)(enum devdrv_s2s_msg_type msg_type, s2s_msg_recv func);
    int (*s2s_msg_unregister_func)(enum devdrv_s2s_msg_type msg_type);
    int (*s2s_msg_send_func)(u32 devid, u32 sdid, enum devdrv_s2s_msg_type msg_type, u32 direction,
                             struct data_input_info *data_info);
    int (*get_spod_info_func)(unsigned int udevid, struct spod_info *s);
    int (*get_parse_sdid_func)(unsigned int sdid, struct sdid_parse_info *s);
    int (*common_send_func)(u32 devid, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len, int msg_type);
    void *(*obmm_alloc_func)(int numaid, unsigned long len, int flags);
    void (*obmm_free_func)(void *p, int flags);
    struct device *(*uda_get_device_func)(u32 udevid);
    dma_addr_t (*dma_map_page_func)(struct device *dev, struct page *page,
                                    size_t offset, size_t size, enum dma_data_direction dir);
};
struct gvm_symbol_manager *g_mgr = NULL;

int gvm_symbol_get(void)
{
    g_mgr = kzalloc(sizeof(struct gvm_symbol_manager), GFP_KERNEL | __GFP_ACCOUNT);
    if (g_mgr == NULL) {
        hybm_gvm_err("Kzalloc gvm_symbol_manager fail.");
        return -ENOMEM;
    }

    g_mgr->davinci_register_func = __symbol_get("drv_davinci_register_sub_module");
    if (g_mgr->davinci_register_func == NULL) {
        hybm_gvm_err("get symbol drv_davinci_register_sub_module fail.");
        goto failed;
    }

    g_mgr->davinci_unregister_func = __symbol_get("drv_ascend_unregister_sub_module");
    if (g_mgr->davinci_unregister_func == NULL) {
        hybm_gvm_err("get symbol drv_ascend_unregister_sub_module fail.");
        goto failed;
    }

    g_mgr->s2s_msg_register_func = __symbol_get("devdrv_register_s2s_msg_proc_func");
    if (g_mgr->s2s_msg_register_func == NULL) {
        hybm_gvm_err("get symbol devdrv_register_s2s_msg_proc_func fail.");
        goto failed;
    }

    g_mgr->s2s_msg_unregister_func = __symbol_get("devdrv_unregister_s2s_msg_proc_func");
    if (g_mgr->s2s_msg_unregister_func == NULL) {
        hybm_gvm_err("get symbol devdrv_unregister_s2s_msg_proc_func fail.");
        goto failed;
    }

    g_mgr->s2s_msg_send_func = __symbol_get("devdrv_s2s_msg_send");
    if (g_mgr->s2s_msg_send_func == NULL) {
        hybm_gvm_err("get symbol devdrv_s2s_msg_send fail.");
        goto failed;
    }

    g_mgr->get_spod_info_func = __symbol_get("dbl_get_spod_info");
    if (g_mgr->get_spod_info_func == NULL) {
        hybm_gvm_err("get symbol dbl_get_spod_info fail.");
        goto failed;
    }

    g_mgr->get_parse_sdid_func = __symbol_get("dbl_parse_sdid");
    if (g_mgr->get_parse_sdid_func == NULL) {
        hybm_gvm_err("get symbol dbl_parse_sdid fail.");
        goto failed;
    }

    g_mgr->common_send_func = __symbol_get("devdrv_common_msg_send");
    if (g_mgr->common_send_func == NULL) {
        hybm_gvm_err("get symbol devdrv_common_msg_send fail.");
        goto failed;
    }

    g_mgr->obmm_alloc_func = __symbol_get("obmm_alloc");
    if (g_mgr->obmm_alloc_func == NULL) {
        hybm_gvm_err("get symbol obmm_alloc fail.");
        goto failed;
    }

    g_mgr->obmm_free_func = __symbol_get("obmm_free");
    if (g_mgr->obmm_free_func == NULL) {
        hybm_gvm_err("get symbol obmm_free fail.");
        goto failed;
    }

    g_mgr->uda_get_device_func = __symbol_get("uda_get_device");
    if (g_mgr->uda_get_device_func == NULL) {
        hybm_gvm_err("get symbol uda_get_device fail.");
        goto failed;
    }

    g_mgr->dma_map_page_func = __symbol_get("devdrv_dma_map_page");
    if (g_mgr->dma_map_page_func == NULL) {
        hybm_gvm_err("get symbol devdrv_dma_map_page fail.");
        goto failed;
    }

    return 0;
failed:
    gvm_symbol_put();
    return -ENXIO;
}

void gvm_symbol_put(void)
{
    if (g_mgr == NULL) {
        return;
    }

    if (g_mgr->davinci_register_func != NULL) {
        __symbol_put("drv_davinci_register_sub_module");
    }
    if (g_mgr->davinci_unregister_func != NULL) {
        __symbol_put("drv_ascend_unregister_sub_module");
    }
    if (g_mgr->s2s_msg_register_func != NULL) {
        __symbol_put("devdrv_register_s2s_msg_proc_func");
    }
    if (g_mgr->s2s_msg_unregister_func != NULL) {
        __symbol_put("devdrv_unregister_s2s_msg_proc_func");
    }
    if (g_mgr->s2s_msg_send_func != NULL) {
        __symbol_put("devdrv_s2s_msg_send");
    }
    if (g_mgr->get_spod_info_func != NULL) {
        __symbol_put("dbl_get_spod_info");
    }
    if (g_mgr->get_parse_sdid_func != NULL) {
        __symbol_put("dbl_parse_sdid");
    }
    if (g_mgr->common_send_func != NULL) {
        __symbol_put("devdrv_common_msg_send");
    }
    if (g_mgr->obmm_alloc_func != NULL) {
        __symbol_put("obmm_alloc");
    }
    if (g_mgr->obmm_free_func != NULL) {
        __symbol_put("obmm_free");
    }
    if (g_mgr->uda_get_device_func != NULL) {
        __symbol_put("uda_get_device");
    }
    if (g_mgr->dma_map_page_func != NULL) {
        __symbol_put("devdrv_dma_map_page");
    }

    kfree(g_mgr);
    g_mgr = NULL;
}

int drv_davinci_register_sub_module(const char *module_name, const struct file_operations *ops)
{
    if (g_mgr == NULL || g_mgr->davinci_register_func == NULL) {
        hybm_gvm_err("symbol drv_davinci_register_sub_module not get.");
        return -ENXIO;
    }

    return g_mgr->davinci_register_func(module_name, ops);
}

int drv_ascend_unregister_sub_module(const char *module_name)
{
    if (g_mgr == NULL || g_mgr->davinci_unregister_func == NULL) {
        hybm_gvm_err("symbol drv_ascend_unregister_sub_module not get.");
        return -ENXIO;
    }

    return g_mgr->davinci_unregister_func(module_name);
}

int devdrv_register_s2s_msg_proc_func(enum devdrv_s2s_msg_type msg_type, s2s_msg_recv func)
{
    if (g_mgr == NULL || g_mgr->s2s_msg_register_func == NULL) {
        hybm_gvm_err("symbol devdrv_register_s2s_msg_proc_func not get.");
        return -ENXIO;
    }

    return g_mgr->s2s_msg_register_func(msg_type, func);
}

int devdrv_unregister_s2s_msg_proc_func(enum devdrv_s2s_msg_type msg_type)
{
    if (g_mgr == NULL || g_mgr->s2s_msg_unregister_func == NULL) {
        hybm_gvm_err("symbol devdrv_unregister_s2s_msg_proc_func not get.");
        return -ENXIO;
    }

    return g_mgr->s2s_msg_unregister_func(msg_type);
}

int devdrv_s2s_msg_send(u32 devid, u32 sdid, enum devdrv_s2s_msg_type msg_type, u32 direction,
                        struct data_input_info *data_info)
{
    if (g_mgr == NULL || g_mgr->s2s_msg_send_func == NULL) {
        hybm_gvm_err("symbol devdrv_s2s_msg_send not get.");
        return -ENXIO;
    }

    return g_mgr->s2s_msg_send_func(devid, sdid, msg_type, direction, data_info);
}

int dbl_get_spod_info(unsigned int udevid, struct spod_info *s)
{
    if (g_mgr == NULL || g_mgr->get_spod_info_func == NULL) {
        hybm_gvm_err("symbol dbl_get_spod_info not get.");
        return -ENXIO;
    }

    return g_mgr->get_spod_info_func(udevid, s);
}

int dbl_parse_sdid(unsigned int sdid, struct sdid_parse_info *s)
{
    if (g_mgr == NULL || g_mgr->get_parse_sdid_func == NULL) {
        hybm_gvm_err("symbol dbl_parse_sdid not get.");
        return -ENXIO;
    }

    return g_mgr->get_parse_sdid_func(sdid, s);
}

int devdrv_common_msg_send(u32 devid, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len, int msg_type)
{
    if (g_mgr == NULL || g_mgr->common_send_func == NULL) {
        hybm_gvm_err("symbol devdrv_common_msg_send not get.");
        return -ENXIO;
    }

    return g_mgr->common_send_func(devid, data, in_data_len, out_data_len, real_out_len, msg_type);
}

void *obmm_alloc(int numaid, unsigned long len, int flags)
{
    if (g_mgr == NULL || g_mgr->obmm_alloc_func == NULL) {
        hybm_gvm_err("symbol obmm_alloc not get.");
        return NULL;
    }

    return g_mgr->obmm_alloc_func(numaid, len, flags);
}

void obmm_free(void *p, int flags)
{
    if (g_mgr == NULL || g_mgr->obmm_free_func == NULL) {
        hybm_gvm_err("symbol obmm_free not get.");
        return;
    }

    return g_mgr->obmm_free_func(p, flags);
}

struct device *uda_get_device(u32 udevid)
{
    if (g_mgr == NULL || g_mgr->uda_get_device_func == NULL) {
        hybm_gvm_err("symbol uda_get_device not get.");
        return NULL;
    }

    return g_mgr->uda_get_device_func(udevid);
}

dma_addr_t devdrv_dma_map_page(struct device *dev, struct page *page,
                               size_t offset, size_t size, enum dma_data_direction dir)
{
    if (g_mgr == NULL || g_mgr->dma_map_page_func == NULL) {
        hybm_gvm_err("symbol devdrv_dma_map_page not get.");
        return 0;
    }

    return g_mgr->dma_map_page_func(dev, page, offset, size, dir);
}