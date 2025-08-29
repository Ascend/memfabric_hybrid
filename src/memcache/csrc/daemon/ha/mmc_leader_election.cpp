/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <chrono>

#include "mmc_logger.h"
#include "mmc_leader_election.h"

namespace ock {
namespace mmc {
std::string g_defaultMemCacheWhlName = "meta_service_leader_election";
constexpr uint32_t LEASE_RETRY_PERIOD = 3;

MmcMetaServiceLeaderElection::MmcMetaServiceLeaderElection(
    const std::string &name, const std::string &pod, const std::string &ns, const std::string &lease)
    : name_(name), podName_(pod), leaseName_(lease), ns_(ns)
{}

Result MmcMetaServiceLeaderElection::Start()
{
    try {
        if (running_) {
            MMC_LOG_INFO("Leader election already started");
            return MMC_OK;
        }

        MMC_FALSE_ERROR(!this->podName_.empty(), "Meta pod name is empty, please check env variable META_POD_NAME");
        MMC_FALSE_ERROR(!this->leaseName_.empty(), "Meta lease name is empty, please check env variable META_LEASE_NAME");
        MMC_FALSE_ERROR(!this->ns_.empty(), "Meta namespace is empty, please check env variable META_NAMESPACE");

        pybind11::gil_scoped_acquire acquire;

        pybind11::module modelModule = pybind11::module_::import(g_defaultMemCacheWhlName.c_str());
        MMC_FALSE_ERROR(pybind11::hasattr(modelModule, "MetaServiceLeaderElection"),
                        "The class named MetaServiceLeaderElection was not found.");

        pybind11::object metaServiceLeaderElection = modelModule.attr("MetaServiceLeaderElection");
        leaderElection_ =
            metaServiceLeaderElection(this->leaseName_, this->ns_, this->podName_, LEASE_RETRY_PERIOD, 1, "");
        MMC_FALSE_ERROR(!leaderElection_.is_none(),
                        "Can not start election before MetaServiceLeaderElection initialized");
        MMC_FALSE_ERROR(pybind11::hasattr(leaderElection_, "update_lease"),
                        "No member function 'update_lease' is found in class 'MetaServiceLeaderElection'");
        MMC_FALSE_ERROR(pybind11::hasattr(leaderElection_, "check_leader_status"),
                        "No member function 'check_leader_status' is found in class 'MetaServiceLeaderElection'");
        MMC_FALSE_ERROR(pybind11::hasattr(leaderElection_, "update_pod_to_master"),
                        "No member function 'update_pod_to_master' is found in class 'MetaServiceLeaderElection'");
        MMC_FALSE_ERROR(pybind11::hasattr(leaderElection_, "update_pod_to_backup"),
                        "No member function 'update_pod_to_backup' is found in class 'MetaServiceLeaderElection'");

        pybind11::gil_scoped_release release;

        MMC_LOG_INFO("Start leader election 0");
        running_ = true;
        this->electionThread_ = std::thread(std::bind(&MmcMetaServiceLeaderElection::ElectionLoop, this));
        this->renewThread_ = std::thread(std::bind(&MmcMetaServiceLeaderElection::RenewLoop, this));
    } catch (const std::exception &e) {
        MMC_LOG_ERROR("Leader election start election failed." << typeid(e).name() << ", " << e.what());
        return MMC_ERROR;
    } catch (...) {
        MMC_LOG_ERROR("Leader election start election failed due to an unknown error");
        return MMC_ERROR;
    }

    return MMC_OK;
}

void MmcMetaServiceLeaderElection::ElectionLoop()
{
    MMC_LOG_INFO("Start to elect leader, current pod is " << this->podName_);

    uint64_t count = 1uLL;
    try {
        while (running_) {
            MMC_LOG_DEBUG("Election loop circle, pod=" << this->podName_ << ",  times=" << ++count);
            pybind11::gil_scoped_acquire acquire;  // 安全
            if (!running_) {
                pybind11::gil_scoped_release release;
                break;
            }

            CheckLeaderStatus();

            pybind11::gil_scoped_release release;
            std::this_thread::sleep_for(std::chrono::seconds(LEASE_RETRY_PERIOD));
        }
    } catch (const std::exception &e) {
        MMC_LOG_ERROR("Election loop failed: " << typeid(e).name() << ", " << e.what());
    } catch (...) {
        MMC_LOG_ERROR("Election loop failed due to an unknown error");
    }
}

void MmcMetaServiceLeaderElection::CheckLeaderStatus()
{
    pybind11::object result = leaderElection_.attr("check_leader_status")();
    std::string currentLeader = result.cast<std::string>();
    if (currentLeader == this->podName_) {
        if (!this->isLeader_) {
            MMC_LOG_WARN("Pod " << this->podName_ << " became the leader");
            this->isLeader_ = true;
            OnStartLeading();
        }
    } else {
        if (this->isLeader_) {
            MMC_LOG_WARN("Pod " << this->podName_ << " became a backup");
            this->isLeader_ = false;
            OnStopLeading();
        } else {
            pybind11::object result = leaderElection_.attr("update_lease")(false);
            bool success = result.cast<bool>();
            if (success) {
                MMC_LOG_WARN("Pod " << this->podName_ << " became the leader");
                this->isLeader_ = true;
                OnStartLeading();
            }
        }
    }
}

void MmcMetaServiceLeaderElection::RenewLoop()
{
    MMC_LOG_INFO("Start to renew lease, current pod is " << this->podName_);
    uint64_t count = 1uLL;
    try {
        while (this->running_) {
            if (!this->isLeader_) {
                std::this_thread::sleep_for(std::chrono::seconds(LEASE_RETRY_PERIOD));
                continue;
            }
            MMC_LOG_DEBUG("The leader renew lease loop circle, pod=" << this->podName_ << ",  times=" << ++count);
            pybind11::gil_scoped_acquire acquire;  // 安全
            if (!running_) {
                pybind11::gil_scoped_release release;
                break;
            }

            pybind11::object result = leaderElection_.attr("update_lease")(true);
            bool success = result.cast<bool>();
            if (!success && this->isLeader_) {
                MMC_LOG_ERROR("Failed in renewing lease and became a backup, pod=" << this->podName_);
                this->isLeader_ = false;
                OnStopLeading();
            }

            pybind11::gil_scoped_release release;
            std::this_thread::sleep_for(std::chrono::seconds(LEASE_RETRY_PERIOD));
        }
    }  catch (const std::exception &e) {
        MMC_LOG_ERROR("Renew loop failed: " << typeid(e).name() << ", " << e.what());
    } catch (...) {
        MMC_LOG_ERROR("Renew loop failed due to an unknown error");
    }
}

void MmcMetaServiceLeaderElection::OnStartLeading()
{
    leaderElection_.attr("update_pod_to_master")();
}

void MmcMetaServiceLeaderElection::OnStopLeading()
{
    leaderElection_.attr("update_pod_to_backup")();
}

void MmcMetaServiceLeaderElection::Stop()
{
    if (!running_) {
        return;
    }

    running_ = false;
    if (this->electionThread_.joinable()) {
        this->electionThread_.join();
    }
    if (this->renewThread_.joinable()) {
        this->renewThread_.join();
    }
    MMC_LOG_INFO("Stop leader election");
}

MmcMetaServiceLeaderElection::~MmcMetaServiceLeaderElection()
{
    this->Stop();
}

}
}