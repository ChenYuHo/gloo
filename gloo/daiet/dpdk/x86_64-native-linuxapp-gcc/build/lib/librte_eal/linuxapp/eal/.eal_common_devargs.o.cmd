cmd_eal_common_devargs.o = gcc -Wp,-MD,./.eal_common_devargs.o.d.tmp  -m64 -pthread  -march=native -DRTE_MACHINE_CPUFLAG_SSE -DRTE_MACHINE_CPUFLAG_SSE2 -DRTE_MACHINE_CPUFLAG_SSE3 -DRTE_MACHINE_CPUFLAG_SSSE3 -DRTE_MACHINE_CPUFLAG_SSE4_1 -DRTE_MACHINE_CPUFLAG_SSE4_2 -DRTE_MACHINE_CPUFLAG_AES -DRTE_MACHINE_CPUFLAG_PCLMULQDQ -DRTE_MACHINE_CPUFLAG_AVX -DRTE_MACHINE_CPUFLAG_RDRAND -DRTE_MACHINE_CPUFLAG_AVX2  -I/home/asapio/Documents/workspace/DAIET-ML/PktGen/dpdk/x86_64-native-linuxapp-gcc/include -include /home/asapio/Documents/workspace/DAIET-ML/PktGen/dpdk/x86_64-native-linuxapp-gcc/include/rte_config.h -DALLOW_EXPERIMENTAL_API -I/home/asapio/Documents/workspace/DAIET-ML/PktGen/dpdk/lib/librte_eal/linuxapp/eal/include -I/home/asapio/Documents/workspace/DAIET-ML/PktGen/dpdk/lib/librte_eal/common -I/home/asapio/Documents/workspace/DAIET-ML/PktGen/dpdk/lib/librte_eal/common/include -W -Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wold-style-definition -Wpointer-arith -Wcast-align -Wnested-externs -Wcast-qual -Wformat-nonliteral -Wformat-security -Wundef -Wwrite-strings -Wdeprecated -O3    -o eal_common_devargs.o -c /home/asapio/Documents/workspace/DAIET-ML/PktGen/dpdk/lib/librte_eal/common/eal_common_devargs.c 
