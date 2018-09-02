/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

# include "params.hpp"

namespace daiet {

    void print_dpdk_params() {

        LOG_INFO("** DPDK parameters **");
        LOG_INFO("Port ID: " + to_string(dpdk_par.portid));
        LOG_INFO("Port rx ring size: " + to_string(dpdk_par.port_rx_ring_size));
        LOG_INFO("Port tx ring size: " + to_string(dpdk_par.port_tx_ring_size));
        LOG_INFO("Ring rx size: " + to_string(dpdk_par.ring_rx_size));
        LOG_INFO("Ring tx size: " + to_string(dpdk_par.ring_tx_size));
        LOG_INFO("Pool size: " + to_string(dpdk_par.pool_size));
        LOG_INFO("Pool cache size: " + to_string(dpdk_par.pool_cache_size));
        LOG_INFO("Burst size rx read: " + to_string(dpdk_par.burst_size_rx_read));
        LOG_INFO("Burst size worker: " + to_string(dpdk_par.burst_size_worker));
        LOG_INFO("Burst size tx read: " + to_string(dpdk_par.burst_size_tx_read));
        LOG_INFO("Burst size tx write: " + to_string(dpdk_par.burst_size_tx_write));
    }

    daiet_params::daiet_params() {
        // Defaults

        mode ="worker";

        num_updates = 16;
        payload_size = EXT_HEADER_SIZE + INT_HEADER_SIZE + (num_updates * 2 * update_size);

        max_num_pending_messages = 40960;
        num_workers = 1;
        max_num_msgs = 1048576;

        cell_value = 1;
        cell_value_be = rte_cpu_to_be_32(cell_value);

        worker_port_be = rte_cpu_to_be_16(4000);
        ps_port_be = rte_cpu_to_be_16(5000);
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
        LOG_INFO("Num workers: " + to_string(num_workers));
        LOG_INFO("Max num msgs: " + to_string(max_num_msgs));
        LOG_INFO("Cell value: " + to_string(cell_value));
        LOG_INFO("Worker port: " + to_string(rte_be_to_cpu_16(worker_port_be)));
        LOG_INFO("Ps port: " + to_string(rte_be_to_cpu_16(ps_port_be)));

        LOG_INFO("Worker IP: " + ip_to_str(worker_ip_be));

        for (int i = 0; i < num_ps; i++) {

            LOG_INFO("PS" + to_string(i) + ": " + mac_to_str(ps_macs_be[i]) + " " + ip_to_str(ps_ips_be[i]));
        }
    }

    string& daiet_params::getMode() {
        return mode;
    }

    const uint32_t daiet_params::getMaxSeqNum() const {
        return max_seq_num;
    }

    void daiet_params::setNumUpdates(uint8_t numUpdates) {
        num_updates = numUpdates;
        payload_size = EXT_HEADER_SIZE + INT_HEADER_SIZE + (num_updates * 2 * update_size);
    }

    uint& daiet_params::getMaxNumPendingMessages() {
        return max_num_pending_messages;
    }

    uint& daiet_params::getNumWorkers() {
        return num_workers;
    }

    uint& daiet_params::getMaxNumMsgs() {
        return max_num_msgs;
    }

    int32_t daiet_params::getCellValue() const {
        return cell_value;
    }

    void daiet_params::setCellValue(int32_t cellValue) {
        cell_value = cellValue;
        cell_value_be = rte_cpu_to_be_32(cell_value);
    }

    void daiet_params::setWorkerPort(uint16_t workerPort) {
        worker_port_be = rte_cpu_to_be_16(workerPort);
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

        for (int i = 0; i < num_ps; i++) {

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
