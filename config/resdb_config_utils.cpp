#include "config/resdb_config_utils.h"

#include <fcntl.h>
#include <glog/logging.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <fstream>

namespace resdb {
namespace {

KeyInfo ReadKey(const std::string& file_name) {
  int fd = open(file_name.c_str(), O_RDONLY, 0666);
  if (fd < 0) {
    LOG(ERROR) << "open file:" << file_name << " fail:" << strerror(errno);
  }
  assert(fd >= 0);

  std::string res;
  int read_len = 0;
  char tmp[1024];
  while (true) {
    read_len = read(fd, tmp, sizeof(tmp));
    if (read_len <= 0) {
      break;
    }
    res.append(tmp, read_len);
  }
  close(fd);
  KeyInfo key;
  assert(key.ParseFromString(res));
  return key;
}

CertificateInfo ReadCert(const std::string& file_name) {
  int fd = open(file_name.c_str(), O_RDONLY, 0666);
  if (fd < 0) {
    LOG(ERROR) << "open file:" << file_name << " fail" << strerror(errno);
  }
  assert(fd >= 0);

  std::string res;
  int read_len = 0;
  char tmp[1024];
  while (true) {
    read_len = read(fd, tmp, sizeof(tmp));
    if (read_len <= 0) {
      break;
    }
    res.append(tmp, read_len);
  }
  close(fd);
  CertificateInfo info;
  assert(info.ParseFromString(res));
  return info;
}

}  // namespace

ReplicaInfo GenerateReplicaInfo(int id, const std::string& ip, int port) {
  ReplicaInfo info;
  info.set_id(id);
  info.set_ip(ip);
  info.set_port(port);
  return info;
}

std::vector<ReplicaInfo> ReadConfig(const std::string& file_name) {
  std::vector<ReplicaInfo> replicas;
  std::string line;
  std::ifstream infile(file_name.c_str());
  int id;
  std::string ip;
  int port;
  while (infile >> id >> ip >> port) {
    replicas.push_back(GenerateReplicaInfo(id, ip, port));
  }
  if (replicas.size() == 0) {
    LOG(ERROR) << "read config:" << file_name << " fail.";
    assert(replicas.size() > 0);
  }
  return replicas;
}

std::unique_ptr<ResDBConfig> GenerateResDBConfig(
    const std::string& config_file, const std::string& private_key_file,
    const std::string& cert_file, std::optional<ReplicaInfo> self_info,
    std::optional<ConfigGenFunc> gen_func) {
  std::vector<ReplicaInfo> replicas = ReadConfig(config_file);
  KeyInfo private_key = ReadKey(private_key_file);
  CertificateInfo cert_info = ReadCert(cert_file);

  LOG(ERROR) << "private key:" << private_key.DebugString()
             << " cert:" << cert_info.DebugString();
  if (!self_info.has_value()) {
    self_info = ReplicaInfo();
  }

  (*self_info).set_id(cert_info.public_key().public_key_info().node_id());
  (*self_info).set_ip(cert_info.public_key().public_key_info().ip());
  (*self_info).set_port(cert_info.public_key().public_key_info().port());

  *(*self_info).mutable_certificate_info() = cert_info;

  if (gen_func.has_value()) {
    return (*gen_func)(replicas, self_info.value(), private_key, cert_info);
  }
  return std::make_unique<ResDBConfig>(replicas, self_info.value(), private_key,
                                       cert_info);
}

ResDBConfig GenerateResDBConfig(const std::string& config_file) {
  std::vector<ReplicaInfo> replicas = ReadConfig(config_file);
  return ResDBConfig(replicas, ReplicaInfo());
}

}  // namespace resdb
