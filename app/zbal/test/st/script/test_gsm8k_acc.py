from types import SimpleNamespace
import requests
import time

from sglang.test.few_shot_gsm8k import run_eval

SGLANG_IP = "127.0.0.1"
SGLANG_PORT = "7239"
SGLANG_URL = f"http://{SGLANG_IP}:{SGLANG_PORT}"


def test_gsm8k(ip=SGLANG_IP, port=SGLANG_PORT, thres=0.86):
    args = SimpleNamespace(
        num_shots=5,
        data_path="test.jsonl",
        num_questions=1316,
        max_new_tokens=512,
        parallel=256,
        host=f"http://{ip}",
        port=int(port),
    )

    metrics = run_eval(args)
    print(f"{metrics=}")
    print(f"{metrics['accuracy']=}")

    assert metrics["accuracy"] > thres, "gsm8k test failed!"


def wait_for_sglang(base_url=SGLANG_URL, timeout=120):
    """
    循环查询 sglang 服务状态，直到返回 200 OK
    """
    health_url = f"{base_url}/health"  # 或者使用 /v1/models 查看模型是否加载
    start_time = time.time()

    print(f"正在等待 sglang 服务启动: {health_url}")

    while True:
        try:
            response = requests.get(health_url)
            if response.status_code == 200:
                print("服务已就绪 (OK)!")
                return True
        except requests.exceptions.ConnectionError:
            # 服务尚未启动，连接会被拒绝
            pass

        if time.time() - start_time > timeout:
            print("等待超时，服务未能启动。")
            return False

        time.sleep(5)  # 每隔5秒重试一次


def send_query(base_url, prompt):
    """
    服务就绪后发送实际的推理请求
    """
    query_url = f"{base_url}/generate"
    payload = {
        "text": prompt,
        "sampling_params": {"temperature": 0.7, "max_new_tokens": 128}
    }

    response = requests.post(query_url, json=payload)
    return response.json()


if __name__ == "__main__":
    if wait_for_sglang(SGLANG_URL):
        # test one curl
        result = send_query(SGLANG_URL, "你好，请自我介绍一下。")
        print("推理结果:", result)

        # test gsm8k acc
        test_gsm8k()
    else:
        raise RuntimeError("sglang启动超时！")
