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
	{ 0x3c6cf1a0, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0xf20dabd8, __VMLINUX_SYMBOL_STR(free_irq) },
	{ 0x7485e15e, __VMLINUX_SYMBOL_STR(unregister_chrdev_region) },
	{ 0x3a8ccca1, __VMLINUX_SYMBOL_STR(class_destroy) },
	{ 0x22ed0c77, __VMLINUX_SYMBOL_STR(device_destroy) },
	{ 0x2eaf235c, __VMLINUX_SYMBOL_STR(cdev_del) },
	{ 0xb4d3fc7d, __VMLINUX_SYMBOL_STR(device_create) },
	{ 0xf7e87b32, __VMLINUX_SYMBOL_STR(cdev_add) },
	{ 0x1e3e56e6, __VMLINUX_SYMBOL_STR(cdev_init) },
	{ 0x4c51a4f6, __VMLINUX_SYMBOL_STR(__class_create) },
	{ 0x29537c9e, __VMLINUX_SYMBOL_STR(alloc_chrdev_region) },
	{ 0xd6b8e852, __VMLINUX_SYMBOL_STR(request_threaded_irq) },
	{ 0xb086f4e0, __VMLINUX_SYMBOL_STR(gpiod_to_irq) },
	{ 0x29e6b29b, __VMLINUX_SYMBOL_STR(gpio_to_desc) },
	{ 0x27bbf221, __VMLINUX_SYMBOL_STR(disable_irq_nosync) },
	{ 0xd4a31b08, __VMLINUX_SYMBOL_STR(send_sig_info) },
	{ 0x2469810f, __VMLINUX_SYMBOL_STR(__rcu_read_unlock) },
	{ 0x59fbe6e2, __VMLINUX_SYMBOL_STR(pid_task) },
	{ 0x9c7728b9, __VMLINUX_SYMBOL_STR(find_vpid) },
	{ 0x8d522714, __VMLINUX_SYMBOL_STR(__rcu_read_lock) },
	{ 0xfa2a45e, __VMLINUX_SYMBOL_STR(__memzero) },
	{ 0x28cc25db, __VMLINUX_SYMBOL_STR(arm_copy_from_user) },
	{ 0xefd6cf06, __VMLINUX_SYMBOL_STR(__aeabi_unwind_cpp_pr0) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

