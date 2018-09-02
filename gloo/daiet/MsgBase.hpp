/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

#pragma once

#include "MemBlock.hpp"

namespace daiet {

    struct MsgBase {
        public:
            MsgBase() {
            }

            explicit MsgBase(void *msg) {
                mem_.Reset(msg);
            }

            virtual ~MsgBase() {
            }

            uint8_t *get_mem() {
                return mem_.get_mem();
            }

            void *ReleaseMem() {
                return mem_.Release();
            }

        protected:
            virtual void InitMsg() {
            }

            MemBlock mem_;
    };

    struct NumberedMsg: public MsgBase {
        public:
            NumberedMsg() {
            }

            explicit NumberedMsg(void *msg) :
                    MsgBase(msg) {
            }

            uint8_t &get_seq_num() {
                return *(reinterpret_cast<uint8_t*>(mem_.get_mem()));
            }

            virtual size_t get_size() {
                return sizeof(uint8_t);
            }
    };

    struct ArbitrarySizedMsg: public NumberedMsg {
        public:
            ArbitrarySizedMsg() {
            }

            explicit ArbitrarySizedMsg(int32_t avai_size) {
                mem_.Alloc(get_header_size() + avai_size);
                InitMsg(avai_size);
            }

            explicit ArbitrarySizedMsg(void *msg) :
                    NumberedMsg(msg) {
            }

            virtual size_t get_header_size() {
                return NumberedMsg::get_size() + sizeof(size_t);
            }

            /**
             *  Get size field
             */
            size_t &get_avai_size() {
                return *(reinterpret_cast<size_t*>(mem_.get_mem() + NumberedMsg::get_size()));
            }

            // won't be available until the object is constructed
            virtual size_t get_size() {
                return get_header_size() + get_avai_size();
            }

        protected:
            virtual void InitMsg(int32_t avai_size) {
                NumberedMsg::InitMsg();
                get_avai_size() = avai_size;
            }
    };
} // End namespace
