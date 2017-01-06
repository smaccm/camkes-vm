/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(NICTA_GPL)
 */

/*
 * Hello World Component.
 *
 * Tests the vchan connection between the comonent (me) and a server that should
 * be running on linux component.
 */

#include <stdio.h>
#include <stdint.h>

#include <sel4/sel4.h>
#include <sel4utils/util.h>

#include <camkes.h>


void client_recv_ready_callback(seL4_Word badge) {
    printf("\n[%s, %d] %x\n", __func__, __LINE__, badge);
}

int run(void) {
    printf("Hello, World!\n");
    return 0;
}
