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
#include <gtest/gtest.h>

#include "hybm_logger.h"
#include "sender_side_queue.h"
#include "receiver_side_queue.h"

using namespace ock::mf::transport::device;

class EmptyThreadContext : public ThreadContext {
public:
    int ThreadStartup() noexcept override
    {
        return 0;
    }

    void ThreadShutdown() noexcept override {}
};

class IndirectAsyncQueueTest : public testing::Test {
public:
    IndirectAsyncQueueTest()
        : senderSideQueue_{8U, SenderPhraseProcessors()}, receiverSideQueue_{4U, ReceiverPhraseProcessors()}
    {
        // ock::mf::OutLogger::Instance().SetLogLevel(ock::mf::LogLevel::DEBUG_LEVEL);
        counter = new std::atomic<uint64_t>(0);
    }
    ~IndirectAsyncQueueTest()
    {
        delete counter;
    }
    void SetUp() override
    {
        counter->store(0UL);
        auto sendSuccess = senderSideQueue_.Start();
        ASSERT_TRUE(sendSuccess);
        auto recvSuccess = receiverSideQueue_.Start();
        ASSERT_TRUE(recvSuccess);

        auto ret = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
        ASSERT_EQ(0, ret) << "failed: " << errno << ": " << strerror(errno);

        senderSideQueue_.AddRankIdSocket(1, fds[0]);
        receiverSideQueue_.AddAcceptSocket(fds[1]);
    }

    // Tears down the test fixture.
    void TearDown() override
    {
        senderSideQueue_.RemoveRankIdSocket(1);
        receiverSideQueue_.RemoveAcceptSocket(fds[1]);
        close(fds[0]);
        close(fds[1]);
        fds[0] = fds[1] = -1;
        senderSideQueue_.Stop();
        receiverSideQueue_.Stop();
        counter->store(0UL);
    }

protected:
    std::unordered_map<uint16_t, SendPhProcess> SenderPhraseProcessors()
    {
        std::unordered_map<uint16_t, SendPhProcess> processors;
        for (uint16_t i = 0; i < 5U; i++) {
            processors.emplace(
                i, [this, i](const QueueMessage &res, QueueMessage &nextReq, bool &finished, void *ctx) -> int {
                    auto counter = (std::atomic<uint64_t> *)ctx;
                    counter->fetch_add(1UL);
                    finished = (i >= 4U);
                    if (!finished) {
                        nextReq.head = res.head;
                        nextReq.head.request = 1U;
                        nextReq.head.opCode++;
                        nextReq.body.resize(sizeof(uint64_t) * 2UL);
                        auto resPt = (uint64_t *)res.body.data();
                        auto reqPt = (uint64_t *)nextReq.body.data();
                        reqPt[0] = resPt[0];
                        reqPt[1] = resPt[1];
                    } else {
                        lastResponse = res;
                    }
                    return 0;
                });
        }
        return processors;
    }
    std::unordered_map<uint16_t, RecvPhProcess> ReceiverPhraseProcessors()
    {
        std::unordered_map<uint16_t, RecvPhProcess> processors;
        for (uint16_t i = 0; i < 5U; i++) {
            processors.emplace(i, [i](const QueueMessage &request, QueueMessage &response) -> int {
                constexpr uint64_t times = 100UL;
                response.head = request.head;
                response.head.request = 0U;
                auto req = (uint64_t *)request.body.data();

                response.body.resize(sizeof(uint64_t) * 2UL);
                auto res = (uint64_t *)response.body.data();
                res[0] = req[0] * times + req[1];
                res[1] = req[1];
                return 0;
            });
        }
        return processors;
    }

protected:
    SenderSideQueue senderSideQueue_;
    ReceiverSideQueue receiverSideQueue_;
    std::atomic<uint64_t> *counter;
    QueueMessage lastResponse;
    int fds[2];
};

TEST_F(IndirectAsyncQueueTest, aa)
{
    QueueMessage request;
    request.head.request = 1U;
    request.head.requestId = 1111UL;
    request.head.srcRankId = 0;
    request.head.dstRankId = 1;
    request.head.bodySize = 16U;
    request.body.resize(16U);
    auto req = (uint64_t *)request.body.data();
    req[0] = 0;
    req[1] = 37UL;
    auto ret = senderSideQueue_.BeginRequest(std::move(request), counter);
    ASSERT_EQ(0, ret);

    int times = 0;
    while (counter->load() < 5U && times++ < 20U) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100U));
    }
    ASSERT_EQ(5U, counter->load());
    auto res = (uint64_t *)lastResponse.body.data();
    ASSERT_NE(nullptr, res);
    ASSERT_EQ(3737373737UL, res[0]);
    ASSERT_EQ(37UL, res[1]);
}
