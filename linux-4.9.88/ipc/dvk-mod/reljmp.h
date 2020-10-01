/*
 * Copyright (C) 2013 Hewlett-Packard Development Company, L.P.
 *
 * Author: Juerg Haefliger <juerg.haefliger@hp.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __RELJMP_H__
#define __RELJMP_H__

#define RELATIVEJUMP_OPCODE 0xe9
#define RELATIVEJUMP_SIZE 5

struct reljmp {
	const char *from_symbol_name;
	const char *to_symbol_name;
	unsigned long from_addr;
	unsigned long to_addr;
	s32 relative_addr;
	char from_code[RELATIVEJUMP_SIZE];
	char jump_code[RELATIVEJUMP_SIZE];
	int (*init_handler)(void);
};

void reljmp_unregister(struct reljmp *);
void reljmp_register(struct reljmp *);
int reljmp_init(struct reljmp *);
int reljmp_init_once(void);

#define RELJMP_LOOKUP_TYPE(name, type) \
	{ \
		unsigned long __addr = kallsyms_lookup_name(#name); \
		if (!__addr) { \
			printk(KERN_ERR "reljmp: failed to lookup %s\n", \
			       #name); \
			return -ENODEV; \
		} \
		reljmp_##name = type __addr ; \
		printk("reljmp: %s @ 0x%p\n", #name, (void *)__addr); \
	}

#define RELJMP_LOOKUP_FUNC(name) RELJMP_LOOKUP_TYPE(name, (void *))

#endif /* __RELJMP__ */
