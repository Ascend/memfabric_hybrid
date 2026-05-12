import ctypes
from pathlib import Path
import os
import torch
import torch_npu
import torch.distributed as dist
from torch.distributed import ReduceOp
from zbal.zbal import ZBALBootstrapType, ZBALBootstrapOption
from zbal.zbal import zbal_version, zbal_bootstrap, zbal_unbootstrap, zbal_comm_destroy_all
from zbal.zbal.allocator import get_heap_stats

CURRENT_DIR = Path(__file__).resolve().parent
ZBAL_LIB = list(CURRENT_DIR.glob("zbal.*.so"))[0]
ZBAL_ENABLE_GRAPH = int(os.environ.get("ZBAL_ENABLE_GRAPH", 0)) == 1

__version__ = zbal_version()


def zbal_init(world_size: int,
              device_id: int,
              rank_id: int,
              device_mem_size: int,
              bootstrap_type: ZBALBootstrapType = ZBALBootstrapType.BOOT_BY_MEMFABRIC,
              start_config_server: bool = False,
              data_op_type: int = 0,
              comm_meta_space_size: int = 1024,
              comm_group_cap: int = 64,
              flags: int = 0,
              ip_port: str = ""):
    '''
    Initialize zbal library

    :param world_size: size of ranks to init zbal
    :param device_id: current device id
    :param rank_id: current rank id
    :param device_mem_size: used device mem per device
    :param bootstrap_type: under memory bootstrap type, memfabric support only
    :param start_config_server: whether to start config server
    :param data_op_type: data operator type
    :param comm_meta_space_size: collective communication meta space size, unit is KB
    :param comm_group_cap: number of collective communication
    :param flag: reserve flag
    :param ip_port: bootstrap used ip port
    :return: 0 if success else error code
    '''

    # get env of MemFabric home
    mem_fabric_lib_path = os.environ.get("MEMFABRIC_HYBRID_LIBRARY_PATH")
    if mem_fabric_lib_path is None:
        # try to import memfabric from python package
        import memfabric_hybrid as mf
        mem_fabric_lib_path = mf.get_lib_path()
        if mem_fabric_lib_path is not None:
            os.environ["MEMFABRIC_HYBRID_LIBRARY_PATH"] = mem_fabric_lib_path
            print(f"Set MEMFABRIC_HYBRID_LIBRARY_PATH to {mem_fabric_lib_path} on rank {rank_id}")

    # init mem allocator, switch before set_device
    if torch.npu.memory.get_allocator_backend() == 'native':
        switch_to_allocator()
    torch.npu.set_device(device_id)
    # set ip_port by torch_run parameters master_addr and master_port
    if ip_port == "":
        bootstrap_svr_ip = "127.0.0.1"
        bootstrap_svr_port = "6789"
        if os.environ.get("MASTER_ADDR") is not None:
            bootstrap_svr_ip = os.environ.get("MASTER_ADDR")
        master_port_str = os.environ.get("MASTER_PORT")
        if master_port_str is not None:
            try:
                master_port = int(master_port_str)
                bootstrap_svr_port = str(master_port - 20)
            except ValueError:
                print(f"Get bootstrap_svr_port {master_port_str} failed, set default value 6789")
                bootstrap_svr_port = "6789"
        ip_port = "tcp://" + bootstrap_svr_ip + ":" + bootstrap_svr_port
        print(f"Set bootstrap ip:port to {ip_port} based on the setting of MASTER_ADDR and MASTER_PORT")

    # bootstrap
    opt = ZBALBootstrapOption()
    opt.flags = flags
    opt.btType = bootstrap_type
    opt.ipPort = ip_port
    opt.worldSize = world_size
    opt.rankId = rank_id
    opt.deviceId = device_id
    opt.startConfigServer = start_config_server
    opt.deviceMemorySize = device_mem_size
    opt.dataOperationType = data_op_type
    opt.commMetaSpaceSize = comm_meta_space_size
    opt.commGroupCap = comm_group_cap
    ret = zbal_bootstrap(opt)
    # init world group meta (world group do not support lazy init)
    if dist.group.WORLD is not None:
        dist.barrier(group=dist.group.WORLD)

    return ret == 0


def zbal_uninit(flags: int = 0):
    '''
    Un-initialize zbal library
    :return:
    '''

    # un-init comm
    zbal_comm_destroy_all(flags)

    # un-init allocator

    # un-init bootstrap
    zbal_unbootstrap(flags)

    return True


def switch_to_allocator():
    new_alloc = torch_npu.npu.memory.NPUPluggableAllocator(ZBAL_LIB,
                                                           "zbal_pluggable_malloc", "zbal_pluggable_free")
    # Swap the current allocator
    torch_npu.npu.memory.change_current_allocator(new_alloc)
    zbal_allocator = ctypes.CDLL(ZBAL_LIB)

    init_fn = ctypes.cast(getattr(zbal_allocator, "zbal_pluggable_init"), ctypes.c_void_p).value
    empty_fn = ctypes.cast(getattr(zbal_allocator, "zbal_pluggable_empty_cache"), ctypes.c_void_p).value
    record_stream_fn = ctypes.cast(getattr(zbal_allocator, "zbal_pluggable_record_stream"), ctypes.c_void_p).value
    erase_stream_fn = ctypes.cast(getattr(zbal_allocator, "zbal_pluggable_erase_stream"), ctypes.c_void_p).value
    get_device_stats_fn = ctypes.cast(getattr(zbal_allocator, "zbal_pluggable_get_device_stats"), ctypes.c_void_p).value

    new_alloc.allocator().set_init_fn(init_fn)
    new_alloc.allocator().set_reset_fn(empty_fn)
    new_alloc.allocator().set_record_stream_fn(record_stream_fn)
    new_alloc.allocator().set_erase_stream_fn(erase_stream_fn)
    new_alloc.allocator().set_get_device_stats_fn(get_device_stats_fn)

    if ZBAL_ENABLE_GRAPH:
        begin_allocate_to_pool_fn = ctypes.cast(getattr(zbal_allocator, "zbal_pluggable_begin_allocate_to_pool"),
                                                ctypes.c_void_p).value
        end_allocate_to_pool_fn = ctypes.cast(getattr(zbal_allocator, "zbal_pluggable_end_allocate_to_pool"),
                                              ctypes.c_void_p).value
        release_pool_fn = ctypes.cast(getattr(zbal_allocator, "zbal_pluggable_release_pool"), ctypes.c_void_p).value

        new_alloc.allocator().set_begin_allocate_to_pool_fn(begin_allocate_to_pool_fn)
        new_alloc.allocator().set_end_allocate_to_pool_fn(end_allocate_to_pool_fn)
        new_alloc.allocator().set_release_pool_fn(release_pool_fn)


def zbal_get_symm_base_addr():
    print("zbal_get_symm_base_addr is deprecated, using zbal_init instead")
    zbal_allocator = ctypes.CDLL(ZBAL_LIB)
    zbal_allocator.zbal_get_symm_base_addr.restype = ctypes.c_void_p
    return zbal_allocator.zbal_get_symm_base_addr()


def mem_get_info():
    # this api is used to fulfill torch.npu.mem_get_info functions when sma take control of memory
    return get_heap_stats()[1] - get_heap_stats()[0], get_heap_stats()[1]
