#include "ordering/pbft/commitment.h"

#include <unistd.h>

#include "common/utils/utils.h"
#include "glog/logging.h"
#include "ordering/pbft/transaction_utils.h"

namespace resdb {

Commitment::Commitment(const ResDBConfig& config,
                       TransactionManager* transaction_manager,
                       ResDBReplicaClient* replica_client,
                       SignatureVerifier* verifier)
    : config_(config),
      transaction_manager_(transaction_manager),
      stop_(false),
      replica_client_(replica_client),
      verifier_(verifier) {
  executed_thread_ = std::thread(&Commitment::PostProcessExecutedMsg, this);
  global_stats_ = Stats::GetGlobalStats();
}

Commitment::~Commitment() {
  stop_ = true;
  if (executed_thread_.joinable()) {
    executed_thread_.join();
  }
}

// Handle the client request and send a pre-prepare message to others.
// TODO if not a primary, redicet to the primary replica.
int Commitment::ProcessNewRequest(std::unique_ptr<Context> context,
                                  std::unique_ptr<Request> client_request) {
  if (context == nullptr || context->signature.signature().empty()) {
    LOG(ERROR) << "client request doesn't contain signature, reject";
    return -2;
  }

  auto seq = transaction_manager_->AssignNextSeq();
  if (!seq.ok()) {
    global_stats_->SeqFail();
    return -2;
  }

  global_stats_->IncClientRequest();
  client_request->set_type(Request::TYPE_PRE_PREPARE);
  client_request->set_current_view(transaction_manager_->GetCurrentView());
  client_request->set_seq(*seq);
  client_request->set_sender_id(config_.GetSelfInfo().id());
  replica_client_->BroadCast(*client_request);
  return 0;
}

// Receive the pre-prepare message from the primary.
// TODO check whether the sender is the primary.
int Commitment::ProcessProposeMsg(std::unique_ptr<Context> context,
                                  std::unique_ptr<Request> request) {
  if (context == nullptr || context->signature.signature().empty()) {
    LOG(ERROR) << "client request doesn't contain signature, reject";
    return -2;
  }
  global_stats_->IncPropose();
  std::unique_ptr<Request> prepare_request = resdb::NewRequest(
      Request::TYPE_PREPARE, *request, config_.GetSelfInfo().id());
  prepare_request->clear_data();

  // Add request to message_manager.
  // If it has received enough same requests(2f+1), broadcast the prepare
  // message.
  CollectorResultCode ret = transaction_manager_->AddConsensusMsg(
      context->signature, std::move(request));
  if (ret == CollectorResultCode::STATE_CHANGED) {
    replica_client_->BroadCast(*prepare_request);
  }
  return ret == CollectorResultCode::INVALID ? -2 : 0;
}

// If receive 2f+1 prepare message, broadcast a commit message.
int Commitment::ProcessPrepareMsg(std::unique_ptr<Context> context,
                                  std::unique_ptr<Request> request) {
  if (context == nullptr || context->signature.signature().empty()) {
    LOG(ERROR) << "client request doesn't contain signature, reject";
    return -2;
  }
  global_stats_->IncPrepare();
  std::unique_ptr<Request> commit_request = resdb::NewRequest(
      Request::TYPE_COMMIT, *request, config_.GetSelfInfo().id());
  // Add request to message_manager.
  // If it has received enough same requests(2f+1), broadcast the commit
  // message.
  CollectorResultCode ret = transaction_manager_->AddConsensusMsg(
      context->signature, std::move(request));
  if (ret == CollectorResultCode::STATE_CHANGED) {
    replica_client_->BroadCast(*commit_request);
  }
  return ret == CollectorResultCode::INVALID ? -2 : 0;
}

// If receive 2f+1 commit message, commit the request.
int Commitment::ProcessCommitMsg(std::unique_ptr<Context> context,
                                 std::unique_ptr<Request> request) {
  if (context == nullptr || context->signature.signature().empty()) {
    LOG(ERROR) << "client request doesn't contain signature, reject";
    return -2;
  }
  global_stats_->IncCommit();
  // Add request to message_manager.
  // If it has received enough same requests(2f+1), message manager will
  // commit the request.
  CollectorResultCode ret = transaction_manager_->AddConsensusMsg(
      context->signature, std::move(request));
  return ret == CollectorResultCode::INVALID ? -2 : 0;
}

// =========== private threads ===========================
// If the transaction is executed, send back to the proxy.
int Commitment::PostProcessExecutedMsg() {
  while (!stop_) {
    auto batch_resp = transaction_manager_->GetResponseMsg();
    if (batch_resp == nullptr) {
      continue;
    }
    Request request;
    request.set_seq(batch_resp->seq());
    request.set_type(Request::TYPE_RESPONSE);
    request.set_sender_id(config_.GetSelfInfo().id());
    request.set_current_view(batch_resp->current_view());
    request.set_proxy_id(batch_resp->proxy_id());

    batch_resp->SerializeToString(request.mutable_data());
    replica_client_->SendMessage(request, request.proxy_id());
  }
  return 0;
}

}  // namespace resdb
