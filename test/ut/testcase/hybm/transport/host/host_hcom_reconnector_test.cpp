/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include <gtest/gtest.h>
#include <list>
#include <vector>
#include <unordered_map>
#include "hybm.h"
#include "hybm_logger.h"
#include "host_hcom_reconnector.h"

using namespace ock::mf;
using namespace ock::mf::transport::host;

class HostHcomReconnectorTest : public testing::Test {
public:
    HostHcomReconnectorTest() : connector_(1, 4) {}

    void SetUp() override
    {
        hybm_set_log_level(1);
        connResStubs_.clear();
        connCallCounts_.clear();
        auto res = connector_.Start([this](uint32_t rk, const std::string &nic) { return ConnectMock(rk, nic); });
        ASSERT_EQ(BM_OK, res);
    }

    void TearDown() override
    {
        connector_.Stop();
        connResStubs_.clear();
        connCallCounts_.clear();
    }

protected:
    Result ConnectMock(uint32_t rankId, const std::string &nic) noexcept
    {
        BM_LOG_DEBUG("connect for rank: " << rankId << ", url = " << nic);
        connCallCounts_[rankId]++;
        auto pos = connResStubs_.find(rankId);
        if (pos == connResStubs_.end() || pos->second.second.empty()) {
            BM_LOG_DEBUG("rank: " << rankId << ", no mock return ok.");
            return BM_OK;
        }

        auto &pair = pos->second;
        BM_LOG_DEBUG("rank: " << rankId << ", " << toString(pair));
        auto index = pair.first++;
        if (index >= pair.second.size()) {
            return pair.second[pair.second.size() - 1];
        }
        return pair.second[index];
    }

    int64_t ConnectCAllCount(uint32_t rankId) const noexcept
    {
        auto pos = connCallCounts_.find(rankId);
        if (pos == connCallCounts_.end()) {
            return 0L;
        }
        return pos->second.load();
    }

    static std::string toString(const std::pair<uint32_t, std::vector<Result>> &pair)
    {
        std::stringstream ss;
        ss << "(times: " << pair.first << ", res=[";
        for (auto r : pair.second) {
            ss << r << ", ";
        }
        ss << "])";
        return ss.str();
    }

protected:
    HcomReconnector connector_;
    std::unordered_map<uint32_t, std::pair<uint32_t, std::vector<Result>>> connResStubs_;
    std::unordered_map<uint32_t, std::atomic<int64_t>> connCallCounts_;
};

TEST_F(HostHcomReconnectorTest, add_task_invalid_ranks)
{
    auto res = connector_.AddReconnectTask(1, "rank-1-url");
    ASSERT_NE(BM_OK, res);
    ASSERT_EQ(0L, ConnectCAllCount(1));
}

TEST_F(HostHcomReconnectorTest, add_task_simple_connect)
{
    connector_.AddRank(0);
    auto res = connector_.AddReconnectTask(0, "rank-1-url");
    ASSERT_EQ(BM_OK, res);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_EQ(1L, ConnectCAllCount(0));
}

TEST_F(HostHcomReconnectorTest, add_task_retry_connect)
{
    connector_.AddRank(1);
    connResStubs_[1].second.emplace_back(BM_ERROR);
    connResStubs_[1].second.emplace_back(BM_OK);
    auto res = connector_.AddReconnectTask(1, "rank-1-url");
    ASSERT_EQ(BM_OK, res);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_EQ(2L, ConnectCAllCount(1));
}

TEST_F(HostHcomReconnectorTest, add_task_retry_multi_ranks_connect)
{
    connector_.AddRank(1);
    connector_.AddRank(2);

    connResStubs_[1].second.emplace_back(BM_ERROR);
    connResStubs_[1].second.emplace_back(BM_OK);
    connResStubs_[2].second.emplace_back(BM_ERROR);
    connResStubs_[2].second.emplace_back(BM_ERROR);
    connResStubs_[2].second.emplace_back(BM_OK);

    auto res = connector_.AddReconnectTask(1, "rank-1-url");
    ASSERT_EQ(BM_OK, res);
    res = connector_.AddReconnectTask(2, "rank-2-url");
    ASSERT_EQ(BM_OK, res);

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    ASSERT_EQ(2L, ConnectCAllCount(1));
    ASSERT_EQ(3L, ConnectCAllCount(2));
}

TEST_F(HostHcomReconnectorTest, add_task_retry_failed_remove_rank)
{
    connector_.AddRank(1);
    connector_.AddRank(2);

    connResStubs_[1].second.emplace_back(BM_ERROR);
    connResStubs_[2].second.emplace_back(BM_ERROR);
    auto res = connector_.AddReconnectTask(1, "rank-1-url");
    ASSERT_EQ(BM_OK, res);
    res = connector_.AddReconnectTask(2, "rank-2-url");
    ASSERT_EQ(BM_OK, res);

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    connector_.RemoveRank(1);
    connector_.RemoveRank(2);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    auto rank1count = ConnectCAllCount(1);
    auto rank2count = ConnectCAllCount(2);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    auto rank1countB = ConnectCAllCount(1);
    auto rank2countB = ConnectCAllCount(2);

    ASSERT_EQ(rank1count, rank1countB);
    ASSERT_EQ(rank2count, rank2countB);
}
