cmd_base/dpaa2_hw_dpni.o = gcc -Wp,-MD,base/.dpaa2_hw_dpni.o.d.tmp  -m64 -pthread  -march=native -DRTE_MACHINE_CPUFLAG_SSE -DRTE_MACHINE_CPUFLAG_SSE2 -DRTE_MACHINE_CPUFLAG_SSE3 -DRTE_MACHINE_CPUFLAG_SSSE3 -DRTE_MACHINE_CPUFLAG_SSE4_1 -DRTE_MACHINE_CPUFLAG_SSE4_2 -DRTE_MACHINE_CPUFLAG_AES -DRTE_MACHINE_CPUFLAG_PCLMULQDQ -DRTE_MACHINE_CPUFLAG_AVX -DRTE_MACHINE_CPUFLAG_RDRAND -DRTE_MACHINE_CPUFLAG_FSGSBASE -DRTE_MACHINE_CPUFLAG_F16C -DRTE_MACHINE_CPUFLAG_AVX2  -I/home/ubuntu/gloo/daiet/dpdk/build/include -include /home/ubuntu/gloo/daiet/dpdk/build/include/rte_config.h -O3 -W -Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wold-style-definition -Wpointer-arith -Wcast-align -Wnested-externs -Wcast-qual -Wformat-nonliteral -Wformat-security -Wundef -Wwrite-strings -Wdeprecated -I/home/ubuntu/gloo/daiet/dpdk/drivers/net/dpaa2 -I/home/ubuntu/gloo/daiet/dpdk/drivers/net/dpaa2/mc -I/home/ubuntu/gloo/daiet/dpdk/drivers/bus/fslmc -I/home/ubuntu/gloo/daiet/dpdk/drivers/bus/fslmc/qbman/include -I/home/ubuntu/gloo/daiet/dpdk/drivers/bus/fslmc/mc -I/home/ubuntu/gloo/daiet/dpdk/drivers/bus/fslmc/portal -I/home/ubuntu/gloo/daiet/dpdk/drivers/mempool/dpaa2 -I/home/ubuntu/gloo/daiet/dpdk/lib/librte_eal/linuxapp/eal -DALLOW_EXPERIMENTAL_API    -o base/dpaa2_hw_dpni.o -c /home/ubuntu/gloo/daiet/dpdk/drivers/net/dpaa2/base/dpaa2_hw_dpni.c 
