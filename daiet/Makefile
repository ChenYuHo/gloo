# DAIET project
# author: amedeo.sapio@kaust.edu.sa

ifeq ($(DAIET_PATH),)
DAIET_PATH = $(shell pwd)
export DAIET_PATH
endif

RTE_SDK = ${DAIET_PATH}/lib/dpdk
RTE_TARGET = build

include $(RTE_SDK)/mk/rte.vars.mk

# binary name
LIB = libdaiet.a

# all source are stored in SRCS-y
SRCS-y := $(shell find ${DAIET_PATH}/src -maxdepth 1 -name "*.cpp")

CFLAGS += $(WERROR_FLAGS)
CXXFLAGS += -std=c++11 -fPIC -I ${DAIET_PATH}/.. -mavx2 -fabi-version=0 -mfma
LDFLAGS += -lstdc++ -l boost_program_options

ifeq ($(colocated),y)
$(info "Colocated")
CXXFLAGS += -DCOLOCATED
endif

ifeq ($(save_latencies),y)
$(info "Save_latencies")
CXXFLAGS += -DSAVE_LATENCIES
endif

ifeq ($(debug),y)
$(info "Debug")
CFLAGS += -O0
CXXFLAGS += -DDEBUG -g -O0
else
CFLAGS += -O3
CXXFLAGS += -O3
endif

include $(RTE_SDK)/mk/rte.extlib.mk

distclean: clean
	$(RM) -r build
