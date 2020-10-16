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

#ifdef RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xaebbc080, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x7a559c46, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0x12da5bb2, __VMLINUX_SYMBOL_STR(__kmalloc) },
	{ 0xd1465ba3, __VMLINUX_SYMBOL_STR(debugfs_create_dir) },
	{ 0x47c718da, __VMLINUX_SYMBOL_STR(mem_map) },
	{ 0xc3145478, __VMLINUX_SYMBOL_STR(param_ops_int) },
	{ 0x96d8e85d, __VMLINUX_SYMBOL_STR(get_task_pid) },
	{ 0xd0d8621b, __VMLINUX_SYMBOL_STR(strlen) },
	{ 0xe3e7fc4, __VMLINUX_SYMBOL_STR(pid_vnr) },
	{ 0xb0909cba, __VMLINUX_SYMBOL_STR(find_vpid) },
	{ 0xdad0e963, __VMLINUX_SYMBOL_STR(boot_cpu_data) },
	{ 0xf7d9209, __VMLINUX_SYMBOL_STR(__x86_indirect_thunk_eax) },
	{ 0x179651ac, __VMLINUX_SYMBOL_STR(_raw_read_lock) },
	{ 0x6eb7db60, __VMLINUX_SYMBOL_STR(remove_proc_entry) },
	{ 0x89fb7eb8, __VMLINUX_SYMBOL_STR(__register_chrdev) },
	{ 0xb2f135b8, __VMLINUX_SYMBOL_STR(mutex_unlock) },
	{ 0x7a2af7b4, __VMLINUX_SYMBOL_STR(cpu_number) },
	{ 0x91715312, __VMLINUX_SYMBOL_STR(sprintf) },
	{ 0x343a1a8, __VMLINUX_SYMBOL_STR(__list_add) },
	{ 0xe2d5255a, __VMLINUX_SYMBOL_STR(strcmp) },
	{ 0x9e88526, __VMLINUX_SYMBOL_STR(__init_waitqueue_head) },
	{ 0xc671e369, __VMLINUX_SYMBOL_STR(_copy_to_user) },
	{ 0x82b93805, __VMLINUX_SYMBOL_STR(PDE_DATA) },
	{ 0xfe7c4287, __VMLINUX_SYMBOL_STR(nr_cpu_ids) },
	{ 0x7a2add7d, __VMLINUX_SYMBOL_STR(current_kernel_time64) },
	{ 0x9e5ee8c3, __VMLINUX_SYMBOL_STR(proc_mkdir) },
	{ 0xa73d457f, __VMLINUX_SYMBOL_STR(current_task) },
	{ 0xc5eacef, __VMLINUX_SYMBOL_STR(__mutex_init) },
	{ 0x8026fa61, __VMLINUX_SYMBOL_STR(__x86_indirect_thunk_esi) },
	{ 0x50eedeb8, __VMLINUX_SYMBOL_STR(printk) },
	{ 0xa1c76e0a, __VMLINUX_SYMBOL_STR(_cond_resched) },
	{ 0xb6ed1e53, __VMLINUX_SYMBOL_STR(strncpy) },
	{ 0xb4390f9a, __VMLINUX_SYMBOL_STR(mcount) },
	{ 0xb824ec16, __VMLINUX_SYMBOL_STR(debugfs_remove) },
	{ 0x16305289, __VMLINUX_SYMBOL_STR(warn_slowpath_null) },
	{ 0x3bee2492, __VMLINUX_SYMBOL_STR(mutex_lock) },
	{ 0x521445b, __VMLINUX_SYMBOL_STR(list_del) },
	{ 0xc2cdbf1, __VMLINUX_SYMBOL_STR(synchronize_sched) },
	{ 0x2324ebb9, __VMLINUX_SYMBOL_STR(pid_task) },
	{ 0x5603e4c8, __VMLINUX_SYMBOL_STR(debugfs_create_file_unsafe) },
	{ 0xe007de41, __VMLINUX_SYMBOL_STR(kallsyms_lookup_name) },
	{ 0x622598b1, __VMLINUX_SYMBOL_STR(init_wait_entry) },
	{ 0xd89bb79b, __VMLINUX_SYMBOL_STR(dvk_vm_rw) },
	{ 0x4a5c397c, __VMLINUX_SYMBOL_STR(module_put) },
	{ 0xc6cbbc89, __VMLINUX_SYMBOL_STR(capable) },
	{ 0xb2fd5ceb, __VMLINUX_SYMBOL_STR(__put_user_4) },
	{ 0xc19a68ab, __VMLINUX_SYMBOL_STR(local_nodeid) },
	{ 0xdb7305a1, __VMLINUX_SYMBOL_STR(__stack_chk_fail) },
	{ 0x4292364c, __VMLINUX_SYMBOL_STR(schedule) },
	{ 0xd62c833f, __VMLINUX_SYMBOL_STR(schedule_timeout) },
	{ 0xd7d79132, __VMLINUX_SYMBOL_STR(put_online_cpus) },
	{ 0xe3460c96, __VMLINUX_SYMBOL_STR(__x86_indirect_thunk_ecx) },
	{ 0x3efb35c9, __VMLINUX_SYMBOL_STR(get_online_cpus) },
	{ 0x97dee519, __VMLINUX_SYMBOL_STR(__x86_indirect_thunk_edx) },
	{ 0x8f4b09a, __VMLINUX_SYMBOL_STR(kmem_cache_alloc_trace) },
	{ 0x4302d0eb, __VMLINUX_SYMBOL_STR(free_pages) },
	{ 0xa6bbd805, __VMLINUX_SYMBOL_STR(__wake_up) },
	{ 0x2207a57f, __VMLINUX_SYMBOL_STR(prepare_to_wait_event) },
	{ 0xefc22c70, __VMLINUX_SYMBOL_STR(proc_create_data) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0x365a1e48, __VMLINUX_SYMBOL_STR(send_sig_info) },
	{ 0xe3578a7d, __VMLINUX_SYMBOL_STR(__put_task_struct) },
	{ 0xf08242c2, __VMLINUX_SYMBOL_STR(finish_wait) },
	{ 0x2ccdf43b, __VMLINUX_SYMBOL_STR(task_active_pid_ns) },
	{ 0x7f02188f, __VMLINUX_SYMBOL_STR(__msecs_to_jiffies) },
	{ 0xb5419b40, __VMLINUX_SYMBOL_STR(_copy_from_user) },
	{ 0xf9201977, __VMLINUX_SYMBOL_STR(find_pid_ns) },
	{ 0x88db9f48, __VMLINUX_SYMBOL_STR(__check_object_size) },
	{ 0xaad2799e, __VMLINUX_SYMBOL_STR(try_module_get) },
	{ 0xe914e41e, __VMLINUX_SYMBOL_STR(strcpy) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "A3CF143983FDE57BD074C8D");
