#include "ordering/poc/pow/miner.h"

#include <assert.h>
#include <glog/logging.h>

#include <boost/format.hpp>
#include <thread>

#include "crypto/signature_verifier.h"
#include "ordering/poc/pow/miner_utils.h"

namespace resdb {

Miner::Miner(const ResDBPoCConfig& config) : config_(config) {
  // the number of zeros ahead of the binary value.
  difficulty_ = config_.GetDifficulty();
  worker_num_ = config_.GetWokerNum();
  assert(difficulty_ > 0);
  LOG(INFO) << " target config value: difficulty:" << difficulty_;
  SetSliceIdx(0);
}

std::vector<std::pair<uint64_t, uint64_t>> Miner::GetMiningSlices() {
  return mining_slices_;
}

int32_t Miner::GetSliceIdx() const { return shift_idx_; }

void Miner::SetTargetValue(const HashValue& target_value) {
  target_value_ = target_value;
  LOG(INFO) << " set target value:" << target_value_.DebugString();
}

void Miner::SetSliceIdx(int slice_idx) {
  shift_idx_ = slice_idx;

  // the maximun bits used for mining.
  uint64_t total_slice = config_.GetMaxNonceBit();
  assert(total_slice <= 64);
  total_slice = 1ll << total_slice;

  size_t replica_size = config_.GetReplicaNum();

  // id starts from 1.
  uint32_t replica_id = config_.GetSelfInfo().id();
  assert(total_slice >= replica_size);

  int idx = (replica_id - 1 + shift_idx_) % replica_size + 1;

  // the default slice is [min_slice, max_slice].
  uint64_t min_slice = total_slice / replica_size * (idx - 1);
  uint64_t max_slice = total_slice / replica_size * idx - 1;
  LOG(ERROR) << "slice idx :" << slice_idx
             << " total slice size:" << total_slice
             << " replica id:" << replica_id << " replica size:" << replica_size
             << " idx:" << idx << " min slice:" << min_slice
             << " max slice:" << max_slice;
  mining_slices_.clear();
  mining_slices_.push_back(std::make_pair(min_slice, max_slice));
}

absl::Status Miner::Mine(Block* new_block) {
	if(new_block->header().height() == 10 && new_block->max_seq() - new_block->min_seq() +1<= config_.BatchTransactionNum() && shift_idx_ == 0){
		LOG(ERROR)<<"skip fake fail";
		return absl::NotFoundError("solution not found");
	}
	if(new_block->header().height() == 30 && new_block->max_seq() - new_block->min_seq() +1<= config_.BatchTransactionNum() && shift_idx_ == 0){
		LOG(ERROR)<<"skip fake fail";
		return absl::NotFoundError("solution not found");
	}
  LOG(ERROR) << " start mine block slice:" << shift_idx_;
  stop_ = false;
  std::vector<std::pair<uint64_t, uint64_t>> slices = GetMiningSlices();

  Block::Header header(new_block->header());

  for (const auto& slice : slices) {
    uint64_t max_slice = slice.second;
    uint64_t min_slice = slice.first;

    std::vector<std::thread> ths;
    std::atomic<bool> solution_found = false;

    uint64_t step = static_cast<uint64_t>(worker_num_);
    for (uint32_t i = 0; i < worker_num_; ++i) {
      uint64_t current_slice_start = i + min_slice;

      ths.push_back(std::thread(
          [&](Block::Header header, std::pair<uint64_t, uint64_t> slice) {
            std::string header_hash;
            header_hash += GetHashDigest(header.pre_hash());
            header_hash += GetHashDigest(header.merkle_hash());

            for (uint64_t nonce = slice.first;
                 nonce <= max_slice && !stop_ && !solution_found;
                 nonce += step) {
              header.set_nonce(nonce);
              std::string str_value =
                  header_hash + std::to_string(header.nonce());
              std::string hash_digest = GetHashValue(str_value);

              if (IsValidDigest(hash_digest, difficulty_)) {
                solution_found = true;
                *new_block->mutable_hash() = DigestToHash(hash_digest);
                *new_block->mutable_header() = header;
                LOG(ERROR) << "nonce:" << nonce
                           << " hex string:" << GetDigestHexString(hash_digest)
                           << " target:" << difficulty_;
                return;
              }
            }
            LOG(ERROR) << "mine done slice:" << slice.first << " len:" << step;
          },
          header, std::make_pair(current_slice_start, 0)));
    }

    for (auto& th : ths) {
      th.join();
    }
    if (stop_) {
      LOG(ERROR) << "minning has been terminated.";
      return absl::CancelledError("terminated");
    }

    if (solution_found) {
      LOG(ERROR) << "find solution:" << new_block->header().DebugString()
                 << " hashvalue:" << new_block->hash().DebugString();
      return absl::OkStatus();
    }
  }
  LOG(ERROR) << "solution not found";
  return absl::NotFoundError("solution not found");
}

void Miner::Terminate() {
  LOG(ERROR) << "terminate mining";
  stop_ = true;
}

// Calculate the hash value: SHA256(SHA256(header))
// The hash value will be a 32bit integer.
std::string Miner::CalculatePoWHashDigest(const Block::Header& header) {
  std::string str_value;
  str_value += GetHashDigest(header.pre_hash());
  str_value += GetHashDigest(header.merkle_hash());
  str_value += std::to_string(header.nonce());
  return SignatureVerifier::CalculateHash(
      SignatureVerifier::CalculateHash(str_value));
}

HashValue Miner::CalculatePoWHash(const Block* new_block) {
  std::string digest = CalculatePoWHashDigest(new_block->header());
  return DigestToHash(digest);
}

bool Miner::IsValidHash(const Block* block) {
  return CalculatePoWHash(block) == block->hash();
}

}  // namespace resdb
