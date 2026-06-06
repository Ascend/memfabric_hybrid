#!/usr/bin/env python3
"""
Extract EID addresses with a given port count from net_layer==1 CLOS layers.

For each device in rank_list, find the first rank_addr_list entry in the
net_layer==1 CLOS level where addr_type=="EID" and len(ports) equals the
specified port count (default: 2).  Output "device_id: addr" lines
to a text file (default: device_eid.txt).
"""

import argparse
import json
import sys


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Extract EID addresses for net_layer==1 CLOS with a given port count."
    )
    parser.add_argument(
        "--input",
        "-i",
        default="hccl_rooinfo.json",
        help="Input JSON file (default: hccl_rooinfo.json)",
    )
    parser.add_argument(
        "--output",
        "-o",
        default="device_eid.txt",
        help="Output text file (default: device_eid.txt)",
    )
    parser.add_argument(
        "--port-count",
        type=int,
        default=6,
        help="Expected number of ports in the EID entry (default: 2)",
    )
    args = parser.parse_args()

    port_count = args.port_count

    with open(args.input, "r") as f:
        data = json.load(f)

    rank_list = data.get("rank_list", [])
    results: list[str] = []
    found_any = False

    for rank in rank_list:
        device_id = rank.get("device_id")
        matched_addr = None

        for level in rank.get("level_list", []):
            if level.get("net_layer") != 1:
                continue
            net_type = level.get("net_type", "")
            if net_type != "CLOS":
                continue

            for addr_entry in level.get("rank_addr_list", []):
                if addr_entry.get("addr_type") != "EID":
                    continue
                ports = addr_entry.get("ports")
                if isinstance(ports, list) and len(ports) == port_count:
                    matched_addr = addr_entry.get("addr")
                    break

        if matched_addr is not None:
            results.append(f"{device_id}: {matched_addr}")
            found_any = True
        else:
            print(
                f"warning: device {device_id} has no EID with len(ports)=={port_count} "
                f"in net_layer==1 CLOS",
                file=sys.stderr,
            )

    with open(args.output, "w") as f:
        for line in results:
            print(line, file=f)

    if not found_any:
        print("error: no matching entries found in any device", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
