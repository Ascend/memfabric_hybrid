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
#include <atomic>

#include "hybm_logger.h"
#include "receiver_side_queue.h"

using namespace ock::mf::transport::device;

namespace {
constexpr uint32_t TEST_DEFAULT_THREAD_COUNT = 2U;
constexpr uint32_t TEST_ALT_THREAD_COUNT = 4U;
constexpr uint32_t TEST_SINGLE_THREAD_COUNT = 1U;
constexpr int TEST_RANK_ID = 1;
constexpr int TEST_INVALID_SOCKET_FD = 999;
constexpr size_t TEST_RESPONSE_BODY_SIZE = sizeof(int32_t) * 2;
constexpr size_t TEST_LARGE_BODY_SIZE = 1024 * 50;
constexpr int TEST_NUM_MESSAGES = 5;
constexpr uint64_t TEST_MULTI_MSG_REQUEST_ID_BASE = 4000;
constexpr int TEST_PROCESSING_WAIT_MS = 500;
constexpr int TEST_SHORT_WAIT_MS = 50;
constexpr int TEST_LONG_WAIT_MS = 1000;
constexpr int TEST_STARTUP_WAIT_MS = 100;
constexpr int TEST_STARTUP_FAIL_WAIT_MS = 200;
constexpr uint16_t TEST_INVALID_OPCODE = 99;
constexpr int32_t TEST_PROCESSOR_MULTIPLIER = 2;
constexpr int32_t TEST_PROCESSOR_ADDEND = 100;
constexpr int32_t TEST_INPUT_VALUE_A = 25;
constexpr int32_t TEST_INPUT_VALUE_B = 50;
constexpr int32_t TEST_INPUT_VALUE_C = 30;
constexpr int32_t TEST_INPUT_VALUE_D = 60;
constexpr int TEST_OPCODE_MODULO = 2;
constexpr int TEST_DATA_MULTIPLIER = 10;
constexpr int TEST_BYTE_VALUE_RANGE = 256;
}

class ReceiverMockThreadContext : public ThreadContext {
public:
    int ThreadStartup() noexcept override
    {
        startupCalled_ = true;
        return startupRet_;
    }

    void ThreadShutdown() noexcept override
    {
        shutdownCalled_ = true;
    }

    bool startupCalled_ = false;
    bool shutdownCalled_ = false;
    int startupRet_ = 0;
};

class ReceiverSideQueueTest : public testing::Test {
public:
    ReceiverSideQueueTest()
        : receiverQueue_{TEST_DEFAULT_THREAD_COUNT, CreateProcessors()},
          context_(std::make_shared<ReceiverMockThreadContext>()),
          requestCount_(new std::atomic<uint64_t>(0))
    {}

    ~ReceiverSideQueueTest() override
    {
        delete requestCount_;
        receiverQueue_.Stop();
    }

    void SetUp() override
    {
        requestCount_->store(0UL);
        
        auto ret = socketpair(AF_UNIX, SOCK_STREAM, 0, fds_);
        ASSERT_EQ(0, ret) << "socketpair failed: " << errno << ": " << strerror(errno);
        
        ASSERT_TRUE(receiverQueue_.Start(context_));
        
        receiverQueue_.AddAcceptSocket(fds_[1]);
    }

    void TearDown() override
    {
        receiverQueue_.RemoveAcceptSocket(fds_[1]);
        close(fds_[0]);
        close(fds_[1]);
        fds_[0] = fds_[1] = -1;
        receiverQueue_.Stop();
        requestCount_->store(0UL);
    }

protected:
    std::unordered_map<uint16_t, RecvPhProcess> CreateProcessors()
    {
        std::unordered_map<uint16_t, RecvPhProcess> processors;
        
        processors.emplace(0, [this](const QueueMessage &request, QueueMessage &response) -> int {
            response.head = request.head;
            response.head.request = 0U;
            
            if (request.body.size() < TEST_RESPONSE_BODY_SIZE) {
                response.body.resize(0);
                response.head.bodySize = 0;
                return 0;
            }
            
            response.body.resize(TEST_RESPONSE_BODY_SIZE);
            response.head.bodySize = TEST_RESPONSE_BODY_SIZE;
            
            auto reqData = reinterpret_cast<const int32_t*>(request.body.data());
            auto respData = reinterpret_cast<int32_t*>(response.body.data());
            respData[0] = reqData[0] * TEST_PROCESSOR_MULTIPLIER;
            respData[1] = reqData[1];
            
            return 0;
        });
        
        processors.emplace(1, [this](const QueueMessage &request, QueueMessage &response) -> int {
            response.head = request.head;
            response.head.request = 0U;
            
            if (request.body.size() < TEST_RESPONSE_BODY_SIZE) {
                response.body.resize(0);
                response.head.bodySize = 0;
                return 0;
            }
            
            response.body.resize(TEST_RESPONSE_BODY_SIZE);
            response.head.bodySize = TEST_RESPONSE_BODY_SIZE;
            
            auto reqData = reinterpret_cast<const int32_t*>(request.body.data());
            auto respData = reinterpret_cast<int32_t*>(response.body.data());
            respData[0] = reqData[0] + TEST_PROCESSOR_ADDEND;
            respData[1] = reqData[1];
            
            return 0;
        });
        
        return processors;
    }

protected:
    ReceiverSideQueue receiverQueue_;
    std::shared_ptr<ReceiverMockThreadContext> context_;
    std::atomic<uint64_t>* requestCount_;
    int fds_[2] = {-1, -1};
};

TEST_F(ReceiverSideQueueTest, Construction)
{
    ReceiverSideQueue testQueue{TEST_ALT_THREAD_COUNT, {}};
    SUCCEED();
}

TEST_F(ReceiverSideQueueTest, StartAndStop)
{
    ReceiverSideQueue testQueue{TEST_DEFAULT_THREAD_COUNT, {}};
    
    EXPECT_TRUE(testQueue.Start());
    EXPECT_TRUE(testQueue.Start());
    
    testQueue.Stop();
    testQueue.Stop();
}

TEST_F(ReceiverSideQueueTest, StartWithThreadContext)
{
    auto ctx = std::make_shared<ReceiverMockThreadContext>();
    ReceiverSideQueue testQueue{TEST_SINGLE_THREAD_COUNT, {}};
    
    EXPECT_TRUE(testQueue.Start(ctx));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(TEST_STARTUP_WAIT_MS));
    
    EXPECT_TRUE(ctx->startupCalled_);
    
    testQueue.Stop();
    EXPECT_TRUE(ctx->shutdownCalled_);
}

TEST_F(ReceiverSideQueueTest, StartWithFailingThreadContext)
{
    auto ctx = std::make_shared<ReceiverMockThreadContext>();
    ctx->startupRet_ = -1;
    
    ReceiverSideQueue testQueue{TEST_SINGLE_THREAD_COUNT, {}};
    EXPECT_TRUE(testQueue.Start(ctx));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(TEST_STARTUP_FAIL_WAIT_MS));
    
    EXPECT_TRUE(ctx->startupCalled_);
    EXPECT_TRUE(ctx->shutdownCalled_);
    
    testQueue.Stop();
}

TEST_F(ReceiverSideQueueTest, RemoveAcceptSocket)
{
    receiverQueue_.RemoveAcceptSocket(fds_[1]);
    
    receiverQueue_.RemoveAcceptSocket(TEST_INVALID_SOCKET_FD);
}

TEST_F(ReceiverSideQueueTest, CloseAllSockets)
{
    receiverQueue_.CloseAllSockets();
}

TEST_F(ReceiverSideQueueTest, ReceiveMessageOpCode0)
{
    QueueMessage request;
    request.head.request = 1U;
    request.head.opCode = 0;
    request.head.srcRankId = 0;
    request.head.dstRankId = TEST_RANK_ID;
    request.head.requestId = 1000UL;
    request.head.bodySize = TEST_RESPONSE_BODY_SIZE;
    request.body.resize(TEST_RESPONSE_BODY_SIZE);
    
    auto reqData = reinterpret_cast<int32_t*>(request.body.data());
    reqData[0] = TEST_INPUT_VALUE_A;
    reqData[1] = TEST_INPUT_VALUE_B;
    
    auto headPtr = reinterpret_cast<uint8_t*>(&request.head);
    auto bodyPtr = request.body.data();
    
    ssize_t written = write(fds_[0], headPtr, sizeof(QueueMessageHead));
    ASSERT_GT(written, 0);
    
    written = write(fds_[0], bodyPtr, request.body.size());
    ASSERT_GT(written, 0);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(TEST_PROCESSING_WAIT_MS));
    
    SUCCEED();
}

TEST_F(ReceiverSideQueueTest, ReceiveMessageOpCode1)
{
    QueueMessage request;
    request.head.request = 1U;
    request.head.opCode = 1;
    request.head.srcRankId = 0;
    request.head.dstRankId = TEST_RANK_ID;
    request.head.requestId = 2000UL;
    request.head.bodySize = TEST_RESPONSE_BODY_SIZE;
    request.body.resize(TEST_RESPONSE_BODY_SIZE);
    
    auto reqData = reinterpret_cast<int32_t*>(request.body.data());
    reqData[0] = TEST_INPUT_VALUE_C;
    reqData[1] = TEST_INPUT_VALUE_D;
    
    auto headPtr = reinterpret_cast<uint8_t*>(&request.head);
    auto bodyPtr = request.body.data();
    
    ssize_t written = write(fds_[0], headPtr, sizeof(QueueMessageHead));
    ASSERT_GT(written, 0);
    
    written = write(fds_[0], bodyPtr, request.body.size());
    ASSERT_GT(written, 0);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(TEST_PROCESSING_WAIT_MS));
    
    SUCCEED();
}

TEST_F(ReceiverSideQueueTest, ReceiveMessageEmptyBody)
{
    QueueMessage request;
    request.head.request = 1U;
    request.head.opCode = 0;
    request.head.srcRankId = 0;
    request.head.dstRankId = TEST_RANK_ID;
    request.head.requestId = 3000UL;
    request.head.bodySize = 0;
    
    auto headPtr = reinterpret_cast<uint8_t*>(&request.head);
    ssize_t written = write(fds_[0], headPtr, sizeof(QueueMessageHead));
    ASSERT_GT(written, 0);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(TEST_PROCESSING_WAIT_MS));
    
    SUCCEED();
}

TEST_F(ReceiverSideQueueTest, ReceiveMultipleMessages)
{
    for (int i = 0; i < TEST_NUM_MESSAGES; ++i) {
        QueueMessage request;
        request.head.request = 1U;
        request.head.opCode = static_cast<uint16_t>(i % TEST_OPCODE_MODULO);
        request.head.srcRankId = 0;
        request.head.dstRankId = TEST_RANK_ID;
        request.head.requestId = static_cast<uint64_t>(TEST_MULTI_MSG_REQUEST_ID_BASE + i);
        request.head.bodySize = TEST_RESPONSE_BODY_SIZE;
        request.body.resize(TEST_RESPONSE_BODY_SIZE);
        
        auto reqData = reinterpret_cast<int32_t*>(request.body.data());
        reqData[0] = i * TEST_DATA_MULTIPLIER;
        reqData[1] = i;
        
        auto headPtr = reinterpret_cast<uint8_t*>(&request.head);
        auto bodyPtr = request.body.data();
        
        write(fds_[0], headPtr, sizeof(QueueMessageHead));
        write(fds_[0], bodyPtr, request.body.size());
        
        std::this_thread::sleep_for(std::chrono::milliseconds(TEST_SHORT_WAIT_MS));
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(TEST_LONG_WAIT_MS));
    
    SUCCEED();
}

TEST_F(ReceiverSideQueueTest, ReceiveMessageLargeBody)
{
    QueueMessage request;
    request.head.request = 1U;
    request.head.opCode = 0;
    request.head.srcRankId = 0;
    request.head.dstRankId = TEST_RANK_ID;
    request.head.requestId = 5000UL;
    
    request.head.bodySize = TEST_LARGE_BODY_SIZE;
    request.body.resize(TEST_LARGE_BODY_SIZE);
    
    for (size_t i = 0; i < TEST_LARGE_BODY_SIZE; ++i) {
        request.body[i] = static_cast<uint8_t>(i % TEST_BYTE_VALUE_RANGE);
    }
    
    auto headPtr = reinterpret_cast<uint8_t*>(&request.head);
    auto bodyPtr = request.body.data();
    
    ssize_t written = write(fds_[0], headPtr, sizeof(QueueMessageHead));
    ASSERT_GT(written, 0);
    
    written = write(fds_[0], bodyPtr, request.body.size());
    ASSERT_GT(written, 0);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(TEST_PROCESSING_WAIT_MS));
    
    SUCCEED();
}

TEST_F(ReceiverSideQueueTest, DestructorStopsQueue)
{
    auto* testQueue = new ReceiverSideQueue{TEST_SINGLE_THREAD_COUNT, {}};
    ASSERT_TRUE(testQueue->Start());
    
    delete testQueue;
    SUCCEED();
}

TEST_F(ReceiverSideQueueTest, ProcessorReturningError)
{
    std::unordered_map<uint16_t, RecvPhProcess> failingProcessors;
    failingProcessors.emplace(0, [](const QueueMessage &request, QueueMessage &response) -> int {
        response.head = request.head;
        response.head.request = 0U;
        return -1;
    });
    
    ReceiverSideQueue failingQueue{TEST_SINGLE_THREAD_COUNT, std::move(failingProcessors)};
    ASSERT_TRUE(failingQueue.Start());
    
    int extraFds[2];
    auto ret = socketpair(AF_UNIX, SOCK_STREAM, 0, extraFds);
    ASSERT_EQ(0, ret);
    
    failingQueue.AddAcceptSocket(extraFds[1]);
    
    QueueMessage request;
    request.head.request = 1U;
    request.head.opCode = 0;
    request.head.requestId = 6000UL;
    request.head.bodySize = 0;
    
    auto headPtr = reinterpret_cast<uint8_t*>(&request.head);
    write(extraFds[0], headPtr, sizeof(QueueMessageHead));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(TEST_PROCESSING_WAIT_MS));
    
    failingQueue.RemoveAcceptSocket(extraFds[1]);
    close(extraFds[0]);
    close(extraFds[1]);
    failingQueue.Stop();
    
    SUCCEED();
}

TEST_F(ReceiverSideQueueTest, InvalidOpcodeHandling)
{
    std::unordered_map<uint16_t, RecvPhProcess> limitedProcessors;
    limitedProcessors.emplace(0, [](const QueueMessage &request, QueueMessage &response) -> int {
        response.head = request.head;
        response.head.request = 0U;
        return 0;
    });
    
    ReceiverSideQueue limitedQueue{TEST_SINGLE_THREAD_COUNT, std::move(limitedProcessors)};
    ASSERT_TRUE(limitedQueue.Start());
    
    int extraFds[2];
    auto ret = socketpair(AF_UNIX, SOCK_STREAM, 0, extraFds);
    ASSERT_EQ(0, ret);
    
    limitedQueue.AddAcceptSocket(extraFds[1]);
    
    QueueMessage request;
    request.head.request = 1U;
    request.head.opCode = TEST_INVALID_OPCODE;
    request.head.requestId = 7000UL;
    request.head.bodySize = 0;
    
    auto headPtr = reinterpret_cast<uint8_t*>(&request.head);
    write(extraFds[0], headPtr, sizeof(QueueMessageHead));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(TEST_PROCESSING_WAIT_MS));
    
    limitedQueue.RemoveAcceptSocket(extraFds[1]);
    close(extraFds[0]);
    close(extraFds[1]);
    limitedQueue.Stop();
    
    SUCCEED();
}
