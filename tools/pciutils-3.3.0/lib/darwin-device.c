/*
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include "internal.h"


#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach_error.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/pci/IOPCIPrivate.h>


static void
darwin_config(struct pci_access *a UNUSED)
{
}

static int
darwin_detect(struct pci_access *a)
{
	io_registry_entry_t    service;
	io_connect_t           connect;
	kern_return_t          status;

	service = IOServiceGetMatchingService(kIOMasterPortDefault, 
																					IOServiceMatching("IOPCIBridge"));
	if (service) 
	{
		status = IOServiceOpen(service, mach_task_self(), kIOPCIDiagnosticsClientType, &connect);
		IOObjectRelease(service);
	}

  if (!service || (kIOReturnSuccess != status))
	{
		a->warning("Cannot open IOPCIBridge (add boot arg debug=0x144 & run as root)");
		return 0;
	}
  a->debug("...using IOPCIBridge");
  a->fd = connect;
  return 1;
}

static void
darwin_init(struct pci_access *a UNUSED)
{
}

static void
darwin_cleanup(struct pci_access *a UNUSED)
{
}

static int
darwin_read(struct pci_dev *d, int pos, byte *buf, int len)
{
  if (!(len == 1 || len == 2 || len == 4))
    return pci_generic_block_read(d, pos, buf, len);

    IOPCIDiagnosticsParameters param;
    kern_return_t              status;

    param.spaceType = kIOPCIConfigSpace;
    param.bitWidth  = len * 8;
    param.options   = 0;

	param.address.pci.offset   = pos;
	param.address.pci.function = d->func;
	param.address.pci.device   = d->dev;
	param.address.pci.bus      = d->bus;
	param.address.pci.segment  = d->domain;
	param.address.pci.reserved = 0;
	param.value                = -1ULL;

	size_t outSize = sizeof(param);
	status = IOConnectCallStructMethod(d->access->fd, kIOPCIDiagnosticsMethodRead,
																					&param, sizeof(param),
																					&param, &outSize);
  if ((kIOReturnSuccess != status))
	{
		d->access->error("darwin_read: kIOPCIDiagnosticsMethodRead failed: %s",
							mach_error_string(status));
	}

  switch (len)
	{
    case 1:
      buf[0] = (u8) param.value;
      break;
    case 2:
      ((u16 *) buf)[0] = cpu_to_le16((u16) param.value);
      break;
    case 4:
      ((u32 *) buf)[0] = cpu_to_le32((u32) param.value);
      break;
	}
  return 1;
}

static int
darwin_write(struct pci_dev *d, int pos, byte *buf, int len)
{
  if (!(len == 1 || len == 2 || len == 4))
    return pci_generic_block_write(d, pos, buf, len);

    IOPCIDiagnosticsParameters param;
    kern_return_t              status;

    param.spaceType = kIOPCIConfigSpace;
    param.bitWidth  = len * 8;
    param.options   = 0;
    param.address.pci.offset   = pos;
    param.address.pci.function = d->func;
    param.address.pci.device   = d->dev;
    param.address.pci.bus      = d->bus;
    param.address.pci.segment  = d->domain;
    param.address.pci.reserved = 0;
  switch (len)
	{
    case 1:
      param.value = buf[0];
      break;
    case 2:
      param.value = le16_to_cpu(((u16 *) buf)[0]);
      break;
    case 4:
      param.value = le32_to_cpu(((u32 *) buf)[0]);
      break;
	}

	size_t outSize = 0;
	status = IOConnectCallStructMethod(d->access->fd, kIOPCIDiagnosticsMethodWrite,
																					&param, sizeof(param),
																					NULL, &outSize);
  if ((kIOReturnSuccess != status))
	{
		d->access->error("darwin_read: kIOPCIDiagnosticsMethodWrite failed: %s",
							mach_error_string(status));
	}

  return 1;
}

struct pci_methods pm_darwin_device = {
  "darwin-device",
  "Darwin device",
  darwin_config,
  darwin_detect,
  darwin_init,
  darwin_cleanup,
  pci_generic_scan,
  pci_generic_fill_info,
  darwin_read,
  darwin_write,
  NULL,                                 /* read_vpd */
  NULL,                                 /* dev_init */
  NULL                                  /* dev_cleanup */
};
