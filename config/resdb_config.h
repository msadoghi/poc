#pragma once

#include "proto/replica_info.pb.h"
#include "proto/signature_info.pb.h"

namespace resdb {

// TODO read from a proto json file.
class ResDBConfig {
 public:
  ResDBConfig(const std::vector<ReplicaInfo>& replicas,
              const ReplicaInfo& self_info);

  ResDBConfig(const std::vector<ReplicaInfo>& replicas,
              const ReplicaInfo& self_info, const KeyInfo& private_key,
              const CertificateInfo& public_key_cert_info);

  // Get the private key.
  KeyInfo GetPrivateKey() const;

  // Get the public key with its certificate.
  CertificateInfo GetPublicKeyCertificateInfo() const;

  // Each replica infomation, including the binding urls(or ip,port).
  const std::vector<ReplicaInfo>& GetReplicaInfos() const;
  // The current replica infomation, including the binding urls(or ip,port).
  const ReplicaInfo& GetSelfInfo() const;
  // The total number of replicas.
  size_t GetReplicaNum() const;
  // The minimum number of messages that replicas have to receive after jumping
  // to the next status..
  int GetMinDataReceiveNum() const;
  // The max malicious replicas to be tolerated (the number of f).
  size_t GetMaxMaliciousReplicaNum() const;
  // const int GetMaxMaliciousNum() const;
  // The minimum number of messages that client has to receive from the
  // replicas.
  int GetMinClientReceiveNum() const;

  // The timeout microseconds to process a client request.
  // Default value is 3s.
  void SetClientTimeoutMs(int timeout_ms);
  int GetClientTimeoutMs() const;

  // CheckPoint Window
  int GetCheckPointWaterMark() const;
  void SetCheckPointWaterMark(int water_mark);

  // Logging
  std::string GetCheckPointLoggingPath() const;
  void SetCheckPointLoggingPath(const std::string& path);

  // Checkpoint flag
  void EnableCheckPoint(bool is_enable);
  bool IsCheckPointEnabled();

  // HeartBeat
  bool HeartBeatEnabled();
  void SetHeartBeatEnabled(bool enable_heartbeat);

  // Signature Verifier
  bool SignatureVerifierEnabled();
  void SetSignatureVerifierEnabled(bool enable_sv);

  // Performance setting
  bool IsPerformanceRunning();
  void RunningPerformance(bool);

  bool IsTestMode() const;
  void SetTestMode(bool);

  // The maximun transactions being processed.
  uint32_t GetMaxProcessTxn() const;
  void SetMaxProcessTxn(uint32_t num);

  uint32_t ClientBatchWaitTimeMS() const;
  void SetClientBatchWaitTimeMS(uint32_t wait_time_ms);

  uint32_t ClientBatchNum() const;
  void SetClientBatchNum(uint32_t num);

  uint32_t GetWorkerNum() const;
  uint32_t GetInputWorkerNum() const;
  uint32_t GetOutputWorkerNum() const;

 private:
  std::vector<ReplicaInfo> replicas_;
  ReplicaInfo self_info_;
  const KeyInfo private_key_;
  const CertificateInfo public_key_cert_info_;
  int client_timeout_ms_ = 3000000;
  std::string checkpoint_logging_path_;
  int checkpoint_water_mark_ = 5;
  bool is_enable_checkpoint_ = false;
  bool hb_enabled_ = true;
  bool signature_verifier_enabled_ = true;
  bool is_performance_running_ = false;
  bool is_test_mode_ = false;
  uint32_t max_process_txn_ = 2048;
  uint32_t client_batch_wait_time_ms_ = 100000;
  uint32_t client_batch_num_ = 1;

  uint32_t worker_num_ = 8;
  uint32_t input_worker_num_ = 8;
  uint32_t output_worker_num_ = 8;
};

}  // namespace resdb
