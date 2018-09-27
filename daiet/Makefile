# DAIET project
# author: amedeo.sapio@kaust.edu.sa

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

ifeq ($(DAIET_PATH),)
DAIET_PATH = $(shell pwd)
export DAIET_PATH
endif

# Default target, can be overriden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc

include $(RTE_SDK)/mk/rte.vars.mk

# binary name
LIB = libdaiet.a

# all source are stored in SRCS-y
SRCS-y := $(shell find ${DAIET_PATH} -maxdepth 1 -name "*.cpp")

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
CXXFLAGS += -I ${DAIET_PATH}/.. 
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
CXXFLAGS += -std=c++11 -g
else
CXXFLAGS += -std=c++11 -O3 -fPIC
endif

include $(RTE_SDK)/mk/rte.extlib.mk

distclean: clean
	$(RM) -r build
