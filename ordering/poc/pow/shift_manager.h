#pragma once

#include "config/resdb_poc_config.h"
#include "ordering/poc/proto/pow.pb.h"
#include <condition_variable>

namespace resdb {

class ShiftManager {
 public:
  ShiftManager(const ResDBPoCConfig& config);
  virtual ~ShiftManager() = default;

  void AddSliceInfo(const SliceInfo& slice_info);
  virtual bool Check(const SliceInfo& slice_info, int timeout_ms=10000);

 private:
  ResDBPoCConfig config_;
  std::map<std::pair<uint64_t, uint64_t>, std::set<uint32_t>> data_;
	std::mutex mutex_;
	std::condition_variable cv_;
};

}  // namespace resdb
