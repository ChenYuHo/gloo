cmd_ark_mpu.o = gcc -Wp,-MD,./.ark_mpu.o.d.tmp  -m64 -pthread  -march=native -DRTE_MACHINE_CPUFLAG_SSE -DRTE_MACHINE_CPUFLAG_SSE2 -DRTE_MACHINE_CPUFLAG_SSE3 -DRTE_MACHINE_CPUFLAG_SSSE3 -DRTE_MACHINE_CPUFLAG_SSE4_1 -DRTE_MACHINE_CPUFLAG_SSE4_2 -DRTE_MACHINE_CPUFLAG_AES -DRTE_MACHINE_CPUFLAG_PCLMULQDQ -DRTE_MACHINE_CPUFLAG_AVX -DRTE_MACHINE_CPUFLAG_RDRAND -DRTE_MACHINE_CPUFLAG_AVX2  -I/home/asapio/Documents/workspace/DAIET-ML/PktGen/dpdk/build/include -include /home/asapio/Documents/workspace/DAIET-ML/PktGen/dpdk/build/include/rte_config.h -O3 -I./ -W -Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wold-style-definition -Wpointer-arith -Wcast-align -Wnested-externs -Wcast-qual -Wformat-nonliteral -Wformat-security -Wundef -Wwrite-strings -Wdeprecated -Werror    -o ark_mpu.o -c /home/asapio/Documents/workspace/DAIET-ML/PktGen/dpdk/drivers/net/ark/ark_mpu.c 
