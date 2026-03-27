#!/usr/bin/env python3
import os
import struct
import re
import subprocess

PAGE_SIZE = os.sysconf("SC_PAGE_SIZE")
KPF_BUDDY = 10

# ===== 地址段配置 =====
RANGES = [
    0x29580000000,
    0xa9580000000,
    0x129580000000,
    0x1a9580000000,
]

RANGE_SIZE = 682 * 1024 * 1024 * 1024  # 682GB


def get_all_nodes():
    base = "/sys/devices/system/node/"
    return sorted([
        int(d.replace("node", ""))
        for d in os.listdir(base)
        if d.startswith("node")
    ])


def get_memory_block_size():
    with open("/sys/devices/system/memory/block_size_bytes") as f:
        return int(f.read().strip(), 16)


def get_node_blocks(node):
    path = f"/sys/devices/system/node/node{node}/"
    block_size = get_memory_block_size()
    blocks = []

    for name in os.listdir(path):
        if name.startswith("memory"):
            idx = int(name.replace("memory", ""))
            start = idx * block_size // PAGE_SIZE
            end = (idx * block_size + block_size) // PAGE_SIZE - 1
            blocks.append((start, end))

    return sorted(blocks)


def is_buddy(flags):
    return (flags >> KPF_BUDDY) & 1


def addr_to_pfn(addr):
    return addr // PAGE_SIZE


def pfn_to_addr(pfn):
    return pfn * PAGE_SIZE


def intersect(a_start, a_end, b_start, b_end):
    s = max(a_start, b_start)
    e = min(a_end, b_end)
    if s <= e:
        return s, e
    return None


def scan_node_free_segments(node):
    blocks = get_node_blocks(node)

    with open("/proc/kpageflags", "rb") as f:
        merged_start = None
        prev_state = None
        prev_end = None

        for start_pfn, end_pfn in blocks:

            # 处理 NUMA 内不连续
            if prev_end is not None and start_pfn > prev_end + 1:
                merged_start = None
                prev_state = None

            f.seek(start_pfn * 8)

            for pfn in range(start_pfn, end_pfn + 1):
                data = f.read(8)
                if not data:
                    break

                flags = struct.unpack("Q", data)[0]
                free = is_buddy(flags)

                if prev_state is None:
                    prev_state = free
                    merged_start = pfn

                elif free != prev_state:
                    if prev_state:
                        yield (merged_start, pfn - 1)
                    merged_start = pfn
                    prev_state = free

            prev_end = end_pfn

        if prev_state:
            yield (merged_start, prev_end)


def fmt(addr):
    return f"0x{addr:016x}"


def get_force_max_zoneorder():
    # 获取当前内核版本
    kernel_ver = subprocess.check_output(['uname', '-r'], text=True).strip()

    # 内核配置文件路径
    config_paths = f"/boot/config-{kernel_ver}"
    for path in config_paths:
        try:
            with open(path, 'r', errors='ignore') as f:
                content = f.read()

            # 正则匹配 CONFIG_FORCE_MAX_ZONEORDER=11 这种
            match = re.search(r'CONFIG_FORCE_MAX_ZONEORDER=(\d+)', content)
            if match:
                return int(match.group(1))

        except Exception:
            continue


    os_info = subprocess.check_output(['uname', '-a'], text=True)
    if 'Ubuntu' in os_info:
        return 13
    else:
        return 11


def round_up(a, b):
    return (a + b - 1) // b * b


def round_down(a, b):
    return a // b * b


def run_for_node(node, min_mb, target_ranges, zone_page):
    print(f"\n===== NUMA node {node} =====")

    total_intersect_mb = 0
    min_mb = min_mb << 20

    for free_start, free_end in scan_node_free_segments(node):
        for r_start, r_end in target_ranges:
            inter = intersect(free_start, free_end, r_start, r_end)
            if not inter:
                continue

            s, e = inter
            s = round_up(s * PAGE_SIZE, zone_page)
            e = round_down((e + 1) * PAGE_SIZE, zone_page)
            this_size = e - s

            if min_mb < zone_page:
                this_size = this_size // zone_page * min_mb
            else:
                this_size = this_size // min_mb * min_mb
            if this_size < min_mb:
                continue

            size_mb = this_size >> 20
            total_intersect_mb += size_mb

            print(f"FREE∩RANGE: {fmt(pfn_to_addr(s))} - {fmt(pfn_to_addr(e))}  size={size_mb:.2f} MB")

    print(f"Total intersect FREE: {total_intersect_mb:.2f} MB")
    return total_intersect_mb


def main():
    import argparse
    parser = argparse.ArgumentParser(description="NUMA free memory intersect tool (all nodes supported)")
    parser.add_argument("-n", "--node", type=int, help="NUMA node ID (default: all)")
    parser.add_argument("-m", "--min-mb", type=int, default=2)
    args = parser.parse_args()

    # 构造 range PFN
    target_ranges = []
    for base in RANGES:
        start = addr_to_pfn(base)
        end = addr_to_pfn(base + RANGE_SIZE - 1)
        target_ranges.append((start, end))

    # 决定扫描哪些 node
    if args.node is not None:
        nodes = [args.node]
    else:
        nodes = get_all_nodes()

    zone_page = PAGE_SIZE * (1 << (get_force_max_zoneorder() - 1))
    print(f"Get page size: {hex(PAGE_SIZE)}, zone page size: {hex(zone_page)}")
    print("Scanning NUMA nodes...\n")

    numa_free_mb = []
    for node in nodes:
        numa_free_mb.append(run_for_node(node, args.min_mb, target_ranges, zone_page))


    print(f"\n===== Summary =====")
    for node, free_mb in zip(nodes, numa_free_mb):
        print(f"NUMA node {node}: {free_mb:.2f} MB")

    total_free_mb = sum(numa_free_mb)
    print(f"PageSize: {hex(PAGE_SIZE)}, ZonePageSize: {hex(zone_page)} TotalFree: {(total_free_mb / 1024):.2f} GB")

if __name__ == "__main__":
    main()
