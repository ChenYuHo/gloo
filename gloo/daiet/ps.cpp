/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

#include "ps.hpp"

namespace daiet {

    rte_rwlock_t ps_workers_ip_to_mac_lock;
    map<uint32_t, struct ether_addr> ps_workers_ip_to_mac;
    rte_atomic32_t** ps_aggregated_messages;
    rte_atomic32_t* ps_received_message_counters;

    void ps_msg_setup(ClientSendOpLogMsg* new_msg, uint16_t offset, uint32_t seq_num) {

        int32_t value_n;
        new_msg->get_num_updates() = daiet_par.getNumUpdates();
//        new_msg->get_offset_position() = rte_cpu_to_be_16(offset);
//        new_msg->get_ack_num() = new_msg->get_seq_num();
        new_msg->get_seq_num() = rte_cpu_to_be_32(seq_num);

        // Size
//        new_msg->get_avai_size() = INT_HEADER_SIZE + (daiet_par.getNumUpdates() * 2 * daiet_par.getUpdateSize());
        uint8_t* row_ptr = new_msg->get_first_entry_ptr();

        for (uint32_t i = 0; i < daiet_par.getNumUpdates() * 2; i++) {
            // Write update
            value_n = rte_atomic32_exchange(reinterpret_cast<volatile uint32_t*>(&(ps_aggregated_messages[i][offset].cnt)), 0);
            value_n = rte_cpu_to_be_32(value_n);

            rte_memcpy((void*) row_ptr, &value_n, daiet_par.getUpdateSize());
            row_ptr += daiet_par.getUpdateSize();
        }
    }

    /* Returns true if the aggregation for the offset is complete */
    bool ps_aggregate_message(ClientSendOpLogMsg* msg, uint32_t be_src_ip, struct ether_addr src_mac, uint16_t& offset) {

        offset = rte_be_to_cpu_16(msg->get_offset_position());
        uint8_t* row_ptr = msg->get_first_entry_ptr();

        for (int i = 0; i < msg->get_num_updates() * 2; i++) {
            rte_atomic32_add(&ps_aggregated_messages[i][offset], rte_be_to_cpu_32(*(reinterpret_cast<int32_t*>(row_ptr))));
            row_ptr += sizeof(int32_t);
        }

        // READ LOCK
        rte_rwlock_read_lock(&ps_workers_ip_to_mac_lock);
        if (unlikely(ps_workers_ip_to_mac.size() < daiet_par.getNumWorkers())) {

            // READ UNLOCK
            rte_rwlock_read_unlock(&ps_workers_ip_to_mac_lock);

            // WRITE LOCK
            rte_rwlock_write_lock(&ps_workers_ip_to_mac_lock);

            if (ps_workers_ip_to_mac.find(be_src_ip) == ps_workers_ip_to_mac.end()) {

                char ipstring[INET_ADDRSTRLEN];

                if (inet_ntop(AF_INET, &be_src_ip, ipstring, INET_ADDRSTRLEN) == NULL) {
                    LOG_FATAL("Wrong IP: error " + to_string(errno));
                }

                LOG_INFO("Worker: " + string(ipstring) + " " + mac_to_str(src_mac));

                ps_workers_ip_to_mac[be_src_ip] = src_mac;
            }

            // WRITE UNLOCK
            rte_rwlock_write_unlock(&ps_workers_ip_to_mac_lock);

        } else {
            // READ UNLOCK
            rte_rwlock_read_unlock(&ps_workers_ip_to_mac_lock);
        }

        if (rte_atomic32_dec_and_test (&ps_received_message_counters[offset])) {
            rte_atomic32_set(&ps_received_message_counters[offset], daiet_par.getNumWorkers());
            return true;
        }

        return false;
    }

    void ps_setup() {
        ps_aggregated_messages = new rte_atomic32_t*[daiet_par.getNumUpdates() * 2];
        for (int i = 0; i < daiet_par.getNumUpdates() * 2; i++) {
            ps_aggregated_messages[i] = new rte_atomic32_t[daiet_par.getMaxNumPendingMessages()];

            for (int j = 0; j < daiet_par.getMaxNumPendingMessages(); j++) {
                rte_atomic32_init (&ps_aggregated_messages[i][j]);
            }
        }

        ps_received_message_counters = new rte_atomic32_t[daiet_par.getMaxNumPendingMessages()];
        for (int i = 0; i < daiet_par.getMaxNumPendingMessages(); i++) {
            rte_atomic32_init (&ps_received_message_counters[i]);
            rte_atomic32_set(&ps_received_message_counters[i], daiet_par.getNumWorkers());
        }

        rte_rwlock_init(&ps_workers_ip_to_mac_lock);
    }

    void ps_cleanup() {
        delete[] ps_received_message_counters;

        for (int i = 0; i < daiet_par.getNumUpdates() * 2; i++) {
            delete[] ps_aggregated_messages[i];
        }
        delete[] ps_aggregated_messages;
    }

    int ps(__attribute__((unused)) void*) {

        int ret;
        uint ind = 0;

        unsigned lcore_id;
        unsigned nb_rx = 0, j = 0;

        uint32_t worker_id;

        uint max_num_msgs = daiet_par.getMaxNumMsgs();

        struct rte_mbuf **pkts_burst;
        struct rte_mbuf* m;
        struct rte_mbuf* clone;

        struct ether_hdr* eth;
        struct ipv4_hdr * ip_hdr;
        struct udp_hdr * udp_hdr;
        ClientSendOpLogMsg* msg = new ClientSendOpLogMsg((void*) NULL);
        uint32_t seq_num = 0;
        uint16_t offset = 0;

        // Get core ID
        lcore_id = rte_lcore_id();
        worker_id = core_to_workers_ids[lcore_id];
        LOG_DEBUG("PS core: " + to_string(lcore_id) + " worker id: " + to_string(worker_id));

        pkts_burst = (rte_mbuf **) rte_malloc_socket(NULL, dpdk_par.burst_size_worker * sizeof(struct rte_mbuf*), RTE_CACHE_LINE_SIZE, rte_socket_id());
        if (pkts_burst == NULL)
            LOG_FATAL("PS thread: cannot allocate pkts burst");

        while (!force_quit && !ps_stop) {

            // Read packet from RX queues
            nb_rx = rte_ring_dequeue_burst(dpdk_data.p_ring_rx, (void **) pkts_burst, dpdk_par.burst_size_worker, NULL);
            for (j = 0; j < nb_rx; j++) {

                m = pkts_burst[j];

                rte_prefetch0 (rte_pktmbuf_mtod(m, void *));eth
                = rte_pktmbuf_mtod(m, struct ether_hdr *);

#if COLOCATED
                msg->Reset((void*) ((uint8_t *) (&eth[1]) + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr)));
#else
                if (likely(is_daiet_pkt(eth, m->data_len, msg))) {
#endif

                    rte_atomic64_inc(&pkt_stats.p_rx);
                    ip_hdr = (struct ipv4_hdr *) (&eth[1]);
                    udp_hdr = (struct udp_hdr *) (&ip_hdr[1]);

                    if (ps_aggregate_message(msg, ip_hdr->src_addr, eth->s_addr, offset)) {

                        ps_msg_setup(msg, offset, seq_num);
                        seq_num = (seq_num + 1) % daiet_par.getMaxSeqNum();

                        // Swap ports
                        swap((uint16_t&) (udp_hdr->dst_port), (uint16_t&) (udp_hdr->src_port));

                        // Set src IP
                        ip_hdr->src_addr = ip_hdr->dst_addr;

                        // Set src MAC
                        ether_addr_copy(&(eth->d_addr), &(eth->s_addr));

                        // READ LOCK
                        rte_rwlock_read_lock(&ps_workers_ip_to_mac_lock);
                        for (auto const& worker_addr : ps_workers_ip_to_mac) {

                            // Clone packet
                            clone = rte_pktmbuf_alloc(dpdk_data.pool);
                            if (clone == NULL)
                                LOG_FATAL("Cannot allocate clone pkt");

                            deep_copy_single_segment_pkt(clone, m);

                            eth = rte_pktmbuf_mtod(clone, struct ether_hdr *);

                            // Set dst MAC
                            ether_addr_copy(&(worker_addr.second), &(eth->d_addr));

                            // Set dst IP
                            ip_hdr = (struct ipv4_hdr *) (&eth[1]);
                            ip_hdr->dst_addr = worker_addr.first;

                            // Send packet
                            ret = rte_ring_enqueue(dpdk_data.p_ring_tx, clone);
                            if (ret < 0)
                                LOG_FATAL("Cannot enqueue one packet");

                            rte_atomic64_inc(&pkt_stats.p_tx);
                        }
                        // READ UNLOCK
                        rte_rwlock_read_unlock(&ps_workers_ip_to_mac_lock);

                        // Free original packet
                        rte_pktmbuf_free(m);

                        if (unlikely(rte_atomic64_read(&pkt_stats.p_tx) >= max_num_msgs))
                            ps_stop = true;
                    } else {
                        // Free original packet
                        rte_pktmbuf_free(m);
                    }
#if !COLOCATED
                } else {
                    // Free original packet
                    rte_pktmbuf_free(m);
                }
#endif
            }
        }

        // Cleanup
        rte_free(pkts_burst);

        delete msg;
        msg = 0;

        return 0;
    }
}
