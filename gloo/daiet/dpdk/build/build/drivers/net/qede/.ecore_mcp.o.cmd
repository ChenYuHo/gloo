cmd_ecore_mcp.o = gcc -Wp,-MD,./.ecore_mcp.o.d.tmp  -m64 -pthread  -march=native -DRTE_MACHINE_CPUFLAG_SSE -DRTE_MACHINE_CPUFLAG_SSE2 -DRTE_MACHINE_CPUFLAG_SSE3 -DRTE_MACHINE_CPUFLAG_SSSE3 -DRTE_MACHINE_CPUFLAG_SSE4_1 -DRTE_MACHINE_CPUFLAG_SSE4_2 -DRTE_MACHINE_CPUFLAG_AES -DRTE_MACHINE_CPUFLAG_PCLMULQDQ -DRTE_MACHINE_CPUFLAG_AVX -DRTE_MACHINE_CPUFLAG_RDRAND -DRTE_MACHINE_CPUFLAG_AVX2  -I/home/asapio/Documents/workspace/DAIET-ML/PktGen/dpdk/build/include -include /home/asapio/Documents/workspace/DAIET-ML/PktGen/dpdk/build/include/rte_config.h -O3 -W -Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wold-style-definition -Wpointer-arith -Wcast-align -Wnested-externs -Wcast-qual -Wformat-nonliteral -Wformat-security -Wundef -Wwrite-strings -Wdeprecated -Wno-unused-parameter -Wno-sign-compare -Wno-missing-prototypes -Wno-cast-qual -Wno-unused-function -Wno-unused-variable -Wno-strict-aliasing -Wno-missing-prototypes -Wno-unused-value -Wno-format-nonliteral -Wno-unused-but-set-variable -Wno-missing-declarations -Wno-maybe-uninitialized -Wno-strict-prototypes -Wno-shift-negative-value   -o ecore_mcp.o -c /home/asapio/Documents/workspace/DAIET-ML/PktGen/dpdk/drivers/net/qede/base/ecore_mcp.c 
