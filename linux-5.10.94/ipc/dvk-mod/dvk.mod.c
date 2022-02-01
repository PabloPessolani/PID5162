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
	{ 0x99444e10, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x4d3c153f, __VMLINUX_SYMBOL_STR(sigprocmask) },
	{ 0x34196249, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0x12da5bb2, __VMLINUX_SYMBOL_STR(__kmalloc) },
	{ 0xd419990c, __VMLINUX_SYMBOL_STR(debugfs_create_dir) },
	{ 0x4f9fe53f, __VMLINUX_SYMBOL_STR(mem_map) },
	{ 0x560113e3, __VMLINUX_SYMBOL_STR(param_ops_int) },
	{ 0x14da26e8, __VMLINUX_SYMBOL_STR(get_task_pid) },
	{ 0xd0d8621b, __VMLINUX_SYMBOL_STR(strlen) },
	{ 0x578ef677, __VMLINUX_SYMBOL_STR(pid_vnr) },
	{ 0xc4fc70e2, __VMLINUX_SYMBOL_STR(find_vpid) },
	{ 0xdad0e963, __VMLINUX_SYMBOL_STR(boot_cpu_data) },
	{ 0xf7d9209, __VMLINUX_SYMBOL_STR(__x86_indirect_thunk_eax) },
	{ 0x179651ac, __VMLINUX_SYMBOL_STR(_raw_read_lock) },
	{ 0x816c0da3, __VMLINUX_SYMBOL_STR(remove_proc_entry) },
	{ 0xaa761369, __VMLINUX_SYMBOL_STR(__register_chrdev) },
	{ 0x448c73f9, __VMLINUX_SYMBOL_STR(mutex_unlock) },
	{ 0x7a2af7b4, __VMLINUX_SYMBOL_STR(cpu_number) },
	{ 0x91715312, __VMLINUX_SYMBOL_STR(sprintf) },
	{ 0x343a1a8, __VMLINUX_SYMBOL_STR(__list_add) },
	{ 0xe2d5255a, __VMLINUX_SYMBOL_STR(strcmp) },
	{ 0x9e88526, __VMLINUX_SYMBOL_STR(__init_waitqueue_head) },
	{ 0xc671e369, __VMLINUX_SYMBOL_STR(_copy_to_user) },
	{ 0x65b955a7, __VMLINUX_SYMBOL_STR(PDE_DATA) },
	{ 0xfe7c4287, __VMLINUX_SYMBOL_STR(nr_cpu_ids) },
	{ 0x7a2add7d, __VMLINUX_SYMBOL_STR(current_kernel_time64) },
	{ 0xff25fca0, __VMLINUX_SYMBOL_STR(proc_mkdir) },
	{ 0x9b78c6e2, __VMLINUX_SYMBOL_STR(current_task) },
	{ 0xdee08877, __VMLINUX_SYMBOL_STR(__mutex_init) },
	{ 0x8026fa61, __VMLINUX_SYMBOL_STR(__x86_indirect_thunk_esi) },
	{ 0x50eedeb8, __VMLINUX_SYMBOL_STR(printk) },
	{ 0xa1c76e0a, __VMLINUX_SYMBOL_STR(_cond_resched) },
	{ 0xb6ed1e53, __VMLINUX_SYMBOL_STR(strncpy) },
	{ 0xb4390f9a, __VMLINUX_SYMBOL_STR(mcount) },
	{ 0xb0f3ad32, __VMLINUX_SYMBOL_STR(debugfs_remove) },
	{ 0x16305289, __VMLINUX_SYMBOL_STR(warn_slowpath_null) },
	{ 0x96d92d85, __VMLINUX_SYMBOL_STR(mutex_lock) },
	{ 0x521445b, __VMLINUX_SYMBOL_STR(list_del) },
	{ 0xc2cdbf1, __VMLINUX_SYMBOL_STR(synchronize_sched) },
	{ 0xe5b23de1, __VMLINUX_SYMBOL_STR(pid_task) },
	{ 0xccbe9e52, __VMLINUX_SYMBOL_STR(debugfs_create_file_unsafe) },
	{ 0xe007de41, __VMLINUX_SYMBOL_STR(kallsyms_lookup_name) },
	{ 0x622598b1, __VMLINUX_SYMBOL_STR(init_wait_entry) },
	{ 0xe91b2306, __VMLINUX_SYMBOL_STR(dvk_vm_rw) },
	{ 0x7b1a9c2f, __VMLINUX_SYMBOL_STR(module_put) },
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
	{ 0x502343ed, __VMLINUX_SYMBOL_STR(kmem_cache_alloc_trace) },
	{ 0x4302d0eb, __VMLINUX_SYMBOL_STR(free_pages) },
	{ 0xa6bbd805, __VMLINUX_SYMBOL_STR(__wake_up) },
	{ 0x2207a57f, __VMLINUX_SYMBOL_STR(prepare_to_wait_event) },
	{ 0x7b0e1e2c, __VMLINUX_SYMBOL_STR(proc_create_data) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0xe151efbb, __VMLINUX_SYMBOL_STR(send_sig_info) },
	{ 0x4942c43e, __VMLINUX_SYMBOL_STR(__put_task_struct) },
	{ 0xf08242c2, __VMLINUX_SYMBOL_STR(finish_wait) },
	{ 0x1aa6643a, __VMLINUX_SYMBOL_STR(task_active_pid_ns) },
	{ 0x7f02188f, __VMLINUX_SYMBOL_STR(__msecs_to_jiffies) },
	{ 0xb5419b40, __VMLINUX_SYMBOL_STR(_copy_from_user) },
	{ 0x522e9973, __VMLINUX_SYMBOL_STR(param_ops_ulong) },
	{ 0xb3d0828e, __VMLINUX_SYMBOL_STR(find_pid_ns) },
	{ 0x88db9f48, __VMLINUX_SYMBOL_STR(__check_object_size) },
	{ 0x53947e8a, __VMLINUX_SYMBOL_STR(try_module_get) },
	{ 0xe914e41e, __VMLINUX_SYMBOL_STR(strcpy) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "09E2D05B48C415644940634");
