/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <sel4/sel4.h>
#include <camkes/dataport.h>
#include <lwip/udp.h>
#include <sync/sem-bare.h>
#include <string.h>
#include <camkes/sel4.h>

/*- set ep = alloc('ep', seL4_EndpointObject, read=True, write=True) -*/

/* Assume a function exists to get a dataport */
void */*? me.to_interface.name?*/_buf_buf(unsigned int id);

void lwip_lock();
void lwip_unlock();

/*- set bufs = configuration[me.to_instance.name].get('num_client_recv_bufs') -*/
/*- set clients = [] -*/
/*- for id, c in enumerate(composition.connections) -*/
    /*- if c.to_instance.name == me.to_instance.name and c.to_interface.name == me.to_interface.name -*/
        /*- if c.type.name == me.type.name -*/
            /*- set port = configuration[c.from_instance.name].get('%s_port' % c.from_interface.name) -*/
            /*- set client = configuration[c.from_instance.name].get('%s_attributes' % c.from_interface.name) -*/
            /*- set client = client.strip('"') -*/
            /*- set is_reader = False -*/
            /*- set instance = c.from_instance.name -*/
            /*- set interface = c.from_interface.name -*/
            /*- include 'global-endpoint.template.c' -*/
            /*- set aep = pop('aep') -*/
            /*- do clients.append( (client, port, aep) ) -*/
        /*- endif -*/
    /*- endif -*/
/*- endfor -*/

typedef struct udp_message {
    struct pbuf *pbuf;
    ip_addr_t addr;
    uint16_t port;
    struct udp_message *next;
}udp_message_t;

typedef struct udp_client {
    struct udp_pcb *upcb;
    int client_id;
    uint16_t port;
    int need_signal;
    seL4_CPtr aep;
    udp_message_t *free_head;
    udp_message_t *used_head;
    udp_message_t *used_tail;
    udp_message_t message_memory[ /*? bufs ?*/];
} udp_client_t;

static udp_client_t udp_clients[/*? len(clients) ?*/] = {
/*- for client,port,aep in clients -*/
    {.upcb = NULL, .client_id = /*? client ?*/, .port = /*? port ?*/, .need_signal = 1, .aep = /*? aep ?*/, .used_head = NULL},
/*- endfor -*/
};

static void udprecv(void *arg, struct udp_pcb *pcb, struct pbuf *p, ip_addr_t *addr, u16_t port) {
    udp_client_t *client = (udp_client_t*)arg;
    if (!client->free_head) {
        pbuf_free(p);
        return;
    }
    udp_message_t *m = client->free_head;
    client->free_head = client->free_head->next;

    m->pbuf = p;
    m->addr = *addr;
    m->port = port;
    m->next = NULL;

    if (client->need_signal) {
        seL4_Signal(client->aep);
        client->need_signal = 0;
    }

    if (!client->used_head) {
        client->used_head = client->used_tail = m;
    } else {
        client->used_tail->next = m;
        client->used_tail = m;
    }
}

void /*? me.to_interface.name ?*/__run(void) {
    while (1) {
        /*- set cnode = alloc_cap('cnode', my_cnode, write=True) -*/
        /*- set reply_cap_slot = alloc_cap('reply_cap_slot', None) -*/
        int len;
        int result UNUSED;
        seL4_Word badge;
        seL4_Wait(/*? ep ?*/, &badge);
        udp_client_t *client = NULL;
        for (int i = 0; i < /*? len(clients) ?*/ && !client; i++) {
            if (udp_clients[i].client_id == badge) {
                client = &udp_clients[i];
            }
        }
        assert(client);
        result = camkes_cnode_save_caller(/*? cnode ?*/, /*? reply_cap_slot ?*/, 32);
        assert(result == seL4_NoError);
        lwip_lock();
        len = 0;
        if (!client->used_head) {
            seL4_SetMR(0, -1);
            len = 1;
            client->need_signal = 1;
        } else {
            unsigned int packet_len = 0;
            void *p = /*? me.to_interface.name ?*/_buf_buf(badge);
            udp_message_t *m = client->used_head;
            client->used_head = client->used_head->next;
            if (!client->used_head) {
                client->need_signal = 1;
            }

            for (struct pbuf *q = m->pbuf; q; q = q->next) {
                memcpy(p + packet_len, q->payload, q->len);
                packet_len += q->len;
            }
            pbuf_free(m->pbuf);
            seL4_SetMR(0, client->used_head ? 0 : 1);
            seL4_SetMR(1, packet_len);
            seL4_SetMR(2, m->port);
            seL4_SetMR(3, m->addr.addr);
            len = 4;

            m->next = client->free_head;
            client->free_head = m;
        }
        seL4_Send(/*? reply_cap_slot ?*/, seL4_MessageInfo_new(0, 0, 0, len));
        lwip_unlock();
    }
}

void /*? me.to_interface.name ?*/__init(void) {
    int err;
    int i, j;
    lwip_lock();
    for (i = 0; i < /*? len(clients) ?*/; i++) {
        for (j = 0; j < /*? bufs ?*/; j++) {
            if (j == 0) {
                udp_clients[i].message_memory[j] =
                    (udp_message_t){.pbuf = NULL, .port = 0, .next = NULL};
            } else {
                udp_clients[i].message_memory[j] =
                    (udp_message_t){.pbuf = NULL, .port = 0, .next = &udp_clients[i].message_memory[j - 1]};
            }
        }
        udp_clients[i].free_head = &udp_clients[i].message_memory[/*? bufs ?*/ - 1];
        udp_clients[i].upcb = udp_new();
        assert(udp_clients[i].upcb);
        udp_recv(udp_clients[i].upcb, udprecv, &udp_clients[i]);
        err = udp_bind(udp_clients[i].upcb, NULL, udp_clients[i].port);
        assert(!err);
    }
    lwip_unlock();
}
