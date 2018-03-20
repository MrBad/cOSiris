/**
 * NE2000 - ne2000 PCI compatible network driver.
 * DOCS: http://www.ethernut.de/pdf/8019asds.pdf
 *       http://www.osdever.net./documents/DP8390Overview.pdf
 * ---------------------------------------------------------------------------
 * Copyright (c) 2018 MrBadNews <viorel dot irimia at gmail dot come>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <string.h>
#include <stdlib.h>
#include "console.h"
#include "pci.h"
#include "driver.h"
#include "i386.h"
#include "delay.h"
#include "port.h"
#include "ne2000.h"
#include "irq.h"
#include "net.h"

/* Page 0 registers */
enum {
    NE_CR = 0x0,        /* Command Register */
    NE_CLDA0 = 0x1,     /* Current Local DMA Address, low (r) */
    NE_PSTART = 0x1,    /* Page START register (w) */
    NE_CLDA1 = 0x02,    /* Current Local DMA Address, high (r) */
    NE_PSTOP = 0x02,    /* Page STOP register (w)*/
    NE_BNRY = 0x03,     /* Boundary pointer (r/w) */
    NE_TSR = 0x04,      /* Transmit Status Register (r) */
    NE_TPSR = 0x04,     /* Transmit Page Start Register (w) */
    NE_NCR = 0x05,      /* Number of Collisions Register (r) */
    NE_TBCR0 = 0x05,    /* Transmit Byte Count Register, low (r) */
    NE_FIFO = 0x06,     /* FIFO register (r) */
    NE_TBCR1 = 0x06,    /* Transmit Byte Count, high (r) */
    NE_ISR = 0x07,      /* Interrupt Status Register (r/w) */
    NE_CRDA0 = 0x08,    /* Current Remote DMA Address, high (r) */
    NE_RSAR0 = 0x08,    /* Remote Start Address Register, low, (w) */
    NE_CRDA1 = 0x09,    /* Current Remote DMA Address, high (r) */
    NE_RSAR1 = 0x09,    /* Remote Start Address Register, high (w) */
    NE_RBCR0 = 0x0A,    /* Remote Byte Count Register, low (r/w)*/
    NE_RBCR1 = 0x0B,    /* Remote Byte Count Register, high (r/w) */
    NE_RSR = 0x0C,      /* Receive Status Register (r) */
    NE_RCR = 0x0C,      /* Receive Configuration Register (w) */
    NE_CNTR0 = 0x0D,    /* Frame alignment error counter register (r) */
    NE_TCR = 0x0D,      /* Transmit Configuration Register (w) */
    NE_CNTR1 = 0x0E,    /* CRC Error Tally Counter Register (r) */
    NE_DCR = 0x0E,      /* Data Configuration Register (w) */
    NE_CNTR2 = 0x0F,    /* Missed packet counter (r) */
    NE_IMR = 0x0F,      /* Interrupt Mask Register (w)*/
} ne_regs_pg0;

/* Page 1 registers */
enum {
    NE_PAR0 = 0x01,     /* Physical Address Registers 0-6 (r/w) */
    NE_PAR1 = 0x02,
    NE_PAR2 = 0x03,
    NE_PAR3 = 0x04,
    NE_PAR4 = 0x05,
    NE_PAR5 = 0x06,
    NE_CURR = 0x07,     /* Current Page Register (r/w) */
    NE_MAR0 = 0x08,     /* Multicast Address Registers 0-6 (r/w)*/
    NE_MAR1 = 0x09,
    NE_MAR2 = 0x0A,
    NE_MAR3 = 0x0B,
    NE_MAR4 = 0x0C,
    NE_MAR5 = 0x0D,
    NE_MAR6 = 0x0E,
    NE_MAR7 = 0x0F,
} ne_regs_pg1;

/* Page 2 registers */
enum {
    NE_RNPP = 0x03,     /* Remote Next Packet Pointer */
    NE_LNPP = 0x05,     /* Local Next Packet Pointer */
    NE_ACU = 0x06,      /* Address Counter Upper */
    NE_ACL = 0x07,      /* Address Counter Lower */
} ne_regs_pg2;

/* Command register (CR) bits, r/w any page */
enum {
    NE_CR_STP = 0001,   /* Stop */
    NE_CR_STA = 0002,   /* Start */
    NE_CR_TXP = 0004,   /* Transmit Packet */
    NE_CR_RD0 = 0010,   /* Remote DMA command, bit 0 */
    NE_CR_RD1 = 0020,   /* Remote DMA command, bit 1 */
    NE_CR_RD2 = 0040,   /* Remote DMA command, bit 2 */
    NE_CR_PS0 = 0100,   /* Page Select bit 0 */
    NE_CR_PS1 = 0200,   /* Page Select bit 1 */
} ne_cr_bits;

#define NE_CR_READ (NE_CR_RD0)   /* Remote read command */
#define NE_CR_WRITE (NE_CR_RD1)  /* Remote write command */
#define NE_CR_SEND (NE_CR_RD0 | NE_CR_RD1) /* Send packet command */
#define NE_CR_DMA_ABORT (NE_CR_RD2)  /* Abort/complete remote DMA */
#define NE_CR_DMA_WR (NE_CR_RD1)
#define NE_CR_DMA_RD (NE_CR_RD0)
#define NE_CR_PG0 0x0
#define NE_CR_PG1 (NE_CR_PS0)
#define NE_CR_PG2 (NE_CR_PS0 | NE_CR_PS1)

/* Interrupt Status Register (ISR) bits, r/w in page 0 */
enum {
    NE_ISR_PRX = 0001,  /* Packet received with no error */
    NE_ISR_PTX = 0002,  /* Packet transmitted with no error */
    NE_ISR_RXE = 0004,  /* set on rx errors: CRC, frame align, missed packet */
    NE_ISR_TXE = 0010,  /* set on tx errors */
    NE_ISR_OVW = 0020,  /* set when rx buffer exhausted */
    NE_ISR_CNT = 0040,  /* set when MSB of tally counters has been set */
    NE_ISR_RDC = 0100,  /* set when DMA op completed */
    NE_ISR_RST = 0200,  /* reset status */
} ne_isr_bits;

/* IMR(w pg0, r pg2) has same bits as ISR, setting individual bits to 1
 * will enable coresponding interrupts. */
enum {
    NE_IMR_PRXE = 0001, /* Packet Reveived interrupt Enable */
    NE_IMR_PTXE = 0002, /* Packet Transmitted interrupt Enable */
    NE_IMR_RXEE = 0004, /* Receive Error interrupt Enable */
    NE_IMR_TXEE = 0010, /* Transmit Error interrupt Enable */
    NE_IMR_OVWE = 0020, /* OVerwrite Warning interrupt Enable */
    NE_IMR_CNTE = 0040, /* CouNTer overflow interrupt Enable */
    NE_IMR_RDCE = 0100, /* Remote DMA Complete interrupt Enable */
} ne_imr_bits;

/* Data Configuration Register (DCR) bits, w in pg0, r in pg2*/
enum {
    NE_DCR_WTS = 0001, /* Word Transfer Select; 1: word, 0: byte DMA transfer */
    NE_DCR_BOS = 0002, /* Byte Order Select; 0: MSB on 15-8 */
    NE_DCR_LAS = 0004, /* Long Address Select */
    NE_DCR_LS  = 0010, /* Loopback Select; 0 loopback, 1 normal */
    NE_DCR_ARM = 0020, /* Auto-initialize remote */
    NE_DCR_FT0 = 0040, /* FIFO Threshold select, bit 0 */
    NE_DCR_FT1 = 0100, /* FIFO Threshold select, bit 1 */
} ne_dcr_bits;

#define NE_DCR_2B 0
#define NE_DCR_4B (NE_DCR_FT1)
#define NE_DCR_8B (NE_DCR_FT0)
#define NE_DCR_12B (NE_DCR_FT0 | NE_DCR_FT1)

/* Transmit Configuration Register (TCR) bits, w in pg0, r in pg 2 */
enum {
    NE_TCR_NORMAL = 0000,
    NE_TCR_CRC = 0001, /* Inhibit transmitter CRC, if set */
    NE_TCR_LB0 = 0002, /* Loopback control, bit 0 */
    NE_TCR_LB1 = 0004, /* Loopback control, bit 1 */
    NE_TCR_ATD = 0010, /* Auto Transmit Disable */
    NE_TCR_OFST= 0020, /* Collision Offset Enable */
} ne_tcr_bits;

/* Transmit Status Register (TSR) bits, r in pg0 */
enum {
    NE_TSR_PTX = 0001, /* Transmit completes with no errors */
    NE_TSR_COL = 0004, /* Transmission collided with other station */
    NE_TSR_ABT = 0010, /* Transmission aborted */
    NE_TSR_CRS = 0020, /* Carrier Sense Lost */
    NE_TSR_FU  = 0040,
    NE_TSR_CDH = 0100, /* CD Heartbeat */
    NE_TSR_OWC = 0200, /* Out of Window Collision */
} ne_tsr_bits;

/* Receiver Configuration Register (RCR) bits, w in pg0, r in pg2 */
enum {
    NE_RCR_SEP = 0001, /* Accept packets with errors */
    NE_RCR_AR =  0002, /* Accept packets with length < 64 bytes */
    NE_RCR_AB =  0004, /* Accept packets with broadcast destination address */
    NE_RCR_AM =  0010, /* Accept packets with multicast destination address */
    NE_RCR_PRO = 0020, /* Promiscuous mode if 1, else accept only PAR0-5 addr */
    NE_RCR_MON = 0040, /* Monitor mode */
} ne_rcr_bits;

/* Receiver Status Register (RSR) bits, r in pg0 */
enum {
    NE_RSR_PRX = 0001, /* Received with no errors */
    NE_RSR_CRC = 0002, /* Packet received with CRC errors */
    NE_RSR_FAE = 0004, /* Frame Alignment Error */
    NE_RSR_FO  = 0010, /* Fifo Overrun */
    NE_RSR_MPA = 0020, /* Missed Packet */
    NE_RSR_PHY = 0040, /* Physical addr, set on multicast or broadcast packet */
    NE_RSR_DIS = 0100, /* Receiver Disabled */
    NE_RSR_DFR = 0200, /* Defferring. Set when carrier or collision detected. */
} ne_rsr_bits;

/* XXX size and start of on chip memory */
#define NE_DATA 0x10
#define NE_PG_SIZE 256      /* Size of a page, in bytes */
#define NE_START 0x4000     /* On chip start memory. QEMU starts at 16k */
#define NE_DATA_SIZE 0x4000 /* Size of the on chip buffer, 16k. QEMU use 32k */
#define NE_RESET 0x1F

/* Reserve 8 256 bytes pages for TX buffer. */
#define TX_START (NE_START / NE_PG_SIZE)
#define TX_SIZE 8
/* RX ring buffer starts after TX buffer. Values are in 256 pages. */
#define RX_START (TX_START + TX_SIZE)
#define RX_STOP (TX_START + (NE_DATA_SIZE / NE_PG_SIZE))

#define RESET_RETRIES 0x1000
#define MAC_LEN 6
#define ARR_LEN(arr) ((int)(sizeof(arr) / sizeof(*arr)))

struct driver ne2000_driver;

struct eth_stats {
    uint32_t sent;  /* Number of successfuly transmitted frames */
    uint32_t sent_errs;
    uint32_t collisions;
    uint32_t aborted_errs;
    uint32_t carrier_errs;
    uint32_t underrruns;
    uint32_t heartbeat_errs;
    uint32_t sent_window_errs;
    uint32_t recv;
    uint32_t recv_errs;
    uint32_t recv_crc_errs;
    uint32_t recv_frame_errs;
    uint32_t recv_missed;
    uint32_t recv_overruns;
};

/* ne2000 structure */
struct ne2000 {
    int addr;   /* Card IO base address */
    int irq;
    int nic;    /* NIC id */
    spin_lock_t tx_lock;
    uint8_t mac[MAC_LEN];   /* MAC address */
    uint8_t attached;   /* Is the driver attached to card? */
    uint8_t up;         /* Is the card up? */
    uint8_t tx_pstart;  /* TX (256) page 0. */
    uint8_t rx_pstart;  /* RX start page 0 */
    uint8_t rx_pstop;   /* RX end page */
    struct eth_stats stats;
};

struct recv_hdr {
    uint8_t stat;
    uint8_t next;
    uint8_t rbcl;
    uint8_t rbch;
};

/* List of ne2000 compatible devices */
struct pci_id ne2000_compat[] = {
    { 0x10ec, 0x8029 }, // qemu RTL8029
};

/* An ARP test packet */
uint8_t test_packet[] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x52, 0x54, 0x00, 0x12, 0x34, 0x56,
    0x08, 0x06,
    0x00, 0x01,
    0x08, 0x00,
    0x06, 
    0x04,
    0x00, 0x01,
    0x52, 0x54, 0x00, 0x12, 0x34, 0x56,
    10, 0, 2, 15,
    0, 0, 0, 0, 0, 0,
    10, 0, 2, 1,
};

/* Prototypes */
void ne2000_handler(struct iregs *regs);
int ne2000_up(struct ne2000 *ne);
int ne2000_down(struct ne2000 *ne);
int ne2000_send(struct ne2000 *ne, void *buf, uint16_t len);
int ne2000_recv(struct ne2000 *ne);

/* Net Iface bindings */
int ne2000_netif_up(netif_t *netif)
{
    struct ne2000 *ne = netif->priv;
    return ne2000_up(ne);
}

int ne2000_netif_down(netif_t *netif)
{
    struct ne2000 *ne = netif->priv;
    return ne2000_down(ne);
}

int ne2000_netif_send(netif_t *netif, void *buf, int len)
{
    struct ne2000 *ne = netif->priv;
    return ne2000_send(ne, buf, len);
}

/**
 * Writes size bytes from buf to chip memory ne_addr
 * Returns number of bytes written
 */
int ne2000_put(struct ne2000 *ne, void *buf, int ne_addr, int size)
{
    int n;
    uint16_t c;

    if (!ne->up)
        return -1;
    outb(ne->addr + NE_RBCR0, size & 0xFF);
    outb(ne->addr + NE_RBCR1, (size >> 8) & 0xFF);
    outb(ne->addr + NE_RSAR0, ne_addr & 0xFF);
    outb(ne->addr + NE_RSAR1, (ne_addr >> 8) & 0xFF);
    outb(ne->addr + NE_CR, NE_CR_PG0 | NE_CR_DMA_WR | NE_CR_STA);
    n = port_write(ne->addr + NE_DATA, buf, size / 2);
    n = n << 1;
    if (size & 0x1) {
        c = *((char *) buf + n);
        outw(ne->addr + NE_DATA, c);
        n++;
    }

    return n;
}

/**
 * Reads size bytes from chip memory addr into buf
 * Returns number of bytes read
 */
int ne2000_get(struct ne2000 *ne, void *buf, int ne_addr, int size)
{
    int n;
    uint16_t c;

    if (!ne->up)
        return -1;

    outb(ne->addr + NE_RBCR0, size & 0xFF);
    outb(ne->addr + NE_RBCR1, (size >> 8) & 0xFF);
    outb(ne->addr + NE_RSAR0, ne_addr & 0xFF);
    outb(ne->addr + NE_RSAR1, (ne_addr >> 8) & 0xFF);
    outb(ne->addr + NE_CR, NE_CR_PG0 | NE_CR_DMA_RD | NE_CR_STA);

    n = port_read(ne->addr + NE_DATA, buf, size / 2);
    n = n << 1;
    if (size & 0x1) {
        c = inw(ne->addr + NE_DATA);
        *((char *) buf + n) = (char) c;
        n++;
    }

    return n;
}

/* Init driver. Called once per ne2000 type. */
static int ne2000_init(struct driver *driver)
{
    driver->inited = true;
    return 0;
}

/* Reset the card */
static int ne2000_reset(uint16_t ioaddr)
{
    int i;

    outb(ioaddr + NE_RESET, inb(ioaddr + NE_RESET)); /* Reset card */
    for (i = 0; i < RESET_RETRIES; i++)
        if ((inb(ioaddr + NE_ISR) & NE_ISR_RST) != 0) /* Wait for reset */
            break;

    return (i == RESET_RETRIES ? -1 : 0);
}

/**
 * Gets the base card io address from dev
 * Usually this should be first bar, but just in case
 */
static uint16_t ne2000_getio(struct pci_dev *dev)
{
    int i;
    uint16_t ioaddr = -1;

    for (i = 0; i < ARR_LEN(dev->bars); i++) {
        if (dev->bars[i].type == PCI_BAR_IO) {
            ioaddr = dev->bars[i].base;
            break;
        }
    }

    return ioaddr;
}

/**
 * Probe if pci device can be handled by this driver
 */
static int ne2000_probe(struct pci_dev *dev)
{
    int i, ioaddr;

    for (i = 0; i < ARR_LEN(ne2000_compat); i++)
        if (ne2000_compat[i].vendor_id == dev->vendor_id
            && ne2000_compat[i].device_id == dev->device_id)
            break;
    if (i == ARR_LEN(ne2000_compat))
        return -1;

    if ((ioaddr = ne2000_getio(dev)) < 0)
        return -1;

    if (ne2000_reset(ioaddr) < 0)
        return -1;

    kprintf("Found ne2000 on bus:%#x, slot: %#x, "
            "vendor_id: %#x, device_id: %#x\n",
            dev->bus, dev->slot, dev->vendor_id, dev->device_id);

    return 0;
}

/**
 * Attach an instance of the driver to pci device
 */
int ne2000_attach(struct pci_dev *dev)
{
    unsigned int i;
    struct ne2000 *ne = NULL;
    kprintf("attach ne2000\n");

    if (!(ne = calloc(1, sizeof(*ne))))
        return -1;
    if ((ne->addr = ne2000_getio(dev)) < 0) {
        free(ne);
        return -1;
    }
    if (ne2000_reset(ne->addr) < 0) {
        free(ne);
        return -1;
    }
    ne->irq = dev->irq;

    /* Configure TCR, DCR and put receiver in monitor mode */
    outb(ne->addr + NE_TCR, 0);
    outb(ne->addr + NE_RCR, NE_RCR_MON);
    outb(ne->addr + NE_DCR, NE_DCR_8B | NE_DCR_LS | NE_DCR_WTS);

    /* Do a read, to get MAC address */
    outb(ne->addr + NE_RBCR0, MAC_LEN * 2);
    outb(ne->addr + NE_RBCR1, 0);
    outb(ne->addr + NE_RSAR0, 0);
    outb(ne->addr + NE_RSAR1, 0);
    outb(ne->addr + NE_CR, NE_CR_READ | NE_CR_PG0 |  NE_CR_STA);

    for (i = 0; i < MAC_LEN; i++)
        ne->mac[i] = inb(ne->addr + NE_DATA);

    kprintf("MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n",
            ne->mac[0], ne->mac[1], ne->mac[2],
            ne->mac[3], ne->mac[4], ne->mac[5]);

    irq_install_handler(ne->irq, ne2000_handler);
    ne->attached = 1;
    /* Register it to global network interface array */
    netifs[nics].id = nics;
    netifs[nics].up = ne2000_netif_up;
    netifs[nics].down = ne2000_netif_down;
    netifs[nics].send = ne2000_netif_send;
    netifs[nics].priv = ne;
    memcpy(&netifs[nics].mac, ne->mac, sizeof(netifs[nics].mac));
    ne->nic = nics++;
    dev->priv = ne;
    if (ne2000_up(ne) < 0) {
        kprintf("Cannot bring up the network interface\n");
        return -1;
    }

    return 0;
}

/**
 * Bring the card up
 */
int ne2000_up(struct ne2000 *ne)
{
    int i;

    if (!ne->attached)
        return -1;

    ne->tx_pstart = TX_START;
    ne->rx_pstart = RX_START;
    ne->rx_pstop = RX_STOP;

    outb(ne->addr + NE_CR, NE_CR_DMA_ABORT | NE_CR_STP | NE_CR_PG0);
    outb(ne->addr + NE_DCR, NE_DCR_WTS | NE_DCR_8B | NE_DCR_LS);
    outb(ne->addr + NE_RBCR0, 0);
    outb(ne->addr + NE_RBCR1, 0);
    outb(ne->addr + NE_RCR, NE_RCR_AB);
    outb(ne->addr + NE_TCR, NE_TCR_LB0);

    outb(ne->addr + NE_BNRY, ne->rx_pstart);
    outb(ne->addr + NE_PSTART, ne->rx_pstart);
    outb(ne->addr + NE_PSTOP, ne->rx_pstop);

    outb(ne->addr + NE_ISR, 0xFF);
    outb(ne->addr + NE_IMR, NE_IMR_PRXE | NE_IMR_PTXE | NE_IMR_RXEE
            | NE_IMR_TXEE | NE_IMR_OVWE | NE_IMR_PRXE);

    outb(ne->addr + NE_CR, NE_CR_PG1 | NE_CR_DMA_ABORT | NE_CR_STP);

    for (i = 0; i < MAC_LEN; i++)
        outb(ne->addr + NE_PAR0 + i, ne->mac[i]);
    for (i = NE_MAR0; i < NE_MAR7; i++)
        outb(ne->addr + i, 0xFF);
    outb(ne->addr + NE_CURR, ne->rx_pstart + 1);
    outb(ne->addr + NE_CR, NE_CR_DMA_ABORT | NE_CR_STA | NE_CR_PG0);
    outb(ne->addr + NE_TCR, NE_TCR_NORMAL);

    inb(ne->addr + NE_CNTR0);
    inb(ne->addr + NE_CNTR1);
    inb(ne->addr + NE_CNTR2);

    ne->up = 1;
#if 0
    ne2000_send(dev, test_packet, sizeof(test_packet));
#endif

    return 0;
}

/**
 * Bring the card down
 */
int ne2000_down(struct ne2000 *ne)
{
    if (!ne->up)
        return -1;
    outb(ne->addr + NE_CR, NE_CR_PG0 | NE_CR_DMA_ABORT | NE_CR_STP);
    outb(ne->addr + NE_IMR, 0);
    outb(ne->addr + NE_ISR, 0xFF);
    ne->up = 0;

    return 0;
}

/**
 * Card interrupt handler
 */
void ne2000_handler(struct iregs *regs)
{
    int i, irq;
    int isr, tsr;
    struct pci_dev *dev;
    struct ne2000 *ne = NULL;

    irq = regs->int_no - 0x20;
    for (i = 0; i < pci_devs.idx; i++) {
        dev = &pci_devs.devs[i];
        if (dev->irq == irq) {
            ne = dev->priv;
            break;
        }
    }
    if (!ne)
        return;

    isr = inb(ne->addr + NE_ISR);
    outb(ne->addr + NE_ISR, 0xFF);      /* ACK interrupt */

    if (isr & NE_ISR_PTX) {
        ne->stats.sent++;
    } else if (isr & NE_ISR_TXE) {
        tsr = inb(ne->addr + NE_TSR);
        if (tsr & NE_TSR_COL)
            ne->stats.collisions++;
        if (tsr & NE_TSR_ABT)
            ne->stats.aborted_errs++;
        if (tsr & NE_TSR_CRS)
            ne->stats.carrier_errs++;
        if (tsr & NE_TSR_FU)
            ne->stats.underrruns++;
        if (tsr & NE_TSR_CDH)
            ne->stats.heartbeat_errs++;
        if (tsr & NE_TSR_OWC)
            ne->stats.sent_window_errs++;
    }
    if (isr & (NE_ISR_PTX | NE_ISR_TXE))
        spin_unlock(&ne->tx_lock);
    if (isr & NE_ISR_PRX) {
        ne2000_recv(ne);
    } else if (isr & NE_ISR_RXE) {
        ne->stats.recv_errs++;
    }
    if (isr & NE_ISR_CNT) {
        ne->stats.recv_frame_errs += inb(ne->addr + NE_CNTR0);
        ne->stats.recv_crc_errs += inb(ne->addr + NE_CNTR1);
        ne->stats.recv_missed += inb(ne->addr + NE_CNTR2);
    }
    //kprintf("ne2000 interrupt [isr: %b]\n", isr);
}

/**
 * Send a packet in buf
 */
int ne2000_send(struct ne2000 *ne, void *buf, uint16_t len)
{
    if (!ne->up)
        return -1;

    spin_lock(&ne->tx_lock);
    ne2000_put(ne, buf, ne->tx_pstart * NE_PG_SIZE, len);

    outb(ne->addr + NE_TPSR, ne->tx_pstart);
    outb(ne->addr + NE_TBCR0, len & 0xFF);
    outb(ne->addr + NE_TBCR1, (len >> 8) & 0xFF);
    outb(ne->addr + NE_CR, NE_CR_TXP | NE_CR_STA);

    return 0;
}

static int ne2000_recv_frame(struct ne2000 *ne, int page, int size)
{
    int pg_end, size1, n;
    struct net_buf *net_buf;

    if (!ne->up)
        return -1;

    if (!(net_buf = net_buf_alloc(size, ne->nic)))
        return -1;
    pg_end = page + size / NE_PG_SIZE;
    if (pg_end >= ne->rx_pstop) {
        size1 = (ne->rx_pstop - page) * NE_PG_SIZE;
        n = ne2000_get(ne, net_buf->data, page * NE_PG_SIZE+4, size1);
        n += ne2000_get(ne, net_buf->data + size1, ne->rx_pstart * NE_PG_SIZE,
                size - size1);
    } else {
        n = ne2000_get(ne, net_buf->data, page * NE_PG_SIZE+4, size);
    }
    if (!netq_push(net_buf)) {
        net_buf_free(net_buf);
        return -1;
    }
    ne->stats.recv++;

    return n;
}

/**
 * Called from interrupt, to grab a frame from the device
 * and push it into network queue (netq)
 */
int ne2000_recv(struct ne2000 *ne)
{
    int bnry, curr, next, size;
    struct recv_hdr recv_hdr;

    if (!ne->up)
        return -1;

    for (;;) {
        outb(ne->addr + NE_CR, NE_CR_PG1 | NE_CR_DMA_ABORT | NE_CR_STA);
        curr = inb(ne->addr + NE_CURR);
        outb(ne->addr + NE_CR, NE_CR_PG0 | NE_CR_DMA_ABORT | NE_CR_STA);
        bnry = inb(ne->addr + NE_BNRY);
        bnry += 1;
        if (bnry == ne->rx_pstop)
            bnry = ne->rx_pstart;
        if (curr == bnry)
            break;
        ne2000_get(ne, &recv_hdr, bnry * NE_PG_SIZE, sizeof(recv_hdr));
#if 0
        kprintf("curr: %x, bnry: %x\n", curr, bnry);
        kprintf("pstart: %x, pstop: %x\n", ne->rx_pstart, ne->rx_pstop);
        kprintf("next: %x, stat: %x, rbch: %x, rbcl: %x\n", 
                recv_hdr.next, recv_hdr.stat, recv_hdr.rbch, recv_hdr.rbcl);
#endif
        size = (((uint32_t)recv_hdr.rbch) << 8 | recv_hdr.rbcl);
        size -= sizeof(recv_hdr);
        next = recv_hdr.next;

        if (size < 60 || size > 1518) {
            kprintf("invalid packet size: %d\n", size);
            next = curr;
        } else if (next < ne->rx_pstart || next > ne->rx_pstop) {
            kprintf("invalid next frame: %d\n", next);
            next = curr;
        } else if (recv_hdr.stat & NE_RSR_FO) {
            kprintf("FIFO overrun!\n");
            ne->stats.recv_overruns++;
            next = curr;
        } else if (recv_hdr.stat & NE_RSR_PRX) {
            ne2000_recv_frame(ne, bnry, size);
        }
        if (next == ne->rx_pstart)
            next = ne->rx_pstop - 1;
        else 
            next--;

        outb(ne->addr + NE_BNRY, next);
    }

    return 0;
}

struct driver ne2000_driver = {
    .name = "ne2000 PCI",
    .inited = false,
    .init = ne2000_init,
    .probe = ne2000_probe,
    .attach = ne2000_attach,
};

