#ifdef __cplusplus
extern "C" {
#endif
#include <pci/pci.h>
#ifdef __cplusplus
}
#endif

#include "debugsystem.h"
#include "devscan_pci.h"
#include "libpci.h"

using namespace std;

//-------------------------------------------------------------------------------------------

#define FLAG_CHAR(val, mask) ((val & mask) ? '+' : '-')

// Переименовано: device -> SPciConfigHandle
struct SPciConfigHandle
{
    struct pci_dev *pciDev;
    unsigned int cachedSize;
    unsigned int bufferSize;
    BYTE *configData;
    BYTE *validMask;
};

//-------------------------------------------------------------------------------------------

// Вспомогательная функция проверки диапазона
static void VerifyConfigSpaceRange(struct SPciConfigHandle *handle, unsigned int offset, unsigned int length)
{
    while (length)
    {
        if (!handle->validMask[offset])
        {
            Error("Internal bug: Accessing non-read configuration byte at position %x\n", offset);
            exit(-1);
        }
        offset++;
        length--;
    }
}

BYTE ReadConfigByte(struct SPciConfigHandle *handle, unsigned int offset)
{
    VerifyConfigSpaceRange(handle, offset, 1);
    return handle->configData[offset];
}

WORD ReadConfigWord(struct SPciConfigHandle *handle, unsigned int offset)
{
    VerifyConfigSpaceRange(handle, offset, 2);
    return handle->configData[offset] | (handle->configData[offset + 1] << 8);
}

DWORD ReadConfigLong(struct SPciConfigHandle *handle, unsigned int offset)
{
    VerifyConfigSpaceRange(handle, offset, 4);
    return handle->configData[offset] | (handle->configData[offset + 1] << 8) |
           (handle->configData[offset + 2] << 16) | (handle->configData[offset + 3] << 24);
}

//-------------------------------------------------------------------------------------------

int UpdateConfigurationCache(struct SPciConfigHandle *handle, unsigned int offset, unsigned int length)
{
    unsigned int endOffset = offset + length;
    int readResult;

    while (offset < handle->bufferSize && length && handle->validMask[offset]) { offset++; length--; }
    while (offset + length <= handle->bufferSize && length && handle->validMask[offset + length - 1]) { length--; }

    if (!length) return 1;

    if (endOffset > handle->bufferSize)
    {
        int oldSize = handle->bufferSize;
        while (endOffset > handle->bufferSize) handle->bufferSize *= 2;
        handle->configData = (BYTE*)realloc(handle->configData, handle->bufferSize);
        handle->validMask = (BYTE*)realloc(handle->validMask, handle->bufferSize);
        memset(handle->validMask + oldSize, 0, handle->bufferSize - oldSize);
    }

    readResult = pci_read_block(handle->pciDev, offset, handle->configData + offset, length);
    if (readResult) memset(handle->validMask + offset, 1, length);

    return readResult;
}

//-------------------------------------------------------------------------------------------

// Переименовано: cap_express -> ParsePcieExpressCapability
static void ParsePcieExpressCapability(struct SPciConfigHandle *cfg, int offset, int capFlags, SPciDeviceUnit& unit)
{
    int devType = (capFlags & PCI_EXP_FLAGS_TYPE) >> 4;
    int fetchSize;
    int hasSlot = 0;

    Trace("Express (v%d) ", capFlags & PCI_EXP_FLAGS_VERS);
    switch (devType)
    {
        case PCI_EXP_TYPE_ENDPOINT:   Trace("Endpoint"); break;
        case PCI_EXP_TYPE_LEG_END:    Trace("Legacy Endpoint"); break;
        case PCI_EXP_TYPE_ROOT_PORT:  hasSlot = capFlags & PCI_EXP_FLAGS_SLOT; Trace("Root Port (Slot%c)", FLAG_CHAR(capFlags, PCI_EXP_FLAGS_SLOT)); break;
        case PCI_EXP_TYPE_UPSTREAM:   Trace("Upstream Port"); break;
        case PCI_EXP_TYPE_DOWNSTREAM: hasSlot = capFlags & PCI_EXP_FLAGS_SLOT; Trace("Downstream Port (Slot%c)", FLAG_CHAR(capFlags, PCI_EXP_FLAGS_SLOT)); break;
        case PCI_EXP_TYPE_PCI_BRIDGE: Trace("PCI/PCI-X Bridge"); break;
        default: Trace("Unknown type %d", devType);
    }
    Trace(", MSI %02x\n", (capFlags & PCI_EXP_FLAGS_IRQ) >> 9);

    fetchSize = 16;
    if (hasSlot) fetchSize = 24;
    if (devType == PCI_EXP_TYPE_ROOT_PORT) fetchSize = 32;

    if (!UpdateConfigurationCache(cfg, offset + PCI_EXP_DEVCAP, fetchSize)) return;

    WORD linkStatus = ReadConfigWord(cfg, offset + PCI_EXP_LNKSTA);

    unit.pcieGen   = (linkStatus & PCI_EXP_LNKSTA_SPEED);
    unit.pcieLanes = (linkStatus & PCI_EXP_LNKSTA_WIDTH) >> 4;
}

//-------------------------------------------------------------------------------------------

static struct pci_access *g_PciAccessCtx = NULL;
static struct pci_dev    *g_CurrentDevPtr = NULL;

void InitializePciAccess()
{
    FUNCTION_TRACE
    g_PciAccessCtx = pci_alloc();
    pci_init(g_PciAccessCtx);

    pci_load_name_list(g_PciAccessCtx);

    pci_scan_bus(g_PciAccessCtx);

    if (g_PciAccessCtx) g_CurrentDevPtr = g_PciAccessCtx->devices;
}


void FinalizePciAccess()
{
    FUNCTION_TRACE
    if (g_PciAccessCtx) pci_cleanup(g_PciAccessCtx);
}

bool FetchNextPciDevice(SPciDeviceUnit& unit)
{
    FUNCTION_TRACE
    if (!g_CurrentDevPtr) return false;

    // 1. Сначала заполняем базовую информацию об устройстве
    // PCI_FILL_IDENT заполнит vendor_id и device_id
    // PCI_FILL_CLASS заполнит device_class
    pci_fill_info(g_CurrentDevPtr, PCI_FILL_IDENT | PCI_FILL_CLASS | PCI_FILL_IRQ | PCI_FILL_BASES);

    unit.busAddr[0] = g_CurrentDevPtr->domain;
    unit.busAddr[1] = g_CurrentDevPtr->bus;
    unit.busAddr[2] = g_CurrentDevPtr->dev;
    unit.busAddr[3] = g_CurrentDevPtr->func;

    unit.vendorDeviceId[0] = g_CurrentDevPtr->vendor_id;
    unit.vendorDeviceId[1] = g_CurrentDevPtr->device_id;

    // 2. Теперь, когда поля в g_CurrentDevPtr точно заполнены, запрашиваем имена
    char classBuffer[128], nameBuffer[128];

    // Получаем название класса
    unit.deviceClass = pci_lookup_name(g_PciAccessCtx, classBuffer, sizeof(classBuffer),
                                       PCI_LOOKUP_CLASS, g_CurrentDevPtr->device_class);

    // Получаем название устройства
    unit.deviceName  = pci_lookup_name(g_PciAccessCtx, nameBuffer, sizeof(nameBuffer),
                                       PCI_LOOKUP_VENDOR | PCI_LOOKUP_DEVICE,
                                       g_CurrentDevPtr->vendor_id, g_CurrentDevPtr->device_id);

    errno = 0;
    unit.pcieGen = 0;
    unit.pcieLanes = 0;

    // 3. Работа с конфигурационным пространством для поиска PCI Express Capability
    struct SPciConfigHandle cfgHandle;
    memset(&cfgHandle, 0, sizeof(cfgHandle));

    cfgHandle.pciDev = g_CurrentDevPtr;
    cfgHandle.cachedSize = cfgHandle.bufferSize = 64;
    cfgHandle.configData = (BYTE*)malloc(64);
    cfgHandle.validMask = (BYTE*)malloc(64);
    memset(cfgHandle.validMask, 1, 64);

    if (pci_read_block(g_CurrentDevPtr, 0, cfgHandle.configData, 64))
    {
        pci_setup_cache(g_CurrentDevPtr, cfgHandle.configData, cfgHandle.cachedSize);

        if (ReadConfigWord(&cfgHandle, PCI_STATUS) & PCI_STATUS_CAP_LIST)
        {
            int capPointer = ReadConfigByte(&cfgHandle, PCI_CAPABILITY_LIST) & ~3;
            BYTE visited[256];
            memset(visited, 0, 256);

            while (capPointer)
            {
                if (!UpdateConfigurationCache(&cfgHandle, capPointer, 4)) break;

                int capId   = ReadConfigByte(&cfgHandle, capPointer + PCI_CAP_LIST_ID);
                int nextPtr = ReadConfigByte(&cfgHandle, capPointer + PCI_CAP_LIST_NEXT) & ~3;
                int flags   = ReadConfigWord(&cfgHandle, capPointer + PCI_CAP_FLAGS);

                if (visited[capPointer]++) break;
                if (capId == 0xff) break;

                if (capId == PCI_CAP_ID_EXP)
                {
                    ParsePcieExpressCapability(&cfgHandle, capPointer, flags, unit);
                }
                capPointer = nextPtr;
            }
        }
    }

    free(cfgHandle.configData);
    free(cfgHandle.validMask);

    g_CurrentDevPtr = g_CurrentDevPtr->next;
    return true;
}
