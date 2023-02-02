#include "ordering/pbft/commitment.h"

#include <glog/logging.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <future>

#include "client/mock_resdb_client.h"
#include "common/test/test_macros.h"
#include "config/resdb_config_utils.h"
#include "crypto/mock_signature_verifier.h"
#include "ordering/pbft/transaction_manager.h"
#include "server/mock_resdb_replica_client.h"

namespace resdb {
namespace {

using ::resdb::testing::EqualsProto;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::Test;

class CommitmentTest : public Test {
 public:
  CommitmentTest()
      :  // just set the monitor time to 1 second to return early.
        global_stats_(Stats::GetGlobalStats(1)),
        config_({GenerateReplicaInfo(1, "127.0.0.1", 1234),
                 GenerateReplicaInfo(2, "127.0.0.1", 1235),
                 GenerateReplicaInfo(3, "127.0.0.1", 1236),
                 GenerateReplicaInfo(4, "127.0.0.1", 1237)},
                GenerateReplicaInfo(1, "127.0.0.1", 1234)),
        system_info_(config_),
        transaction_manager_(std::make_unique<TransactionManager>(
            config_, nullptr, nullptr, &system_info_)),
        commitment_(
            std::make_unique<Commitment>(config_, transaction_manager_.get(),
                                         &replica_client_, &verifier_)) {}

  std::unique_ptr<Context> GetContext() {
    auto context = std::make_unique<Context>();
    context->signature.set_signature("signature");
    return context;
  }

  int AddProposeMsg(int sender_id, bool need_resp = false, int proxy_id = 1) {
    auto context = std::make_unique<Context>();
    context->signature.set_signature("signature");

    Request request;
    request.set_current_view(1);
    request.set_seq(1);
    request.set_type(Request::TYPE_PRE_PREPARE);
    request.set_sender_id(sender_id);
    request.set_need_response(need_resp);
    request.set_proxy_id(proxy_id);

    return commitment_->ProcessProposeMsg(std::move(context),
                                          std::make_unique<Request>(request));
  }

  int AddPrepareMsg(int sender_id) {
    auto context = std::make_unique<Context>();
    context->signature.set_signature("signature");

    Request request;
    request.set_current_view(1);
    request.set_seq(1);
    request.set_type(Request::TYPE_PREPARE);
    request.set_sender_id(sender_id);
    return commitment_->ProcessPrepareMsg(std::move(context),
                                          std::make_unique<Request>(request));
  }

  int AddCommitMsg(int sender_id) {
    auto context = std::make_unique<Context>();
    context->signature.set_signature("signature");

    Request request;
    request.set_current_view(1);
    request.set_seq(1);
    request.set_type(Request::TYPE_COMMIT);
    request.set_sender_id(sender_id);
    return commitment_->ProcessCommitMsg(std::move(context),
                                         std::make_unique<Request>(request));
  }

 protected:
  Stats* global_stats_;
  ResDBConfig config_;
  SystemInfo system_info_;
  MockResDBReplicaClient replica_client_;
  MockSignatureVerifier verifier_;
  std::unique_ptr<TransactionManager> transaction_manager_;
  std::unique_ptr<Commitment> commitment_;
};

TEST_F(CommitmentTest, NotContesxt) {
  EXPECT_EQ(
      commitment_->ProcessProposeMsg(nullptr, std::make_unique<Request>()), -2);
  EXPECT_EQ(
      commitment_->ProcessPrepareMsg(nullptr, std::make_unique<Request>()), -2);
  EXPECT_EQ(commitment_->ProcessCommitMsg(nullptr, std::make_unique<Request>()),
            -2);
}

TEST_F(CommitmentTest, NoSignature) {
  EXPECT_EQ(commitment_->ProcessProposeMsg(std::make_unique<Context>(),
                                           std::make_unique<Request>()),
            -2);
  EXPECT_EQ(commitment_->ProcessPrepareMsg(std::make_unique<Context>(),
                                           std::make_unique<Request>()),
            -2);
  EXPECT_EQ(commitment_->ProcessCommitMsg(std::make_unique<Context>(),
                                          std::make_unique<Request>()),
            -2);
}

TEST_F(CommitmentTest, NewRequest) {
  auto context = std::make_unique<Context>();
  context->signature.set_signature("signature");

  std::promise<bool> propose_done;
  std::future<bool> propose_done_future = propose_done.get_future();
  EXPECT_CALL(replica_client_, BroadCast).WillOnce(Invoke([&]() {
    propose_done.set_value(true);
  }));

  EXPECT_EQ(commitment_->ProcessNewRequest(std::move(context),
                                           std::make_unique<Request>()),
            0);
  propose_done_future.get();
}

TEST_F(CommitmentTest, SeqConsumeAll) {
  config_.SetMaxProcessTxn(2);
  commitment_ = nullptr;
  transaction_manager_ = std::make_unique<TransactionManager>(
      config_, nullptr, nullptr, &system_info_);
  commitment_ = std::make_unique<Commitment>(
      config_, transaction_manager_.get(), &replica_client_, &verifier_);
  std::promise<bool> done;
  std::future<bool> done_future = done.get_future();
  for (int i = 0; i < 3; ++i) {
    std::unique_ptr<MockResDBClient> resp_client =
        std::make_unique<MockResDBClient>("127.0.0.1", 0);
    auto context = std::make_unique<Context>();
    context->signature.set_signature("signature");
    if (i < 2) {
      EXPECT_EQ(commitment_->ProcessNewRequest(std::move(context),
                                               std::make_unique<Request>()),
                0);
    } else {
      EXPECT_EQ(commitment_->ProcessNewRequest(std::move(context),
                                               std::make_unique<Request>()),
                -2);
    }
  }
}

TEST_F(CommitmentTest, ProposeMsg) {
  EXPECT_CALL(replica_client_, BroadCast).Times(1);

  EXPECT_EQ(AddProposeMsg(Request::TYPE_PRE_PREPARE, 1), 0);
}

TEST_F(CommitmentTest, ProposeMsgOnlyBCOnce) {
  EXPECT_CALL(replica_client_, BroadCast).Times(1);

  EXPECT_EQ(AddProposeMsg(Request::TYPE_PRE_PREPARE, 1), 0);
  EXPECT_EQ(AddProposeMsg(Request::TYPE_PRE_PREPARE, 2), -2);
}

TEST_F(CommitmentTest, ProcessPrepareMsg) {
  EXPECT_CALL(replica_client_, BroadCast).Times(2);

  EXPECT_EQ(AddProposeMsg(1), 0);

  EXPECT_EQ(AddPrepareMsg(1), 0);
  EXPECT_EQ(AddPrepareMsg(2), 0);
  EXPECT_EQ(AddPrepareMsg(3), 0);
}

TEST_F(CommitmentTest, ProcessPrepareMsgNotEnough) {
  EXPECT_CALL(replica_client_, BroadCast).Times(1);

  EXPECT_EQ(AddProposeMsg(1), 0);

  EXPECT_EQ(AddPrepareMsg(1), 0);
}

TEST_F(CommitmentTest, ProcessPrepareMsgNoProposeMsg) {
  EXPECT_CALL(replica_client_, BroadCast).Times(0);

  EXPECT_EQ(AddPrepareMsg(1), 0);
  EXPECT_EQ(AddPrepareMsg(2), 0);
  EXPECT_EQ(AddPrepareMsg(3), 0);
}

TEST_F(CommitmentTest, ProcessPrepareMsgProposeMsgDelay) {
  EXPECT_CALL(replica_client_, BroadCast).Times(2);
  EXPECT_EQ(AddPrepareMsg(2), 0);
  EXPECT_EQ(AddPrepareMsg(3), 0);

  EXPECT_EQ(AddProposeMsg(1), 0);

  EXPECT_EQ(AddPrepareMsg(1), 0);
}

TEST_F(CommitmentTest, ProcessCommitMsg) {
  EXPECT_CALL(replica_client_, BroadCast).Times(2);  // 1 propose + 1 prepare

  EXPECT_EQ(AddProposeMsg(1), 0);

  EXPECT_EQ(AddPrepareMsg(1), 0);
  EXPECT_EQ(AddPrepareMsg(2), 0);
  EXPECT_EQ(AddPrepareMsg(3), 0);

  EXPECT_EQ(AddCommitMsg(1), 0);
  EXPECT_EQ(AddCommitMsg(2), 0);
  EXPECT_EQ(AddCommitMsg(3), 0);
}

TEST_F(CommitmentTest, ProcessCommitMsgProposeDelay) {
  EXPECT_CALL(replica_client_, BroadCast).Times(2);  // 1 propose + 1 prepare

  EXPECT_EQ(AddPrepareMsg(2), 0);
  EXPECT_EQ(AddPrepareMsg(3), 0);

  EXPECT_EQ(AddCommitMsg(2), 0);
  EXPECT_EQ(AddCommitMsg(3), 0);

  EXPECT_EQ(AddProposeMsg(1), 0);
  EXPECT_EQ(AddPrepareMsg(1), 0);

  EXPECT_EQ(AddCommitMsg(1), 0);
}

TEST_F(CommitmentTest, ProcessCommitMsgWithDuplicated) {
  EXPECT_CALL(replica_client_, BroadCast).Times(2);  // 1 propose + 1 prepare

  EXPECT_EQ(AddProposeMsg(1), 0);
  EXPECT_EQ(AddProposeMsg(1), -2);

  EXPECT_EQ(AddPrepareMsg(1), 0);
  EXPECT_EQ(AddPrepareMsg(2), 0);
  EXPECT_EQ(AddPrepareMsg(2), 0);
  EXPECT_EQ(AddPrepareMsg(3), 0);

  EXPECT_EQ(AddCommitMsg(1), 0);
  EXPECT_EQ(AddCommitMsg(2), 0);
  EXPECT_EQ(AddCommitMsg(3), 0);
  EXPECT_EQ(AddCommitMsg(3), -2);
}

TEST_F(CommitmentTest, ProcessCommitMsgWithResponse) {
  EXPECT_CALL(replica_client_, BroadCast).Times(2);

  std::promise<bool> done;
  std::future<bool> done_future = done.get_future();

  BatchClientResponse batch_resp;

  batch_resp.set_proxy_id(1);
  batch_resp.set_seq(1);
  batch_resp.set_current_view(1);

  // Response Msg
  Request resp_request;
  resp_request.set_type(Request::TYPE_RESPONSE);
  resp_request.set_sender_id(1);
  resp_request.set_current_view(1);
  resp_request.set_seq(1);
  resp_request.set_proxy_id(1);
  batch_resp.SerializeToString(resp_request.mutable_data());

  EXPECT_CALL(replica_client_, SendMessage(EqualsProto(resp_request), 1))
      .WillOnce(Invoke(
          [&](const google::protobuf::Message& request, int64_t node_id) {
            done.set_value(true);
            return 0;
          }));

  EXPECT_EQ(AddProposeMsg(1, true), 0);

  EXPECT_EQ(AddPrepareMsg(1), 0);
  EXPECT_EQ(AddPrepareMsg(2), 0);
  EXPECT_EQ(AddPrepareMsg(3), 0);

  EXPECT_EQ(AddCommitMsg(1), 0);
  EXPECT_EQ(AddCommitMsg(2), 0);
  EXPECT_EQ(AddCommitMsg(3), 0);
  done_future.get();
}

}  // namespace

}  // namespace resdb
