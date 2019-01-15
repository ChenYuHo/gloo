/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

#include "params.hpp"
#include <boost/program_options.hpp>

namespace po = boost::program_options;

namespace daiet {

    struct dpdk_data dpdk_data;
    struct dpdk_params dpdk_par;
    daiet_params daiet_par;

    void parse_parameters() {

        string config_file;
        ifstream ifs;
        float max_float;
        uint16_t worker_port, ps_port;
        uint32_t num_updates;
        string worker_ip_str, ps_ips_str, ps_macs_str;

        po::options_description dpdk_options("DPDK options");
        po::options_description daiet_options("DAIET options");
        po::options_description config_file_options;

        dpdk_options.add_options()
                ("dpdk.cores", po::value<string>(&dpdk_par.corestr)->default_value("0-2"), "List of cores")
                ("dpdk.prefix", po::value<string>(&dpdk_par.prefix)->default_value("daiet"), "Process prefix")
                ("dpdk.extra_eal_options", po::value<string>(&dpdk_par.eal_options)->default_value(""), "Extra EAL options")
                ("dpdk.port_id", po::value<uint16_t>(&dpdk_par.portid)->default_value(0), "Port ID")
                ("dpdk.pool_size", po::value<uint32_t>(&dpdk_par.pool_size)->default_value(8192 * 32), "Pool size")
                ("dpdk.pool_cache_size", po::value<uint32_t>(&dpdk_par.pool_cache_size)->default_value(256 * 2), "Pool cache size")
                ("dpdk.burst_rx", po::value<uint32_t>(&dpdk_par.burst_rx)->default_value(64), "RX burst size")
                ("dpdk.burst_tx", po::value<uint32_t>(&dpdk_par.burst_tx)->default_value(64), "TX burst size")
                ("dpdk.bulk_drain_tx_us", po::value<uint32_t>(&dpdk_par.bulk_drain_tx_us)->default_value(100), "TX bulk drain timer (us)");

        daiet_options.add_options()
                ("daiet.worker_ip", po::value<string>(&worker_ip_str)->default_value("10.0.0.1"), "IP address of this worker")
                ("daiet.worker_port", po::value<uint16_t>(&worker_port)->default_value(4000), "Worker UDP port")
                ("daiet.ps_port", po::value<uint16_t>(&ps_port)->default_value(48879), "PS UDP port")
                ("daiet.ps_ips", po::value<string>(&ps_ips_str)->required(), "Comma-separated list of PS IP addresses")
                ("daiet.ps_macs", po::value<string>(&ps_macs_str)->required(), "Comma-separated list of PS MAC addresses")
                ("daiet.max_num_pending_messages", po::value<uint32_t>(&(daiet_par.getMaxNumPendingMessages()))->default_value(256), "Max number of pending, unaggregated messages")
                ("daiet.num_updates", po::value<uint32_t>(&num_updates)->default_value(32), "Number of updates per packet")
                ("daiet.max_float", po::value<float>(&max_float)->default_value(FLT_MAX), "Max float value")
#ifndef COLOCATED
                ("daiet.mode", po::value<string>(&(daiet_par.getMode()))->default_value("worker"), "Mode (worker or ps)")
#endif
                ("daiet.num_workers", po::value<uint32_t>(&(daiet_par.getNumWorkers()))->default_value(0), "Number of workers (only for PS mode)");


        config_file_options.add(daiet_options).add(dpdk_options);

        config_file = "/etc/daiet.cfg";
        ifs.open(config_file.c_str());
        if(!ifs.good()){
            ifs.close();

            char hostname[500];
            if (gethostname(hostname,sizeof(hostname))!=0)
                LOG_FATAL("gethostname failed: "+ string(strerror(errno)));

            config_file = "daiet-"+string(hostname)+".cfg";
            ifs.open(config_file.c_str());
            if(!ifs.good()){
                ifs.close();

                config_file = "daiet.cfg";
                ifs.open(config_file.c_str());
                if(!ifs.good()){
                    ifs.close();
                    LOG_FATAL("No config file found! (/etc/daiet.cfg, daiet-"+string(hostname)+".cfg, daiet.cfg)");
                }
            }
        }
        LOG_INFO("Configuration file "+config_file);

        po::variables_map vm;
        po::store(po::parse_config_file(ifs, config_file_options), vm);
        po::notify(vm);

#ifndef COLOCATED
        if (daiet_par.getMode() != "worker" && daiet_par.getMode() != "ps")
            LOG_FATAL("Wrong mode: " + daiet_par.getMode());
#endif

        if (!daiet_par.setWorkerIp(worker_ip_str))
            LOG_FATAL("Invalid worker IP: " + worker_ip_str);

        daiet_par.setWorkerPort(worker_port);
        daiet_par.setPsPort(ps_port);

        if (!daiet_par.setPs(ps_ips_str, ps_macs_str))
            LOG_FATAL("Invalid PS address: \n" + ps_ips_str + "\n" + ps_macs_str);

        daiet_par.setMaxFloat(max_float);
        daiet_par.setNumUpdates(num_updates);

#ifndef COLOCATED
        if (daiet_par.getNumWorkers()<=0 && daiet_par.getMode()=="ps")
#else
        if (daiet_par.getNumWorkers()<=0)
#endif
            LOG_FATAL("PS mode requires a positive number of workers.");
    }

    void print_dpdk_params() {

        LOG_INFO("** DPDK parameters **");
        LOG_INFO("Cores: " + dpdk_par.corestr);
        LOG_INFO("Port ID: " + to_string(dpdk_par.portid));
        LOG_INFO("Port RX ring size: " + to_string(dpdk_par.port_rx_ring_size));
        LOG_INFO("Port TX ring size: " + to_string(dpdk_par.port_tx_ring_size));
        LOG_INFO("Pool size: " + to_string(dpdk_par.pool_size));
        LOG_INFO("Pool cache size: " + to_string(dpdk_par.pool_cache_size));
        LOG_INFO("Burst size RX: " + to_string(dpdk_par.burst_rx));
        LOG_INFO("Burst size TX: " + to_string(dpdk_par.burst_tx));
        LOG_INFO("Burst drain TX us: " + to_string(dpdk_par.bulk_drain_tx_us));
        LOG_INFO("Prefix: " + dpdk_par.prefix);
        LOG_INFO("Extra EAL options: " + dpdk_par.eal_options);
    }

    daiet_params::daiet_params() {
        // Defaults

        mode = "worker";

        num_workers = 0;

        num_updates = 32;

        max_num_pending_messages = 40960;

        tx_flags = PKT_TX_IP_CKSUM | PKT_TX_IPV4 | PKT_TX_UDP_CKSUM;

        scaling_factor = INT32_MAX / FLT_MAX;

        worker_port_be = rte_cpu_to_be_16(4000);
        ps_port_be = rte_cpu_to_be_16(48879);
        worker_ip_be = rte_cpu_to_be_32(0x0a000001);

        ps_ips_be = NULL;

        ps_macs_be = NULL;

        num_ps = 0;
    }

    daiet_params::~daiet_params() {
        if (ps_ips_be != NULL)
            delete[] ps_ips_be;
        if (ps_macs_be != NULL)
            delete[] ps_macs_be;
    }

    void daiet_params::print_params() {

        LOG_INFO("** DAIET parameters **");
        LOG_INFO("Num updates: " + to_string(num_updates));
        LOG_INFO("Max num pending messages: " + to_string(max_num_pending_messages));
        LOG_INFO("Worker port: " + to_string(rte_be_to_cpu_16(worker_port_be)));
        LOG_INFO("PS port: " + to_string(rte_be_to_cpu_16(ps_port_be)));
        LOG_INFO("Scaling factor: " + to_string(scaling_factor));

        LOG_INFO("Worker IP: " + ip_to_str(worker_ip_be));

        for (uint32_t i = 0; i < num_ps; i++) {

            LOG_INFO("PS" + to_string(i) + ": " + mac_to_str(ps_macs_be[i]) + " " + ip_to_str(ps_ips_be[i]));
        }
    }

    string& daiet_params::getMode() {
        return mode;
    }

    uint32_t& daiet_params::getNumWorkers() {
        return num_workers;
    }

    void daiet_params::setNumUpdates(uint32_t numUpdates) {
        num_updates = numUpdates;
    }

    void daiet_params::setMaxFloat(float maxFloat) {
        scaling_factor = INT32_MAX / maxFloat;
    }

    void daiet_params::setWorkerPort(uint16_t workerPort) {
        worker_port = workerPort;
    }

    void daiet_params::setPsPort(uint16_t psPort) {
        ps_port_be = rte_cpu_to_be_16(psPort);

    }

    /*
     * Returns false if the IP is invalid
     */
    bool daiet_params::setWorkerIp(string workerIp) {

        struct in_addr addr;

        if (inet_aton(workerIp.c_str(), &addr) == 0)
            return false;

        worker_ip_be = addr.s_addr;
        return true;
    }

    bool daiet_params::setPs(string psIps, string psMacs) {

        int64_t rc;

        vector<string> ips = split(psIps, ", ");
        vector<string> macs = split(psMacs, ", ");

        num_ps = ips.size() < macs.size() ? ips.size() : macs.size();

        if (ps_ips_be != NULL)
            delete[] ps_ips_be;
        if (ps_macs_be != NULL)
            delete[] ps_macs_be;

        ps_ips_be = new uint32_t[num_ps];
        ps_macs_be = new uint64_t[num_ps];

        struct in_addr addr;

        for (uint32_t i = 0; i < num_ps; i++) {

            if (inet_aton(ips[i].c_str(), &addr) == 0)
                return false;

            ps_ips_be[i] = addr.s_addr;

            rc = str_to_mac(macs[i]);
            if (rc < 0)
                return false;

            ps_macs_be[i] = rc;
        }

        return true;
    }
}
