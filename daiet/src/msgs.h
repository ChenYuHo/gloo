/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

#pragma once

namespace daiet {

#ifdef __cplusplus
    extern "C" {
#endif

        /**
         * DAIET Header
         */
        struct daiet_hdr {
                uint8_t num_aggregated;
                uint16_t tno; // tensor number
                uint32_t total_num_msgs;
                uint32_t tsi; /**< tensor start index */
                uint16_t pool_index; /**< pool index */
        }__attribute__((__packed__));

        struct entry_hdr {
                int32_t upd; /**< vector entry */
        }__attribute__((__packed__));

#ifdef __cplusplus
    }
#endif

}  // End namespace
