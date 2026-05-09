/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include <sys/socket.h>
#include <unistd.h>
#include <gtest/gtest.h>
#include <thread>
#include <chrono>

#include "hybm_logger.h"
#include "async_socket_queue.h"

using namespace ock::mf::transport::device;

namespace {
constexpr int TEST_RANK_ID = 1;
constexpr int TEST_INVALID_RANK_ID = 999;
constexpr int TEST_POLL_RETRY_COUNT = 20;
constexpr int TEST_POLL_SLEEP_MS = 100;
constexpr size_t TEST_LARGE_BODY_SIZE = 1024 * 100;
constexpr int TEST_NUM_MESSAGES = 10;
constexpr uint64_t TEST_MULTI_MSG_REQUEST_ID_BASE = 1000;
constexpr size_t TEST_SMALL_BODY_SIZE = 4;
constexpr size_t TEST_MSG_BODY_SIZE = 8;
constexpr uint16_t TEST_STREAM_OPCODE = 5;
constexpr uint32_t TEST_STREAM_SRC_RANK = 10;
constexpr uint32_t TEST_STREAM_DST_RANK = 20;
constexpr int TEST_STREAM_SOCKET_FD = 100;
constexpr uint32_t TEST_STREAM_BODY_SIZE = 256;
constexpr uint64_t TEST_STREAM_REQUEST_ID = 12345;
constexpr uint64_t TEST_BASIC_REQUEST_ID = 100;
constexpr uint64_t TEST_EMPTY_BODY_REQUEST_ID = 200;
constexpr uint64_t TEST_LARGE_BODY_REQUEST_ID = 300;
constexpr uint64_t TEST_INVALID_RANK_REQUEST_ID = 400;
constexpr uint16_t TEST_BODY_FILL_MODULO = 256;
constexpr uint16_t TEST_LARGE_BODY_OPCODE = 2;
constexpr int TEST_MSG_DATA_MULTIPLIER = 10;
}

class AsyncSocketQueueTest : public testing::Test {
public:
    AsyncSocketQueueTest() : queue_{"test_queue"} {}
    
    ~AsyncSocketQueueTest() override
    {
        queue_.Stop();
    }

    void SetUp() override
    {
        auto ret = socketpair(AF_UNIX, SOCK_STREAM, 0, fds_);
        ASSERT_EQ(0, ret) << "socketpair failed: " << errno << ": " << strerror(errno);
        
        ASSERT_TRUE(queue_.Start());
        
        queue_.AddRankIdSocket(TEST_RANK_ID, fds_[0]);
        queue_.AddSocket(fds_[1]);
    }

    void TearDown() override
    {
        queue_.RemoveRankIdSocket(TEST_RANK_ID);
        queue_.RemoveSocket(fds_[1]);
        close(fds_[0]);
        close(fds_[1]);
        fds_[0] = fds_[1] = -1;
        queue_.Stop();
    }

protected:
    AsyncSocketQueue queue_;
    int fds_[2] = {-1, -1};
};

TEST_F(AsyncSocketQueueTest, QueueMessageHeadDefaultConstructor)
{
    QueueMessageHead head;
    EXPECT_EQ(head.request, 1);
    EXPECT_EQ(head.errorCode, 0);
    EXPECT_EQ(head.opCode, 0);
    EXPECT_EQ(head.srcRankId, 0);
    EXPECT_EQ(head.dstRankId, 0);
    EXPECT_EQ(head.socketFd, -1);
    EXPECT_EQ(head.bodySize, 0);
    EXPECT_EQ(head.requestId, 0);
    EXPECT_EQ(head.timestamp, 0UL);
}

TEST_F(AsyncSocketQueueTest, QueueMessageHeadStreamOperator)
{
    QueueMessageHead head;
    head.request = 1;
    head.errorCode = 0;
    head.opCode = TEST_STREAM_OPCODE;
    head.srcRankId = TEST_STREAM_SRC_RANK;
    head.dstRankId = TEST_STREAM_DST_RANK;
    head.socketFd = TEST_STREAM_SOCKET_FD;
    head.bodySize = TEST_STREAM_BODY_SIZE;
    head.requestId = TEST_STREAM_REQUEST_ID;
    
    std::ostringstream oss;
    oss << head;
    
    std::string result = oss.str();
    EXPECT_NE(result.find("req?1"), std::string::npos);
    EXPECT_NE(result.find("err=0"), std::string::npos);
    EXPECT_NE(result.find("opCode=" + std::to_string(TEST_STREAM_OPCODE)), std::string::npos);
    EXPECT_NE(result.find("src=" + std::to_string(TEST_STREAM_SRC_RANK)), std::string::npos);
    EXPECT_NE(result.find("dst=" + std::to_string(TEST_STREAM_DST_RANK)), std::string::npos);
    EXPECT_NE(result.find("fd=" + std::to_string(TEST_STREAM_SOCKET_FD)), std::string::npos);
    EXPECT_NE(result.find("reqId=" + std::to_string(TEST_STREAM_REQUEST_ID)), std::string::npos);
    EXPECT_NE(result.find("bodySz=" + std::to_string(TEST_STREAM_BODY_SIZE)), std::string::npos);
}

TEST_F(AsyncSocketQueueTest, StreamMessageRWConstruction)
{
    StreamMessageRW rw(TEST_RANK_ID, fds_[0]);
    EXPECT_EQ(rw.GetRemoteRankId(), TEST_RANK_ID);
}

TEST_F(AsyncSocketQueueTest, StartAndStop)
{
    AsyncSocketQueue testQueue{"start_stop_test"};
    
    EXPECT_TRUE(testQueue.Start());
    
    EXPECT_TRUE(testQueue.Start());
    
    testQueue.Stop();
    
    testQueue.Stop();
}

TEST_F(AsyncSocketQueueTest, ExistRankIdSocket)
{
    EXPECT_TRUE(queue_.ExistRankIdSocket(TEST_RANK_ID));
    
    EXPECT_FALSE(queue_.ExistRankIdSocket(TEST_INVALID_RANK_ID));
}

TEST_F(AsyncSocketQueueTest, RemoveRankIdSocket)
{
    queue_.RemoveRankIdSocket(TEST_RANK_ID);
    EXPECT_FALSE(queue_.ExistRankIdSocket(TEST_RANK_ID));
    
    queue_.RemoveRankIdSocket(TEST_INVALID_RANK_ID);
}

TEST_F(AsyncSocketQueueTest, EnqueueMessageNotStarted)
{
    AsyncSocketQueue testQueue{"not_started"};
    
    QueueMessage message;
    message.head.requestId = 1;
    message.head.dstRankId = TEST_RANK_ID;
    
    EXPECT_FALSE(testQueue.EnqueueMessage(std::move(message)));
}

TEST_F(AsyncSocketQueueTest, DequeueMessageNotStarted)
{
    AsyncSocketQueue testQueue{"not_started_deq"};
    
    QueueMessage message;
    EXPECT_FALSE(testQueue.DequeueMessage(message));
}

TEST_F(AsyncSocketQueueTest, BasicEnqueueDequeue)
{
    QueueMessage message;
    message.head.request = 1;
    message.head.opCode = 0;
    message.head.srcRankId = 0;
    message.head.dstRankId = TEST_RANK_ID;
    message.head.requestId = TEST_BASIC_REQUEST_ID;
    message.head.bodySize = TEST_MSG_BODY_SIZE;
    message.body.resize(TEST_MSG_BODY_SIZE);
    
    EXPECT_TRUE(queue_.EnqueueMessage(std::move(message)));
    
    QueueMessage received;
    bool success = false;
    for (int i = 0; i < TEST_POLL_RETRY_COUNT; ++i) {
        if (queue_.DequeueMessage(received)) {
            success = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(TEST_POLL_SLEEP_MS));
    }
    
    EXPECT_TRUE(success);
    EXPECT_EQ(received.head.requestId, TEST_BASIC_REQUEST_ID);
    EXPECT_EQ(received.head.opCode, 0);
}

TEST_F(AsyncSocketQueueTest, CloseSockets)
{
    int extraFds[2];
    auto ret = socketpair(AF_UNIX, SOCK_STREAM, 0, extraFds);
    ASSERT_EQ(0, ret);
    
    queue_.AddSocket(extraFds[0]);
    queue_.AddSocket(extraFds[1]);
    
    queue_.CloseSockets();
    
    EXPECT_FALSE(queue_.ExistRankIdSocket(TEST_RANK_ID));
    
    close(extraFds[0]);
    close(extraFds[1]);
}

TEST_F(AsyncSocketQueueTest, MessageWithEmptyBody)
{
    QueueMessage message;
    message.head.request = 1;
    message.head.opCode = 1;
    message.head.srcRankId = 0;
    message.head.dstRankId = TEST_RANK_ID;
    message.head.requestId = TEST_EMPTY_BODY_REQUEST_ID;
    message.head.bodySize = 0;
    
    EXPECT_TRUE(queue_.EnqueueMessage(std::move(message)));
    
    QueueMessage received;
    bool success = false;
    for (int i = 0; i < TEST_POLL_RETRY_COUNT; ++i) {
        if (queue_.DequeueMessage(received)) {
            success = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(TEST_POLL_SLEEP_MS));
    }
    
    EXPECT_TRUE(success);
    EXPECT_EQ(received.head.requestId, TEST_EMPTY_BODY_REQUEST_ID);
    EXPECT_EQ(received.head.bodySize, 0);
    EXPECT_TRUE(received.body.empty());
}

TEST_F(AsyncSocketQueueTest, MessageWithLargeBody)
{
    QueueMessage message;
    message.head.request = 1;
    message.head.opCode = TEST_LARGE_BODY_OPCODE;
    message.head.srcRankId = 0;
    message.head.dstRankId = TEST_RANK_ID;
    message.head.requestId = TEST_LARGE_BODY_REQUEST_ID;
    
    message.head.bodySize = TEST_LARGE_BODY_SIZE;
    message.body.resize(TEST_LARGE_BODY_SIZE);
    
    for (size_t i = 0; i < TEST_LARGE_BODY_SIZE; ++i) {
        message.body[i] = static_cast<uint8_t>(i % TEST_BODY_FILL_MODULO);
    }
    
    EXPECT_TRUE(queue_.EnqueueMessage(std::move(message)));
    
    QueueMessage received;
    bool success = false;
    for (int i = 0; i < TEST_POLL_RETRY_COUNT; ++i) {
        if (queue_.DequeueMessage(received)) {
            success = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(TEST_POLL_SLEEP_MS));
    }
    
    EXPECT_TRUE(success);
    EXPECT_EQ(received.head.requestId, TEST_LARGE_BODY_REQUEST_ID);
    EXPECT_EQ(received.body.size(), TEST_LARGE_BODY_SIZE);
    
    for (size_t i = 0; i < TEST_LARGE_BODY_SIZE; ++i) {
        EXPECT_EQ(received.body[i], static_cast<uint8_t>(i % TEST_BODY_FILL_MODULO));
    }
}

TEST_F(AsyncSocketQueueTest, MultipleMessagesSequence)
{
    for (int i = 0; i < TEST_NUM_MESSAGES; ++i) {
        QueueMessage message;
        message.head.request = 1;
        message.head.opCode = static_cast<uint16_t>(i);
        message.head.srcRankId = 0;
        message.head.dstRankId = TEST_RANK_ID;
        message.head.requestId = static_cast<uint64_t>(TEST_MULTI_MSG_REQUEST_ID_BASE + i);
        message.head.bodySize = TEST_SMALL_BODY_SIZE;
        message.body.resize(TEST_SMALL_BODY_SIZE);
        *reinterpret_cast<int32_t*>(message.body.data()) = i * TEST_MSG_DATA_MULTIPLIER;
        
        EXPECT_TRUE(queue_.EnqueueMessage(std::move(message)));
    }
    
    for (int i = 0; i < TEST_NUM_MESSAGES; ++i) {
        QueueMessage received;
        bool success = false;
        for (int j = 0; j < TEST_POLL_RETRY_COUNT; ++j) {
            if (queue_.DequeueMessage(received)) {
                success = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(TEST_POLL_SLEEP_MS));
        }
        
        ASSERT_TRUE(success) << "Failed to receive message " << i;
        EXPECT_EQ(received.head.requestId, static_cast<uint64_t>(TEST_MULTI_MSG_REQUEST_ID_BASE + i));
        EXPECT_EQ(received.head.opCode, static_cast<uint16_t>(i));
        
        int32_t value = *reinterpret_cast<int32_t*>(received.body.data());
        EXPECT_EQ(value, i * TEST_MSG_DATA_MULTIPLIER);
    }
}

TEST_F(AsyncSocketQueueTest, EnqueueMessageInvalidRank)
{
    QueueMessage message;
    message.head.request = 1;
    message.head.opCode = 0;
    message.head.srcRankId = 0;
    message.head.dstRankId = TEST_INVALID_RANK_ID;
    message.head.requestId = TEST_INVALID_RANK_REQUEST_ID;
    message.head.bodySize = 0;
    
    EXPECT_FALSE(queue_.EnqueueMessage(std::move(message)));
}

TEST_F(AsyncSocketQueueTest, DestructorStopsQueue)
{
    auto* testQueue = new AsyncSocketQueue{"destructor_test"};
    ASSERT_TRUE(testQueue->Start());
    
    delete testQueue;
    SUCCEED();
}
