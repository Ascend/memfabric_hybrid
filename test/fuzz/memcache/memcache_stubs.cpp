#include <mockcpp/mockcpp.hpp>
#include "mmc_bm_proxy.h"
#include "mmc_local_service_default.h"
#include "mmc_msg_base.h"
#include "acc_tcp_server.h"
#include "memcache_stubs.h"

using namespace ock::mmc;
using namespace ock::acc;

// smem
void MockSmemBm()
{
    MOCKER(smem_bm_config_init).stubs().will(returnValue(0));
    MOCKER(smem_bm_init).stubs().will(returnValue(0));
    MOCKER(smem_bm_uninit).stubs().will(returnValue(0));
    MOCKER(smem_bm_get_rank_id).stubs().will(ignoreReturnValue());
    MOCKER(smem_bm_create).stubs().will(returnValue((void *)(1)));
    MOCKER(smem_bm_destroy).stubs().will(ignoreReturnValue());
    MOCKER(smem_bm_join).stubs().will(returnValue(0));
    MOCKER(smem_bm_leave).stubs().will(returnValue(0));
    MOCKER(smem_bm_get_local_mem_size).stubs().will(returnValue(0));
    MOCKER(smem_bm_ptr).stubs().will(returnValue((void *)(1)));
    MOCKER(smem_bm_copy).stubs().will(returnValue(0));
    MOCKER(smem_bm_copy_2d).stubs().will(returnValue(0));
}

// meta service
Response resp {};
AllocResponse allocResp {};
IsExistResponse existResp {};
QueryResponse queryResp {};
BatchQueryResponse batchQueryResp {};
BatchIsExistResponse batchExistResp {};
BatchRemoveResponse batchRemoveResp {};
BatchAllocResponse batchAllocResp {};
BatchUpdateResponse batchUpdateResp {};

Response *GetResp() { return &resp; }
AllocResponse *GetAllocResp() { return &allocResp; }
IsExistResponse *GetIsExistResp() { return &existResp; }
QueryResponse *GetQueryResp() { return &queryResp; }
BatchQueryResponse *GetBatchQueryResp() { return &batchQueryResp; }
BatchIsExistResponse *GetBatchIsExistResp() { return &batchExistResp; }
BatchRemoveResponse *GetBatchRemoveResp() { return &batchRemoveResp; }
BatchAllocResponse *GetBatchAllocResp() { return &batchAllocResp; }
BatchUpdateResponse *GetBatchUpdateResp() { return &batchUpdateResp; }

void ResetResp()
{
    resp = Response();
    allocResp = AllocResponse();
    existResp = IsExistResponse();
    queryResp = QueryResponse();
    batchQueryResp = BatchQueryResponse();
    batchExistResp = BatchIsExistResponse();
    batchRemoveResp = BatchRemoveResponse();
    batchAllocResp = BatchAllocResponse();
    batchUpdateResp = BatchUpdateResponse();
}


int32_t _HandleAlloc(const NetContextPtr &context)
{
    AllocRequest req;
    return context->Reply(req.msgId, allocResp);
}

int32_t _HandleBmRegister(const NetContextPtr &context)
{
    BmRegisterRequest req;
    return context->Reply(req.msgId, resp);
}

int32_t _HandlePing(const NetContextPtr &context)
{
    return 0; // unused
}

int32_t _HandleUpdate(const NetContextPtr &context)
{
    UpdateRequest req;
    return context->Reply(req.msgId, resp);
}

int32_t _HandleBatchUpdate(const NetContextPtr &context)
{
    BatchUpdateRequest req;
    return context->Reply(req.msgId, batchUpdateResp);
}

int32_t _HandleGet(const NetContextPtr &context)
{
    GetRequest req;
    return context->Reply(req.msgId, allocResp);
}

int32_t _HandleBatchGet(const NetContextPtr &context)
{
    BatchGetRequest req;
    return context->Reply(req.msgId, batchAllocResp);
}

int32_t _HandleRemove(const NetContextPtr &context)
{
    RemoveRequest req;
    return context->Reply(req.msgId, resp);
}

int32_t _HandleBatchRemove(const NetContextPtr &context)
{
    BatchRemoveRequest req;
    return context->Reply(req.msgId, batchRemoveResp);
}

int32_t _HandleIsExist(const NetContextPtr &context)
{
    IsExistRequest req;
    return context->Reply(req.msgId, existResp);
}

int32_t _HandleBatchIsExist(const NetContextPtr &context)
{
    BatchIsExistRequest req;
    return context->Reply(req.msgId, batchExistResp);
}

int32_t _HandleBmUnregister(const NetContextPtr &context)
{
    BmUnregisterRequest req;
    return context->Reply(req.msgId, resp);
}

int32_t _HandleQuery(const NetContextPtr &context)
{
    QueryRequest req;
    return context->Reply(req.msgId, queryResp);
}

int32_t _HandleBatchQuery(const NetContextPtr &context)
{
    BatchQueryRequest req;
    return context->Reply(req.msgId, batchQueryResp);
}

int32_t _HandleBatchAlloc(const NetContextPtr &context)
{
    BatchAllocRequest req;
    return context->Reply(req.msgId, batchAllocResp);
}

int32_t _HandleNewLink(const NetLinkPtr &link)
{
    return 0;
}

int32_t _HandleLinkBroken(const NetLinkPtr &link)
{
    return 0;
}

void RegisterMockHandles(NetEnginePtr mServer)
{
    auto handleAlloc =        [](const NetContextPtr &context) { return _HandleAlloc(context); };
    auto handleBmRegister =   [](const NetContextPtr &context) { return _HandleBmRegister(context); };
    auto handlePing =         [](const NetContextPtr &context) { return _HandlePing(context); };
    auto handleUpdate =       [](const NetContextPtr &context) { return _HandleUpdate(context); };
    auto handleBatchUpdate =  [](const NetContextPtr &context) { return _HandleBatchUpdate(context); };
    auto handleGet =          [](const NetContextPtr &context) { return _HandleGet(context); };
    auto handleBatchGet =     [](const NetContextPtr &context) { return _HandleBatchGet(context); };
    auto handleRemove =       [](const NetContextPtr &context) { return _HandleRemove(context); };
    auto handleBatchRemove =  [](const NetContextPtr &context) { return _HandleBatchRemove(context); };
    auto handleIsExist =      [](const NetContextPtr &context) { return _HandleIsExist(context); };
    auto handleBatchIsExist = [](const NetContextPtr &context) { return _HandleBatchIsExist(context); };
    auto handleBmUnregister = [](const NetContextPtr &context) { return _HandleBmUnregister(context); };
    auto handleQuery =        [](const NetContextPtr &context) { return _HandleQuery(context); };
    auto handleBatchQuery =   [](const NetContextPtr &context) { return _HandleBatchQuery(context); };
    auto handleBatchAlloc =   [](const NetContextPtr &context) { return _HandleBatchAlloc(context); };
    auto handleNewLink =      [](const NetLinkPtr &link)       { return _HandleNewLink(link); };
    auto handleLinkBroken =   [](const NetLinkPtr &link)       { return _HandleLinkBroken(link); };
    mServer->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_ALLOC_REQ, handleAlloc);
    mServer->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_BM_REGISTER_REQ, handleBmRegister);
    mServer->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_PING_REQ, handlePing);
    mServer->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_UPDATE_REQ, handleUpdate);
    mServer->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_BATCH_UPDATE_REQ, handleBatchUpdate);
    mServer->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_GET_REQ, handleGet);
    mServer->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_BATCH_GET_REQ, handleBatchGet);
    mServer->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_REMOVE_REQ, handleRemove);
    mServer->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_BATCH_REMOVE_REQ, handleBatchRemove);
    mServer->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_IS_EXIST_REQ, handleIsExist);
    mServer->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_BATCH_IS_EXIST_REQ, handleBatchIsExist);
    mServer->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_BM_UNREGISTER_REQ, handleBmUnregister);
    mServer->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_QUERY_REQ, handleQuery);
    mServer->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_BATCH_QUERY_REQ, handleBatchQuery);
    mServer->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_BATCH_ALLOC_REQ, handleBatchAlloc);
    mServer->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::LM_PING_REQ, nullptr);
    mServer->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::LM_META_REPLICATE_REQ, nullptr);
    mServer->RegNewLinkHandler(handleNewLink);
    mServer->RegLinkBrokenHandler(handleLinkBroken);
}

// acc tcp server
MockAccTcpLinkComplex::MockAccTcpLinkComplex(int fd, const std::string &ipPort, uint32_t id) : AccTcpLinkComplex(fd, ipPort, id) {}
MockAccTcpLinkComplex::~MockAccTcpLinkComplex() = default;
int32_t MockAccTcpLinkComplex::NonBlockSend(int16_t msgType, const AccDataBufferPtr &d, const AccDataBufferPtr &cbCtx) { return 0; }
int32_t MockAccTcpLinkComplex::NonBlockSend(int16_t msgType, uint32_t seqNo, const AccDataBufferPtr &d, const AccDataBufferPtr &cbCtx) { return 0; }
int32_t MockAccTcpLinkComplex::NonBlockSend(int16_t msgType, int16_t opCode, uint32_t seqNo, const AccDataBufferPtr &d, const AccDataBufferPtr &cbCtx) { return 0; }
int32_t MockAccTcpLinkComplex::EnqueueAndModifyEpoll(const AccMsgHeader &h, const AccDataBufferPtr &d, const AccDataBufferPtr &cbCtx) { return 0; };
int32_t MockAccTcpLinkComplex::BlockSend(void *data, uint32_t len) { return 0; }
int32_t MockAccTcpLinkComplex::BlockRecv(void *data, uint32_t demandLen) { return 0; }
int32_t MockAccTcpLinkComplex::PollingInput(int32_t timeoutInMs) const { return 0; }
int32_t MockAccTcpLinkComplex::SetSendTimeout(uint32_t timeoutInUs) const { return 0; }
int32_t MockAccTcpLinkComplex::SetReceiveTimeout(uint32_t timeoutInUs) const { return 0; }
int32_t MockAccTcpLinkComplex::EnableNoBlocking() const { return 0; }
void MockAccTcpLinkComplex::Close() {}
bool MockAccTcpLinkComplex::IsConnected() const { return true; }
void MockAccTcpLinkComplex::UpCtx(uint64_t context) {}
uint64_t MockAccTcpLinkComplex::UpCtx() { return 1; }

MockAccTcpServer::MockAccTcpServer()
{
    MockAccTcpLinkComplex *mockAccTcpLinkComplex = new MockAccTcpLinkComplex(0, "127.0.0.1:5559", 0);
    mockAccTcpLinkComplexPtr.Set(mockAccTcpLinkComplex);
}
MockAccTcpServer::~MockAccTcpServer() = default;
int32_t MockAccTcpServer::Start(const AccTcpServerOptions &opt) { return 0; }
int32_t MockAccTcpServer::Start(const AccTcpServerOptions &opt, const AccTlsOption &tlsOption) { return 0; }
void MockAccTcpServer::Stop() {}
void MockAccTcpServer::StopAfterFork() {}
int32_t MockAccTcpServer::ConnectToPeerServer(const std::string &peerIp, uint16_t port, const AccConnReq &req,
    uint32_t maxRetryTimes, AccTcpLinkComplexPtr &newLink)
{
    newLink = mockAccTcpLinkComplexPtr;
    return 0;
}
int32_t MockAccTcpServer::ConnectToPeerServer(const std::string &peerIp, uint16_t port, const AccConnReq &req,
    AccTcpLinkComplexPtr &newLink)
{
    newLink = mockAccTcpLinkComplexPtr;
    return 0;
}
void MockAccTcpServer::RegisterNewRequestHandler(int16_t msgType, const AccNewReqHandler &h) {}
void MockAccTcpServer::RegisterRequestSentHandler(int16_t msgType, const AccReqSentHandler &h) {}
void MockAccTcpServer::RegisterLinkBrokenHandler(const AccLinkBrokenHandler &h) {}
void MockAccTcpServer::RegisterNewLinkHandler(const AccNewLinkHandler &h) {}
void MockAccTcpServer::RegisterDecryptHandler(const AccDecryptHandler &h) {}
int32_t MockAccTcpServer::LoadDynamicLib(const std::string &dynLibPath) { return 0; }
