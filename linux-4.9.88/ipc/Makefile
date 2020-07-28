#
# Makefile for the linux ipc.
#

obj-$(CONFIG_SYSVIPC_COMPAT) += compat.o
obj-$(CONFIG_SYSVIPC) += util.o msgutil.o msg.o sem.o shm.o syscall.o
obj-$(CONFIG_SYSVIPC_SYSCTL) += ipc_sysctl.o
obj_mq-$(CONFIG_COMPAT) += compat_mq.o
obj-$(CONFIG_POSIX_MQUEUE) += mqueue.o msgutil.o $(obj_mq-y)
obj-$(CONFIG_IPC_NS) += namespace.o
obj-$(CONFIG_POSIX_MQUEUE_SYSCTL) += mq_sysctl.o
obj-$(CONFIG_DVKIPC) += ./dvk-mod/dvk_hyper.o   ./dvk-mod/dvk_ipc.o \
						./dvk-mod/dvk_rproxy.o  ./dvk-mod/dvk_migrate.o \
						./dvk-mod/dvk_debugfs.o ./dvk-mod/dvk_procfs.o \
						./dvk-mod/dvk_utils.o 	./dvk-mod/dvk_acks.o \
						./dvk-mod/dvk_sproxy.o   ./dvk_decode.o 
