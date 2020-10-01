cmd_fs/hostfs/hostfs_user.o := gcc -Wp,-MD,fs/hostfs/.hostfs_user.o.d -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration -Wno-format-security -std=gnu89 -fno-PIE -m32 -funit-at-a-time -march=i686 -Wa,-mtune=generic32 -mpreferred-stack-boundary=2 -ffreestanding -D__arch_um__ -Dvmap=kernel_vmap -Din6addr_loopback=kernel_in6addr_loopback -Din6addr_any=kernel_in6addr_any -Dstrrchr=kernel_strrchr -D_LARGEFILE64_SOURCE  -fno-delete-null-pointer-checks -Wno-frame-address -Os -Wno-maybe-uninitialized --param=allow-store-data-races=0 -DCC_HAVE_ASM_GOTO -Wframe-larger-than=2048 -fno-stack-protector -Wno-unused-but-set-variable -Wno-unused-const-variable -fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-var-tracking-assignments -g -Wdeclaration-after-statement -Wno-pointer-sign -fno-strict-overflow -fno-stack-check -fconserve-stack -Werror=implicit-int -Werror=strict-prototypes -Werror=date-time -Werror=incompatible-pointer-types -I./arch/um/include/shared -I./arch/x86/um/shared -I./arch/um/include/shared/skas  -D_FILE_OFFSET_BITS=64 -idirafter ./include -idirafter ./include -D__KERNEL__ -D__UM_HOST__ -D_GNU_SOURCE -D_LARGEFILE64_SOURCE -include ./include/linux/kern_levels.h -include user.h  -c -o fs/hostfs/hostfs_user.o fs/hostfs/hostfs_user.c

source_fs/hostfs/hostfs_user.o := fs/hostfs/hostfs_user.c

deps_fs/hostfs/hostfs_user.o := \
  include/linux/kern_levels.h \
  arch/um/include/shared/user.h \
    $(wildcard include/config/printk.h) \
  include/generated/asm-offsets.h \
    $(wildcard include/config/no/hz/common.h) \
    $(wildcard include/config/uml/x86.h) \
  /usr/lib/gcc/i686-linux-gnu/6/include/stddef.h \
  /usr/include/stdio.h \
  /usr/include/features.h \
  /usr/include/stdc-predef.h \
  /usr/include/i386-linux-gnu/sys/cdefs.h \
  /usr/include/i386-linux-gnu/bits/wordsize.h \
  /usr/include/i386-linux-gnu/gnu/stubs.h \
  /usr/include/i386-linux-gnu/gnu/stubs-32.h \
  /usr/include/i386-linux-gnu/bits/types.h \
  /usr/include/i386-linux-gnu/bits/typesizes.h \
  /usr/include/libio.h \
  /usr/include/_G_config.h \
  /usr/include/wchar.h \
  /usr/lib/gcc/i686-linux-gnu/6/include/stdarg.h \
  /usr/include/i386-linux-gnu/bits/stdio_lim.h \
  /usr/include/i386-linux-gnu/bits/sys_errlist.h \
  /usr/include/unistd.h \
  /usr/include/i386-linux-gnu/bits/posix_opt.h \
  /usr/include/i386-linux-gnu/bits/environments.h \
  /usr/include/i386-linux-gnu/bits/confname.h \
  /usr/include/getopt.h \
  /usr/include/dirent.h \
  /usr/include/i386-linux-gnu/bits/dirent.h \
  /usr/include/i386-linux-gnu/bits/posix1_lim.h \
  /usr/include/i386-linux-gnu/bits/local_lim.h \
  /usr/include/linux/limits.h \
  /usr/include/errno.h \
  /usr/include/i386-linux-gnu/bits/errno.h \
  /usr/include/linux/errno.h \
  /usr/include/i386-linux-gnu/asm/errno.h \
  /usr/include/asm-generic/errno.h \
  /usr/include/asm-generic/errno-base.h \
  /usr/include/fcntl.h \
  /usr/include/i386-linux-gnu/bits/fcntl.h \
  /usr/include/i386-linux-gnu/bits/fcntl-linux.h \
  /usr/include/i386-linux-gnu/bits/uio.h \
  /usr/include/i386-linux-gnu/sys/types.h \
  /usr/include/time.h \
  /usr/include/endian.h \
  /usr/include/i386-linux-gnu/bits/endian.h \
  /usr/include/i386-linux-gnu/bits/byteswap.h \
  /usr/include/i386-linux-gnu/bits/byteswap-16.h \
  /usr/include/i386-linux-gnu/sys/select.h \
  /usr/include/i386-linux-gnu/bits/select.h \
  /usr/include/i386-linux-gnu/bits/sigset.h \
  /usr/include/i386-linux-gnu/bits/time.h \
  /usr/include/i386-linux-gnu/sys/sysmacros.h \
  /usr/include/i386-linux-gnu/bits/pthreadtypes.h \
  /usr/include/i386-linux-gnu/bits/stat.h \
  /usr/include/string.h \
  /usr/include/xlocale.h \
  /usr/include/i386-linux-gnu/sys/stat.h \
  /usr/include/i386-linux-gnu/sys/time.h \
  /usr/include/i386-linux-gnu/sys/vfs.h \
  /usr/include/i386-linux-gnu/sys/statfs.h \
  /usr/include/i386-linux-gnu/bits/statfs.h \
  /usr/include/i386-linux-gnu/sys/syscall.h \
  /usr/include/i386-linux-gnu/asm/unistd.h \
  /usr/include/i386-linux-gnu/asm/unistd_32.h \
  /usr/include/i386-linux-gnu/bits/syscall.h \
  fs/hostfs/hostfs.h \
  arch/um/include/shared/os.h \
    $(wildcard include/config/64bit.h) \
  arch/um/include/shared/irq_user.h \
  arch/x86/um/shared/sysdep/ptrace.h \
  include/generated/user_constants.h \
  arch/x86/um/shared/sysdep/faultinfo.h \
  arch/x86/um/shared/sysdep/faultinfo_32.h \
  arch/x86/um/shared/sysdep/ptrace_32.h \
  arch/um/include/shared/longjmp.h \
  arch/x86/um/shared/sysdep/archsetjmp.h \
  arch/x86/um/shared/sysdep/archsetjmp_32.h \
  arch/um/include/shared/skas/mm_id.h \
    $(wildcard include/config/uml/dvk/nulo.h) \
  /usr/include/utime.h \

fs/hostfs/hostfs_user.o: $(deps_fs/hostfs/hostfs_user.o)

$(deps_fs/hostfs/hostfs_user.o):
