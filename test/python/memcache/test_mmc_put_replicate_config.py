# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

import time
import os
import unittest

from memcache import DistributedObjectStore
from memcache import ReplicateConfig


class TestExample(unittest.TestCase):
    key_1 = f"key_1{os.getpid()}"
    key_2 = f"key_2{os.getpid()}"
    key_3 = f"key_2{os.getpid()}"  # 多副本测试
    original_data = b"some data!"

    def setUp(self):
        self._distributed_object_store = DistributedObjectStore()
        res = self._distributed_object_store.init(0)
        self.assertEqual(res, 0)

    def test_1(self):
        print(self.key_1)
        local_service_id = self._distributed_object_store.get_local_service_id()
        print("local_service_id is:")
        print(local_service_id)
        # check existence before put
        exist_res = self._distributed_object_store.is_exist(self.key_1)
        self.assertEqual(exist_res, 0)

        # test put
        config = ReplicateConfig()
        print(config.preferredLocalServiceIDs)
        config.preferredLocalServiceIDs = [local_service_id]
        print(config.preferredLocalServiceIDs)
        print(config.replicaNum)
        put_res = self._distributed_object_store.put(self.key_1, self.original_data, config)
        self.assertEqual(put_res, 0)

        # test put2
        put_res = self._distributed_object_store.put(self.key_2, self.original_data)
        self.assertEqual(put_res, 0)

        # check existence after put
        exist_res = self._distributed_object_store.is_exist(self.key_1)
        self.assertEqual(exist_res, 1)

        key_info = self._distributed_object_store.get_key_info(self.key_1)
        print(key_info)

        retrieved_data = self._distributed_object_store.get(self.key_1)
        print(retrieved_data)
        self.assertEqual(retrieved_data, self.original_data)

        time.sleep(3) # wait for the lease to expire

        # test remove
        rm_res = self._distributed_object_store.remove(self.key_1)
        self.assertEqual(rm_res, 0)
        rm_res = self._distributed_object_store.remove(self.key_2)
        self.assertEqual(rm_res, 0)

        # check existence after remove
        exist_res = self._distributed_object_store.is_exist(self.key_1)
        self.assertEqual(exist_res, 0)
        if int(local_service_id) > 0:
            print("put replicaNum=2, key is ")
            print(self.key_3)
            config3 = ReplicateConfig()
            config3.replicaNum = 2
            put_res = self._distributed_object_store.put(self.key_3, self.original_data, config3)
            self.assertEqual(put_res, 0)
            rm_res = self._distributed_object_store.remove(self.key_3)
            self.assertEqual(rm_res, 0)
        # 大于等于3，测试复制数=3，ranklist=[0,1]
        if int(local_service_id) > 1:
            print("大于等于3，测试复制数=3，ranklist=[0,1]")
            print(self.key_3)
            config4 = ReplicateConfig()
            config4.replicaNum = 3
            config4.preferredLocalServiceIDs = [0, 1]
            print(config.preferredLocalServiceIDs)
            put_res = self._distributed_object_store.put(self.key_3, self.original_data, config4)
            self.assertEqual(put_res, 0)
            rm_res = self._distributed_object_store.remove(self.key_3)
            self.assertEqual(rm_res, 0)

    def tearDown(self):
        time.sleep(5)
        self._distributed_object_store.close()
        print("object store destroyed")


if __name__ == '__main__':
    unittest.main()