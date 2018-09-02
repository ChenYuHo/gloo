/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

#pragma once

#include "MsgBase.hpp"

namespace daiet {

    struct ClientSendOpLogMsg: public NumberedMsg {
        public:
            explicit ClientSendOpLogMsg(int32_t avai_size) {
                mem_.Alloc(get_header_size() + avai_size);
            }

            //TODO this is silly. Get the previous reference, get its pointer and add only the last value

            explicit ClientSendOpLogMsg(void *msg) :
        NumberedMsg(msg) {
            }

            ~ClientSendOpLogMsg() {
                this->ReleaseMem(); // Do not Free
            }

            void Reset(void *msg) {
                this->ReleaseMem(); // Do not Free
                mem_.Reset(msg);
            }

            size_t get_header_size() {
                return NumberedMsg::get_size();
            }

            void *get_data() {
                return mem_.get_mem() + get_header_size();
            }

            /**
             * Packet size
             */

            int32_t &get_row_id() {
                return *(reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(get_data())));
            }

            uint8_t &get_num_updates() {
                // Double reinterpret_cast<uint8_t*>
                return *(reinterpret_cast<uint8_t*>(get_data()) + sizeof(int32_t));
            }

            uint16_t &get_offset_position() {
                return *(reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(get_data()) + sizeof(int32_t) + sizeof(uint8_t)));
            }

            int32_t &get_first_entry() {
                return *(reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(get_data()) + sizeof(int32_t) + sizeof(uint8_t) + sizeof(uint16_t)));
            }

            uint8_t *get_first_entry_ptr() {
                return reinterpret_cast<uint8_t*>(get_data()) + sizeof(int32_t) + sizeof(uint8_t) + sizeof(uint16_t);
            }

        protected:

    };

}  // End namespace
