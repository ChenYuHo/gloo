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

    static __rte_always_inline struct daiet_hdr * is_daiet_pkt_to_ps(struct ether_hdr* eth_hdr, uint16_t size) {

        int idx;
        uint16_t etherType;
        struct ipv4_hdr* ip_hdr;
        struct udp_hdr* udp_hdr;

        idx = sizeof(struct ether_hdr);
        etherType = rte_be_to_cpu_16(eth_hdr->ether_type);

        if (etherType == ETHER_TYPE_IPv4 && size >= idx + sizeof(struct ipv4_hdr)) {

            idx += sizeof(struct ipv4_hdr);
            ip_hdr = (struct ipv4_hdr *) (eth_hdr + 1);

            if (ip_hdr->next_proto_id == IPPROTO_UDP && size >= idx + sizeof(struct udp_hdr)) {
                idx += sizeof(struct udp_hdr);
                udp_hdr = (struct udp_hdr *) (ip_hdr + 1);

                if (udp_hdr->dst_port == daiet_par.getPsPortBe() && size >= idx + sizeof(struct daiet_hdr)) {

                    return (struct daiet_hdr *) (udp_hdr + 1);
                }
            }
        }
        return NULL;
    }

    static __rte_always_inline void ps_msg_setup(struct daiet_hdr * daiet, uint16_t pool_index, uint32_t tsi) {

        struct entry_hdr *entry;

        daiet->tsi = rte_cpu_to_be_32(tsi);
        //daiet->pool_index = rte_cpu_to_be_16(pool_index);

        entry = (struct entry_hdr *) (daiet + 1);
        for (uint32_t i = 0; i < daiet_par.getNumUpdates(); i++) {
            entry->upd = rte_cpu_to_be_32(rte_atomic32_exchange(reinterpret_cast<volatile uint32_t*>(&(ps_aggregated_messages[i][pool_index].cnt)), 0));
            entry++;
        }
    }

    /* Returns true if the aggregation for the offset is complete */
    static __rte_always_inline bool ps_aggregate_message(struct daiet_hdr* daiet, uint32_t be_src_ip, struct ether_addr src_mac, uint16_t pool_index) {

        struct entry_hdr * entry = (struct entry_hdr *) (daiet + 1);
        for (uint32_t i = 0; i < daiet_par.getNumUpdates(); i++) {
            rte_atomic32_add(&ps_aggregated_messages[i][pool_index], rte_be_to_cpu_32(entry->upd));
            entry++;
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

        if (rte_atomic32_dec_and_test(&ps_received_message_counters[pool_index])) {
            rte_atomic32_set(&ps_received_message_counters[pool_index], daiet_par.getNumWorkers());
            return true;
        }

        return false;
    }

    void ps_setup() {
        ps_aggregated_messages = new rte_atomic32_t*[daiet_par.getNumUpdates()];
        for (int i = 0; i < daiet_par.getNumUpdates(); i++) {
            ps_aggregated_messages[i] = new rte_atomic32_t[daiet_par.getMaxNumPendingMessages()];

            for (int j = 0; j < daiet_par.getMaxNumPendingMessages(); j++) {
                rte_atomic32_init(&ps_aggregated_messages[i][j]);
            }
        }

        ps_received_message_counters = new rte_atomic32_t[daiet_par.getMaxNumPendingMessages()];
        for (int i = 0; i < daiet_par.getMaxNumPendingMessages(); i++) {
            rte_atomic32_init(&ps_received_message_counters[i]);
            rte_atomic32_set(&ps_received_message_counters[i], daiet_par.getNumWorkers());
        }

        rte_rwlock_init(&ps_workers_ip_to_mac_lock);
    }

    void ps_cleanup() {
        delete[] ps_received_message_counters;

        for (int i = 0; i < daiet_par.getNumUpdates(); i++) {
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
        struct ipv4_hdr * ip;
        struct udp_hdr * udp;
        struct daiet_hdr* daiet;
        uint32_t tsi = 0;
        uint16_t pool_index = 0;

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

                rte_prefetch0(rte_pktmbuf_mtod(m, void *));
                eth = rte_pktmbuf_mtod(m, struct ether_hdr *);

#if !COLOCATED
                daiet = is_daiet_pkt_to_ps(eth, m->data_len);
                if (likely(daiet!=NULL)) {
#else
                    daiet = (struct daiet_hdr *) ((uint8_t *) (eth+1) + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));
#endif

                    rte_atomic64_inc(&pkt_stats.p_rx);
                    ip = (struct ipv4_hdr *) (eth + 1);
                    udp = (struct udp_hdr *) (ip + 1);

                    pool_index = rte_be_to_cpu_16(daiet->pool_index);
                    tsi = rte_be_to_cpu_32(daiet->tsi);

                    if (ps_aggregate_message(daiet, ip->src_addr, eth->s_addr, pool_index)) {

                        // Checksum offload
                        m->l2_len = sizeof(struct ether_hdr);
                        m->l3_len = sizeof(struct ipv4_hdr);
                        m->ol_flags |= daiet_par.getTxFlags();

                        // Set src MAC
                        ether_addr_copy(&(eth->d_addr), &(eth->s_addr));

                        // Set src IP
                        ip->hdr_checksum = 0;
                        ip->src_addr = ip->dst_addr;

                        // Swap ports
                        swap((uint16_t&) (udp->dst_port), (uint16_t&) (udp->src_port));
                        udp->dgram_cksum = rte_ipv4_phdr_cksum(ip, m->ol_flags);

                        ps_msg_setup(daiet, pool_index, tsi);

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
                            ip = (struct ipv4_hdr *) (eth + 1);
                            ip->dst_addr = worker_addr.first;

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

        return 0;
    }
}
