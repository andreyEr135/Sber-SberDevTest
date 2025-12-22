#ifndef libpciH
#define libpciH

#include "devscan_pci.h"

void InitializePciAccess();
void FinalizePciAccess();
bool FetchNextPciDevice(SPciDeviceUnit& deviceUnit);

#endif
