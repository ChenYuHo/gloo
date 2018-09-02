/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

#pragma once

#include "params.hpp"
#include "PsMsgs.hpp"
#include "dpdk.h"

namespace daiet {

    extern rte_atomic32_t* sent_message_counters;

#if SAVE_LATENCIES
    extern uint64_t* latencies;
#endif

    void msg_setup(ClientSendOpLogMsg*);
    void build_pkt(rte_mbuf*, unsigned, uint16_t, uint32_t);
    void worker_setup();
    void worker_cleanup();
    int worker(void*);

    __rte_always_inline bool is_daiet_pkt(struct ether_hdr* eth, uint16_t size, ClientSendOpLogMsg* msg) {

        int idx;
        uint16_t etherType;
        struct ipv4_hdr* ip_hdr;
        struct udp_hdr* udp_hdr;

        idx = sizeof(struct ether_hdr);
        etherType = rte_be_to_cpu_16(eth->ether_type);

        if (etherType == ETHER_TYPE_IPv4 && size >= idx + sizeof(struct ipv4_hdr)) {

            idx += sizeof(struct ipv4_hdr);
            ip_hdr = (struct ipv4_hdr *) (&eth[1]);

            if (ip_hdr->next_proto_id == IPPROTO_UDP && size >= idx + sizeof(struct udp_hdr)) {
                idx += sizeof(struct udp_hdr);
                udp_hdr = (struct udp_hdr *) (&ip_hdr[1]);
                uint16_t dst_port_be = udp_hdr->dst_port;

                if ((dst_port_be == daiet_par.getPsPortBe() || dst_port_be == daiet_par.getWorkerPortBe()) && size >= idx + EXT_HEADER_SIZE + INT_HEADER_SIZE) {

                    msg->Reset((void*) (&udp_hdr[1]));

                    return true;
                }
            }
        }
        return false;
    }

    __rte_always_inline bool is_daiet_pkt(struct ether_hdr* eth, uint16_t size, bool& to_ps) {

        int idx;
        uint16_t etherType;
        struct ipv4_hdr* ip_hdr;
        struct udp_hdr* udp_hdr;

        idx = sizeof(struct ether_hdr);
        etherType = rte_be_to_cpu_16(eth->ether_type);

        if (etherType == ETHER_TYPE_IPv4 && size >= idx + sizeof(struct ipv4_hdr)) {

            idx += sizeof(struct ipv4_hdr);
            ip_hdr = (struct ipv4_hdr *) (&eth[1]);

            if (ip_hdr->next_proto_id == IPPROTO_UDP && size >= idx + sizeof(struct udp_hdr)) {
                idx += sizeof(struct udp_hdr);
                udp_hdr = (struct udp_hdr *) (&ip_hdr[1]);
                uint16_t dst_port_be = udp_hdr->dst_port;

                if (dst_port_be == daiet_par.getPsPortBe()) {
                    to_ps = true;
                } else if (dst_port_be == daiet_par.getWorkerPortBe()) {
                    to_ps = false;
                } else {
                    return false;
                }

                if (size >= idx + EXT_HEADER_SIZE + INT_HEADER_SIZE) {
                    return true;
                }
            }
        }
        return false;
    }
}
