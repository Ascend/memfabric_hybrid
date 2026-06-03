#!/usr/bin/env python3
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
from pathlib import Path
import subprocess
import copy
import logging
import xml.etree.ElementTree as ET
import pytest


def merge_junit_reports(ut_report_path, st_report_path, final_output_path):
    # 1. 创建标准根结构
    testsuites_elem = ET.Element("testsuites")
    testsuite_elem = ET.SubElement(testsuites_elem, "testsuite", name="all")

    stats = {"tests": 0, "failures": 0, "errors": 0, "skipped": 0, "time": 0.0}

    # 2. 合并两个报告中的用例和统计
    for report_file in (ut_report_path, st_report_path):
        if not report_file.exists():
            continue

        tree = ET.parse(report_file)
        root = tree.getroot()

        # 寻找源报告中的 testsuite 元素（可能嵌套在 testsuites 下）
        source_suite = None
        if root.tag == "testsuites":
            # 如果根是 testsuites，取第一个（或遍历所有）子 testsuite
            source_suite = root.find("testsuite")
        elif root.tag == "testsuite":
            source_suite = root

        if source_suite is None:
            # 兜底：尝试查找任意 testsuite 标签
            source_suite = root.find(".//testsuite")
            if source_suite is None:
                source_suite = root  # 最后降级

        # 累加统计
        for key in ["tests", "failures", "errors", "skipped"]:
            stats[key] += int(source_suite.get(key, 0))
        stats["time"] += float(source_suite.get("time", 0))

        # 合并所有 testcase
        for testcase in source_suite.findall("testcase"):
            # 深拷贝，避免节点归属冲突
            testsuite_elem.append(copy.deepcopy(testcase))

    # 3. 设置 testsuite 属性 (testsuites 标签通常不设这些属性)
    testsuite_elem.set("name", "all")
    testsuite_elem.set("tests", str(stats["tests"]))
    testsuite_elem.set("failures", str(stats["failures"]))
    testsuite_elem.set("errors", str(stats["errors"]))
    testsuite_elem.set("skipped", str(stats["skipped"]))
    testsuite_elem.set("time", f"{stats['time']:.3f}")

    # 4. 计算并添加通过率作为自定义属性 (可被CI工具读取)
    passed = stats["tests"] - stats["failures"] - stats["errors"]
    pass_rate = (passed / stats["tests"] * 100) if stats["tests"] > 0 else 0
    properties = ET.SubElement(testsuite_elem, "properties")
    ET.SubElement(properties, "property", name="pass.rate", value=f"{pass_rate:.2f}%")
    ET.SubElement(properties, "property", name="passed.tests", value=str(passed))

    # 5. 生成最终文件
    tree = ET.ElementTree(testsuites_elem)
    tree.write(final_output_path, encoding="utf-8", xml_declaration=True)

    # 6. 打印摘要
    logging.info(f"报告合并完成，标准格式已生成。")
    logging.info(f"总计: {stats['tests']} 用例, 通过: {passed}, 失败: {stats['failures']}, 错误: {stats['errors']}")
    logging.info(f"通过率: {pass_rate:.1f}%")


def main():
    run_st = False  # 提交MR时请置为False

    result_dir = Path("tests/test_result")
    # 确保目录存在
    result_dir.mkdir(parents=True, exist_ok=True)
    # 清理文件
    # 2. 清理旧文件（现在清理的是 result_dir 目录下的）
    [p.unlink() for p in result_dir.glob("*") if p.is_file()]  # 清理目录下所有文件

    # 1. 运行UT测试
    logging.info("运行UT测试...")
    pytest.main([
        "tests/ut/",
        "-q",
        "--cov=tensor_rtl",
        "--cov-branch",
        f"--cov-report=xml:{result_dir / 'coverage.xml'}",
        f"--junitxml={result_dir / '.report_ut.xml'}"
    ])

    # 2. 运行ST测试
    if run_st:
        logging.info("\n运行ST测试...")
        result = subprocess.run([
            "/usr/bin/python", "-m", "torch.distributed.run",
            "--nproc_per_node=8",  # GPU数量
            "--standalone",
            "-m", "pytest",
            "tests/st/",
            "-q",
            "--no-cov",
            f"--junitxml={result_dir / '.report_st.xml'}"
        ], capture_output=True, text=True)

        logging.info("Return Code:", result.returncode)
        logging.info("Stdout:", result.stdout)
        logging.info("Stderr:", result.stderr)

    # 3. 合并报告（只统计数字）
    logging.info("\n合并测试报告...")
    merge_junit_reports(
        ut_report_path=result_dir / ".report_ut.xml",
        st_report_path=result_dir / ".report_st.xml",
        final_output_path=result_dir / "final.xml"  # 最终报告路径
    )

    # 清理临时文件
    (result_dir / ".report_ut.xml").unlink(missing_ok=True)
    (result_dir / ".report_st.xml").unlink(missing_ok=True)


if __name__ == "__main__":
    main()
