from typing import Optional, Tuple

import torch
import torch_npu
from zbal.zbal.deepep_adaptor import Config, EventHandle


class EventOverlap:

    def __init__(
        self,
        event: Optional[EventHandle] = None,
        extra_tensors: Optional[Tuple[torch.Tensor]] = None,
    ) -> None:
        """
        Initialize the class.

        Arguments:
            event: the NPU event captured.
            extra_tensors: an easier way to simulate PyTorch tensor `record_stream`, may be useful with NPU graph.
        """
        self.event = event

        # NOTES: we use extra tensors to achieve stream recording, otherwise,
        # stream recording will be incompatible with NPU graph.
        self.extra_tensors = extra_tensors

    def current_stream_wait(self) -> None:
        """
        The current stream waits for the event to be finished.
        """
        pass
