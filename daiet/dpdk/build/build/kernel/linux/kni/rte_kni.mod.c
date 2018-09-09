#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xb6fd8534, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x757047db, __VMLINUX_SYMBOL_STR(up_read) },
	{ 0xc4f331c6, __VMLINUX_SYMBOL_STR(cpu_online_mask) },
	{ 0x79aa04a2, __VMLINUX_SYMBOL_STR(get_random_bytes) },
	{ 0xb9f22b6, __VMLINUX_SYMBOL_STR(netif_carrier_on) },
	{ 0xad3f9ec3, __VMLINUX_SYMBOL_STR(netif_carrier_off) },
	{ 0x44b1d426, __VMLINUX_SYMBOL_STR(__dynamic_pr_debug) },
	{ 0x47256491, __VMLINUX_SYMBOL_STR(mutex_unlock) },
	{ 0xa773db4e, __VMLINUX_SYMBOL_STR(__put_net) },
	{ 0x56d1136f, __VMLINUX_SYMBOL_STR(kthread_create_on_node) },
	{ 0x7d11c268, __VMLINUX_SYMBOL_STR(jiffies) },
	{ 0x94088c5d, __VMLINUX_SYMBOL_STR(down_read) },
	{ 0xe2d5255a, __VMLINUX_SYMBOL_STR(strcmp) },
	{ 0xb2b512e8, __VMLINUX_SYMBOL_STR(kthread_bind) },
	{ 0xc1c8d843, __VMLINUX_SYMBOL_STR(__netdev_alloc_skb) },
	{ 0x9e88526, __VMLINUX_SYMBOL_STR(__init_waitqueue_head) },
	{ 0x4488ae17, __VMLINUX_SYMBOL_STR(param_ops_charp) },
	{ 0x765f961b, __VMLINUX_SYMBOL_STR(misc_register) },
	{ 0xfb578fc5, __VMLINUX_SYMBOL_STR(memset) },
	{ 0xf6904ce5, __VMLINUX_SYMBOL_STR(netif_rx_ni) },
	{ 0xce35f341, __VMLINUX_SYMBOL_STR(unregister_pernet_subsys) },
	{ 0x738dcdcc, __VMLINUX_SYMBOL_STR(netif_tx_wake_queue) },
	{ 0xb47d047e, __VMLINUX_SYMBOL_STR(current_task) },
	{ 0x14f2edb, __VMLINUX_SYMBOL_STR(__mutex_init) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0x9fe281a9, __VMLINUX_SYMBOL_STR(kthread_stop) },
	{ 0xadb52576, __VMLINUX_SYMBOL_STR(free_netdev) },
	{ 0xa1c76e0a, __VMLINUX_SYMBOL_STR(_cond_resched) },
	{ 0x9166fada, __VMLINUX_SYMBOL_STR(strncpy) },
	{ 0x1b4b00c5, __VMLINUX_SYMBOL_STR(register_netdev) },
	{ 0x5a921311, __VMLINUX_SYMBOL_STR(strncmp) },
	{ 0xaf1bb66e, __VMLINUX_SYMBOL_STR(skb_push) },
	{ 0xbfe8ed5b, __VMLINUX_SYMBOL_STR(mutex_lock) },
	{ 0x1b4cffd1, __VMLINUX_SYMBOL_STR(up_write) },
	{ 0x1f76a10f, __VMLINUX_SYMBOL_STR(down_write) },
	{ 0xa916b694, __VMLINUX_SYMBOL_STR(strnlen) },
	{ 0xdb7305a1, __VMLINUX_SYMBOL_STR(__stack_chk_fail) },
	{ 0xd62c833f, __VMLINUX_SYMBOL_STR(schedule_timeout) },
	{ 0xcf8613b7, __VMLINUX_SYMBOL_STR(alloc_netdev_mqs) },
	{ 0x2ea2c95c, __VMLINUX_SYMBOL_STR(__x86_indirect_thunk_rax) },
	{ 0x38ae1547, __VMLINUX_SYMBOL_STR(eth_type_trans) },
	{ 0x7f24de73, __VMLINUX_SYMBOL_STR(jiffies_to_usecs) },
	{ 0xe384fbc9, __VMLINUX_SYMBOL_STR(wake_up_process) },
	{ 0x50f0872f, __VMLINUX_SYMBOL_STR(register_pernet_subsys) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
	{ 0xf08783a, __VMLINUX_SYMBOL_STR(ether_setup) },
	{ 0xa6bbd805, __VMLINUX_SYMBOL_STR(__wake_up) },
	{ 0xb3f7646e, __VMLINUX_SYMBOL_STR(kthread_should_stop) },
	{ 0x2207a57f, __VMLINUX_SYMBOL_STR(prepare_to_wait_event) },
	{ 0x9c55cec, __VMLINUX_SYMBOL_STR(schedule_timeout_interruptible) },
	{ 0x69acdf38, __VMLINUX_SYMBOL_STR(memcpy) },
	{ 0x58cbda28, __VMLINUX_SYMBOL_STR(dev_trans_start) },
	{ 0xf08242c2, __VMLINUX_SYMBOL_STR(finish_wait) },
	{ 0xd53f7c6, __VMLINUX_SYMBOL_STR(unregister_netdev) },
	{ 0xc3afc768, __VMLINUX_SYMBOL_STR(consume_skb) },
	{ 0x715fe9b9, __VMLINUX_SYMBOL_STR(skb_put) },
	{ 0x4f6b400b, __VMLINUX_SYMBOL_STR(_copy_from_user) },
	{ 0x1c2e85e1, __VMLINUX_SYMBOL_STR(misc_deregister) },
	{ 0x1d46aff, __VMLINUX_SYMBOL_STR(__init_rwsem) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "55505146AAAB3CAC79C98D0");
