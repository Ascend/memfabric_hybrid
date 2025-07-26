from test_case_common import TestClient

if __name__ == "__main__":
    client = TestClient('127.0.0.1', 10001)
    client.execute("help")
    client.execute("getServerCommands")

    test_key = "test"
    media = 0

    print("============ test cpu =========== ")
    client.init_mmc()
    client.is_exist(test_key)
    client.put_from(test_key, 0, media)
    client.is_exist(test_key)
    client.get_into(test_key, 1, media)
    client.tensor_sum(0, media)
    client.tensor_sum(1, media)
    client.remove(test_key)
    client.is_exist(test_key)

    print("============ test npu =========== ")
    media = 1
    client.put_from(test_key, 2, media)
    client.is_exist(test_key)
    client.get_into(test_key, 3, media)
    client.tensor_sum(2, media)
    client.tensor_sum(3, media)
    client.remove(test_key)
    client.is_exist(test_key)

    client.close_mmc()
