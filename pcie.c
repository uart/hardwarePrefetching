#include <stdio.h>
#include <pci/pci.h>

#include "log.h"

#define TAG "PCIE"

static struct pci_access *pacc;

// Initialize the PCIe interface and the static/local pacc structure.
int pcie_init(void)
{
	pacc = pci_alloc();
	if(pacc == NULL) {
		loge(TAG, "PCIe init failed\n");
		return -1;
	}

	pci_init(pacc);
	pci_scan_bus(pacc);	// Populate the pacc->devices list
	if(pacc->devices == NULL) {
		loge(TAG, "PCIe init failed, no devices found\n");
		return -1;
	}


	return 0;
}

//Cleanup the pacc structure
void pcie_deinit(void)
{
	pci_cleanup(pacc);
}

//returns the handle to the pci access struct
/*struct pci_access* pcie_gethandle()
{
	return pacc;
}*/

//returns the handle to the pci device struct
struct pci_dev* pcie_get_devices()
{
	return pacc->devices;
}

