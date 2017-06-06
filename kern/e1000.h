#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <inc/x86.h>
#include <inc/assert.h>
#include <inc/string.h>
#include <kern/pci.h>
#include <kern/pcireg.h>
#include <kern/pmap.h>
#include <inc/memlayout.h>
#include <inc/mmu.h>
#include <kern/e1000.h>
#include <inc/error.h>

#define E1000_VENDOR    0x8086
#define E1000_DEVICE    0x100e
#define E1000_TXDESC_SIZE   64
#define E1000_RXDESC_SIZE   128
#define E1000_PKT_SIZE  1518
#define E1000_REC_PKT_SIZE  2048

/* regs */
#define E1000_STATUS   0x00008  /* Device Status - RO */
#define E1000_TDBAL    0x03800  /* TX Descriptor Base Address Low - RW */
#define E1000_TDBAH    0x03804  /* TX Descriptor Base Address High - RW */
#define E1000_TDLEN    0x03808  /* TX Descriptor Length - RW */
#define E1000_TDH      0x03810  /* TX Descriptor Head - RW */
#define E1000_TDT      0x03818  /* TX Descripotr Tail - RW */
#define E1000_TCTL     0x00400  /* TX Control - RW */
#define E1000_TIPG     0x00410  /* TX Inter-packet gap -RW */
#define E1000_RAL0     0x05400  /* Receive Address - Low 0 */
#define E1000_RAH0     0x05404  /* Receive Address - High 0 */
#define E1000_RDBAL    0x02800  /* RX Descriptor Base Address Low - RW */
#define E1000_RDBAH    0x02804  /* RX Descriptor Base Address High - RW */
#define E1000_RDLEN    0x02808  /* RX Descriptor Length - RW */
#define E1000_RDH      0x02810  /* RX Descriptor Head - RW */
#define E1000_RDT      0x02818  /* RX Descriptor Tail - RW */
#define E1000_RCTL     0x00100  /* RX Control - RW */
#define E1000_EERD     0x00014  /* EEPROM Read - RW */


#define E1000_EEPROM_RW_REG_DATA   16   /* Offset to data in EEPROM read/write registers */
#define E1000_EEPROM_RW_REG_DONE   0x10 /* Offset to READ/WRITE done bit */
#define E1000_EEPROM_RW_REG_START  1    /* First bit for telling part to start operation */
#define E1000_EEPROM_RW_ADDR_SHIFT 8    /* Shift to the address bits */


#define E1000_RAH_AV  0x80000000        /* Receive descriptor valid */

/* Transmit Control */
#define E1000_TCTL_EN     0x00000002    /* enable tx */
#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000ff0    /* collision threshold */
#define E1000_TCTL_COLD   0x003ff000    /* collision distance */

#define E1000_TCTL_CT_ETHERNET 0x00000100
#define E1000_TCTL_COLD_FDUPLEX 0x00040000


/* Receive Control */
#define E1000_RCTL_EN             0x00000002    /* enable */
#define E1000_RCTL_LPE            0x00000020    /* long packet enable */
#define E1000_RCTL_LBM            0x000000c0    /* no loopback mode */
#define E1000_RCTL_RDMTS          0x00000300    /* rx desc min threshold size */
#define E1000_RCTL_MO             0x00003000    /* multicast offset 15:4 */
#define E1000_RCTL_BAM            0x00008000    /* broadcast enable */
/* these buffer sizes are valid if E1000_RCTL_BSEX is 0 */
#define E1000_RCTL_SZ_2048        0x00000000    /* rx buffer size 2048 */
#define E1000_RCTL_SECRC          0x04000000    /* Strip Ethernet CRC */


// TIPG
#define E1000_TIPG_IPGT   10
#define E1000_TIPG_IPGR1  (4 << 10)
#define E1000_TIPG_IPGR2  (6 << 20)


// tx_desc status
#define E1000_TXD_STAT_DD    0x01 /* Descriptor Done */
#define E1000_TXD_STAT_EC    0x02 /* Excess Collisions */
#define E1000_TXD_STAT_LC    0x04 /* Late Collisions */
#define E1000_TXD_STAT_TU    0x08 /* Transmit underrun */

// tx_desc cmd
#define E1000_TXD_CMD_EOP    0x01 /* End of Packet */
#define E1000_TXD_CMD_IFCS   0x02 /* Insert FCS (Ethernet CRC) */
#define E1000_TXD_CMD_IC     0x04 /* Insert Checksum */
#define E1000_TXD_CMD_RS     0x08 /* Report Status */
#define E1000_TXD_CMD_RPS    0x10 /* Report Packet Sent */
#define E1000_TXD_CMD_DEXT   0x20 /* Descriptor extension (0 = legacy) */
#define E1000_TXD_CMD_VLE    0x40 /* Add VLAN tag */
#define E1000_TXD_CMD_IDE    0x80 /* Enable Tidv register */

// rx_desc status
#define E1000_RXD_STAT_DD       0x01    /* Descriptor Done */
#define E1000_RXD_STAT_EOP      0x02    /* End of Packet */

struct tx_desc
{
    uint64_t addr;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
};

struct tx_pkt
{
    uint8_t pkt[E1000_PKT_SIZE];
};

struct rx_desc
{
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
};

struct rx_pkt
{
    uint8_t pkt[E1000_REC_PKT_SIZE];
};

int e1000_func_enable(struct pci_func *f);
int transmit_pkt(uint8_t *data, int len);
int receive_pkt(uint8_t *data);
void e1000_get_mac_addr(uint32_t* hi, uint32_t* lo);

#endif	// JOS_KERN_E1000_H
