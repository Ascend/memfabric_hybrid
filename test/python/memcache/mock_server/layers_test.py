from test_case_common import TestClient


if __name__ == "__main__":
    client = TestClient("127.0.0.1", 8558)
    number = 64
    size1 = [16 * 1024 for _ in range(61)]
    size2 = [128 * 1024 for _ in range(61)]
    size = size1 + size2
    layer_keys = ['test_layers_' + str(i) for i in range(number)]
    for key in layer_keys:
        res = client.put_from_layers(
            key,
            size,
            1
        )

    for key in layer_keys:
        res = client.get_into_layers(
            key,
            size,
            1
        )

    # 批量接口
    # count = 1
    # keys = ['test_evict_' + str(i) for i in range(count)]
    # res = client.batch_put_from_layers(keys, [size for _ in range(count)],1)
    # res = client.batch_get_into_layers(keys, [size for _ in range(count)],1)