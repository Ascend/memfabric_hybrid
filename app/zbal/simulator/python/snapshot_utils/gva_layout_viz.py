import json
import pickle

import matplotlib.pyplot as plt
import matplotlib.patches as patches
from matplotlib.lines import Line2D
from snapshot_utils.gva_layout_analyzer import MemoryAllocatorSimulator


def visualize_sim_memory(simulator: MemoryAllocatorSimulator, display_segments=False, display_blocks=True):
    """
    simulator: a MemoryAllocatorSimulator inited from torch memviz pkl
    display_segments: whether display segments
    display_blocks: whether display blocks
    """
    # 绘图准备
    fig, ax = plt.subplots(figsize=(12, 2))

    # 绘制总背景（空闲内存）
    ax.add_patch(plt.Rectangle((simulator.gva_base_addr, 0), simulator.gva_size, 1, color='#f0f0f0', label='Free'))

    # 绘制已分配segments
    if display_segments:
        for start_addr, end_addr in simulator.get_all_segments():
            ax.broken_barh([(start_addr, end_addr - start_addr)], (0.05, 0.9),
                           facecolors='#D3D3D3', edgecolor='black', alpha=0.3)

    # 绘制已分配blocks
    if display_blocks:
        for used_block in simulator.get_used_blocks():
            ax.broken_barh([(used_block.addr, used_block.size)], (0.1, 0.8),
                           facecolors='#4CAF50', edgecolor='black', alpha=0.8)

    # 设置坐标轴
    ax.set_xlim(simulator.gva_base_addr, simulator.gva_base_addr + simulator.gva_size)
    ax.set_ylim(0, 1)
    ax.set_yticks([])

    # 格式化坐标轴为 GB 或 Hex
    def format_func(value, tick_number):
        gb_val = (value - simulator.gva_base_addr) / (1024 ** 3)
        return f"{gb_val:.1f}G"

    ax.xaxis.set_major_formatter(plt.FuncFormatter(format_func))
    plt.title(f"Memory Layout Visualization (Total: {simulator.gva_size / (1024 ** 3)}GB)")
    plt.xlabel("Address")

    # 添加图例
    legend_elements = [Line2D([0], [0], color='#f0f0f0', lw=4, label='Unused')]
    if display_segments:
        legend_elements.append(Line2D([0], [0], color='#f0f0f0', lw=4, label='Allocated Segment'))
    if display_blocks:
        legend_elements.append(Line2D([0], [0], color='#f0f0f0', lw=4, label='Allocated Block'))
    ax.legend(handles=legend_elements, loc='upper right')

    plt.tight_layout()
    plt.show()


def parser_file_ops(input_file_path, device_id=0, get_traces=True):
    """helper func for both pkl & json type"""
    if input_file_path.endswith(".pkl"):
        with open(input_file_path, 'rb') as f1:
            ori_snap = pickle.load(f1)
    elif input_file_path.endswith(".json"):
        with open(input_file_path, 'r', encoding='utf-8') as f1:
            ori_snap = json.load(f1)
    else:
        raise ValueError('not support layout file format')

    if get_traces:
        return ori_snap['device_traces'][device_id]
    else:
        return ori_snap['segments']


def visualize_file_memory(total_size_gb, start_addr, operations, display_segments=True, display_blocks=True):
    """
    可视化traces文件
    total_size_gb: (e.g. 30 stands for 30G in total mem poll)
    start_addr: start addr in hex (e.g. 0x0)
    operations: torch memviz pkl format with PKL['device_traces'][device_id]
    display_segments: whether display segments
    display_blocks: whether display blocks
    """

    total_size_bytes = total_size_gb * 1024 * 1024 * 1024
    start_addr_int = int(start_addr, 16) if isinstance(start_addr, str) else start_addr

    base_addr = start_addr

    active_blocks = {}
    blk_max_cnt = 0
    active_segments = {}
    seg_max_cnt = 0

    # 处理序列
    if display_segments:
        for op in operations:
            op_type = op['action']
            if op_type == 'segment_alloc':
                addr, size = op['addr'], op['size']
                addr_int = int(addr, 16) if isinstance(addr, str) else addr
                base_addr = min(base_addr, addr_int)
                active_segments[addr_int] = size
                seg_max_cnt = max(seg_max_cnt, len(active_segments))
            elif op_type == 'segment_free':
                addr = op['addr']
                addr_int = int(addr, 16) if isinstance(addr, str) else addr
                if addr_int in active_segments:
                    del active_segments[addr_int]

    if display_blocks:
        for op in operations:
            op_type = op['action']
            if op_type == 'alloc':
                addr, size = op['addr'], op['size']
                addr_int = int(addr, 16) if isinstance(addr, str) else addr
                base_addr = min(base_addr, addr_int)
                active_blocks[addr_int] = size
                blk_max_cnt = max(blk_max_cnt, len(active_blocks))
            elif op_type == 'free_completed':
                addr = op['addr']
                addr_int = int(addr, 16) if isinstance(addr, str) else addr
                if addr_int in active_blocks:
                    del active_blocks[addr_int]

    print(f"blocks info: max num:[{blk_max_cnt}], active num:[{len(active_blocks)}]")
    print(f"segments info: max num:[{seg_max_cnt}], active num:[{len(active_segments)}]")

    # 绘图准备
    start_addr_int = base_addr
    fig, ax = plt.subplots(figsize=(12, 2))

    # 绘制总背景（空闲内存）
    ax.add_patch(plt.Rectangle((start_addr_int, 0), total_size_bytes, 1, color='#f0f0f0', label='Free'))

    # 绘制已分配segments
    for addr, size in active_segments.items():
        ax.broken_barh([(addr, size)], (0.05, 0.9), facecolors='#D3D3D3', edgecolor='black', alpha=0.3)

    # 绘制已分配blocks
    for addr, size in active_blocks.items():
        ax.broken_barh([(addr, size)], (0.1, 0.8), facecolors='#4CAF50', edgecolor='black', alpha=0.8)

    # 设置坐标轴
    ax.set_xlim(start_addr_int, start_addr_int + total_size_bytes)
    ax.set_ylim(0, 1)
    ax.set_yticks([])

    # 格式化坐标轴为 GB 或 Hex
    def format_func(value, tick_number):
        gb_val = (value - start_addr_int) / (1024 ** 3)
        return f"{gb_val:.1f}G"

    ax.xaxis.set_major_formatter(plt.FuncFormatter(format_func))
    plt.title(f"Memory Layout Visualization (Total: {total_size_gb}GB)")
    plt.xlabel("Address Offset from Start")

    # 添加图例
    legend_elements = [Line2D([0], [0], color='#4CAF50', lw=4, label='Allocated'),
                       Line2D([0], [0], color='#f0f0f0', lw=4, label='Unused')]
    ax.legend(handles=legend_elements, loc='upper right')

    plt.tight_layout()
    plt.show()


def visualize_file_snapshot(total_size_gb, start_addr, operations):
    """
    可视化snapshot文件
    total_size_gb: (e.g. 30 stands for 30G in total mem poll)
    start_addr: start addr in hex (e.g. 0x0)
    operations: torch memviz pkl format with PKL['segments']
    """
    # --- 1. 数据预处理与空隙统计 ---
    # 确保按地址排序以识别空隙
    sorted_segs = sorted(operations, key=lambda x: x['address'])

    gaps = []
    all_addresses = [s['address'] for s in operations]
    all_ends = [s['address'] + s['total_size'] for s in operations]

    # 计算空隙
    for i in range(len(sorted_segs) - 1):
        curr_end = sorted_segs[i]['address'] + sorted_segs[i]['total_size']
        next_start = sorted_segs[i + 1]['address']
        if next_start > curr_end:
            gap_size = next_start - curr_end
            gaps.append({
                'gap_start': curr_end,
                'gap_end': next_start,
                'size': gap_size,
                'size_mb': gap_size / (1024**2)
            })

    # 按大小降序排列空隙列表用于返回
    sorted_gap_list = sorted(gaps, key=lambda x: x['size'], reverse=True)

    # --- 2. 绘图坐标计算 ---
    base_addr = min(min(all_addresses), start_addr)
    # 如果指定了 total_size_gb 则使用，否则根据数据范围动态计算
    total_range_bytes = total_size_gb * (1024**3) if total_size_gb > 0 else (max(all_ends) - base_addr)

    fig, ax = plt.subplots(figsize=(14, 3))

    # 颜色配置
    colors = {'active': '#4CAF50', 'inactive': '#FF9800', 'segment': '#D3D3D3', 'gap': '#FFCCCC'}

    # 绘制总背景
    ax.add_patch(patches.Rectangle((base_addr, 0), total_range_bytes, 1, color='#f9f9f9', label='Range'))

    # --- 3. 循环绘制 ---
    # A. 绘制空隙 (Gaps) - 使用红色淡化区域表示“无地址”
    for gap in gaps:
        ax.broken_barh([(gap['gap_start'], gap['size'])], (0, 1),
                       facecolors=colors['gap'], alpha=0.4, hatch='//', edgecolor='#ff5252')
        # 在较大的空隙上标注大小
        if gap['size_mb'] > 100:
            ax.text(gap['gap_start'] + gap['size'] / 2, 0.05, f"Gap: {gap['size_mb']:.0f}MB",
                    ha='center', color='red', fontsize=8)

    # B. 绘制 Segments 和 Blocks
    for seg in sorted_segs:
        ax.broken_barh([(seg['address'], seg['total_size'])], (0.05, 0.9),
                       facecolors=colors['segment'], edgecolor='black', alpha=0.3)

        for blk in seg['blocks']:
            color = colors['active'] if blk['state'] == 'active_allocated' else colors['inactive']
            ax.broken_barh([(blk['address'], blk['size'])], (0.2, 0.6),
                           facecolors=color, edgecolor='black', alpha=0.8)

    # --- 4. 格式化与输出 ---
    ax.set_xlim(base_addr, base_addr + total_range_bytes)
    ax.set_ylim(0, 1.2)
    ax.set_yticks([])

    def format_gb(value, tick_number):
        gb_offset = (value - base_addr) / (1024**3)
        return f"+{gb_offset:.2f}G"

    ax.xaxis.set_major_formatter(plt.FuncFormatter(format_gb))

    # 图例
    custom_lines = [
        Line2D([0], [0], color=colors['segment'], lw=6, alpha=0.3),
        Line2D([0], [0], color=colors['active'], lw=6),
        Line2D([0], [0], color=colors['inactive'], lw=6)
    ]
    ax.legend(custom_lines, ['Segment', 'Active Block', 'Inactive Block'],
              loc='upper right', bbox_to_anchor=(1, 1.25), ncol=4)

    plt.title(f"Memory Layout & Gaps (Base: {hex(base_addr)})", pad=30)
    plt.xlabel("Address Offset (GB)")
    plt.tight_layout()
    plt.show()

    return sorted_gap_list # 返回排序后的空隙列表


def parse_mem_to_profile(operations, output_file, export_segments=True, with_unfree=False):
    """
    将内存操作记录解析为 Chrome Trace Profiler 格式。

    参数:
    - operations: 包含内存操作的列表 (来自 torch memviz pkl)
    - output_file: 输出的 JSON 文件路径
    - export_segments: 是否导出 segment 数据 (True) 或 block 数据 (False)
    - with_unfree: 是否包含未释放的内存块
    """

    def get_size_group(size_bytes):
        """内部子函数：根据内存大小返回对应的分组名称（泳道）"""
        mb = size_bytes / (1024 * 1024)
        if mb < 16:
            return "< 16MB (Small)"
        elif 16 <= mb <= 128:
            return "16MB - 128MB (Medium)"
        else:
            return "> 128MB (Large)"

    trace_events = []
    active_allocs = {}  # 记录当前活跃的分配: addr -> (start_ts, size, op_info)

    current_ts = 0
    step_duration = 100  # 逻辑时间步长
    total_usage = 0

    # 根据配置确定 malloc/free 的判定 Key
    if export_segments:
        malloc_key = "segment_alloc"
        free_key = "segment_free"
    else:
        malloc_key = "alloc"
        free_key = "free_completed"

    # --- 1. 预处理：定义线程（泳道）的名称和排序权重 ---
    groups = ["> 128MB (Large)", "16MB - 128MB (Medium)", "< 16MB (Small)"]
    for i, group_name in enumerate(groups):
        # 定义泳道显示名称
        trace_events.append({
            "name": "thread_name",
            "ph": "M",
            "pid": "NPU_HBM",
            "tid": group_name,
            "args": {"name": group_name}
        })
        # 定义泳道排序索引（让大的块显示在最上方）
        trace_events.append({
            "name": "thread_sort_index",
            "ph": "M",
            "pid": "NPU_HBM",
            "tid": group_name,
            "args": {"sort_index": i}
        })

    # --- 2. 遍历操作流 ---
    for op in operations:
        current_ts += step_duration
        addr = op['addr']
        action = op['action']

        if action == malloc_key:
            size = op['size']
            # 存入活跃字典以便 free 时计算时长
            active_allocs[addr] = (current_ts, size, op)

            # 更新计数器：内存增加
            total_usage += size
            trace_events.append({
                "name": "Memory Usage",
                "ph": "C",
                "ts": current_ts,
                "pid": "NPU_HBM",
                "args": {"Allocated_MB": total_usage / (1024 * 1024)}
            })

        elif action == free_key:
            if addr in active_allocs:
                start_ts, size, alloc_op = active_allocs.pop(addr)
                duration = current_ts - start_ts

                # 判定该块属于哪个泳道
                group_name = get_size_group(size)

                # 生成矩形块事件 (X 事件)
                trace_events.append({
                    "name": f"Block_{size / (1024 * 1024):.2f}_MB",
                    "ph": "X",
                    "ts": start_ts,
                    "dur": duration,
                    "pid": "NPU_HBM",
                    "tid": group_name,
                    "args": {
                        "address": hex(addr),
                        "size": f"{size / (1024 * 1024):.2f} MB",
                        "alloc_step": start_ts // step_duration
                    }
                })

                # 更新计数器：内存减少
                total_usage -= size
                trace_events.append({
                    "name": "Memory Usage",
                    "ph": "C",
                    "ts": current_ts,
                    "pid": "NPU_HBM",
                    "args": {"Allocated_MB": total_usage / (1024 * 1024)}
                })

    # --- 3. 可选：处理未释放内存 ---
    if with_unfree:
        for addr, (start_ts, size, _) in active_allocs.items():
            group_name = get_size_group(size)
            trace_events.append({
                "name": f"Unfreed_{size / (1024 * 1024):.2f}_MB",
                "ph": "X",
                "ts": start_ts,
                "dur": current_ts - start_ts,
                "pid": "NPU_HBM",
                "tid": group_name,
                "args": {
                    "address": hex(addr),
                    "size": f"{size / (1024 * 1024):.2f} MB",
                    "status": "still_allocated"
                }
            })

    # --- 4. 导出 JSON ---
    with open(output_file, 'w') as f:
        json.dump({"traceEvents": trace_events}, f, indent=2)

    print(f"Success: {output_file} generated with size-based grouping.")


if __name__ == "__main__":
    file_path = "../../../test/workspace/oom_log.6.json"
    ops = parser_file_ops(file_path, get_traces=False)
    # visualize_file_memory(50, 0xFFFFFFFFFFFF, ops)
    # parse_mem_to_profile(ops, output_file="../../../test/workspace/prof.json")
    ret = visualize_file_snapshot(0, 0xFFFFFFFFFFFF, ops)
