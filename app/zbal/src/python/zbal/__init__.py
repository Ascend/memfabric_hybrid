import os
import sys
import logging
from importlib.metadata import version

import torch
import torch_npu

_TORCH_NPU_VERSION_ = 0
_RT_MAJOR_VER_MIN_ = 9
logger = logging.getLogger(__name__)
logging.basicConfig(level=logging.INFO)

try:
    # check pta version
    cur_torch_npu_version = version('torch_npu')
    cur_major, cur_minor, *_ = cur_torch_npu_version.split(".")
    exp_major, exp_minor, *_ = str(_TORCH_NPU_VERSION_).split(".")
    if cur_major != exp_major or cur_minor != exp_minor:
        raise ValueError(f"[ERROR] Expect torch_npu version {_TORCH_NPU_VERSION_} but found {cur_torch_npu_version}")

    # check cann version
    ascend_home_path = os.environ.get("ASCEND_HOME_PATH", "un-defined env ASCEND_HOME_PATH")
    runtime_version_file = f"{ascend_home_path}/share/info/runtime/version.info"
    version_line = next((line.strip() for line in open(runtime_version_file) if 'Version=' in line), None)
    _, cur_rt_version = version_line.split("=")
    cur_rt_major_version, *_ = cur_rt_version.split(".")
    if int(cur_rt_major_version) < _RT_MAJOR_VER_MIN_:
        logger.warning(f"[WARNING] The CANN version = {cur_rt_version} is too low, expect version is 9.0.0 or higher.")
except Exception as e:
    logger.exception(f"[ERROR] zbal init module failed: {e}")
    raise e

from zbal.zbal.allocator import record_memory_history, dump_snapshot, simulate_init, is_mix_alloc
from zbal.zbal import ProcessGroupZBAL
from zbal.zbal.deepep_adaptor import Config
from zbal.zbal import ZBALBootstrapType, zbal_set_logger_level

from .zbal_module import zbal_init, zbal_uninit
from .zbal_module import switch_to_allocator, zbal_get_symm_base_addr
from .zbal_module import mem_get_info
from .zbal_module import __version__

from .zbal_buffer import Buffer
from .zbal_utils import EventOverlap

__all__ = [
    "switch_to_allocator",
    "zbal_get_symm_base_addr",
    "record_memory_history",
    "dump_snapshot",
    "get_heap_stats",
    "zbal_init",
    "zbal_uninit",
    "ZBALBootstrapType",
    "zbal_set_logger_level",
    "simulate_init",
    "is_mix_alloc",
    "__version__"
]


def _new_process_helper(dist_backend_opts, pg_options):
    store = dist_backend_opts.store
    group_rank = dist_backend_opts.group_rank
    group_size = dist_backend_opts.group_size

    if pg_options is None or not isinstance(pg_options, ProcessGroupZBAL.Options):
        pg_options = ProcessGroupZBAL.Options()

    pg_options.is_high_priority_stream = False
    pg_options.op_timeout = dist_backend_opts.timeout
    pg_options.global_ranks_in_group = dist_backend_opts.global_ranks_in_group
    pg_options.group_id = dist_backend_opts.group_id
    return ProcessGroupZBAL(store, group_rank, group_size, pg_options)


torch.distributed.Backend.register_backend("zbal", lambda dist_backend_opts, pg_options:
    _new_process_helper(dist_backend_opts, pg_options), extended_api=True, devices=["npu"])