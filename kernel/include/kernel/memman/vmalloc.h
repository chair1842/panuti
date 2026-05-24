#ifndef _KERNEL_MEMMAN_VMALLOC_H
#define _KERNEL_MEMMAN_VMALLOC_H

void vmalloc_init(void);
void* vmalloc_pg(void);
void vmalloc_free(void* addr);

#endif