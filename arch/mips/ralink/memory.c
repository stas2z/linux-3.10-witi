/**************************************************************************
 *
 *  BRIEF MODULE DESCRIPTION
 *     memory setup for Ralink RT2880 solution
 *
 *  Copyright 2007 Ralink Inc. (bruce_chang@ralinktech.com.tw)
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 **************************************************************************
 * May 2007 Bruce Chang
 *
 * Initial Release
 *
 *
 *
 **************************************************************************
 */


#include <linux/init.h>
#include <linux/mm.h>
#include <linux/bootmem.h>
#include <linux/ioport.h>
#include <asm/bootinfo.h>
#include <asm/page.h>
#include <linux/sizes.h>

#include <asm/mach-ralink/prom.h>
#define DEBUG

enum surfboard_memtypes {
	surfboard_dontuse,
	surfboard_rom,
	surfboard_ram,
};
struct prom_pmemblock mdesc[PROM_MAX_PMEMBLOCKS];

#ifdef DEBUG
static char *mtypes[3] = {
	"Dont use memory",
	"Used ROM memory",
	"Free RAM memory",
};
#endif

/* References to section boundaries */
extern char _end;

#if defined(CONFIG_RT2880_ASIC) || defined(CONFIG_RT2880_FPGA)
#define RAM_FIRST       0x08000400  /* Leave room for interrupt vectors */
#define RAM_SIZE        CONFIG_RALINK_RAM_SIZE*1024*1024
#define RAM_END         (0x08000000 + RAM_SIZE)
#else
#define RAM_FIRST       0x00000400  /* Leave room for interrupt vectors */
#define RAM_SIZE        CONFIG_RALINK_RAM_SIZE*1024*1024
#define RAM_END         (0x00000000 + RAM_SIZE)
#endif
struct resource rt2880_res_ram = {
        .name = "RAM",
        .start = 0,
        .end = RAM_SIZE,
        .flags = IORESOURCE_MEM
};


#define PFN_ALIGN(x)    (((unsigned long)(x) + (PAGE_SIZE - 1)) & PAGE_MASK)


struct prom_pmemblock * __init prom_getmdesc(void)
{
	char *env_str;
	unsigned int ramsize, rambase;

	env_str = prom_getenv("ramsize");
	if (!env_str) { env_str = prom_getenv("memsize"); }
	if (!env_str) {
		ramsize = CONFIG_RALINK_RAM_SIZE * 1024 * 1024;
		prom_printf("[noenv] ramsize = %d MBytes\n", CONFIG_RALINK_RAM_SIZE );
	} else {
		prom_printf("[env] ramsize = %s\n", env_str);
		ramsize = memparse(env_str, NULL);
		if ( ramsize < SZ_1M ) { ramsize *= SZ_1M; }
	}

	env_str = prom_getenv("rambase");
	if (!env_str) {
#if defined(CONFIG_RT2880_ASIC) || defined(CONFIG_RT2880_FPGA)
		rambase = 0x08000000;
#else
		rambase = 0x00000000;
#endif 
		prom_printf("rambase not set, set to default (0x%08x)\n", rambase);
	} else {
#ifdef DEBUG
		prom_printf("rambase = %s\n", env_str);
#endif
		rambase = simple_strtol(env_str, NULL, 0);
	}

	memset(mdesc, 0, sizeof(mdesc));

	mdesc[0].type = surfboard_ram;
	mdesc[0].base = rambase;
	mdesc[0].size = ramsize;

#if defined(CONFIG_MT7621_ASIC)
	if (ramsize > 448 * SZ_1M){
		mdesc[0].size = 448 * SZ_1M;
#if defined(CONFIG_HIGHMEM)
		mdesc[1].type = surfboard_ram;
		mdesc[1].base = 0x20000000;
		mdesc[1].size = 64 * SZ_1M;
#endif
	}
#endif

	return &mdesc[0];
}
static int __init prom_memtype_classify (unsigned int type)
{
	switch (type) {
	case surfboard_ram:
		return BOOT_MEM_RAM;
	case surfboard_rom:
		return BOOT_MEM_ROM_DATA;
	default:
		return BOOT_MEM_RESERVED;
	}
}

void __init prom_meminit(void)
{
	struct prom_pmemblock *p;
#ifdef DEBUG
	struct prom_pmemblock *psave;
#endif

	
	p = prom_getmdesc();
#ifdef DEBUG
	prom_printf("MEMORY DESCRIPTOR dump:\n");
	psave = p;	/* Save p */
	while (p->size) {
		int i = 0;
		prom_printf("[%d,%p]: base<%08lx> size<%08lx> type<%s>\n",
			    i, p, p->base, p->size, mtypes[p->type]);
		p++;
		i++;
	}
	p = psave;	/* Restore p */

#endif
	while (p->size) {
		long type;
		unsigned long base, size;

		type = prom_memtype_classify (p->type);
		base = p->base;
		size = p->size;
		add_memory_region(base, size, type);
                p++;
	}
}

void __init prom_free_prom_memory(void)
{
        unsigned long addr;
        int i;

        for (i = 0; i < boot_mem_map.nr_map; i++) {
                if (boot_mem_map.map[i].type != BOOT_MEM_ROM_DATA)
                        continue;

                addr = boot_mem_map.map[i].addr;
                free_init_pages("prom memory",
                                addr, addr + boot_mem_map.map[i].size);
        }
}
#if 0
void __init
prom_free_prom_memory (void)
{
	int i;
	unsigned long freed = 0;
	unsigned long addr;

	for (i = 0; i < boot_mem_map.nr_map; i++) {
		if (boot_mem_map.map[i].type != BOOT_MEM_ROM_DATA)
			continue;

		addr = boot_mem_map.map[i].addr;
		while (addr < boot_mem_map.map[i].addr
			      + boot_mem_map.map[i].size) {
			ClearPageReserved(virt_to_page(__va(addr)));
			set_page_count(virt_to_page(__va(addr)), 1);
			free_page((unsigned long)__va(addr));
			addr += PAGE_SIZE;
			freed += PAGE_SIZE;
		}
	}
	printk("Freeing prom memory: %ldkb freed\n", freed >> 10);
}
#endif
