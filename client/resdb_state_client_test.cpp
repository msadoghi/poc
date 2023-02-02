#include "client/resdb_state_client.h"

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "client/mock_resdb_client.h"
#include "common/test/test_macros.h"

namespace resdb {
namespace {

using ::google::protobuf::util::MessageDifferencer;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::Test;

void AddReplicaToList(const std::string& ip, int port,
                      std::vector<ReplicaInfo>* replica) {
  ReplicaInfo info;
  info.set_ip(ip);
  info.set_port(port);
  replica->push_back(info);
}

class MockResDBStateClient : public ResDBStateClient {
 public:
  MockResDBStateClient(const ResDBConfig& config) : ResDBStateClient(config) {}

  MOCK_METHOD(std::unique_ptr<ResDBClient>, GetResDBClient,
              (const std::string&, int), (override));
};

class StateClientTest : public Test {
 public:
  StateClientTest() {
    self_info_.set_ip("127.0.0.1");
    self_info_.set_port(1234);

    AddReplicaToList("127.0.0.1", 1235, &replicas_);
    AddReplicaToList("127.0.0.1", 1236, &replicas_);
    AddReplicaToList("127.0.0.1", 1237, &replicas_);
    AddReplicaToList("127.0.0.1", 1238, &replicas_);

    KeyInfo private_key;
    private_key.set_key("private_key");
    config_ = std::make_unique<ResDBConfig>(replicas_, self_info_, private_key,
                                            CertificateInfo());
  }

 protected:
  ReplicaInfo self_info_;
  std::vector<ReplicaInfo> replicas_;
  std::unique_ptr<ResDBConfig> config_;
};

TEST_F(StateClientTest, GetAllReplicaState) {
  MockResDBStateClient client(*config_);

  std::vector<ReplicaState> replica_states;
  for (auto replica : replicas_) {
    ReplicaState state;
    *state.mutable_replica_info() = replica;
    replica_states.push_back(state);
  }

  std::atomic<int> idx = 0;
  EXPECT_CALL(client, GetResDBClient)
      .Times(4)
      .WillRepeatedly(Invoke([&](const std::string& ip, int port) {
        auto client = std::make_unique<MockResDBClient>(ip, port);
        EXPECT_CALL(*client, RecvRawMessage)
            .WillRepeatedly(Invoke([&](google::protobuf::Message* message) {
              *reinterpret_cast<ReplicaState*>(message) = replica_states[idx++];
              return 0;
            }));
        return client;
      }));
  auto ret = client.GetReplicaStates();
  EXPECT_TRUE(ret.ok());
  std::set<int> results;
  for (auto& state : *ret) {
    auto it =
        std::find_if(replica_states.begin(), replica_states.end(),
                     [&](const ReplicaState& cur_state) {
                       return MessageDifferencer::Equals(cur_state, state);
                     });
    EXPECT_TRUE(it != replica_states.end());
    results.insert(it - replica_states.begin());
  }
  EXPECT_EQ(results.size(), 4);
}

TEST_F(StateClientTest, GetAllReplicaStateButOneFail) {
  MockResDBStateClient client(*config_);

  std::vector<ReplicaState> replica_states;
  for (auto replica : replicas_) {
    ReplicaState state;
    *state.mutable_replica_info() = replica;
    replica_states.push_back(state);
  }

  std::mutex mutex;
  std::atomic<int> idx = 0;
  EXPECT_CALL(client, GetResDBClient)
      .Times(4)
      .WillRepeatedly(Invoke([&](const std::string& ip, int port) {
        auto client = std::make_unique<MockResDBClient>(ip, port);
        EXPECT_CALL(*client, RecvRawMessage)
            .WillRepeatedly(Invoke([&](google::protobuf::Message* message) {
              std::unique_lock<std::mutex> lk(mutex);
              if (idx < 3) {
                *reinterpret_cast<ReplicaState*>(message) =
                    replica_states[idx++];
                return 0;
              } else {
                return -1;
              }
            }));
        return client;
      }));
  auto ret = client.GetReplicaStates();
  EXPECT_TRUE(ret.ok());
  std::set<int> results;
  for (auto& state : *ret) {
    auto it =
        std::find_if(replica_states.begin(), replica_states.end(),
                     [&](const ReplicaState& cur_state) {
                       return MessageDifferencer::Equals(cur_state, state);
                     });
    EXPECT_TRUE(it != replica_states.end());
    results.insert(it - replica_states.begin());
  }
  EXPECT_EQ(results.size(), 3);
}

}  // namespace

}  // namespace resdb
