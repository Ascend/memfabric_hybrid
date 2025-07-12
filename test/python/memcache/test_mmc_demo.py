import unittest

from pymmc import DistributedObjectStore


DISCOVERY_URL = "tcp://127.0.0.1:5000"
DEVICE_ID = 0
RANK_ID = 0
WORLD_SIZE = 1
BM_IP_PORT = "tcp://127.0.0.1:6000"
AUTO_RANKING = 0
CREATE_ID = 0
PROTOCOL = 'sdma'
DRAM_SIZE = 1024 * 1024 * 2
HBM_SIZE = 1024 * 1024 * 2
TIMEOUT = 5


class TestExample(unittest.TestCase):
    def setUp(self):
        self._distributed_object_store = DistributedObjectStore()
        self._distributed_object_store.setup_local_service(DISCOVERY_URL, DEVICE_ID, RANK_ID, WORLD_SIZE, BM_IP_PORT,
            AUTO_RANKING, CREATE_ID, PROTOCOL, DRAM_SIZE, HBM_SIZE
        )
        self._distributed_object_store.init(DISCOVERY_URL, RANK_ID, TIMEOUT)
        print("object store initialized")

    def test_put(self):
        pass

    def tearDown(self):
        self._distributed_object_store.close()
        print("object store destroyed")


if __name__ == '__main__':
    unittest.main()
