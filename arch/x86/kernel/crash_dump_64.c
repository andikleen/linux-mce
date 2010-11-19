/*
 *	Memory preserving reboot related code.
 *
 *	Created by: Hariprasad Nellitheertha (hari@in.ibm.com)
 *	Copyright (C) IBM Corporation, 2004. All rights reserved
 */

#include <linux/errno.h>
#include <linux/crash_dump.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <asm/mce.h>

/* Stores the physical address of elf header of crash image. */
unsigned long long elfcorehdr_addr = ELFCORE_ADDR_MAX;

/**
 * copy_oldmem_page - copy one page from "oldmem"
 * @pfn: page frame number to be copied
 * @buf: target memory address for the copy; this can be in kernel address
 *	space or user address space (see @userbuf)
 * @csize: number of bytes to copy
 * @offset: offset in bytes into the page (based on pfn) to begin the copy
 * @userbuf: if set, @buf is in user address space, use copy_to_user(),
 *	otherwise @buf is in kernel address space, use memcpy().
 *
 * Copy a page from "oldmem". For this page, there is no pte mapped
 * in the current kernel. We stitch up a pte, similar to kmap_atomic.
 */
ssize_t copy_oldmem_page(unsigned long pfn, char *buf,
		size_t csize, unsigned long offset, int userbuf)
{
	void  *vaddr;
	struct page *p[2] = { NULL, NULL };
	unsigned long poff = (unsigned long)buf & ~PAGE_MASK;

	if (!csize)
		return 0;

	if (userbuf) {
		if (get_user_pages_fast((unsigned long)buf,
					((csize + poff) >> PAGE_SHIFT) + 1,
					1, p) < 0)
			return -EFAULT;
	}
	vaddr = ioremap_cache(pfn << PAGE_SHIFT, PAGE_SIZE);
	csize = -ENOMEM;
	if (!vaddr)
		goto out;

	/* Disable MCEs temporarily so that we don't fault on memory errors */
	get_cpu();
	mce_disable_error_reporting();

	if (userbuf) {
		unsigned len = min(csize, PAGE_SIZE - poff);

		memcpy(page_address(p[0]) + poff, vaddr + offset, len);
		if (p[1])
			memcpy(page_address(p[1]), vaddr + offset + len,
				csize - len);
	} else
		memcpy(buf, vaddr + offset, csize);

	mce_reenable_error_reporting();
	put_cpu();

out:
	if (p[0])
		put_page(p[0]);
	if (p[1])
		put_page(p[1]);

	set_iounmap_nonlazy();
	iounmap(vaddr);
	return csize;
}
