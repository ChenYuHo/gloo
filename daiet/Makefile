# DAIET project
# author: amedeo.sapio@kaust.edu.sa

ifeq ($(DAIET_PATH),)
DAIET_PATH = $(shell pwd)
export DAIET_PATH
endif

RTE_SDK = ${DAIET_PATH}/lib/dpdk
RTE_TARGET = build

include $(RTE_SDK)/mk/rte.vars.mk

# App name
APPNAME = daiet

# binary name
LIB = libdaiet.a

# install directory
PREFIX = /usr/local

# all source are stored in SRCS-y
SRCS-y := $(shell find ${DAIET_PATH}/src -maxdepth 1 -name "*.cpp")
HDRS := $(shell find ${DAIET_PATH}/src -maxdepth 1 -name "*.hpp" -o -name "*.h")

CXXFLAGS += $(WERROR_FLAGS) -std=c++11 -fPIC -I ${DAIET_PATH}/.. -mavx2 -fabi-version=0 -mfma
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
CXXFLAGS += -DDEBUG -g -O0
else
CXXFLAGS += -O3
endif

include $(RTE_SDK)/mk/rte.extlib.mk

distclean: clean
	$(RM) -r build


.PHONY: install
install: all 
	mkdir -p $(DESTDIR)$(PREFIX)/lib
	mkdir -p $(DESTDIR)$(PREFIX)/include/$(APPNAME)
	cp $(RTE_OUTPUT)/lib/$(LIB) $(DESTDIR)$(PREFIX)/lib/$(LIB)
	cp $(HDRS) $(DESTDIR)$(PREFIX)/include/$(APPNAME)

.PHONY: uninstall
uninstall:
	$(RM) $(DESTDIR)$(PREFIX)/lib/$(LIB)
	$(RM) -r $(DESTDIR)$(PREFIX)/include/$(APPNAME)
