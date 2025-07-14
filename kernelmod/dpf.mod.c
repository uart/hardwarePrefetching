#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

#ifdef CONFIG_UNWINDER_ORC
#include <asm/orc_header.h>
ORC_HEADER;
#endif

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_MITIGATION_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xedc03953, "iounmap" },
	{ 0x48879f20, "hrtimer_init" },
	{ 0x69acdf38, "memcpy" },
	{ 0x37a0cba, "kfree" },
	{ 0x8cc346e2, "pcpu_hot" },
	{ 0x1035c7c2, "__release_region" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0xc6910aa0, "do_trace_rdpmc" },
	{ 0x65487097, "__x86_indirect_thunk_rax" },
	{ 0x122c3a7e, "_printk" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0x87a21cb3, "__ubsan_handle_out_of_bounds" },
	{ 0x4dfa8d4b, "mutex_lock" },
	{ 0xde80cd09, "ioremap" },
	{ 0x53a1e8d9, "_find_next_bit" },
	{ 0x5a5a2271, "__cpu_online_mask" },
	{ 0x75ca79b5, "__fortify_panic" },
	{ 0x3f372d6a, "__tracepoint_write_msr" },
	{ 0xfb578fc5, "memset" },
	{ 0xd955afa6, "hrtimer_start_range_ns" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0x17de3d5, "nr_cpu_ids" },
	{ 0x6b10bee1, "_copy_to_user" },
	{ 0xbb13595e, "smp_call_function_many" },
	{ 0xe59a8021, "__tracepoint_rdpmc" },
	{ 0x3213f038, "mutex_unlock" },
	{ 0x25db1577, "do_trace_write_msr" },
	{ 0x2cf56265, "__dynamic_pr_debug" },
	{ 0xddbd498e, "remove_proc_entry" },
	{ 0xb43f9365, "ktime_get" },
	{ 0x4a666d56, "hrtimer_cancel" },
	{ 0xf07464e4, "hrtimer_forward" },
	{ 0x82e1753, "kmalloc_trace" },
	{ 0x54b1fac6, "__ubsan_handle_load_invalid_value" },
	{ 0x77358855, "iomem_resource" },
	{ 0xeb233a45, "__kmalloc" },
	{ 0x4eef0042, "kmalloc_caches" },
	{ 0x85bd1608, "__request_region" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0x13c49cc2, "_copy_from_user" },
	{ 0x9ac0714c, "proc_create" },
	{ 0x8810754a, "_find_first_bit" },
	{ 0x3b5c67f4, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "838FF36183BAD54F59C926C");
