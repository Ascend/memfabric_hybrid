#!/usr/bin/env python
# coding=utf-8
# Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
# ZBAL is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

import subprocess
import re
import sys
import os
import signal
import time

# ===================== Configuration =====================
TRAIN_SCRIPT = "/home/CI_HOME_for_25.2.0/zbal/LLaMA-Factory/test_llama_factory_qwen3_8b.sh"
WORK_DIR = "/home/CI_HOME_for_25.2.0/zbal/LLaMA-Factory"
STOP_EPOCH = 6.0
LOSS_TOLERANCE = 0.005    # Allowable loss error
GRADable loss error
GRAD_TOLERANCE = 0.1      # Allowable grad_norm error

TRAIN_START_TIMEOUT = 120
# ==================================================

# Standard sample data
SAMPLE_DATA = [
    {'loss': 2.106, 'grad_norm': 0.5870679616928101, 'epoch': 0.89},
    {'loss': 2.2599, 'grad_norm': 0.6546427011489868, 'epoch': 1.0},
    {'loss': 2.1217, 'grad_norm': 0.5926712155342102, 'epoch': 1.89},
    {'loss': 2.129, 'grad_norm': 0.6073716878890991, 'epoch': 2.0},
    {'loss': 2.1073, 'grad_norm': 0.5887804627418518, 'epoch': 2.89},
    {'loss': 2.2369, 'grad_norm': 0.6405970454216003, 'epoch': 3.0},
    {'loss': 2.1324, 'grad_norm': 0.596942126750946, 'epoch': 3.89},
    {'loss': 2.0394, 'grad_norm': 0.5745413899421692, 'epoch': 4.0},
    {'loss': 2.1369, 'grad_norm': 0.6002771854400635, 'epoch': 4.89},
    {'loss': 1.9938, 'grad_norm': 0.5919603109359741, 'epoch': 5.0},
    {'loss': 2.1101, 'grad_norm': 0.6078227162361145, 'epoch': 5.89},
    {'loss': 2.1963, 'grad_norm': 0.6255742907524109, 'epoch': 6.0},
    {'loss': 2.1081, 'grad_norm': 0.6261933445930481, 'epoch': 6.89},
    {'loss': 2.2111, 'grad_norm': 0.7093288898468018, 'epoch': 7.0},
    {'loss': 2.0971, 'grad_norm': 0.6461774110794067, 'epoch': 7.89},
    {'loss': 2.244, 'grad_norm': 0.7063803672790527, 'epoch': 8.0},
]

# Regex pattern to match loss output lines
pattern = re.compile(r"{'loss':\s*([\d\.-]+),\s*'grad_norm':\s*([\d\.-]+),.*'epoch':\s*([\d\.-]+)")


def find_sample(epoch):
    for sample in SAMPLE_DATA:
        if abs(sample["epoch"] - epoch) < 1e-6:
            return sample
    return None


def main():
    os.chdir(WORK_DIR)
    print(f"[INFO] Working directory: {os.getcwd()}")

    # Launch training script
    print(f"[INFO] Starting training script: {TRAIN_SCRIPT}")
    proc = subprocess.Popen(
        ["bash", "-x", TRAIN_SCRIPT],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
        universal_newlines=True
    )

    start_time = time.time()
    has_seen_loss = False

    for line in iter(proc.stdout.readline, ''):
        line = line.strip()
        print(line)

        # Detect loss output line
        match = pattern.search(line)
        if match:
            has_seen_loss = True  # Launch success once loss is output

            # Parse metrics
            loss = float(match.group(1))
            grad_norm = float(match.group(2))
            epoch = float(match.group(3))
            print(f"\n[INFO] Parsed → epoch={epoch:.2f}, loss={loss:.4f}, grad_norm={grad_norm:.4f}")

            # 1. Auto stop when exceeding target epoch
            if epoch > STOP_EPOCH:
                print(f"[INFO] epoch={epoch:.2f} > {STOP_EPOCH}, terminating training")
                proc.terminate()
                proc.wait()
                print("[INFO] Training stopped normally")
                break

            # 2. Validate against standard sample data
            sample = find_sample(epoch)
            if sample:
                loss_diff = abs(loss - sample["loss"])
                grad_diff = abs(grad_norm - sample["grad_norm"])
                print(f"[INFO] Reference sample → loss={sample['loss']:.4f}, grad_norm={sample['grad_norm']:.4f}")
                print(f"[INFO] Error margin → loss={loss_diff:.4f}, grad_norm={grad_diff:.4f}")

                if loss_diff > LOSS_TOLERANCE or grad_diff > GRAD_TOLERANCE:
                    proc.terminate()
                    proc.wait()
                    raise AssertionError(
                        f"Validation failed!\n"
                        f"Loss error margin = {loss_diff:.4f} (allowed ≤ {LOSS_TOLERANCE})\n"
                        f"Grad_norm error margin = {grad_diff:.4f} (allowed ≤ {GRAD_TOLERANCE})"
                    )

        # ===================== Core logic: No loss output within timeout → launch failure =====================
        if not has_seen_loss and (time.time() - start_time) > TRAIN_START_TIMEOUT:
            proc.terminate()
            proc.wait()
            raise RuntimeError(
                f"Training script launch failed!\n"
                f"No loss output within {TRAIN_START_TIMEOUT}s, abnormal training startup!"
            )

    proc.wait()

    if not has_seen_loss:
        raise RuntimeError("No loss output from training script, launch failed!")

    print("\n[SUCCESS] Training completed with all validations passed!")


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print(f"\n[FATAL] Task exited abnormally: {e}")
        sys.exit(1)