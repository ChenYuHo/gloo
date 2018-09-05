/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

#include "daiet.hpp"


#include <signal.h>


#include "dpdk.h"
#include "common.hpp"
#include "utils.hpp"
#include "params.hpp"
#include "worker.hpp"
#include "ps.hpp"

using namespace std;
using namespace daiet;
namespace po = boost::program_options;

namespace daiet {
    rte_atomic32_t* sent_message_counters;
    uint32_t core_to_workers_ids[RTE_MAX_LCORE];
    struct dpdk_data dpdk_data;
    struct dpdk_params dpdk_par;
    daiet_params daiet_par;

    volatile bool tx_rx_stop = false;

#if SAVE_LATENCIES
    uint64_t* latencies;
#endif

    int rx_loop(__attribute__((unused)) void*) {
        // Get core ID
        unsigned lcore_id = rte_lcore_id();

#if !COLOCATED
        struct rte_ring * ring_rx;

        if (daiet_par.getMode() == "worker")
            ring_rx = dpdk_data.w_ring_rx;
        else if (daiet_par.getMode() == "ps")
            ring_rx = dpdk_data.p_ring_rx;
#else
        int j;
        struct rte_mbuf* m;
        struct ether_hdr *eth;
        bool to_ps;
#endif

        int ret;

        uint16_t n_mbufs;
        struct rte_mbuf *rx_pkts_burst[MAX_PKT_BURST];

        LOG_DEBUG("RX core: " + to_string(lcore_id));

        while (!force_quit && !tx_rx_stop) {

            n_mbufs = rte_eth_rx_burst(dpdk_par.portid, 0, rx_pkts_burst, dpdk_par.burst_size_rx_read);

            if (n_mbufs != 0) {
#if !COLOCATED
                do {
                    ret = rte_ring_sp_enqueue_bulk(ring_rx, (void **) rx_pkts_burst, n_mbufs, NULL);
                } while (ret == 0);
#else
                for (j = 0; j < n_mbufs; j++) {

                    m = rx_pkts_burst[j];

                    rte_prefetch0(rte_pktmbuf_mtod(m, void *));
                    eth = rte_pktmbuf_mtod(m, struct ether_hdr *);

                    if (likely(is_daiet_pkt(eth, m->data_len, to_ps))) {
                        if (to_ps) {

                            ret = rte_ring_enqueue(dpdk_data.p_ring_rx, m);
                            if (unlikely(ret < 0))
                                LOG_FATAL("Cannot enqueue one packet");

                        } else {

                            ret = rte_ring_enqueue(dpdk_data.w_ring_rx, m);
                            if (unlikely(ret < 0))
                                LOG_FATAL("Cannot enqueue one packet");
                        }
                    } else {
                        // Free original packet
                        rte_pktmbuf_free(m);
                    }
                }
#endif
            }
        }
        return 0;
    }

    int tx_loop(__attribute__((unused)) void*) {
        // Get core ID
        unsigned lcore_id = rte_lcore_id();

#if !COLOCATED
        struct rte_ring * ring_tx;

        if (daiet_par.getMode() == "worker")
            ring_tx = dpdk_data.w_ring_tx;
        else if (daiet_par.getMode() == "ps")
            ring_tx = dpdk_data.p_ring_tx;
#endif

        uint32_t n_mbufs = 0, n_pkts, pending_mbufs, sent = 0;
        struct rte_mbuf *tx_pkts_burst[MAX_PKT_BURST];

        LOG_DEBUG("TX core: " + to_string(lcore_id));

        while (!force_quit && !tx_rx_stop) {

#if !COLOCATED
            pending_mbufs = rte_ring_sc_dequeue_bulk(ring_tx, (void **) &tx_pkts_burst[n_mbufs], dpdk_par.burst_size_tx_read, NULL);
#else
            pending_mbufs = rte_ring_sc_dequeue_bulk(dpdk_data.w_ring_tx, (void **) &tx_pkts_burst[n_mbufs], dpdk_par.burst_size_tx_read, NULL);
            pending_mbufs += rte_ring_sc_dequeue_bulk(dpdk_data.p_ring_tx, (void **) &tx_pkts_burst[n_mbufs+pending_mbufs], dpdk_par.burst_size_tx_read, NULL);
#endif
            if (pending_mbufs != 0) {

                n_mbufs += pending_mbufs;

                if (n_mbufs >= dpdk_par.burst_size_tx_write) {

                    do {
                        n_pkts = rte_eth_tx_burst(dpdk_par.portid, 0, &tx_pkts_burst[sent], n_mbufs - sent);

                        sent += n_pkts;

                    } while (sent < n_mbufs);

                    n_mbufs = 0;
                    sent = 0;
                }
            }
        }
        return 0;
    }

    /* Callback for tx errors */
    void tx_buffer_err_print_pkt(struct rte_mbuf **pkts, uint16_t unsent, void *userdata) {
        uint64_t *count = (uint64_t *) userdata;
        unsigned i;
        struct ether_hdr *eth;

        for (i = 0; i < unsent; i++) {

            rte_prefetch0(rte_pktmbuf_mtod(pkts[i], void *));
            eth = rte_pktmbuf_mtod(pkts[i], struct ether_hdr *);
            print_packet(eth, pkts[i]->data_len, daiet_par.getWorkerPortBe(), daiet_par.getPsPortBe());

            rte_pktmbuf_free(pkts[i]);
        }

        *count += unsent;
    }

#if SAVE_LATENCIES
    void write_latencies(string file_name, uint64_t hz) {
        // Write latency file
        LOG_INFO("Writing latency file...");
        ofstream latency_file(file_name);

        if (latency_file.is_open()) {
            for (int i = 0; i < daiet_par.getMaxNumMsgs() && !force_quit; i++) {
                latency_file << ((double) (latencies[i])) * 1000000 / hz << endl;
            }

            latency_file.close();
        } else {
            LOG_ERROR("Unable to open latency file");
        }
    }
#endif

    void mbuf_pool_init() {
        /* Init the buffer pool */
        //LOG_DEBUG("Creating the mbuf pool...");
        dpdk_data.pool = rte_pktmbuf_pool_create("mbuf_pool", dpdk_par.pool_size, dpdk_par.pool_cache_size, 0, dpdk_data.pool_buffer_size, rte_socket_id());
        if (dpdk_data.pool == NULL)
            LOG_FATAL("Cannot init mbuf pool");
    }

    void port_init() {

        int ret;

        uint16_t nb_ports, tmp_portid;
        bool found_portid = false;
        struct rte_eth_dev_info dev_info;

        // Port configuration
        struct rte_eth_conf port_conf = { };
        struct rte_eth_rxmode rxm = { };
        struct rte_eth_txmode txm = { };
        struct rte_eth_rxconf rx_conf = { };
        struct rte_eth_txconf tx_conf = { };

        // Port ethernet address
        struct ether_addr port_eth_addr;

        // Check number of ports
        nb_ports = rte_eth_dev_count_avail();
        if (nb_ports == 0)
            LOG_FATAL("No Ethernet ports");

        RTE_ETH_FOREACH_DEV(tmp_portid)
        {

            if (dpdk_par.portid == tmp_portid)
                found_portid = true;
        }

        if (!found_portid) {
            LOG_DEBUG("DPDK ports enabled: " + to_string(nb_ports));
            LOG_FATAL("Wrong port ID: " + to_string(dpdk_par.portid));
        }

        // Get port info
        rte_eth_dev_info_get(dpdk_par.portid, &dev_info);

        print_dev_info(dev_info);

        // Initialize port
        LOG_DEBUG("Initializing port " + to_string(dpdk_par.portid) + "...");

        rxm.split_hdr_size = 0;
        rxm.header_split = 0; //< Header Split disabled
        rxm.hw_ip_checksum = 1; //IP checksum offload enabled
        rxm.hw_vlan_filter = 0; //< VLAN filtering disabled
        rxm.jumbo_frame = 0; //< Jumbo Frame Support disabled
        rxm.hw_strip_crc = 1; //< CRC stripped by hardware
        // rxm.max_rx_pkt_len = ETHER_MAX_LEN; // < Only used if jumbo_frame enabled.

        txm.mq_mode = ETH_MQ_TX_NONE;
        port_conf.rxmode = rxm;
        port_conf.txmode = txm;
        port_conf.link_speeds = ETH_LINK_SPEED_AUTONEG;
        port_conf.lpbk_mode = 0; // Loopback operation mode disabled
        port_conf.dcb_capability_en = 0; // DCB disabled

        ret = rte_eth_dev_configure(dpdk_par.portid, 1, 1, &port_conf);
        if (ret < 0)
            LOG_FATAL("Cannot configure port. Error code: " + to_string(ret));

        dpdk_par.port_rx_ring_size = dev_info.rx_desc_lim.nb_max;
        dpdk_par.port_tx_ring_size = dev_info.tx_desc_lim.nb_max;

        // Check that numbers of Rx and Tx descriptors satisfy descriptors
        // limits from the ethernet device information, otherwise adjust
        // them to boundaries.
        ret = rte_eth_dev_adjust_nb_rx_tx_desc(dpdk_par.portid, &dpdk_par.port_rx_ring_size, &dpdk_par.port_tx_ring_size);
        if (ret < 0)
            LOG_FATAL("Cannot adjust number of descriptors. Error code: " + to_string(ret));

        // init RX queue
        rx_conf.rx_thresh.pthresh = 8;
        rx_conf.rx_thresh.hthresh = 8;
        rx_conf.rx_thresh.wthresh = 4;
        rx_conf.rx_free_thresh = 64;
        rx_conf.rx_drop_en = 0;

        ret = rte_eth_rx_queue_setup(dpdk_par.portid, 0, dpdk_par.port_rx_ring_size, rte_eth_dev_socket_id(dpdk_par.portid), &rx_conf, dpdk_data.pool);
        if (ret < 0)
            LOG_FATAL("RX queue setup error. Error code: " + to_string(ret));

        // init TX queue on each port

        tx_conf.tx_thresh.pthresh = 36;
        tx_conf.tx_thresh.hthresh = 0;
        tx_conf.tx_thresh.wthresh = 0;
        tx_conf.tx_free_thresh = 0;
        tx_conf.tx_rs_thresh = 0;

        ret = rte_eth_tx_queue_setup(dpdk_par.portid, 0, dpdk_par.port_tx_ring_size, rte_eth_dev_socket_id(dpdk_par.portid), &tx_conf);
        if (ret < 0)
            LOG_FATAL("TX queue setup error. Error code: " + to_string(ret));

        // stats mapping
        if (dev_info.max_rx_queues > 1) {
            ret = rte_eth_dev_set_rx_queue_stats_mapping(dpdk_par.portid, 0, 0);
            if (ret < 0)
                LOG_FATAL("RX queue stats mapping error");
        }

        if (dev_info.max_tx_queues > 1) {
            ret = rte_eth_dev_set_tx_queue_stats_mapping(dpdk_par.portid, 0, 0);
            if (ret < 0)
                LOG_FATAL("TX queue stats mapping error");
        }

        // Start device
        ret = rte_eth_dev_start(dpdk_par.portid);
        if (ret < 0)
            LOG_FATAL("Error starting the port. Error code: " + to_string(ret));

        // Enable promiscuous mode
        //rte_eth_promiscuous_enable(portid);

        //Get the port address
        rte_eth_macaddr_get(dpdk_par.portid, &port_eth_addr);
        LOG_DEBUG("Initialization ended. Port " + to_string(dpdk_par.portid) + " address: " + mac_to_str(port_eth_addr));

        check_port_link_status(dpdk_par.portid);
    }

    void rings_init(string mode) {

        string rx_name, tx_name;
        struct rte_ring * ring_rx;
        struct rte_ring * ring_tx;

        if (mode == "worker") {
            rx_name = "w_ring_rx";
            tx_name = "w_ring_tx";
        } else if (mode == "ps") {
            rx_name = "p_ring_rx";
            tx_name = "p_ring_tx";
        }

        ring_rx = rte_ring_create(rx_name.c_str(), dpdk_par.ring_rx_size, rte_socket_id(), RING_F_SP_ENQ);

        if (ring_rx == NULL)
            LOG_FATAL("Cannot create RX ring");

        ring_tx = rte_ring_create(tx_name.c_str(), dpdk_par.ring_tx_size, rte_socket_id(), RING_F_SC_DEQ);

        if (ring_tx == NULL)
            LOG_FATAL("Cannot create TX ring");

        if (mode == "worker") {
            dpdk_data.w_ring_rx = ring_rx;
            dpdk_data.w_ring_tx = ring_tx;
        } else if (mode == "ps") {
            dpdk_data.p_ring_rx = ring_rx;
            dpdk_data.p_ring_tx = ring_tx;
        }
    }

    void rings_cleanup(string mode) {

        struct rte_ring * ring_rx;
        struct rte_ring * ring_tx;

        if (mode == "worker") {
            ring_rx = dpdk_data.w_ring_rx;
            ring_tx = dpdk_data.w_ring_tx;
        } else if (mode == "ps") {
            ring_rx = dpdk_data.p_ring_rx;
            ring_tx = dpdk_data.p_ring_tx;
        }

        rte_ring_free(ring_rx);
        rte_ring_free(ring_tx);
    }

    void signal_handler(int signum) {
        if (signum == SIGINT || signum == SIGTERM) {
            LOG_DEBUG(" Signal " + to_string(signum) + " received, preparing to exit...");
            force_quit = true;
        }
    }

    void parse_parameters(int argc, char *argv[], string& corestr) {

        string config_file;
        int32_t update_value;
        float max_float;
        uint16_t worker_port, ps_port;
        int num_updates;
        string worker_ip_str, ps_ips_str, ps_macs_str;
        string ps_ips_default = "10.0.0.5, 10.0.0.6, 10.0.0.7, 10.0.0.8";
        string ps_macs_default = "0c:c4:7a:63:76:ea, 0c:c4:7a:63:76:dc, 0c:c4:7a:63:76:e2, 0c:c4:7a:63:78:30";

        po::options_description cmdline_options("Options");
        po::options_description dpdk_options("DPDK options");
        po::options_description daiet_options("DAIET options");
        po::options_description config_file_options;

        cmdline_options.add_options()
                ("version,v", "print version string")
                ("help,h", "produce help message")
                ("config,c", po::value<string>(&config_file)->default_value("daiet.cfg"), "Configuration file name")
#if !COLOCATED
                ("mode,m", po::value<string>(&(daiet_par.getMode()))->default_value("worker"), "Mode (worker or ps)")
#endif
                ;

        dpdk_options.add_options()
                ("dpdk.cores", po::value<string>(&corestr)->default_value("0-2"), "List of cores")
                ("dpdk.port_id", po::value<uint16_t>(&dpdk_par.portid)->default_value(0), "Port ID")
                ("dpdk.port_rx_ring_size", po::value<uint16_t>(&dpdk_par.port_rx_ring_size)->default_value(128), "Port RX ring size")
                ("dpdk.port_tx_ring_size", po::value<uint16_t>(&dpdk_par.port_tx_ring_size)->default_value(512), "Port TX ring size")
                ("dpdk.ring_rx_size", po::value<uint32_t>(&dpdk_par.ring_rx_size)->default_value(65536), "RX ring size")
                ("dpdk.ring_tx_size", po::value<uint32_t>(&dpdk_par.ring_tx_size)->default_value(65536), "TX ring size")
                ("dpdk.pool_size", po::value<uint32_t>(&dpdk_par.pool_size)->default_value(8192 * 32), "Pool size")
                ("dpdk.pool_cache_size", po::value<uint32_t>(&dpdk_par.pool_cache_size)->default_value(256 * 2), "Pool cache size")
                ("dpdk.burst_size_rx_read", po::value<uint32_t>(&dpdk_par.burst_size_rx_read)->default_value(64), "RX read burst size")
                //("dpdk.burst_size_rx_write", po::value<uint32_t>(&dpdk_par.burst_size_rx_write)->default_value(64), "RX write burst size")
                ("dpdk.burst_size_worker", po::value<uint32_t>(&dpdk_par.burst_size_worker)->default_value(64), "Worker burst size")
                ("dpdk.burst_size_tx_read", po::value<uint32_t>(&dpdk_par.burst_size_tx_read)->default_value(64), "TX read burst size")
                ("dpdk.burst_size_tx_write", po::value<uint32_t>(&dpdk_par.burst_size_tx_write)->default_value(64), "TX write burst size");

        daiet_options.add_options()
                ("daiet.num_workers", po::value<uint>(&(daiet_par.getNumWorkers()))->default_value(1), "Number of workers")
                ("daiet.worker_ip", po::value<string>(&worker_ip_str)->default_value("10.0.0.1"), "IP address of this worker")
                ("daiet.worker_port", po::value<uint16_t>(&worker_port)->default_value(4000), "Worker UDP port")
                ("daiet.ps_port", po::value<uint16_t>(&ps_port)->default_value(5000), "PS UDP port")
                ("daiet.ps_ips", po::value<string>(&ps_ips_str)->default_value(ps_ips_default), "Comma-separated list of PS IP addresses")
                ("daiet.ps_macs", po::value<string>(&ps_macs_str)->default_value(ps_macs_default), "Comma-separated list of PS MAC addresses")
                ("daiet.max_num_msgs", po::value<uint>(&(daiet_par.getMaxNumMsgs()))->default_value(1048576), "Total number of messages")
                ("daiet.max_num_pending_messages", po::value<uint>(&(daiet_par.getMaxNumPendingMessages()))->default_value(40960), "Max number of pending, unaggregated messages")
                ("daiet.update_value", po::value<int32_t>(&update_value)->default_value(1), "Update value")
                ("daiet.num_updates", po::value<int>(&num_updates)->default_value(16), "Number of updates per packet")
                ("daiet.max_float", po::value<float>(&max_float)->default_value(FLT_MAX), "Max float value");

        config_file_options.add(daiet_options).add(dpdk_options);

        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(cmdline_options).run(), vm);
        po::notify(vm);

        if (vm.count("help")) {
            LOG_INFO(cmdline_options);
            exit(EXIT_SUCCESS);
        }

        if (vm.count("version")) {
            LOG_INFO("DAIET version " + string(__DAIET_VERSION__));
            exit(EXIT_SUCCESS);
        }

        if (daiet_par.getMode() != "worker" && daiet_par.getMode() != "ps")
            LOG_FATAL("Wrong mode: " + daiet_par.getMode());

        ifstream ifs(config_file.c_str());

        if (!ifs)
            LOG_FATAL("Cannot open config file: " + config_file);

        po::store(po::parse_config_file(ifs, config_file_options), vm);
        po::notify(vm);

        if (!daiet_par.setWorkerIp(worker_ip_str))
            LOG_FATAL("Invalid worker IP: " + worker_ip_str);

        daiet_par.setWorkerPort(worker_port);
        daiet_par.setPsPort(ps_port);

        if (!daiet_par.setPs(ps_ips_str, ps_macs_str))
            LOG_FATAL("Invalid PS address: \n" + ps_ips_str + "\n" + ps_macs_str);

        daiet_par.setCellValue(update_value);
        daiet_par.setMaxFloat(max_float);
        daiet_par.setNumUpdates(num_updates);

        if (num_updates < 0 || num_updates > 255)
            LOG_FATAL("Invalid number of updates: " + to_string(num_updates) + " (must be within [0,255])");

    }

    int master(int argc, char *argv[], BlockingQueue<TensorUpdate*> &in_queue, BlockingQueue<TensorUpdate*> &out_queue) {

        try {

            int ret;

            uint64_t hz;
            ostringstream hz_str;
            clock_t begin_cpu;
            uint64_t begin;
            double elapsed_secs_cpu;
            double elapsed_secs;
            ostringstream elapsed_secs_str, elapsed_secs_cpu_str;

            uint32_t num_workers_threads;
            string corestr;

            force_quit = false;
            ps_stop = false;

            /* Set signal handler */
            //signal(SIGINT, signal_handler);
            //signal(SIGTERM, signal_handler);

            parse_parameters(argc, argv, corestr);

            // Set EAL log file
            FILE * dpdk_log_file;
            dpdk_log_file = fopen("dpdk.log", "w");
            if (dpdk_log_file == NULL) {
                string serror(strerror(errno));
                LOG_ERROR("Failed to open log file: " + serror);
            } else {
                ret = rte_openlog_stream(dpdk_log_file);
                if (ret < 0)
                    LOG_ERROR("Failed to open dpdk log stream");
            }

            daiet_log = std::ofstream("daiet.log", std::ios::out);

            // Set core list
            corestr = string(argv[0]) + " -l " + corestr;
            vector<string> par_vec = split(corestr);

            int args_c = par_vec.size();
            char* args[args_c];
            char* args_ptr[args_c];

            for (vector<string>::size_type i = 0; i != par_vec.size(); i++) {
                args[i] = new char[par_vec[i].size() + 1];
                args_ptr[i] = args[i];
                strcpy(args[i], par_vec[i].c_str());
            }

            // init EAL
            ret = rte_eal_init(args_c, args);
            if (ret < 0)
                LOG_FATAL("Invalid EAL arguments");

            uint32_t n_lcores = 0, lcore_id, worker_id, wid = 0;

            for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
                if (rte_lcore_is_enabled(lcore_id) != 0) {

                    // First core is for the master
                    if (n_lcores == 1)
                        dpdk_data.core_rx = lcore_id;
                    else if (n_lcores == 2)
                        dpdk_data.core_tx = lcore_id;
                    else {
                        core_to_workers_ids[lcore_id] = wid;
                        wid++;
                    }

                    n_lcores++;
                }
            }

            if (n_lcores < 3)
                LOG_FATAL("Number of cores (" + to_string(n_lcores) + ") must be greater than 3");

            num_workers_threads = wid;
            LOG_INFO("Number of worker threads: " + to_string(num_workers_threads));

            // Estimate CPU frequency
            hz = rte_get_timer_hz();
            hz_str << setprecision(3) << (double) hz / 1000000000;
            LOG_INFO("Estimated CPU freq: " + hz_str.str() + " GHz");

            // Create the mbuf pool
            mbuf_pool_init();

            // Initialize rings
    #if COLOCATED
            rings_init("worker");
            rings_init("ps");
    #else
            rings_init(daiet_par.getMode());
    #endif

            // Initialize port
            port_init();

            // Initialize shared variables
    #if COLOCATED
            worker_setup();
            ps_setup();
    #else
            if (daiet_par.getMode() == "worker") {
                worker_setup();
            } else if (daiet_par.getMode() == "ps") {
                ps_setup();
            }
    #endif

            // Check state of slave cores
            RTE_LCORE_FOREACH_SLAVE(lcore_id)
            {
                if (rte_eal_get_lcore_state(lcore_id) != WAIT)
                    LOG_FATAL("Core " + to_string(lcore_id) + " in state " + to_string(rte_eal_get_lcore_state(lcore_id)));
            }

            // Launch RX and TX loops
            rte_eal_remote_launch(rx_loop, NULL, dpdk_data.core_rx);
            rte_eal_remote_launch(tx_loop, NULL, dpdk_data.core_tx);

            sleep(1);

            begin_cpu = clock();
            begin = rte_get_timer_cycles();

            // Launch functions on slave cores
            RTE_LCORE_FOREACH_SLAVE(lcore_id)
            {

                if (lcore_id != dpdk_data.core_rx && lcore_id != dpdk_data.core_tx) {

                    if (daiet_par.getMode() == "worker") {
                        //rte_eal_remote_launch(worker, NULL, lcore_id);
                        LOG_FATAL("Slave worker thread");
    #if COLOCATED
                        // One worker and as many PSs as needed
                        daiet_par.getMode() = "ps";
    #endif
                    } else if (daiet_par.getMode() == "ps") {
                        rte_eal_remote_launch(ps, NULL, lcore_id);
                    }
                }
            }

            // Launch function on master cores
            if (daiet_par.getMode() == "worker")
                worker(in_queue,out_queue);
            else if (daiet_par.getMode() == "ps")
                ps(NULL);

            // Join worker/ps threads
            RTE_LCORE_FOREACH_SLAVE(lcore_id)
            {
                if (lcore_id != dpdk_data.core_rx && lcore_id != dpdk_data.core_tx) {
                    ret = rte_eal_wait_lcore(lcore_id);
                    if (unlikely(ret < 0)) {
                        LOG_DEBUG("Core " + to_string(lcore_id) + " returned " + to_string(ret));
                    }
                }
            }

            elapsed_secs = ((double) (rte_get_timer_cycles() - begin)) / hz;
            elapsed_secs_cpu = double(clock() - begin_cpu) / CLOCKS_PER_SEC;

            tx_rx_stop=true;

            // Wait RX/TX cores
            ret = rte_eal_wait_lcore(dpdk_data.core_rx);
            if (unlikely(ret < 0)) {
                LOG_DEBUG("Core " + to_string(dpdk_data.core_rx) + " returned " + to_string(ret));
            }

            ret = rte_eal_wait_lcore(dpdk_data.core_tx);
            if (unlikely(ret < 0)) {
                LOG_DEBUG("Core " + to_string(dpdk_data.core_tx) + " returned " + to_string(ret));
            }

            // Print stats
            print_dev_stats(dpdk_par.portid);
            //print_dev_xstats(dpdk_par.portid);

    #if !COLOCATED
            if (daiet_par.getMode() == "worker") {
                LOG_INFO("TX " + to_string(rte_atomic64_read(&pkt_stats.w_tx)));
                LOG_INFO("RX " + to_string(rte_atomic64_read(&pkt_stats.w_rx)));
            } else if (daiet_par.getMode() == "ps") {
                LOG_INFO("TX " + to_string(rte_atomic64_read(&pkt_stats.p_tx)));
                LOG_INFO("RX " + to_string(rte_atomic64_read(&pkt_stats.p_rx)));
            }
    #else
            LOG_INFO("Worker TX " + to_string(rte_atomic64_read(&pkt_stats.w_tx)));
            LOG_INFO("Worker RX " + to_string(rte_atomic64_read(&pkt_stats.w_rx)));
            LOG_INFO("PS TX " + to_string(rte_atomic64_read(&pkt_stats.p_tx)));
            LOG_INFO("PS RX " + to_string(rte_atomic64_read(&pkt_stats.p_rx)));
    #endif

    #if !COLOCATED
            if (daiet_par.getMode() == "worker")
    #endif
            {

                LOG_INFO("Faulty " + to_string(rte_atomic64_read(&pkt_stats.faulty)));

                int32_t min = rte_atomic32_read(&sent_message_counters[0]);
                int32_t max = min;
                int32_t tmp;

                for (int i = 1; i < daiet_par.getMaxNumPendingMessages(); i++) {
                    tmp = rte_atomic32_read(&sent_message_counters[i]);
                    if (tmp < min)
                        min = tmp;
                    if (tmp > max)
                        max = tmp;
                }

                LOG_INFO("Number of updates: min " + to_string(min) + " max " + to_string(max));
            }

            elapsed_secs_str << fixed << setprecision(6) << elapsed_secs;
            elapsed_secs_cpu_str << fixed << setprecision(6) << elapsed_secs_cpu;

            LOG_INFO("Time elapsed: " + elapsed_secs_str.str() + " seconds (CPU time: " + elapsed_secs_cpu_str.str() + " seconds)");

    #if SAVE_LATENCIES
    #if !COLOCATED
            if (daiet_par.getMode() == "worker")
    #endif
                write_latencies("latency_usec.dat", hz);
    #endif

            // Cleanup

            LOG_DEBUG("Closing port...");
            rte_eth_dev_stop(dpdk_par.portid);
            rte_eth_dev_close(dpdk_par.portid);
            LOG_DEBUG("Port closed");

    #if COLOCATED
            worker_cleanup();
            ps_cleanup();
    #else
            if (daiet_par.getMode() == "worker") {
                worker_cleanup();
            } else if (daiet_par.getMode() == "ps") {
                ps_cleanup();
            }
    #endif

            fclose(dpdk_log_file);
            daiet_log.close();

            for (vector<string>::size_type i = 0; i != par_vec.size(); i++) {
                delete[] args_ptr[i];
            }

    #if COLOCATED
            rings_cleanup("worker");
            rings_cleanup("ps");
    #else
            rings_cleanup(daiet_par.getMode());
    #endif

            return 0;

        } catch (exception& e) {
            cerr << e.what() << endl;
            return -1;
        }
    }
} // End namespace
