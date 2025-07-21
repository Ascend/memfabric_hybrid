import multiprocessing
import subprocess
import time
import unittest

from pymmc import DistributedObjectStore


DISCOVERY_URL = "tcp://127.0.0.1:5000"
DEVICE_ID = 0
RANK_ID = 0
WORLD_SIZE = 1
BM_IP_PORT = "tcp://127.0.0.1:6000"
BM_IP_PORT_2 = "tcp://127.0.0.1:6001"
AUTO_RANKING = 0
CREATE_ID = 0
PROTOCOL = 'sdma'
DRAM_SIZE = 1024 * 1024 * 2
HBM_SIZE = 1024 * 1024 * 2
TIMEOUT = 5


class TestLinkBroken(unittest.TestCase):
    key_1 = "key_1"
    original_data = b"some data!"

    def setUpSubProcess(self):
        store = DistributedObjectStore()
        store.setup_local_service(
            DISCOVERY_URL, DEVICE_ID, RANK_ID, WORLD_SIZE, BM_IP_PORT,
            AUTO_RANKING, CREATE_ID, PROTOCOL, DRAM_SIZE, HBM_SIZE
        )
        store.init(DISCOVERY_URL, RANK_ID, TIMEOUT)

        exist_res = store.is_exist(self.key_1)
        self.assertEqual(exist_res, 0)  # key not exist

        put_res = store.put(self.key_1, self.original_data)
        self.assertEqual(put_res, 0)  # put success

        exist_res = store.is_exist(self.key_1)
        self.assertEqual(exist_res, 1)  # key exist

        time.sleep(1000)  # subprocess sleep

    def killSubProcess(self):
        subprocess.call(["kill", "-9", str(self._process.pid)])
        print("kill test_link_broken subprocess")

    def setUp(self):
        self._process = multiprocessing.Process(target=self.setUpSubProcess)
        self._process.start()
        time.sleep(3)  # waiting subprocess finish assert
        print("start test_link_broken subprocess")

        self._distributed_object_store = DistributedObjectStore()
        self._distributed_object_store.setup_local_service(
            DISCOVERY_URL, DEVICE_ID, RANK_ID, WORLD_SIZE, BM_IP_PORT_2,
            AUTO_RANKING, CREATE_ID, PROTOCOL, DRAM_SIZE, HBM_SIZE
        )
        self._distributed_object_store.init(DISCOVERY_URL, RANK_ID, TIMEOUT)
        print("start test_link_broken main process")

    def test_abnormal_link_broken(self):
        # check existence
        exist_res = self._distributed_object_store.is_exist(self.key_1)
        self.assertEqual(exist_res, 1)  # key exist

        # kill subprocess -- abnormal link broken
        self.killSubProcess()
        time.sleep(3)  # wait for the lease to expire

        # check existence
        exist_res = self._distributed_object_store.is_exist(self.key_1)
        self.assertEqual(exist_res, 0)  # key not exist

    def tearDown(self):
        print("test finished.")


if __name__ == '__main__':
    unittest.main()
