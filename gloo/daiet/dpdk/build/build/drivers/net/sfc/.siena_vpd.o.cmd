cmd_siena_vpd.o = gcc -Wp,-MD,./.siena_vpd.o.d.tmp  -m64 -pthread  -march=native -DRTE_MACHINE_CPUFLAG_SSE -DRTE_MACHINE_CPUFLAG_SSE2 -DRTE_MACHINE_CPUFLAG_SSE3 -DRTE_MACHINE_CPUFLAG_SSSE3 -DRTE_MACHINE_CPUFLAG_SSE4_1 -DRTE_MACHINE_CPUFLAG_SSE4_2 -DRTE_MACHINE_CPUFLAG_AES -DRTE_MACHINE_CPUFLAG_PCLMULQDQ -DRTE_MACHINE_CPUFLAG_AVX -DRTE_MACHINE_CPUFLAG_RDRAND -DRTE_MACHINE_CPUFLAG_AVX2  -I/home/asapio/Documents/workspace/DAIET-ML/PktGen/dpdk/build/include -include /home/asapio/Documents/workspace/DAIET-ML/PktGen/dpdk/build/include/rte_config.h -DALLOW_EXPERIMENTAL_API -I/home/asapio/Documents/workspace/DAIET-ML/PktGen/dpdk/drivers/net/sfc/base/ -I/home/asapio/Documents/workspace/DAIET-ML/PktGen/dpdk/drivers/net/sfc -O3 -W -Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wold-style-definition -Wpointer-arith -Wcast-align -Wnested-externs -Wcast-qual -Wformat-nonliteral -Wformat-security -Wundef -Wwrite-strings -Wdeprecated -Wno-strict-aliasing -Wextra -Wdisabled-optimization -Waggregate-return -Wnested-externs -Wno-sign-compare -Wno-unused-parameter -Wno-unused-variable -Wno-empty-body -Wno-unused-but-set-variable   -o siena_vpd.o -c /home/asapio/Documents/workspace/DAIET-ML/PktGen/dpdk/drivers/net/sfc/base/siena_vpd.c 
