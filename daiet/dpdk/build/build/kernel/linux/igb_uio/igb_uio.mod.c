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
	{ 0x4488ae17, __VMLINUX_SYMBOL_STR(param_ops_charp) },
	{ 0x94e413f2, __VMLINUX_SYMBOL_STR(pci_unregister_driver) },
	{ 0x22e04e3b, __VMLINUX_SYMBOL_STR(__pci_register_driver) },
	{ 0xe2d5255a, __VMLINUX_SYMBOL_STR(strcmp) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0x16305289, __VMLINUX_SYMBOL_STR(warn_slowpath_null) },
	{ 0x422c1a5d, __VMLINUX_SYMBOL_STR(dev_warn) },
	{ 0xc99164f4, __VMLINUX_SYMBOL_STR(dma_ops) },
	{ 0x78764f4e, __VMLINUX_SYMBOL_STR(pv_irq_ops) },
	{ 0xa310f4c2, __VMLINUX_SYMBOL_STR(arch_dma_alloc_attrs) },
	{ 0xf8466238, __VMLINUX_SYMBOL_STR(__uio_register_device) },
	{ 0xd160a17c, __VMLINUX_SYMBOL_STR(sysfs_create_group) },
	{ 0x4125f34e, __VMLINUX_SYMBOL_STR(dma_supported) },
	{ 0x42c8de35, __VMLINUX_SYMBOL_STR(ioremap_nocache) },
	{ 0x2ea2c95c, __VMLINUX_SYMBOL_STR(__x86_indirect_thunk_rax) },
	{ 0xfdf73752, __VMLINUX_SYMBOL_STR(pci_enable_device) },
	{ 0x14f2edb, __VMLINUX_SYMBOL_STR(__mutex_init) },
	{ 0xe9bcba71, __VMLINUX_SYMBOL_STR(kmem_cache_alloc_trace) },
	{ 0x54fe1637, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0x7f4fa47f, __VMLINUX_SYMBOL_STR(pci_bus_write_config_dword) },
	{ 0xe55b9912, __VMLINUX_SYMBOL_STR(pci_intx) },
	{ 0xa52883fb, __VMLINUX_SYMBOL_STR(pci_cfg_access_unlock) },
	{ 0xb718c7ea, __VMLINUX_SYMBOL_STR(pci_cfg_access_lock) },
	{ 0xed32dab6, __VMLINUX_SYMBOL_STR(__dynamic_dev_dbg) },
	{ 0x22b6c20a, __VMLINUX_SYMBOL_STR(dev_notice) },
	{ 0x88eba210, __VMLINUX_SYMBOL_STR(pci_intx_mask_supported) },
	{ 0x27f0f312, __VMLINUX_SYMBOL_STR(dev_err) },
	{ 0xdbe14608, __VMLINUX_SYMBOL_STR(_dev_info) },
	{ 0x2072ee9b, __VMLINUX_SYMBOL_STR(request_threaded_irq) },
	{ 0xb6b1a614, __VMLINUX_SYMBOL_STR(pci_enable_msi_range) },
	{ 0x5daf96ee, __VMLINUX_SYMBOL_STR(pci_enable_msix) },
	{ 0x530ed545, __VMLINUX_SYMBOL_STR(pci_set_master) },
	{ 0x3880567f, __VMLINUX_SYMBOL_STR(pci_check_and_mask_intx) },
	{ 0x1cead7f9, __VMLINUX_SYMBOL_STR(uio_event_notify) },
	{ 0x3c9898f6, __VMLINUX_SYMBOL_STR(pci_disable_msix) },
	{ 0xedf7ab08, __VMLINUX_SYMBOL_STR(pci_disable_msi) },
	{ 0xf20dabd8, __VMLINUX_SYMBOL_STR(free_irq) },
	{ 0x2041c41b, __VMLINUX_SYMBOL_STR(pci_clear_master) },
	{ 0x47256491, __VMLINUX_SYMBOL_STR(mutex_unlock) },
	{ 0xbfe8ed5b, __VMLINUX_SYMBOL_STR(mutex_lock) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0x64ae1178, __VMLINUX_SYMBOL_STR(pci_disable_device) },
	{ 0xedc03953, __VMLINUX_SYMBOL_STR(iounmap) },
	{ 0x82a9b20a, __VMLINUX_SYMBOL_STR(uio_unregister_device) },
	{ 0xb64e9b91, __VMLINUX_SYMBOL_STR(sysfs_remove_group) },
	{ 0x28318305, __VMLINUX_SYMBOL_STR(snprintf) },
	{ 0x8db8f92d, __VMLINUX_SYMBOL_STR(pci_bus_type) },
	{ 0xdb7305a1, __VMLINUX_SYMBOL_STR(__stack_chk_fail) },
	{ 0xc3567ef3, __VMLINUX_SYMBOL_STR(pci_enable_sriov) },
	{ 0x2f2a5224, __VMLINUX_SYMBOL_STR(pci_disable_sriov) },
	{ 0x1709d3ba, __VMLINUX_SYMBOL_STR(pci_num_vf) },
	{ 0x3c80c06c, __VMLINUX_SYMBOL_STR(kstrtoull) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=uio";


MODULE_INFO(srcversion, "8E5A627216D2E0206485D3E");
