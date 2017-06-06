#include <kern/e1000.h>

// LAB 6: Your driver code here

volatile uint32_t *e1000_bar0;
static struct tx_desc tx_desc_ring[E1000_TXDESC_SIZE] __attribute__((aligned(16)));
static struct tx_pkt tx_pkt_buffer[E1000_TXDESC_SIZE] __attribute__((aligned(16)));
static struct rx_desc rx_desc_ring[E1000_RXDESC_SIZE] __attribute__((aligned(16)));
static struct rx_pkt rx_pkt_buffer[E1000_RXDESC_SIZE] __attribute__((aligned(16)));

static char* test_string = "qemu";

#define E1000_REG(reg) e1000_bar0[(reg) >> 2]

static void transmit_init() {
    int i;

    // transmition initialization
    memset(tx_desc_ring, 0, sizeof(tx_desc_ring));
    memset(tx_pkt_buffer, 0, sizeof(tx_pkt_buffer));
    for (i = 0; i < E1000_TXDESC_SIZE; i++) {
        tx_desc_ring[i].addr = PADDR(tx_pkt_buffer[i].pkt);
        tx_desc_ring[i].status |= E1000_TXD_STAT_DD;
    }

    E1000_REG(E1000_TDBAH) = 0;
    E1000_REG(E1000_TDBAL) = PADDR(tx_desc_ring);
    E1000_REG(E1000_TDLEN) = sizeof(tx_desc_ring);
    E1000_REG(E1000_TDH) = 0x0;
    E1000_REG(E1000_TDT) = 0x0;

    uint32_t tctl = E1000_TCTL_EN | E1000_TCTL_PSP | E1000_TCTL_CT | E1000_TCTL_COLD |
                    E1000_TCTL_CT_ETHERNET | E1000_TCTL_COLD_FDUPLEX;
    // actually ct is not needed
    E1000_REG(E1000_TCTL) = tctl;

    E1000_REG(E1000_TIPG) = E1000_TIPG_IPGT | E1000_TIPG_IPGR1 |
                        E1000_TIPG_IPGR2; // actually ipgr1 and ipgr2 is not needed
}

static void receive_init() {
    int i;

    memset(rx_desc_ring, 0, sizeof(rx_desc_ring));
    memset(rx_pkt_buffer, 0, sizeof(rx_pkt_buffer));
    for (i = 0; i < E1000_RXDESC_SIZE; i++) {
        rx_desc_ring[i].addr = PADDR(rx_pkt_buffer[i].pkt);
    }

    //E1000_REG(E1000_RAL0) = 0x12005452;
    //E1000_REG(E1000_RAH0) = 0x00005634 | E1000_RAH_AV;

    E1000_REG(E1000_RDBAL) = PADDR(rx_desc_ring);
    E1000_REG(E1000_RDBAH) = 0;
    E1000_REG(E1000_RDLEN) = sizeof(rx_desc_ring);
    E1000_REG(E1000_RDH) = 1;
    E1000_REG(E1000_RDT) = 0;

    uint32_t rctl = E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_SZ_2048 |
                        E1000_RCTL_SECRC;
    E1000_REG(E1000_RCTL) = rctl;
}

static void init_mac() {
    E1000_REG(E1000_EERD) = 0x0 << E1000_EEPROM_RW_ADDR_SHIFT;
    E1000_REG(E1000_EERD) |= E1000_EEPROM_RW_REG_START;
    while (!(E1000_REG(E1000_EERD) & E1000_EEPROM_RW_REG_DONE));
    E1000_REG(E1000_RAL0) = (E1000_REG(E1000_EERD) >> E1000_EEPROM_RW_REG_DATA) & 0xffff;

    E1000_REG(E1000_EERD) = 0x1 << E1000_EEPROM_RW_ADDR_SHIFT;
    E1000_REG(E1000_EERD) |= E1000_EEPROM_RW_REG_START;
    while (!(E1000_REG(E1000_EERD) & E1000_EEPROM_RW_REG_DONE));
    E1000_REG(E1000_RAL0) |= ((E1000_REG(E1000_EERD) >> E1000_EEPROM_RW_REG_DATA) & 0xffff) << 16;
    cprintf("mac: %x", E1000_REG(E1000_RAL0));

    E1000_REG(E1000_EERD) = 0x2 << E1000_EEPROM_RW_ADDR_SHIFT;
    E1000_REG(E1000_EERD) |= E1000_EEPROM_RW_REG_START;
    while (!(E1000_REG(E1000_EERD) & E1000_EEPROM_RW_REG_DONE));
    E1000_REG(E1000_RAH0) = (E1000_REG(E1000_EERD) >> E1000_EEPROM_RW_REG_DATA) & 0xffff;
    cprintf("%x\n", E1000_REG(E1000_RAH0));

    E1000_REG(E1000_RAH0) |= E1000_RAH_AV;
}

void e1000_get_mac_addr(uint32_t* hi, uint32_t* lo) {
    *lo = E1000_REG(E1000_RAL0);
    *hi = E1000_REG(E1000_RAH0);
}

int e1000_func_enable(struct pci_func *f) {
    int i;

    pci_func_enable(f);

    uint32_t bar0_pa = f->reg_base[0];
    uint32_t bar0_size = f->reg_size[0];

    boot_map_region(kern_pgdir, KSTACKTOP, bar0_size,
            bar0_pa, PTE_P|PTE_PCD|PTE_PWT|PTE_W);
    e1000_bar0 = (uint32_t*)KSTACKTOP;

    cprintf("e1000 status: 0x%08x\n", E1000_REG(E1000_STATUS));

    init_mac();
    transmit_init();
    receive_init();

    return 0;
}

static inline
int tx_desc_finished(int id) {
    return tx_desc_ring[id].status & E1000_TXD_STAT_DD;
}

int transmit_pkt(uint8_t *data, int len) {
    if (len > E1000_PKT_SIZE) {
        return -E_INVAL;
    }

    uint32_t tdt = E1000_REG(E1000_TDT);
    if (!tx_desc_finished(tdt)) {
        return -E_TXD_FULL;
    }

    memmove(tx_pkt_buffer[tdt].pkt, data, len);
    tx_desc_ring[tdt].length = len;
    tx_desc_ring[tdt].cmd |= E1000_TXD_CMD_RS;
    tx_desc_ring[tdt].status &= (~E1000_TXD_STAT_DD);
    tx_desc_ring[tdt].cmd |= E1000_TXD_CMD_EOP;

    E1000_REG(E1000_TDT) = (tdt+1)%E1000_TXDESC_SIZE;
    return 0;
}

static inline
int rx_desc_finished(int id) {
    return rx_desc_ring[id].status & E1000_RXD_STAT_DD;
}

int receive_pkt(uint8_t* data) {
    uint32_t rdt = (E1000_REG(E1000_RDT) + 1) % E1000_RXDESC_SIZE;
    if (!rx_desc_finished(rdt)) {
        return -E_RXD_NULL;
    }

    int len = rx_desc_ring[rdt].length;
    memmove(data, rx_pkt_buffer[rdt].pkt, len);

    rx_desc_ring[rdt].length = 0;
    rx_desc_ring[rdt].status &= ~E1000_RXD_STAT_DD;
    rx_desc_ring[rdt].status &= ~E1000_RXD_STAT_EOP;

    E1000_REG(E1000_RDT) = rdt;

    return len;
}
