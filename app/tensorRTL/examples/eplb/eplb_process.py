
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.

import logging
from multiprocessing import Process

from sglang.srt.eplb.expert_location import ExpertLocationMetadata


logger = logging.getLogger(__name__)


class EplbProcess:
    def __init__(
        self,
        shared_dict,
        planner_q,
        block_q,
        server_args,
        model_config,
        rank,
    ):
        self.shared_dict = shared_dict
        self.planner_q = planner_q
        self.block_q = block_q

        self._server_args = server_args
        self._model_config = model_config
        self.rank = rank
        self._server_args.device = "cpu"

    def launch_process(self):
        proc = Process(
            target=self._worker_process, args=(self.planner_q, self.block_q), daemon=True
        )
        proc.start()
        return proc

    def _do_algorithm(self):
        logical_count = self.shared_dict["moe_load"]
        return ExpertLocationMetadata.init_by_eplb(
            self._server_args, self._model_config, logical_count, self.rank
        )

    def _worker_process(self, planner_q, block_q):
        while True:
            try:
                planner_q.get()
                update_info = self._do_algorithm()

                _wait_and_put(block_q, update_info)

            except Exception as e:
                logger.warning(
                    f"[EPLB process] Exiting due to error:{e}", exc_info=True
                )
                break


def _wait_and_put(block_q, update_info):
    while True:
        if not block_q.empty():
            continue
        block_q.put(update_info)
        break
