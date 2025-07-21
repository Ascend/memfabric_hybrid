import time
import unittest

from pymmc import DistributedObjectStore


class TestExample(unittest.TestCase):
    key_1 = "key_1"
    original_data = b"some data!"

    def setUp(self):
        self._distributed_object_store = DistributedObjectStore()
        self._distributed_object_store.init()
        print("object store initialized")

    def test_1(self):
        # check existence before put
        exist_res = self._distributed_object_store.is_exist(self.key_1)
        self.assertEqual(exist_res, 0)

        # test put
        put_res = self._distributed_object_store.put(self.key_1, self.original_data)
        self.assertEqual(put_res, 0)

        # check existence after put
        exist_res = self._distributed_object_store.is_exist(self.key_1)
        self.assertEqual(exist_res, 1)

        retrieved_data = self._distributed_object_store.get(self.key_1)
        print(retrieved_data)
        self.assertEqual(retrieved_data, self.original_data)

        time.sleep(3) # wait for the lease to expire

        # test remove
        rm_res = self._distributed_object_store.remove(self.key_1)
        self.assertEqual(rm_res, 0)

        # check existence after remove
        exist_res = self._distributed_object_store.is_exist(self.key_1)
        self.assertEqual(exist_res, 0)

    def tearDown(self):
        self._distributed_object_store.close()
        print("object store destroyed")


if __name__ == '__main__':
    unittest.main()
