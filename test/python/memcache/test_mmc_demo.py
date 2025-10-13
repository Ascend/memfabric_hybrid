import time
import os
import unittest

from memcache import DistributedObjectStore
from memcache import ReplicateConfig


class TestExample(unittest.TestCase):
    key_1 = f"key_1{os.getpid()}"
    key_2 = f"key_2{os.getpid()}"
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

    def tearDown(self):
        self._distributed_object_store.close()
        print("object store destroyed")


if __name__ == '__main__':
    unittest.main()