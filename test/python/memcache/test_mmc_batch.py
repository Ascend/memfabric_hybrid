import time
import unittest

from pymmc import DistributedObjectStore


class TestExample(unittest.TestCase):
    kv = {
        "key_1": b"some data",
        "key_2": b"test data",
        "key_3": b"example",
    }

    @classmethod
    def setUpClass(self):
        self._distributed_object_store = DistributedObjectStore()
        self._distributed_object_store.init(0)
        print("object store initialized")

    def test_batch_exist_batch_remove(self):
        # check existence before put
        exist_res = self._distributed_object_store.batch_is_exist(list(self.kv.keys()))
        self.assertEqual(len(exist_res), len(self.kv))
        self.assertTrue(all(res == 0 for res in exist_res))

        # put data
        for k, v in self.kv.items():
            put_res = self._distributed_object_store.put(k, v)
            self.assertEqual(put_res, 0)

        # check existence after put
        exist_res = self._distributed_object_store.batch_is_exist(list(self.kv.keys()))
        self.assertEqual(len(exist_res), len(self.kv))
        self.assertTrue(all(res == 1 for res in exist_res))

        for k, v in self.kv.items():
            retrieved_data = self._distributed_object_store.get(k)
            print(retrieved_data)
            self.assertEqual(retrieved_data, v)

        time.sleep(3) # wait for the lease to expire

        # test batch remove
        rm_res = self._distributed_object_store.remove_batch(list(self.kv.keys()))
        self.assertTrue(all(res == 0 for res in rm_res))

        # check existence after remove
        exist_res = self._distributed_object_store.batch_is_exist(list(self.kv.keys()))
        self.assertEqual(len(exist_res), len(self.kv))
        self.assertTrue(all(res == 0 for res in exist_res))

    @classmethod
    def tearDownClass(self):
        self._distributed_object_store.close()
        print("object store destroyed")


if __name__ == '__main__':
    unittest.main()
