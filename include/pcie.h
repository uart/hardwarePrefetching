#ifndef __PCIE_H
#define __PCIE_H

#include <pci/pci.h>

int pcie_init(void);
void pcie_deinit(void);
struct pci_dev* pcie_get_devices();

#endif
