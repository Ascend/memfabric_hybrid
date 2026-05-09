from sortedcontainers import SortedDict
from tqdm import tqdm


class Block:
    def __init__(self, addr, size, status, segment_id):
        self.addr = addr
        self.size = size
        self.status = status  # 'FREE' or 'USED'
        self.segment_id = segment_id

    def __repr__(self):
        return f"<{self.status} | Addr: {self.addr}, Size: {self.size}>"


class MemoryAllocatorSimulator:
    def __init__(self, gva_base_addr=None, gva_size=58 * (1024 ** 3)):
        # 使用 SortedDict 维护地址有序性
        self.address_map = SortedDict()
        # 记录 segment 范围：{start_addr: end_addr}
        self.segments = {}
        self.gva_base_addr = gva_base_addr
        self.gva_size = gva_size

    def _segment_alloc(self, addr, size):
        if self.gva_base_addr is None:
            self.gva_base_addr = addr
        self.segments[addr] = addr + size
        # 新 segment 初始为一个大的 free_block
        new_block = Block(addr, size, 'FREE', addr)
        self.address_map[addr] = new_block

    def _segment_free(self, addr):
        if addr not in self.segments:
            raise ValueError("can't free unallocated segment")
        # end_addr = self.segments[addr]
        # 找出该段地址范围内的所有 key
        # keys_to_remove = list(self.address_map.irange(addr, end_addr - 1))
        # assert len(keys_to_remove) == 1
        # del self.address_map[keys_to_remove[0]]
        del self.address_map[addr]
        del self.segments[addr]

    def _alloc(self, addr, size):
        # 查找小于等于 addr 的最大起始地址
        idx = self.address_map.bisect_right(addr) - 1
        if not idx >= 0:
            raise ValueError("block must belong to a segment")

        base_addr = self.address_map.iloc[idx]
        target_block = self.address_map[base_addr]

        # 校验：必须是 FREE 且空间包含请求范围
        if not (target_block.status == 'FREE' and addr + size <= target_block.addr + target_block.size):
            raise ValueError("range must be free")

        old_addr = target_block.addr
        old_size = target_block.size
        seg_id = target_block.segment_id

        # 从 map 中移除旧的 free block
        del self.address_map[old_addr]

        # 1. 前部剩余
        if addr > old_addr:
            self.address_map[old_addr] = Block(old_addr, addr - old_addr, 'FREE', seg_id)

        # 2. 中间分配部
        self.address_map[addr] = Block(addr, size, 'USED', seg_id)

        # 3. 后部剩余
        if old_addr + old_size > addr + size:
            after_addr = addr + size
            after_size = (old_addr + old_size) - after_addr
            self.address_map[after_addr] = Block(after_addr, after_size, 'FREE', seg_id)

    def _free(self, addr):
        if addr not in self.address_map:
            raise ValueError("addr to be free must be allocated")
        curr_block = self.address_map[addr]
        if not curr_block.status == 'USED':
            raise ValueError("status of to be free block must be USED")

        # 状态置为 FREE
        curr_block.status = 'FREE'

        # 尝试与【后一个】块合并
        idx = self.address_map.bisect_left(addr)
        if idx + 1 < len(self.address_map):
            next_addr = self.address_map.iloc[idx + 1]
            next_block = self.address_map[next_addr]
            if next_block.status == 'FREE' and next_block.segment_id == curr_block.segment_id:
                curr_block.size += next_block.size
                del self.address_map[next_addr]

        # 尝试与【前一个】块合并
        # 注意：合并后当前块的 idx 可能会变，所以重新计算
        idx = self.address_map.bisect_left(addr)
        if idx > 0:
            prev_addr = self.address_map.iloc[idx - 1]
            prev_block = self.address_map[prev_addr]
            if prev_block.status == 'FREE' and prev_block.segment_id == curr_block.segment_id:
                prev_block.size += curr_block.size
                del self.address_map[addr]

    # --- 查询接口 ---
    def get_all_segments(self):
        return list(self.segments.items())

    def get_all_blocks(self):
        return list(self.address_map.values())

    def get_free_blocks(self):
        return [b for b in self.address_map.values() if b.status == 'FREE']

    def get_used_blocks(self):
        return [b for b in self.address_map.values() if b.status == 'USED']

    def get_free_space(self):
        sorted_addrs = sorted(self.segments.keys())
        free_sizes_in_span = []
        # 游标：记录当前已检查到的地址位置，初始为起始基地址
        current_pos = self.gva_base_addr
        for addr in sorted_addrs:
            # 如果当前活跃块的起点 > 当前游标，说明中间有一段空闲
            if addr > current_pos:
                free_size = addr - current_pos
                free_sizes_in_span.append(free_size)
            # 将游标移动到当前块的末尾
            current_pos = self.segments[addr]
        free_sizes_in_span.append(self.gva_base_addr + self.gva_size - current_pos)
        return free_sizes_in_span

    def simulate(self, device_trace_operations):
        with tqdm(total=len(device_trace_operations)) as pbar:
            pbar.set_description('Processing simulate:')
            for te in device_trace_operations:
                pbar.update(1)
                action = te['action']
                addr = te['addr']
                size = te['size']
                if action == "segment_alloc":
                    self._segment_alloc(addr, size)
                elif action == "segment_free":
                    self._segment_free(addr)
                elif action == 'alloc':
                    self._alloc(addr, size)
                elif action == 'free_completed':
                    self._free(addr)

    def __eq__(self, other):
        # check if 2 simulator status equal
        self_block_addrs = self.address_map.keys()
        other_block_addrs = other.address_map.keys()

        self_segments_addrs = self.segments.keys()
        other_segments_addrs = other.segments.keys()

        gva_addr_diff = other.gva_base_addr - self.gva_base_addr
        return (len(self_block_addrs) == len(other_block_addrs) and
                all((b - a) == gva_addr_diff for a, b in zip(self_block_addrs, other_block_addrs)) and
                len(self_segments_addrs) == len(other_segments_addrs) and
                all((b - a) == gva_addr_diff for a, b in zip(self_segments_addrs, other_segments_addrs)) and
                self.gva_size == other.gva_size)

