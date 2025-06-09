/*
 * Copyright (C) 2021, Alexandre Iooss <erdnaxe@crans.org>
 * Copyright (C) 2025, Erkai Yu <erkaiyu2@illinois.edu>
 * 
 * Monitor runtime patching by memory access 
 *
 * License: GNU GPL, version 2 or later.
 *   See the COPYING file in the top-level directory.
 */
#include <glib.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <qemu-plugin.h>

static unsigned long long haddr_lo = 0x40210000ULL, haddr_hi = 0x412d5000ULL;

static unsigned long long rewrite_cnt = 0;

static char dump_file[200] = "modified_slots.bin";
static char *buffer;
static char *buffer_ptr;
static size_t buffer_remaining_size;
static size_t buffer_size;

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

static void vcpu_mem(unsigned int cpu_index, qemu_plugin_meminfo_t info,
                     uint64_t vaddr, void *udata)
{
	if (!qemu_plugin_mem_is_store(info)) return;
	struct qemu_plugin_hwaddr *hwaddr;
	hwaddr = qemu_plugin_get_hwaddr(info, vaddr);
	if (hwaddr) {
		uint64_t addr = qemu_plugin_hwaddr_phys_addr(hwaddr);
		if (addr >= haddr_lo && addr < haddr_hi) {
			rewrite_cnt++;	
			
			memcpy(buffer_ptr, &addr, 8);
			
			buffer_ptr += 8;
			buffer_remaining_size -= 8;
		}
	}
}

/**
 * On translation block new translation
 *
 * QEMU convert code by translation block (TB). By hooking here we can then hook
 * a callback on each instruction and memory access.
 */
static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    struct qemu_plugin_insn *insn;
    size_t n_insns = qemu_plugin_tb_n_insns(tb);

    for (size_t i = 0; i < n_insns; i++) {
        // char *insn_disas;
        // uint64_t insn_vaddr;

        /*
         * `insn` is shared between translations in QEMU, copy needed data here.
         * `output` is never freed as it might be used multiple times during
         * the emulation lifetime.
         * We only consider the first 32 bits of the instruction, this may be
         * a limitation for CISC architectures.
         */
        insn = qemu_plugin_tb_get_insn(tb, i);
        //insn_disas = qemu_plugin_insn_disas(insn);
        //insn_vaddr = qemu_plugin_insn_vaddr(insn);

        /* Register callback on memory read or write */
        qemu_plugin_register_vcpu_mem_cb(insn, vcpu_mem,
                                             QEMU_PLUGIN_CB_NO_REGS,
                                             QEMU_PLUGIN_MEM_RW, NULL);
        //g_free(insn_disas);
    }
}

static void plugin_exit(qemu_plugin_id_t id, void *p)
{
        fprintf(stderr, "\n");
        FILE *f = fopen(dump_file, "w");
        if (f) {
                fwrite(buffer, 1, buffer_size - buffer_remaining_size, f);
                fclose(f);
        } else {
                fprintf(stderr, "runtime_patch_monitor.c:%d: Could not open %s for writing\n", 
                                __LINE__, dump_file);
        }
        free(buffer);

        fprintf(stderr, "libruntime_patch_monitor.so:%s: rewrite_cnt=%lld\n", __FUNCTION__, 
                                rewrite_cnt);
        fprintf(stderr, "\n");
}

/**
 * Install the plugin
 */
QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t *info, int argc,
                                           char **argv)
{
	// Set up buffer
	buffer_size = (size_t) 10 * 1024 * 1024 * 1024;
	buffer = malloc(buffer_size);

    if (buffer == NULL) {
		fprintf(stderr, "runtime_patch_monitor.c:%d: Memory allocation failed\n", __LINE__);
                return 1;
    }

    buffer_remaining_size = buffer_size;
    buffer_ptr = buffer;
	
	// Register callbacks
    qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
	qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);

    return 0;
}
