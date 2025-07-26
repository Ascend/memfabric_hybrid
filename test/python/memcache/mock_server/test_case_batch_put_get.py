from test_case_common import TestClient

if __name__ == "__main__":
    client = TestClient('127.0.0.1', 10001)
    client.execute("help")
    client.execute("getServerCommands")

    count = 1
    keys = []
    put_indexes = []
    get_indexes = []
    for i in range(count):
        keys.append('test' + str(i))
        put_indexes.append(i)
        get_indexes.append(i + count)

    print("============ test cpu =========== ")
    media = 0
    client.init_mmc()
    client.batch_put_from(keys, put_indexes, media)
    client.batch_get_into(keys, get_indexes, media)
    for i in range(count):
        client.tensor_sum(put_indexes[i], media)
        client.tensor_sum(get_indexes[i], media)
        client.remove(keys[i])

    print("============ test npu =========== ")
    media = 1
    client.init_mmc()
    client.batch_put_from(keys, put_indexes, media)
    client.batch_get_into(keys, get_indexes, media)
    for i in range(count):
        client.tensor_sum(put_indexes[i], media)
        client.tensor_sum(get_indexes[i], media)
        client.remove(keys[i])

    client.close_mmc()
