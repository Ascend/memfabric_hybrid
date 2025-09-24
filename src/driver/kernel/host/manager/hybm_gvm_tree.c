/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "hybm_gvm_tree.h"
#include "hybm_gvm_log.h"

/*
 * key tree
 */
bool gvm_key_tree_insert(struct gvm_rbtree *rtree, struct gvm_node *data)
{
    struct rb_node **new = NULL;
    struct rb_node *parent = NULL;
    mutex_lock(&rtree->lock);
    new = &(rtree->tree.rb_node);
    while (*new) {
        struct gvm_node *tmp = container_of(*new, struct gvm_node, key_node);
        parent = *new;
        if (data->shm_key < tmp->shm_key) {
            new = &((*new)->rb_left);
        } else if (data->shm_key > tmp->shm_key) {
            new = &((*new)->rb_right);
        } else {
            mutex_unlock(&rtree->lock);
            return false;
        }
    }
    /* Add new node and rebalance tree. */
    rb_link_node(&data->key_node, parent, new);
    rb_insert_color(&data->key_node, &rtree->tree);
    kref_get(&data->ref); // 插入后加引用计数,remove时减去
    mutex_unlock(&rtree->lock);
    return true;
}

void gvm_key_tree_remove(struct gvm_rbtree *rtree, struct gvm_node *data, void (*release)(struct kref *kref))
{
    mutex_lock(&rtree->lock);
    if (!RB_EMPTY_NODE(&data->key_node)) {
        rb_erase(&data->key_node, &rtree->tree);
        RB_CLEAR_NODE(&data->key_node);
        kref_put(&data->ref, release);
    }
    mutex_unlock(&rtree->lock);
}

struct gvm_node *gvm_key_tree_search_and_inc(struct gvm_rbtree *rtree, u64 key)
{
    struct rb_node *node = NULL;
    mutex_lock(&rtree->lock);
    node = rtree->tree.rb_node;
    while (node) {
        struct gvm_node *tmp = container_of(node, struct gvm_node, key_node);
        if (key < tmp->shm_key) {
            node = node->rb_left;
        } else if (key > tmp->shm_key) {
            node = node->rb_right;
        } else {
            kref_get(&tmp->ref);
            mutex_unlock(&rtree->lock);
            return tmp;
        }
    }
    mutex_unlock(&rtree->lock);
    return NULL;
}

/*
 * va tree
 */
bool gvm_va_tree_cross(struct gvm_rbtree *rtree, u64 va, u64 size)
{
    struct rb_node *node = NULL;
    bool ret = false;
    mutex_lock(&rtree->lock);
    node = rtree->tree.rb_node;
    while (node) {
        struct gvm_va_node_head *tmp = container_of(node, struct gvm_va_node_head, va_node);
        if ((va <= tmp->va && tmp->va < (va + size)) || (tmp->va <= va && va < (tmp->va + tmp->size))) {
            ret = true;
            break;
        }
        if (va < tmp->va) {
            node = node->rb_left;
        } else if (va > tmp->va) {
            node = node->rb_right;
        }
    }
    mutex_unlock(&rtree->lock);
    return ret;
}

// search and add ref
void *gvm_va_tree_get_first_and_inc(struct gvm_rbtree *rtree)
{
    struct rb_node *node = NULL;
    struct gvm_va_node_head *tmp = NULL;
    mutex_lock(&rtree->lock);
    node = rb_first(&rtree->tree);
    if (node != NULL) {
        tmp = container_of(node, struct gvm_va_node_head, va_node);
        kref_get(&tmp->ref);
    }
    mutex_unlock(&rtree->lock);
    return (void *)tmp;
}

// search and add ref
void *gvm_va_tree_search_and_inc(struct gvm_rbtree *rtree, u64 va)
{
    struct rb_node *node = NULL;
    mutex_lock(&rtree->lock);
    node = rtree->tree.rb_node;
    while (node) {
        struct gvm_va_node_head *tmp = container_of(node, struct gvm_va_node_head, va_node);
        if (va < tmp->va) {
            node = node->rb_left;
        } else if (va > tmp->va) {
            node = node->rb_right;
        } else {
            kref_get(&tmp->ref);
            mutex_unlock(&rtree->lock);
            return (void *)tmp;
        }
    }
    mutex_unlock(&rtree->lock);
    return NULL;
}

bool gvm_va_tree_insert(struct gvm_rbtree *rtree, void *data)
{
    struct rb_node **new = NULL;
    struct rb_node *parent = NULL;
    struct gvm_va_node_head *input = (struct gvm_va_node_head *)data;
    if (input->va == 0ULL || input->size == 0ULL) {
        hybm_gvm_err("invalid va or size, input:size(0x%llx)", input->size);
        return false;
    }

    mutex_lock(&rtree->lock);
    new = &(rtree->tree.rb_node);
    while (*new) {
        struct gvm_va_node_head *tmp = container_of(*new, struct gvm_va_node_head, va_node);
        if ((input->va <= tmp->va && tmp->va < (input->va + input->size)) ||
            (tmp->va <= input->va && input->va < (tmp->va + tmp->size))) {
            hybm_gvm_err("addr overlaps with existing node, input:size(0x%llx) found:size(0x%llx)",
                         input->size, tmp->size);
            mutex_unlock(&rtree->lock);
            return false;
        }

        parent = *new;
        if (input->va < tmp->va) {
            new = &((*new)->rb_left);
        } else if (input->va > tmp->va) {
            new = &((*new)->rb_right);
        }
    }
    /* Add new node and rebalance tree. */
    rb_link_node(&input->va_node, parent, new);
    rb_insert_color(&input->va_node, &rtree->tree);
    kref_get(&input->ref); // 插入后加引用计数,remove时减去
    mutex_unlock(&rtree->lock);
    return true;
}

// remove node and dec ref
void gvm_va_tree_remove_and_dec(struct gvm_rbtree *rtree, void *data, void (*release)(struct kref *kref))
{
    struct gvm_va_node_head *input = (struct gvm_va_node_head *)data;

    mutex_lock(&rtree->lock);
    if (!RB_EMPTY_NODE(&input->va_node)) {
        rb_erase(&input->va_node, &rtree->tree);
        RB_CLEAR_NODE(&input->va_node);
        kref_put(&input->ref, release); // 减去insert时加的计数
    }
    mutex_unlock(&rtree->lock);
    kref_put(&input->ref, release); // 减去init时的计数,表示可以free了
}
