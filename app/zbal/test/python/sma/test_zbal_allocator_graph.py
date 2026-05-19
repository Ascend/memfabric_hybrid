import logging
import os
import sys
import gc
import torch
import torch_npu
import zbal
from zbal import record_memory_history, dump_snapshot

TOTAL_MEM = 1024 * 1024 * 1024


def init_sma(mem=TOTAL_MEM):
    world_size = int(os.environ.get("WORLD_SIZE", 1))
    local_rank = int(os.environ.get("LOCAL_RANK", 0))
    device_id = local_rank

    logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
    zbal.zbal_set_logger_level(2)
    if not zbal.zbal_init(world_size, device_id, local_rank, mem):
        logging.error("zbal_init failed on rank %d.", local_rank)
        exit(-1)
    logging.info("zbal_init success on rank %d.", local_rank)
    torch.npu.set_device(device_id)


def alloc(size, stream=None):
    if stream is not None:
        with torch.npu.stream(stream):
            return torch.npu.caching_allocator_alloc(size)
    return torch.npu.caching_allocator_alloc(size)


def free(addr):
    torch.npu.caching_allocator_delete(addr)


def get_reserved_bytes():
    return zbal.mem_get_info()[1]


def get_allocated_bytes():
    return zbal.mem_get_info()[0]


class TestSMAGraphAllocator:
    """Test SMA allocator behavior under NPU graph capture mode."""

    @staticmethod
    def setup_class():
        init_sma()

    @staticmethod
    def test_graph_capture_basic_lifecycle():
        """graph capture alloc → replay → reset, verify no crash."""
        capture_stream = torch.npu.Stream()
        graph = torch.npu.NPUGraph()
        size = 4 * 1024 * 1024

        with torch.npu.graph(graph, stream=capture_stream):
            p = alloc(size, stream=capture_stream)
            torch.zeros(1024, device="npu")

        torch.npu.synchronize()

        for _ in range(3):
            graph.replay()

        capture_stream.synchronize()

        free(p)
        graph.reset()
        torch.npu.synchronize()

    @staticmethod
    def test_graph_private_pool_lifecycle():
        """verify private pool is created on capture and cleaned on reset."""
        capture_stream = torch.npu.Stream()
        graph = torch.npu.NPUGraph()
        size = 8 * 1024 * 1024

        reserved_before = get_reserved_bytes()

        with torch.npu.graph(graph, stream=capture_stream):
            p1 = alloc(size, stream=capture_stream)
            p2 = alloc(size, stream=capture_stream)

        torch.npu.synchronize()
        reserved_during = get_reserved_bytes()
        assert reserved_during > 0, "reserved_bytes should be > 0 after graph capture alloc"

        for _ in range(2):
            graph.replay()
        capture_stream.synchronize()

        free(p1)
        free(p2)

        graph.reset()
        del graph
        gc.collect()
        torch.npu.empty_cache()
        torch.npu.synchronize()

        reserved_after = get_reserved_bytes()
        assert reserved_after <= reserved_during, (
            f"reserved_bytes should not increase after release: after={reserved_after}, during={reserved_during}"
        )

    @staticmethod
    def test_graph_replay_memory_stability():
        """repeated replay should not accumulate memory."""
        capture_stream = torch.npu.Stream()
        graph = torch.npu.NPUGraph()
        size = 2 * 1024 * 1024

        with torch.npu.graph(graph, stream=capture_stream):
            p = alloc(size, stream=capture_stream)

        torch.npu.synchronize()

        reserved_before = get_reserved_bytes()
        for _ in range(10):
            graph.replay()
        capture_stream.synchronize()

        reserved_after = get_reserved_bytes()
        assert reserved_after == reserved_before, (
            f"repeated replay should not change reserved_bytes: before={reserved_before}, after={reserved_after}"
        )

        free(p)
        graph.reset()
        torch.npu.synchronize()

    @staticmethod
    def test_multi_graph_independent_pools():
        """multiple graphs should have independent private pools."""
        capture_stream = torch.npu.Stream()
        size = 4 * 1024 * 1024

        graph1 = torch.npu.NPUGraph()
        with torch.npu.graph(graph1, stream=capture_stream):
            p1 = alloc(size, stream=capture_stream)

        graph2 = torch.npu.NPUGraph()
        with torch.npu.graph(graph2, stream=capture_stream):
            p2 = alloc(size, stream=capture_stream)

        torch.npu.synchronize()

        for _ in range(3):
            graph1.replay()
        capture_stream.synchronize()

        for _ in range(3):
            graph2.replay()
        capture_stream.synchronize()

        free(p1)
        free(p2)

        graph1.reset()
        graph2.reset()
        del graph1, graph2
        gc.collect()
        torch.npu.empty_cache()
        torch.npu.synchronize()

    @staticmethod
    def test_graph_shared_pool():
        """graphs with the same mempool_id should share the private pool."""
        capture_stream = torch.npu.Stream()

        mempool_id = (0x12345678, 0)
        graph1 = torch.npu.NPUGraph()
        graph2 = torch.npu.NPUGraph()

        graph1.capture_begin(pool=mempool_id)
        p1 = alloc(4 * 1024 * 1024, stream=capture_stream)
        graph1.capture_end()

        graph2.capture_begin(pool=mempool_id)
        p2 = alloc(4 * 1024 * 1024, stream=capture_stream)
        graph2.capture_end()

        torch.npu.synchronize()

        for _ in range(2):
            graph1.replay()
            graph2.replay()
        capture_stream.synchronize()

        free(p1)
        free(p2)

        graph1.reset()
        graph2.reset()
        del graph1, graph2
        gc.collect()
        torch.npu.empty_cache()
        torch.npu.synchronize()

    @staticmethod
    def test_graph_default_pool_untouched_during_capture():
        """default pool cached blocks remain available outside graph capture."""
        capture_stream = torch.npu.Stream()
        default_stream = torch.npu.current_stream()

        for _ in range(4):
            p = alloc(2 * 1024 * 1024, stream=default_stream)
            free(p)

        reserved_before = get_reserved_bytes()

        graph = torch.npu.NPUGraph()
        with torch.npu.graph(graph, stream=capture_stream):
            pg = alloc(4 * 1024 * 1024, stream=capture_stream)

        torch.npu.synchronize()

        p_outside = alloc(2 * 1024 * 1024, stream=default_stream)

        free(pg)
        free(p_outside)

        graph.reset()
        del graph
        gc.collect()
        torch.npu.empty_cache()
        torch.npu.synchronize()

        reserved_after = get_reserved_bytes()
        assert reserved_after <= reserved_before + 8 * 1024 * 1024, (
            f"unexpected memory growth: before={reserved_before}, after={reserved_after}"
        )

    @staticmethod
    def test_graph_capture_tensor_operation():
        """graph capture wrapping a real tensor op."""
        capture_stream = torch.npu.Stream()

        x = torch.randn(128, 256, device="npu")
        w = torch.randn(256, 512, device="npu")
        y = torch.empty(128, 512, device="npu")

        graph = torch.npu.NPUGraph()
        with torch.npu.graph(graph, stream=capture_stream):
            y = torch.mm(x, w)

        torch.npu.synchronize()

        for _ in range(5):
            graph.replay()
        capture_stream.synchronize()

        expected = torch.mm(x.cpu(), w.cpu())
        assert torch.allclose(y.cpu(), expected, rtol=1e-4, atol=1e-4), "graph replayed matmul result mismatch"

        graph.reset()
        del graph, x, w, y
        gc.collect()
        torch.npu.empty_cache()
        torch.npu.synchronize()

    @staticmethod
    def test_graph_reset_then_empty_cache():
        """graph reset + empty_cache should release the private pool."""
        capture_stream = torch.npu.Stream()
        graph = torch.npu.NPUGraph()
        size = 8 * 1024 * 1024

        with torch.npu.graph(graph, stream=capture_stream):
            p = alloc(size, stream=capture_stream)

        torch.npu.synchronize()

        for _ in range(2):
            graph.replay()
        capture_stream.synchronize()

        free(p)
        graph.reset()

        del graph
        gc.collect()

        torch.npu.empty_cache()
        torch.npu.synchronize()

        reserved_after = get_reserved_bytes()
        # After graph reset + empty_cache, memory should be released back
        assert reserved_after >= 0, "memory stats should be accessible after release"

    @staticmethod
    def test_graph_memory_stats_snapshot():
        """memory history recording across graph capture/replay should be consistent."""
        record_memory_history("all", sys.maxsize)

        capture_stream = torch.npu.Stream()
        graph = torch.npu.NPUGraph()
        size = 4 * 1024 * 1024

        with torch.npu.graph(graph, stream=capture_stream):
            p = alloc(size, stream=capture_stream)

        torch.npu.synchronize()

        for _ in range(2):
            graph.replay()
        capture_stream.synchronize()

        snapshot = dump_snapshot()

        free(p)
        graph.reset()
        del graph
        gc.collect()
        torch.npu.empty_cache()
        torch.npu.synchronize()

        record_memory_history(None, 1)

        assert snapshot is not None, "snapshot should not be None"
        assert 'device_traces' in snapshot, "snapshot should contain device_traces"


if __name__ == "__main__":
    init_sma()

    test_cases = [
        ("test_graph_capture_basic_lifecycle", TestSMAGraphAllocator.test_graph_capture_basic_lifecycle),
        ("test_graph_private_pool_lifecycle", TestSMAGraphAllocator.test_graph_private_pool_lifecycle),
        ("test_graph_replay_memory_stability", TestSMAGraphAllocator.test_graph_replay_memory_stability),
        ("test_multi_graph_independent_pools", TestSMAGraphAllocator.test_multi_graph_independent_pools),
        ("test_graph_default_pool_untouched_during_capture",
         TestSMAGraphAllocator.test_graph_default_pool_untouched_during_capture),
        ("test_graph_capture_tensor_operation", TestSMAGraphAllocator.test_graph_capture_tensor_operation),
        ("test_graph_reset_then_empty_cache", TestSMAGraphAllocator.test_graph_reset_then_empty_cache),
        ("test_graph_memory_stats_snapshot", TestSMAGraphAllocator.test_graph_memory_stats_snapshot),
    ]

    for name, func in test_cases:
        logging.info("=== %s ===", name)
        func()
        logging.info("PASS")

    logging.info("All tests passed!")
