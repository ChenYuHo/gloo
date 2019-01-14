/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

#include "daiet.hpp"

#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>

#include "dpdk.h"
#include "common.hpp"
#include "utils.hpp"
#include "params.hpp"
#include "worker.hpp"
#include "ps.hpp"

using namespace std;

namespace daiet {

    uint32_t core_to_workers_ids[RTE_MAX_LCORE];
    struct dpdk_data dpdk_data;
    struct dpdk_params dpdk_par;
    daiet_params daiet_par;

    void port_init() {

        int ret;

        uint16_t nb_ports, tmp_portid;
        bool found_portid = false;

        struct rte_mempool *pool;

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
        rxm.ignore_offload_bitfield = 1;

        if (dev_info.rx_offload_capa & DEV_RX_OFFLOAD_IPV4_CKSUM) {
            rxm.offloads |= DEV_RX_OFFLOAD_IPV4_CKSUM;
            LOG_DEBUG("RX IPv4 checksum offload enabled");
        }

        if (dev_info.rx_offload_capa & DEV_RX_OFFLOAD_UDP_CKSUM) {
            rxm.offloads |= DEV_RX_OFFLOAD_UDP_CKSUM;
            LOG_DEBUG("RX UDP checksum offload enabled");
        }

        if (dev_info.rx_offload_capa & DEV_RX_OFFLOAD_CRC_STRIP) {
            rxm.offloads |= DEV_RX_OFFLOAD_CRC_STRIP;
            LOG_DEBUG("RX CRC stripped by the hw");
        }

        txm.mq_mode = ETH_MQ_TX_NONE;

        if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_IPV4_CKSUM) {
            txm.offloads |= DEV_TX_OFFLOAD_IPV4_CKSUM;
            LOG_DEBUG("TX IPv4 checksum offload enabled");
        }

        if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_UDP_CKSUM) {
            txm.offloads |= DEV_TX_OFFLOAD_UDP_CKSUM;
            LOG_DEBUG("TX UDP checksum offload enabled");
        }

        if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE) {
            txm.offloads |= DEV_TX_OFFLOAD_MBUF_FAST_FREE;
            LOG_DEBUG("Fast release of mbufs enabled");
        }

        port_conf.rxmode = rxm;
        port_conf.txmode = txm;
        //port_conf.link_speeds = ETH_LINK_SPEED_AUTONEG;
        //port_conf.lpbk_mode = 0; // Loopback operation mode disabled
        //port_conf.dcb_capability_en = 0; // DCB disabled

        /* FDIR
        port_conf.rx_adv_conf.rss_conf.rss_key = NULL;
        port_conf.rx_adv_conf.rss_conf.rss_hf = 0;

        port_conf.fdir_conf.mode = RTE_FDIR_MODE_PERFECT;
        port_conf.fdir_conf.pballoc = RTE_FDIR_PBALLOC_64K;
        port_conf.fdir_conf.status = RTE_FDIR_NO_REPORT_STATUS;
        port_conf.fdir_conf.drop_queue = 127;

        memset(&port_conf.fdir_conf.mask, 0x00, sizeof(struct rte_eth_fdir_masks));
        port_conf.fdir_conf.mask.dst_port_mask = 0xFFFF;
        */

        ret = rte_eth_dev_configure(dpdk_par.portid, 1, 1, &port_conf);
        if (ret < 0)
            LOG_FATAL("Cannot configure port: " + string(rte_strerror(ret)));

        // Fix for mlx5 driver ring size overflow
        dpdk_par.port_rx_ring_size = dev_info.rx_desc_lim.nb_max < 32768 ? dev_info.rx_desc_lim.nb_max : 32768;
        dpdk_par.port_tx_ring_size = dev_info.tx_desc_lim.nb_max < 32768 ? dev_info.tx_desc_lim.nb_max : 32768;

        // Check that numbers of Rx and Tx descriptors satisfy descriptors
        // limits from the ethernet device information, otherwise adjust
        // them to boundaries.
        ret = rte_eth_dev_adjust_nb_rx_tx_desc(dpdk_par.portid, &dpdk_par.port_rx_ring_size, &dpdk_par.port_tx_ring_size);
        if (ret < 0)
            LOG_FATAL("Cannot adjust number of descriptors: " + string(rte_strerror(ret)));

        //Get the port address
        rte_eth_macaddr_get(dpdk_par.portid, &port_eth_addr);

        // Init the buffer pool
        pool = rte_pktmbuf_pool_create("rx_pool", dpdk_par.pool_size, dpdk_par.pool_cache_size, 0, dpdk_data.pool_buffer_size, rte_socket_id());
        if (pool == NULL)
            LOG_FATAL("Cannot init mbuf pool: " + string(rte_strerror(rte_errno)));

        // init RX queue
        rx_conf = dev_info.default_rxconf;
        rx_conf.offloads = port_conf.rxmode.offloads;
        //rx_conf.rx_thresh.pthresh = 8;
        //rx_conf.rx_thresh.hthresh = 8;
        //rx_conf.rx_thresh.wthresh = 4;
        //rx_conf.rx_free_thresh = 64;
        //rx_conf.rx_drop_en = 0;

        ret = rte_eth_rx_queue_setup(dpdk_par.portid, 0, dpdk_par.port_rx_ring_size, rte_eth_dev_socket_id(dpdk_par.portid), &rx_conf, pool);
        if (ret < 0)
            LOG_FATAL("RX queue setup error: " + string(rte_strerror(ret)));

        // init TX queue on each port
        tx_conf = dev_info.default_txconf;
        tx_conf.txq_flags = ETH_TXQ_FLAGS_IGNORE;
        tx_conf.offloads = port_conf.txmode.offloads;
        //tx_conf.tx_thresh.pthresh = 36;
        //tx_conf.tx_thresh.hthresh = 0;
        //tx_conf.tx_thresh.wthresh = 0;
        //tx_conf.tx_free_thresh = 0;
        //tx_conf.tx_rs_thresh = 0;

        ret = rte_eth_tx_queue_setup(dpdk_par.portid, 0, dpdk_par.port_tx_ring_size, rte_eth_dev_socket_id(dpdk_par.portid), &tx_conf);
        if (ret < 0)
            LOG_FATAL("TX queue setup error: " + string(rte_strerror(ret)));

        // stats mapping
        if (dev_info.max_rx_queues > 1) {
            ret = rte_eth_dev_set_rx_queue_stats_mapping(dpdk_par.portid, 0, 0);
            if (ret < 0)
                LOG_ERROR("RX queue stats mapping error " + string(rte_strerror(ret)));
        }

        if (dev_info.max_tx_queues > 1) {
            ret = rte_eth_dev_set_tx_queue_stats_mapping(dpdk_par.portid, 0, 0);
            if (ret < 0)
                LOG_ERROR("TX queue stats mapping error " + string(rte_strerror(ret)));
        }

        // Start device
        ret = rte_eth_dev_start(dpdk_par.portid);
        if (ret < 0)
            LOG_FATAL("Error starting the port: " + string(rte_strerror(ret)));

        // Enable promiscuous mode
        //rte_eth_promiscuous_enable(dpdk_par.portid);

        LOG_DEBUG("Initialization ended. Port " + to_string(dpdk_par.portid) + " address: " + mac_to_str(port_eth_addr));

        check_port_link_status(dpdk_par.portid);
    }

    int master(DaietContext* dctx_ptr) {

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
            string eal_cmdline;

            force_quit = false;
            ps_stop = false;

            daiet_log = std::ofstream("daiet.log", std::ios::out);

            const char *buildString = "Compiled at " __DATE__ ", " __TIME__ ".";
            LOG_INFO (string(buildString));

            parse_parameters();

            // Set EAL log file
            FILE * dpdk_log_file;
            dpdk_log_file = fopen("dpdk.log", "w");
            if (dpdk_log_file == NULL) {
                LOG_ERROR("Failed to open log file: " + string(strerror(errno)));
            } else {
                ret = rte_openlog_stream(dpdk_log_file);
                if (ret < 0)
                    LOG_ERROR("Failed to open dpdk log stream");
            }

            // EAL cmd line
            eal_cmdline = "daiet -l " + dpdk_par.corestr + " --file-prefix " + dpdk_par.prefix + " " + dpdk_par.eal_options;
            vector<string> par_vec = split(eal_cmdline);

            int args_c = par_vec.size();
            char* args[args_c];
            char* args_ptr[args_c];

            for (vector<string>::size_type i = 0; i != par_vec.size(); i++) {
                args[i] = new char[par_vec[i].size() + 1];
                args_ptr[i] = args[i];
                strcpy(args[i], par_vec[i].c_str());
            }

/* mlockall has issues inside docker
#ifndef COLOCATED
            // This is causing some issue with the PS
            if (daiet_par.getMode() == "worker"){
                // Lock pages
                if (mlockall(MCL_CURRENT | MCL_FUTURE)) {
                    LOG_FATAL("mlockall() failed with error: " + string(strerror(rte_errno)));
                }
            }
#endif
*/
            // EAL init
            ret = rte_eal_init(args_c, args);
            if (ret < 0)
                LOG_FATAL("EAL init failed: " + string(rte_strerror(rte_errno)));

            // Count cores/workers
            uint32_t n_lcores = 0, lcore_id, wid = 0;
            for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
                if (rte_lcore_is_enabled(lcore_id) != 0) {

                    core_to_workers_ids[lcore_id] = wid;
                    wid++;
                    n_lcores++;
                }
            }

            num_workers_threads = wid;
            LOG_INFO("Number of worker threads: " + to_string(num_workers_threads));

            // Estimate CPU frequency
            hz = rte_get_timer_hz();
            hz_str << setprecision(3) << (double) hz / 1000000000;
            LOG_INFO("CPU freq: " + hz_str.str() + " GHz");

            // Initialize port
            port_init();

            // Initialize workers/PSs
#ifndef COLOCATED
            worker_setup();
#else
            worker_setup();
            ps_setup();
#endif

            // Check state of slave cores
            RTE_LCORE_FOREACH_SLAVE(lcore_id)
            {
                if (rte_eal_get_lcore_state(lcore_id) != WAIT)
                    LOG_FATAL("Core " + to_string(lcore_id) + " in state " + to_string(rte_eal_get_lcore_state(lcore_id)));
            }

            begin_cpu = clock();
            begin = rte_get_timer_cycles();

            // Launch functions on slave cores
            RTE_LCORE_FOREACH_SLAVE(lcore_id) {

#ifndef COLOCATED
                 rte_eal_remote_launch(worker, dctx_ptr, lcore_id);
#else
                 // FIXME Equal number of workers and PSs
                 rte_eal_remote_launch(ps, NULL, lcore_id);
#endif
            }

            // Launch function on master cores
            worker(dctx_ptr);

            // Join worker/ps threads
            RTE_LCORE_FOREACH_SLAVE(lcore_id) {

                ret = rte_eal_wait_lcore(lcore_id);
                if (unlikely(ret < 0)) {
                    LOG_DEBUG("Core " + to_string(lcore_id) + " returned " + to_string(ret));
                }
            }

            elapsed_secs = ((double) (rte_get_timer_cycles() - begin)) / hz;
            elapsed_secs_cpu = double(clock() - begin_cpu) / CLOCKS_PER_SEC;

            // Print stats
            print_dev_stats(dpdk_par.portid);
            //print_dev_xstats(dpdk_par.portid);

#ifndef COLOCATED

            LOG_INFO("TX " + to_string(pkt_stats.w_tx));
            LOG_INFO("RX " + to_string(pkt_stats.w_rx));
#else
            LOG_INFO("Worker TX " + to_string(pkt_stats.w_tx));
            LOG_INFO("Worker RX " + to_string(pkt_stats.w_rx));
            LOG_INFO("PS TX " + to_string(pkt_stats.p_tx));
            LOG_INFO("PS RX " + to_string(pkt_stats.p_rx));
#endif

#ifdef TIMERS
            LOG_INFO("Timeouts " + to_string(pkt_stats.w_timeouts));
#endif

            elapsed_secs_str << fixed << setprecision(6) << elapsed_secs;
            elapsed_secs_cpu_str << fixed << setprecision(6) << elapsed_secs_cpu;

            LOG_INFO("Time elapsed: " + elapsed_secs_str.str() + " seconds (CPU time: " + elapsed_secs_cpu_str.str() + " seconds)");

            // Cleanup

            LOG_DEBUG("Closing port...");
            rte_eth_dev_stop(dpdk_par.portid);
            rte_eth_dev_close(dpdk_par.portid);
            LOG_DEBUG("Port closed");

#ifndef COLOCATED
            worker_cleanup();
#else
            worker_cleanup();
            ps_cleanup();
#endif


            // EAL cleanup
            ret = rte_eal_cleanup(); // Ignore warning
            if (ret < 0)
                LOG_FATAL("EAL cleanup failed!");

            fclose(dpdk_log_file);
            daiet_log.close();

            for (vector<string>::size_type i = 0; i != par_vec.size(); i++) {
                delete[] args_ptr[i];
            }

            return 0;

        } catch (exception& e) {
            cerr << e.what() << endl;
            return -1;
        }
    }
} // End namespace
