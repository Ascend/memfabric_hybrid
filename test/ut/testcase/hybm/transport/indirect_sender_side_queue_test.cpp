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
#include "sender_side_queue.h"

using namespace ock::mf::transport::device;

namespace {
constexpr uint32_t TEST_DEFAULT_THREAD_COUNT = 2U;
constexpr uint32_t TEST_ALT_THREAD_COUNT = 4U;
constexpr uint32_t TEST_SINGLE_THREAD_COUNT = 1U;
constexpr int TEST_RANK_ID = 1;
constexpr int TEST_INVALID_RANK_ID = 999;
constexpr size_t TEST_NEXT_REQ_BODY_SIZE = 8;
constexpr int TEST_STARTUP_WAIT_MS = 100;
constexpr int TEST_STARTUP_FAIL_WAIT_MS = 200;
constexpr uint64_t TEST_INVALID_RANK_REQUEST_ID = 2000UL;
constexpr uint64_t TEST_NOT_STARTED_REQUEST_ID = 6000UL;
}

class MockThreadContext : public ThreadContext {
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

class SenderSideQueueTest : public testing::Test {
public:
    SenderSideQueueTest()
        : senderQueue_{TEST_DEFAULT_THREAD_COUNT, CreateProcessors()},
          context_(std::make_shared<MockThreadContext>()),
          counter_(new std::atomic<uint64_t>(0))
    {}

    ~SenderSideQueueTest() override
    {
        delete counter_;
        senderQueue_.Stop();
    }

    void SetUp() override
    {
        counter_->store(0UL);
        
        auto ret = socketpair(AF_UNIX, SOCK_STREAM, 0, fds_);
        ASSERT_EQ(0, ret) << "socketpair failed: " << errno << ": " << strerror(errno);
        
        ASSERT_TRUE(senderQueue_.Start(context_));
        
        senderQueue_.AddRankIdSocket(TEST_RANK_ID, fds_[0]);
    }

    void TearDown() override
    {
        senderQueue_.RemoveRankIdSocket(TEST_RANK_ID);
        close(fds_[0]);
        close(fds_[1]);
        fds_[0] = fds_[1] = -1;
        senderQueue_.Stop();
        counter_->store(0UL);
    }

protected:
    std::unordered_map<uint16_t, SendPhProcess> CreateProcessors()
    {
        std::unordered_map<uint16_t, SendPhProcess> processors;
        
        processors.emplace(0, [this](const QueueMessage &res, QueueMessage &nextReq,
                                     bool &finished, void *ctx) -> int {
            auto counter = static_cast<std::atomic<uint64_t>*>(ctx);
            if (counter) {
                counter->fetch_add(1UL);
            }
            
            finished = false;
            nextReq.head = res.head;
            nextReq.head.request = 1U;
            nextReq.head.opCode = 1;
            nextReq.body.resize(TEST_NEXT_REQ_BODY_SIZE);
            
            return 0;
        });
        
        processors.emplace(1, [this](const QueueMessage &res, QueueMessage &nextReq,
                                     bool &finished, void *ctx) -> int {
            auto counter = static_cast<std::atomic<uint64_t>*>(ctx);
            if (counter) {
                counter->fetch_add(1UL);
            }
            
            finished = true;
            lastResponse_ = res;
            
            return 0;
        });
        
        return processors;
    }

protected:
    SenderSideQueue senderQueue_;
    std::shared_ptr<MockThreadContext> context_;
    std::atomic<uint64_t>* counter_;
    QueueMessage lastResponse_;
    int fds_[2] = {-1, -1};
};

TEST_F(SenderSideQueueTest, Construction)
{
    SenderSideQueue testQueue{TEST_ALT_THREAD_COUNT, {}};
    SUCCEED();
}

TEST_F(SenderSideQueueTest, StartAndStop)
{
    SenderSideQueue testQueue{TEST_DEFAULT_THREAD_COUNT, {}};
    
    EXPECT_TRUE(testQueue.Start());
    EXPECT_TRUE(testQueue.Start());
    
    testQueue.Stop();
    testQueue.Stop();
}

TEST_F(SenderSideQueueTest, StartWithThreadContext)
{
    auto ctx = std::make_shared<MockThreadContext>();
    SenderSideQueue testQueue{TEST_SINGLE_THREAD_COUNT, {}};
    
    EXPECT_TRUE(testQueue.Start(ctx));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(TEST_STARTUP_WAIT_MS));
    
    EXPECT_TRUE(ctx->startupCalled_);
    
    testQueue.Stop();
    EXPECT_TRUE(ctx->shutdownCalled_);
}

TEST_F(SenderSideQueueTest, StartWithFailingThreadContext)
{
    auto ctx = std::make_shared<MockThreadContext>();
    ctx->startupRet_ = -1;
    
    SenderSideQueue testQueue{TEST_SINGLE_THREAD_COUNT, {}};
    EXPECT_TRUE(testQueue.Start(ctx));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(TEST_STARTUP_FAIL_WAIT_MS));
    
    EXPECT_TRUE(ctx->startupCalled_);
    EXPECT_TRUE(ctx->shutdownCalled_);
    
    testQueue.Stop();
}

TEST_F(SenderSideQueueTest, ExistRankIdSocket)
{
    EXPECT_TRUE(senderQueue_.ExistRankIdSocket(TEST_RANK_ID));
    EXPECT_FALSE(senderQueue_.ExistRankIdSocket(TEST_INVALID_RANK_ID));
}

TEST_F(SenderSideQueueTest, RemoveRankIdSocket)
{
    senderQueue_.RemoveRankIdSocket(TEST_RANK_ID);
    EXPECT_FALSE(senderQueue_.ExistRankIdSocket(TEST_RANK_ID));
    
    senderQueue_.RemoveRankIdSocket(TEST_INVALID_RANK_ID);
}

TEST_F(SenderSideQueueTest, CloseAllSockets)
{
    senderQueue_.CloseAllSockets();
    EXPECT_FALSE(senderQueue_.ExistRankIdSocket(TEST_RANK_ID));
}

TEST_F(SenderSideQueueTest, BeginRequestInvalidRank)
{
    QueueMessage request;
    request.head.request = 1U;
    request.head.requestId = TEST_INVALID_RANK_REQUEST_ID;
    request.head.srcRankId = 0;
    request.head.dstRankId = TEST_INVALID_RANK_ID;
    request.head.opCode = 0;
    request.head.bodySize = 0;
    
    int ret = senderQueue_.BeginRequest(std::move(request), counter_);
    EXPECT_EQ(-1, ret);
}

TEST_F(SenderSideQueueTest, DestructorStopsQueue)
{
    auto* testQueue = new SenderSideQueue{TEST_SINGLE_THREAD_COUNT, {}};
    ASSERT_TRUE(testQueue->Start());
    
    delete testQueue;
    SUCCEED();
}

TEST_F(SenderSideQueueTest, BeginRequestNotStarted)
{
    SenderSideQueue testQueue{TEST_SINGLE_THREAD_COUNT, {}};
    
    QueueMessage request;
    request.head.requestId = TEST_NOT_STARTED_REQUEST_ID;
    request.head.dstRankId = TEST_RANK_ID;
    
    int ret = testQueue.BeginRequest(std::move(request), nullptr);
    EXPECT_EQ(-1, ret);
}
