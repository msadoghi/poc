#!/bin/sh

import sys

from oracle_script.comm.utils import *
from oracle_script.comm.comm_config import *

def gen_svr_config(config):
    iplist=get_ips(config["svr_ip_file"])
    with open(config["pow_config_path"],"w") as f:
        for idx,ip in iplist:
            port = int(config["base_port"]) + int(idx)
            f.writelines("{} {} {}\n".format(idx, ip, port))


def kill_svr(config):
    run_cmd("sh /home/ubuntu/nexres/oracle_script/pow/killall.sh {}".format(config["svr_ip_file"]))

def run_svr(config):
    run_cmd("sh /home/ubuntu/nexres/oracle_script/pow/run_server.sh {}".format(config["svr_ip_file"]))
    #iplist=get_ips(config["svr_ip_file"])
    #for (idx,svr_ip) in iplist:
    #    private_key="{}/node_{}.key.pri".format("pow_cert",idx)
    #    cert="{}/cert_{}.cert".format("pow_cert",idx)
    #    cmd="nohup {} {} {} {} {} > pow_server{}.log 2>&1 &".format(
    #               get_remote_file_name(config["svr_bin_bazel_path"]),
    #               get_remote_file_name(config["bft_config_path"]),
    #               get_remote_file_name(config["pow_config_path"]),
    #               private_key,
    #               cert, idx)
    #    run_remote_cmd(svr_ip, cmd)
#

def upload_svr(config):
    iplist=get_ips(config["svr_ip_file"])
    rm_cmd=[]
    scp_cmd=[]

    for (idx,svr_ip) in iplist:
        private_key=config["cert_path"]+"/"+ "node_"+idx+".key.pri"
        cert=config["cert_path"]+ "/"+ "cert_"+idx+".cert"
        rm_cmd.append("\"ssh -i {} -n -o BatchMode=yes -o StrictHostKeyChecking=no ubuntu@{} \'rm -rf {}; rm -rf server.config; rm -rf pow_*; mkdir -p pow_cert;\' \"".format(KEY,svr_ip,get_remote_file_name(config["svr_bin_bazel_path"])))
        scp_cmd.append("\"scp -i {} {} ubuntu@{}:/home/ubuntu\"".format(KEY,config["svr_bin_bazel_path"],svr_ip))
        scp_cmd.append("\"scp -i {} {} ubuntu@{}:/home/ubuntu\"".format(KEY,config["bft_config_path"],svr_ip))
        scp_cmd.append("\"scp -i {} {} ubuntu@{}:/home/ubuntu\"".format(KEY,config["pow_config_path"],svr_ip))
        scp_cmd.append("\"scp -i {} {} ubuntu@{}:/home/ubuntu/pow_cert\"".format(KEY,private_key, svr_ip))
        scp_cmd.append("\"scp -i {} {} ubuntu@{}:/home/ubuntu/pow_cert\"".format(KEY,cert, svr_ip))

    cmd_shell = "/home/ubuntu/nexres/oracle_script/pow/shell_cmd.sh"
    resp = run_cmd("sh {} {}".format(cmd_shell, ' '.join(rm_cmd)))
    resp = run_cmd("sh {} {}".format(cmd_shell, ' '.join(scp_cmd)))

if __name__ == '__main__':
    config_file=sys.argv[1]
    config = read_config(config_file)
    print("config:{}".format(config))
    gen_svr_config(config)
    kill_svr(config)
    upload_svr(config)
    run_svr(config)
