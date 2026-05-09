import multiprocessing
import pickle
import sys
import torch
import torch_npu
from torch import nn

from zbal import switch_to_allocator, init_shmem, record_memory_history, dump_snapshot


def train(num_iter=500, device="npu"):
    """a tiny transformer training process"""
    model = nn.Transformer(
        d_model=512, nhead=2, num_encoder_layers=2, num_decoder_layers=2
    ).to(device=device)
    x = torch.randn(size=(1, 1024, 512), device=device)
    tgt = torch.rand(size=(1, 1024, 512), device=device)
    model.train()
    labels = torch.rand_like(model(x, tgt))
    criterion = torch.nn.CrossEntropyLoss()
    optimizer = torch.optim.Adam(model.parameters())
    for _i in range(num_iter):
        y = model(x, tgt)
        loss = criterion(y, labels)
        loss.backward()
        if _i % 20 == 0:
            print(f"[step{_i}] loss: {loss.item()}")
        optimizer.step()
        optimizer.zero_grad(set_to_none=True)


def infer(device="npu"):
    from torchvision.io import decode_image
    from torchvision.models import resnet50, ResNet50_Weights

    # img = decode_image("test/assets/encode_jpeg/grace_hopper_517x606.jpg")

    # Step 1: Initialize model with the best available weights
    weights = ResNet50_Weights.DEFAULT
    model = resnet50(weights=weights)
    model.eval().to(device=device)

    # Step 2: Initialize the inference transforms
    preprocess = weights.transforms()

    # Step 3: Apply inference preprocessing transforms
    # batch = preprocess(img).unsqueeze(0)

    batch = torch.zeros((1, 3, 224, 224), device=device)
    # Step 4: Use the model and print the predicted category
    prediction = model(batch).squeeze(0).softmax(0)
    # print(prediction)
    class_id = prediction.argmax().item()
    score = prediction[class_id].item()
    category_name = weights.meta["categories"][class_id]
    print(f"{category_name}: {100 * score:.1f}%")


def subprocess_capture_snapshot(device_id, local_mem_size, meta_size, conn):
    # This will allocate memory in the device using the new allocator
    switch_to_allocator()
    torch.npu.set_device(0)

    init_shmem(device_id, 1, local_mem_size, meta_size, 'tcp://127.0.0.1:37221')
    record_memory_history("all", sys.maxsize)

    npu_tensor = torch.zeros(10, device="npu")
    # full demo test
    train()
    infer()

    snapshot = dump_snapshot()
    record_memory_history(None, 0)
    conn.send(snapshot)


if __name__ == '__main__':
    device_id = 0
    local_mem_size = 10 * (1024 ** 3)
    meta_size = 1024 ** 3
    parent_conn, child_conn = multiprocessing.Pipe()

    p = multiprocessing.Process(target=subprocess_capture_snapshot,
                                args=(device_id, local_mem_size, meta_size, child_conn))
    p.start()
    captured_snapshot = parent_conn.recv()
    p.join()

    ori_snapshot = captured_snapshot
    from snapshot_utils import replay_snapshot
    replayed_snapshot = replay_snapshot(ori_snapshot, device_id, local_mem_size - meta_size)

    ori_device_traces = ori_snapshot['device_traces'][device_id]
    replayed_device_traces = replayed_snapshot['device_traces'][0]

    from snapshot_utils import MemoryAllocatorSimulator
    ori_simulator = MemoryAllocatorSimulator(gva_size=local_mem_size - meta_size)
    replayed_simulator = MemoryAllocatorSimulator(gva_size=local_mem_size - meta_size)

    ori_simulator.simulate(ori_device_traces)
    replayed_simulator.simulate(replayed_device_traces)

    if ori_simulator != replayed_simulator:
        raise ValueError("replayed snapshot mismatch with original")
    print("replayed snapshot is equal to ori snapshot")

    free_blocks = ori_simulator.get_free_blocks()
    free_block_sizes = [block.size for block in free_blocks]
    print(f'free blocks total size: {sum(free_block_sizes) / (1024 ** 2)} MB')

    free_spaces = ori_simulator.get_free_space()
    print(f'free space total size: {sum(free_spaces) / (1024 ** 2)} MB')
