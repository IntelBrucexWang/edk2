/** @file
  ELF Load Image Support
Copyright (c) 2021, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiPei.h>
#include <UniversalPayload/UniversalPayload.h>
#include <Guid/UniversalPayloadBase.h>
#include <UniversalPayload/ExtraData.h>
#include <UniversalPayload/DeviceTree.h>
#include <UniversalPayload/AcpiTable.h>
#include <Ppi/LoadFile.h>
#include <Library/PrintLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/PeiServicesLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/FdtLib.h>
#include "FitLib.h"
#include <Library/FdtLib.h>
#include <Library/DebugPrintErrorLevelLib.h>

CHAR8  *mMemoryAllocType[] = {
  "Reserved",
  "LoaderCode",
  "LoaderData",
  "BootServicesCode",
  "BootServicesData",
  "RuntimeServicesCode",
  "RuntimeServicesData",
  "ConventionalMemory",
  "UnusableMemory",
  "ACPIReclaimMemory",
  "ACPIMemoryNVS",
  "MemoryMappedIO",
  "MemoryMappedIOPortSpace",
  "PalCode",
  "PersistentMemory",
};
/**
  Discover Hobs data and report data into a FDT.
  @param[in] PeiServices       An indirect pointer to the EFI_PEI_SERVICES table published by the PEI Foundation.
  @param[in] NotifyDescriptor  Address of the notification descriptor data structure.
  @param[in] Ppi               Address of the PPI that was installed.
  @retval EFI_SUCCESS          Hobs data is discovered.
  @return Others               No Hobs data is discovered.
**/
EFI_STATUS
EFIAPI
FitFdtPpiNotifyCallback (
  IN EFI_PEI_SERVICES           **PeiServices,
  IN EFI_PEI_NOTIFY_DESCRIPTOR  *NotifyDescriptor,
  IN VOID                       *Ppi
  );

CONST EFI_PEI_PPI_DESCRIPTOR  gReadyToPayloadSignalPpi = {
  (EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
  &gEfiReadyToPayloadPpiGuid,
  NULL
};

EFI_PEI_NOTIFY_DESCRIPTOR  mReadyToPayloadNotifyList = {
    (EFI_PEI_PPI_DESCRIPTOR_NOTIFY_CALLBACK | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
    &gEfiReadyToPayloadPpiGuid,
    FitFdtPpiNotifyCallback
};


/**
  Print FDT data.
  @param[in] FdtBase         Address of the Fdt data.
  @retval EFI_SUCCESS        If it completed successfully.
  @retval Others             If it failed to build required FDT.
**/
VOID
PrintFdt (
  IN     VOID  *FdtBase
  )
{
  UINT8   *Fdt;
  UINT32  i;

  DEBUG ((DEBUG_ERROR, "FDT DTB data:"));
  for (Fdt = FdtBase, i = 0; i < Fdt32ToCpu(((FDT_HEADER *)FdtBase)->TotalSize); i++, Fdt++) {
    if (i % 16 == 0) {
      DEBUG ((DEBUG_ERROR, "\n"));
    }
    DEBUG ((DEBUG_ERROR, "%02x ", *Fdt));
  }
  DEBUG ((DEBUG_ERROR, "\n"));
}

/**
  It will build FDT based on memory information from Hobs.
  @param[in] FdtBase         Address of the Fdt data.
  @retval EFI_SUCCESS        If it completed successfully.
  @retval Others             If it failed to build required FDT.
**/
EFI_STATUS
BuildFdtForMemory (
  IN     VOID  *FdtBase
  )
{
  EFI_STATUS                    Status;
  EFI_PEI_HOB_POINTERS          Hob;
  EFI_HOB_RESOURCE_DESCRIPTOR   *ResourceHob;
  VOID                          *HobStart;
  VOID                          *Fdt;
  INT32                         TempNode;
  UINT8                         TempStr[32];
  UINT64                        RegTmp[2];
  UINT32                        Data32;

  Fdt = FdtBase;

  HobStart = GetFirstHob (EFI_HOB_TYPE_RESOURCE_DESCRIPTOR);
  //
  // Scan resource descriptor hobs to set memory nodes
  //
  for (Hob.Raw = HobStart; !END_OF_HOB_LIST (Hob); Hob.Raw = GET_NEXT_HOB (Hob)) {
    if (GET_HOB_TYPE (Hob) == EFI_HOB_TYPE_RESOURCE_DESCRIPTOR) {
      ResourceHob = Hob.ResourceDescriptor;
      // Memory
      if (ResourceHob->ResourceType == EFI_RESOURCE_SYSTEM_MEMORY) {

        // DEBUG ((DEBUG_ERROR, "Found hob for memory: base %016lX  length %016lX\n", ResourceHob->PhysicalStart, ResourceHob->ResourceLength));

        Status = AsciiSPrint (TempStr, sizeof(TempStr), "memory@%lX", ResourceHob->PhysicalStart);
        DEBUG ((DEBUG_INFO, "BuildFdtForMemory Status:%x \n", Status));
        DEBUG ((DEBUG_INFO, "BuildFdtForMemory TempStr:%s \n", TempStr));
        TempNode = FdtAddSubnode (Fdt, 0, TempStr);
        ASSERT (TempNode > 0);

        Status = FdtSetProp (Fdt, TempNode, "type", "memory", (UINT32) AsciiStrLen("memory")+1);
        ASSERT_EFI_ERROR (Status);

        RegTmp[0] = CpuToFdt64 (ResourceHob->PhysicalStart);
        RegTmp[1] = CpuToFdt64 (ResourceHob->ResourceLength);
        Status = FdtSetProp(Fdt, TempNode, "reg", &RegTmp, sizeof (RegTmp));
        ASSERT_EFI_ERROR (Status);

        if (ResourceHob->ResourceAttribute != MEMORY_ATTRIBUTE_DEFAULT) {
          Data32 = CpuToFdt32 (ResourceHob->ResourceAttribute);
          Status = FdtSetProp (Fdt, TempNode, "Attribute", &Data32, sizeof (Data32));
          ASSERT_EFI_ERROR (Status);
        }
      }
    }
  }

  return Status;
}

/**
  It will build FDT based on reserved memory information from Hobs.
  @param[in] FdtBase         Address of the Fdt data.
  @retval EFI_SUCCESS        If it completed successfully.
  @retval Others             If it failed to build required FDT.
**/
EFI_STATUS
BuildFdtForReservedMemory (
  IN     VOID  *FdtBase
  )
{
  EFI_STATUS                    Status;
  EFI_PEI_HOB_POINTERS          Hob;
  EFI_HOB_RESOURCE_DESCRIPTOR   *ResourceHob;
  VOID                          *HobStart;
  VOID                          *Fdt;
  INT32                         ParentNode;
  INT32                         TempNode;
  UINT8                         TempStr[32];
  UINT64                        RegTmp[2];
  UINT32                        Data32;
  UINT8        y = 0;
  UINT8        x = 0;
  UINT8        z = 0;

  Fdt = FdtBase;

  ParentNode = FdtAddSubnode (Fdt, 0, "reserved-memory");
  DEBUG ((DEBUG_INFO, "Bruce 1st. ParentNode:%x \n", ParentNode));
  ASSERT(ParentNode > 0);

  Data32 = CpuToFdt32 (2);
  Status = FdtSetProp (Fdt, ParentNode, "#address-cells", &Data32, sizeof(UINT32));
  Status = FdtSetProp (Fdt, ParentNode, "#size-cells", &Data32, sizeof(UINT32));

  HobStart = GetFirstHob (EFI_HOB_TYPE_RESOURCE_DESCRIPTOR);
  //
  // Scan resource descriptor hobs to set reserved memory nodes
  //
  for (Hob.Raw = HobStart; !END_OF_HOB_LIST (Hob); Hob.Raw = GET_NEXT_HOB (Hob)) {
    if (GET_HOB_TYPE (Hob) == EFI_HOB_TYPE_RESOURCE_DESCRIPTOR) {
      ResourceHob = Hob.ResourceDescriptor;
      // MMIO and Reserved Memory
      if (ResourceHob->ResourceType != EFI_RESOURCE_SYSTEM_MEMORY &&
          ResourceHob->ResourceType != EFI_RESOURCE_IO &&
          ResourceHob->ResourceType != EFI_RESOURCE_IO_RESERVED) {

        /* DEBUG ((DEBUG_ERROR, "Found hob for reserved memory: base %016lX  length %016lX  type %x\n",
          ResourceHob->PhysicalStart, ResourceHob->ResourceLength, ResourceHob->ResourceType
          )); */

        if (ResourceHob->ResourceType == EFI_RESOURCE_MEMORY_MAPPED_IO ||
            ResourceHob->ResourceType == EFI_RESOURCE_MEMORY_MAPPED_IO_PORT) {
          Status = AsciiSPrint (TempStr, sizeof (TempStr), "mmio@%lX", ResourceHob->PhysicalStart);
          DEBUG ((DEBUG_INFO, "mmio@ TempStr:%s \n", TempStr));
          DEBUG ((DEBUG_INFO, "ResourceHob->PhysicalStart:%x \n", ResourceHob->PhysicalStart));
          x++;
        }
        else if (ResourceHob->ResourceType == EFI_RESOURCE_MEMORY_RESERVED) {
          Status = AsciiSPrint (TempStr, sizeof (TempStr), "reserved@%lX", ResourceHob->PhysicalStart);
          DEBUG ((DEBUG_INFO, "reserved@ TempStr:%s \n", TempStr));
          DEBUG ((DEBUG_INFO, "ResourceHob->PhysicalStart:%x \n", ResourceHob->PhysicalStart));
          y++;
        }
        else {
          Status = AsciiSPrint (TempStr, sizeof (TempStr), "unknown@%lX", ResourceHob->PhysicalStart);
          DEBUG ((DEBUG_INFO, "unknown@ TempStr:%s \n", TempStr));
          DEBUG ((DEBUG_INFO, "ResourceHob->PhysicalStart:%x \n", ResourceHob->PhysicalStart));
          z++;
        }
        //DEBUG ((DEBUG_INFO, "ParentNode:0x%x \n", ParentNode));
        //DEBUG ((DEBUG_INFO, " TempStr:%s \n", TempStr));
        //TempNode = FdtAddSubnode (Fdt, ParentNode, TempStr);
        //DEBUG ((DEBUG_INFO, " TempNode:%x \n", TempNode));
        DEBUG ((DEBUG_INFO, " ------------------------- \n"));
        //if (TempNode <= 0){
        //  continue;
        //}
        //ASSERT (TempNode > 0);

        //RegTmp[0] = CpuToFdt64 (ResourceHob->PhysicalStart);
        //RegTmp[1] = CpuToFdt64 (ResourceHob->ResourceLength);
        //Status = FdtSetProp (Fdt, TempNode, "reg", &RegTmp, sizeof(RegTmp));
        //ASSERT_EFI_ERROR (Status);

        //if (ResourceHob->ResourceAttribute != MEMORY_ATTRIBUTE_DEFAULT) {
        //  Data32 = CpuToFdt32 (ResourceHob->ResourceAttribute);
        //  Status = FdtSetProp (Fdt, TempNode, "Attribute", &Data32, sizeof (Data32));
        //  ASSERT_EFI_ERROR (Status);
       // }
      }
    }
  }
  DEBUG ((DEBUG_INFO, " mmio:%d \n", x));
  DEBUG ((DEBUG_INFO, " reserved:%d \n", y));
  DEBUG ((DEBUG_INFO, " unknown:%d \n", z));
  for (Hob.Raw = HobStart; !END_OF_HOB_LIST (Hob); Hob.Raw = GET_NEXT_HOB (Hob)) {
    if (GET_HOB_TYPE (Hob) == EFI_HOB_TYPE_RESOURCE_DESCRIPTOR) {
      ResourceHob = Hob.ResourceDescriptor;
      // MMIO and Reserved Memory
      if (ResourceHob->ResourceType != EFI_RESOURCE_SYSTEM_MEMORY &&
          ResourceHob->ResourceType != EFI_RESOURCE_IO &&
          ResourceHob->ResourceType != EFI_RESOURCE_IO_RESERVED) {

        /* DEBUG ((DEBUG_ERROR, "Found hob for reserved memory: base %016lX  length %016lX  type %x\n",
          ResourceHob->PhysicalStart, ResourceHob->ResourceLength, ResourceHob->ResourceType
          )); */

        if (ResourceHob->ResourceType == EFI_RESOURCE_MEMORY_MAPPED_IO ||
            ResourceHob->ResourceType == EFI_RESOURCE_MEMORY_MAPPED_IO_PORT) {
          Status = AsciiSPrint (TempStr, sizeof (TempStr), "mmio@%lX", ResourceHob->PhysicalStart);
          DEBUG ((DEBUG_INFO, "mmio@ TempStr:%s \n", TempStr));

          x++;
        }
        else if (ResourceHob->ResourceType == EFI_RESOURCE_MEMORY_RESERVED) {
          Status = AsciiSPrint (TempStr, sizeof (TempStr), "reserved@%lX", ResourceHob->PhysicalStart);
          DEBUG ((DEBUG_INFO, "reserved@ TempStr:%s \n", TempStr));
          y++;
        }
        else {
          Status = AsciiSPrint (TempStr, sizeof (TempStr), "unknown@%lX", ResourceHob->PhysicalStart);
          DEBUG ((DEBUG_INFO, "unknown@ TempStr:%s \n", TempStr));
          z++;
        }
        DEBUG ((DEBUG_INFO, "ParentNode:0x%x \n", ParentNode));
        DEBUG ((DEBUG_INFO, " TempStr:%s \n", TempStr));
        TempNode = FdtAddSubnode (Fdt, ParentNode, TempStr);
        DEBUG ((DEBUG_INFO, " TempNode:%x \n", TempNode));
        DEBUG ((DEBUG_INFO, " mmio:%d \n", x));
        DEBUG ((DEBUG_INFO, " reserved:%d \n", y));
        DEBUG ((DEBUG_INFO, " unknown:%d \n", z));
        if (TempNode <= 0){
          continue;
        }
        y++;
        DEBUG ((DEBUG_INFO, "Bruce y:%d \n", y));
        ASSERT (TempNode > 0);

        RegTmp[0] = CpuToFdt64 (ResourceHob->PhysicalStart);
        RegTmp[1] = CpuToFdt64 (ResourceHob->ResourceLength);
        Status = FdtSetProp (Fdt, TempNode, "reg", &RegTmp, sizeof(RegTmp));
        ASSERT_EFI_ERROR (Status);

        if (ResourceHob->ResourceAttribute != MEMORY_ATTRIBUTE_DEFAULT) {
          Data32 = CpuToFdt32 (ResourceHob->ResourceAttribute);
          Status = FdtSetProp (Fdt, TempNode, "Attribute", &Data32, sizeof (Data32));
          ASSERT_EFI_ERROR (Status);
        }
      }
    }
  }
  return Status;
}

/**
  It will build FDT based on memory allocation information from Hobs.
  @param[in] FdtBase         Address of the Fdt data.
  @retval EFI_SUCCESS        If it completed successfully.
  @retval Others             If it failed to build required FDT.
**/
EFI_STATUS
BuildFdtForMemAlloc (
  IN     VOID  *FdtBase
  )
{
  EFI_STATUS                    Status;
  EFI_PEI_HOB_POINTERS          Hob;
  VOID                          *HobStart;
  VOID                          *Fdt;
  INT32                         ParentNode;
  INT32                         TempNode;
  UINT8                         TempStr[32];
  UINT64                        RegTmp[2];
  UINT32                        AllocMemType;
  EFI_GUID                      *AllocMemName;
  UINT8                         IsStackHob;
  UINT8                         IsBspStore;
  UINT32                        Data32;

  Fdt = FdtBase;

  ParentNode = FdtAddSubnode (Fdt, 0, "memory-allocation");
  ASSERT(ParentNode > 0);

  Data32 = CpuToFdt32 (2);
  Status = FdtSetProp (Fdt, ParentNode, "#address-cells", &Data32, sizeof (UINT32));
  Status = FdtSetProp (Fdt, ParentNode, "#size-cells", &Data32, sizeof (UINT32));

  HobStart = GetFirstHob (EFI_HOB_TYPE_MEMORY_ALLOCATION);
  //
  // Scan memory allocation hobs to set memory type
  //
  for (Hob.Raw = HobStart; !END_OF_HOB_LIST (Hob); Hob.Raw = GET_NEXT_HOB (Hob)) {
    if (GET_HOB_TYPE (Hob) == EFI_HOB_TYPE_MEMORY_ALLOCATION) {

      AllocMemName = NULL;
      IsStackHob = 0;
      IsBspStore = 0;
      if (CompareGuid (&(Hob.MemoryAllocationModule->MemoryAllocationHeader.Name), &gEfiHobMemoryAllocModuleGuid)) {
        continue;
      }
      else if (IsZeroGuid (&(Hob.MemoryAllocationModule->MemoryAllocationHeader.Name)) == FALSE) {
        AllocMemName = &(Hob.MemoryAllocationModule->MemoryAllocationHeader.Name);

        if (CompareGuid (AllocMemName, &gEfiHobMemoryAllocStackGuid)) {
          IsStackHob = 1;
        }
        else if (CompareGuid (AllocMemName, &gEfiHobMemoryAllocBspStoreGuid)) {
          IsBspStore = 1;
        }
      }

      /* DEBUG ((DEBUG_ERROR, "Found hob for rsvd memory alloc: base %016lX  length %016lX  type %x\n",
        Hob.MemoryAllocation->AllocDescriptor.MemoryBaseAddress,
        Hob.MemoryAllocation->AllocDescriptor.MemoryLength,
        Hob.MemoryAllocation->AllocDescriptor.MemoryType
        )); */

      AllocMemType = Hob.MemoryAllocation->AllocDescriptor.MemoryType;
      if (IsStackHob == 1) {
        Status = AsciiSPrint (TempStr, sizeof(TempStr), "%a@%lX",
          "stackhob",
          Hob.MemoryAllocation->AllocDescriptor.MemoryBaseAddress
          );
      }
      else if (IsBspStore == 1) {
        Status = AsciiSPrint (TempStr, sizeof(TempStr), "%a@%lX",
          "bspstore",
          Hob.MemoryAllocation->AllocDescriptor.MemoryBaseAddress
          );
      }
      else {
        Status = AsciiSPrint (TempStr, sizeof(TempStr), "%a@%lX",
          mMemoryAllocType[AllocMemType],
          Hob.MemoryAllocation->AllocDescriptor.MemoryBaseAddress
          );
      }

      TempNode = FdtAddSubnode (Fdt, ParentNode, TempStr);
      ASSERT(TempNode > 0);

      RegTmp[0] = CpuToFdt64 (Hob.MemoryAllocation->AllocDescriptor.MemoryBaseAddress);
      RegTmp[1] = CpuToFdt64 (Hob.MemoryAllocation->AllocDescriptor.MemoryLength);
      Status = FdtSetProp (Fdt, TempNode, "reg", &RegTmp, sizeof (RegTmp));
      ASSERT_EFI_ERROR (Status);

      Data32 = CpuToFdt32 (AllocMemType);
      Status = FdtSetProp (Fdt, TempNode, "type", &Data32, sizeof (Data32));
      ASSERT_EFI_ERROR (Status);

      if (AllocMemName != NULL) {
        AllocMemName->Data1 = CpuToFdt32 (AllocMemName->Data1);
        AllocMemName->Data2 = CpuToFdt16 (AllocMemName->Data2);
        AllocMemName->Data3 = CpuToFdt16 (AllocMemName->Data3);
        Status = FdtSetProp (Fdt, TempNode, "guid", AllocMemName, sizeof (EFI_GUID));
      }
    }
  }

  return Status;
}

/**
  It will build FDT for UPL required data.
  @param[in] FdtBase         Address of the Fdt data.
  @retval EFI_SUCCESS        If it completed successfully.
  @retval Others             If it failed to build required FDT.
**/
EFI_STATUS
BuildFdtForUplRequired (
  IN     VOID  *FdtBase
  )
{
  EFI_STATUS                          Status;
  VOID                                *Fdt;
  INT32                               TempNode;
  EFI_HOB_CPU                         *CpuHob;
//  UNIVERSAL_PAYLOAD_BASE              *PayloadBase;
//  UINT8                               *GuidHob;
  UINT64                              Data64;
  UINT32                              Data32;
  VOID                                *HobPtr;

  Fdt = FdtBase;

/*
  //
  // Create UPL FV FDT node
  //
  GuidHob = GetFirstGuidHob (&gUniversalPayloadBaseGuid);
  ASSERT (GuidHob != NULL);
  PayloadBase = (UNIVERSAL_PAYLOAD_BASE *) GET_GUID_HOB_DATA (GuidHob);
  TempNode = FdtAddSubnode (Fdt, 0, "PayloadBase");
  ASSERT (TempNode > 0);
  Data64 = CpuToFdt64 ((UINT64)PayloadBase->Entry);
  Status = FdtSetProp (Fdt, TempNode, "entry", &Data64, sizeof (Data64));
*/

  //
  // Create CPU info FDT node
  //
  CpuHob = GetFirstHob (EFI_HOB_TYPE_CPU);
  ASSERT (CpuHob != NULL);

  TempNode = FdtAddSubnode (Fdt, 0, "cpu-info");
  ASSERT (TempNode > 0);

  Data32 = CpuToFdt32 ((UINT32) CpuHob->SizeOfMemorySpace);
  Status = FdtSetProp (Fdt, TempNode, "address_width", &Data32, sizeof (Data32));
  ASSERT_EFI_ERROR (Status);

  //
  // Create PCI Root Bridge Info FDT node.
  //
  TempNode = FdtAddSubnode (Fdt, 0, "pcirbinfo");
  ASSERT(TempNode > 0);

  Data32 = 0;
  Status = FdtSetProp (Fdt, TempNode, "count", &Data32, sizeof (Data32));
  ASSERT_EFI_ERROR (Status);

  Data32 = 0;
  Status = FdtSetProp (Fdt, TempNode, "ResourceAssigned", &Data32, sizeof (Data32));
  ASSERT_EFI_ERROR (Status);

  //
  // Create Hob list FDT node.
  //
  TempNode = FdtAddSubnode (Fdt, 0, "hoblistptr");
  ASSERT (TempNode > 0);

  HobPtr = GetHobList ();
  Data64 = CpuToFdt64 ((UINT64) (EFI_PHYSICAL_ADDRESS) HobPtr);
  Status = FdtSetProp (Fdt, TempNode, "hoblistptr", &Data64, sizeof (Data64));
  ASSERT_EFI_ERROR (Status);

  //
  // Create DebugPrintErrorLevel FDT node.
  //
  TempNode = FdtAddSubnode (Fdt, 0, "DebugPrintErrorLevel");
  ASSERT (TempNode > 0);

  Data32 = CpuToFdt32 (GetDebugPrintErrorLevel ());
  Status = FdtSetProp (Fdt, TempNode, "errorlevel", &Data32, sizeof (Data32));
  ASSERT_EFI_ERROR (Status);

  return Status;
}

/**
  It will build FDT for UPL consumed.
  @param[in] FdtBase         Address of the Fdt data.
  @retval EFI_SUCCESS        If it completed successfully.
  @retval Others             If it failed to build required FDT.
**/
EFI_STATUS
BuildFdtForUPL (
  IN     VOID  *FdtBase
  )
{
  EFI_STATUS     Status;

  //
  // Build FDT for memory related
  //
  Status = BuildFdtForMemory (FdtBase);
  ASSERT_EFI_ERROR (Status);

  Status = BuildFdtForReservedMemory (FdtBase);
  ASSERT_EFI_ERROR (Status);
  if (FdtBase == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: AllocatePages failed\n", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

//  Status = BuildFdtForMemAlloc (FdtBase);
//  ASSERT_EFI_ERROR (Status);

  //Status = BuildFdtForSerial (FdtBase);
  //ASSERT_EFI_ERROR (Status);

  Status = BuildFdtForUplRequired (FdtBase);
  ASSERT_EFI_ERROR (Status);

  return Status;
}

/**
  Discover Hobs data and report data into a FDT.
  @param[in] PeiServices       An indirect pointer to the EFI_PEI_SERVICES table published by the PEI Foundation.
  @param[in] NotifyDescriptor  Address of the notification descriptor data structure.
  @param[in] Ppi               Address of the PPI that was installed.
  @retval EFI_SUCCESS          Hobs data is discovered.
  @return Others               No Hobs data is discovered.
**/
EFI_STATUS
EFIAPI
FitFdtPpiNotifyCallback (
  IN EFI_PEI_SERVICES           **PeiServices,
  IN EFI_PEI_NOTIFY_DESCRIPTOR  *NotifyDescriptor,
  IN VOID                       *Ppi
  )
{
  EFI_STATUS                     Status;
  UNIVERSAL_PAYLOAD_DEVICE_TREE  *Fdt;
  VOID                           *FdtBase;
  UINTN                          FdtSize;
  UINTN                          FdtPages;
  UINT32                         Data32;
  UINT64                         Data64;
  UNIVERSAL_PAYLOAD_ACPI_TABLE   *AcpiTable;
  UINT8                          *GuidHob;
  DEBUG ((DEBUG_INFO, "Bruce %a\n", __FUNCTION__));
  Status = EFI_SUCCESS;
  FdtSize = 4 * EFI_PAGE_SIZE;
  FdtPages = EFI_SIZE_TO_PAGES (FdtSize);
  FdtBase = AllocatePages (FdtPages);
  if (FdtBase == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: AllocatePages failed\n", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

  Status = FdtCreateEmptyTree (FdtBase, (UINT32) FdtSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: cannot create FDT\n", __FUNCTION__));
  }
  // Set cell property of root node
  Data32 = CpuToFdt32 (2);
  Status = FdtSetProp (FdtBase, 0, "#address-cells", &Data32, sizeof (UINT32));
  Status = FdtSetProp (FdtBase, 0, "#size-cells", &Data32, sizeof (UINT32));

  //
  // Set ACPI property of root node
  //
  GuidHob = GetFirstGuidHob (&gUniversalPayloadAcpiTableGuid);
  ASSERT (GuidHob != NULL);
  AcpiTable = (UNIVERSAL_PAYLOAD_ACPI_TABLE *) GET_GUID_HOB_DATA (GuidHob);

  Data64 = CpuToFdt64 ((UINT64) AcpiTable->Rsdp);
  Status = FdtSetProp (FdtBase, 0, "acpi", &Data64, sizeof (Data64));
  Status = BuildFdtForUPL (FdtBase);
  ASSERT_EFI_ERROR (Status);
  PrintFdt (FdtBase);

  Fdt = BuildGuidHob (&gUniversalPayloadDeviceTreeGuid, sizeof (UNIVERSAL_PAYLOAD_DEVICE_TREE));
  if (Fdt == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Build FDT Hob failed\n", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

  DEBUG ((
    DEBUG_ERROR,
    "%a: fdt at 0x%x (size %d)\n",
    __FUNCTION__,
    FdtBase,
    Fdt32ToCpu (((FDT_HEADER *) FdtBase)->TotalSize)
    ));

  Fdt->Header.Revision   = UNIVERSAL_PAYLOAD_DEVICE_TREE_REVISION;
  Fdt->Header.Length     = sizeof (UNIVERSAL_PAYLOAD_DEVICE_TREE);
  Fdt->DeviceTreeAddress = (UINT64) FdtBase;
  DEBUG ((DEBUG_INFO, "Fit Fdt->Header.Revision: 0x%x\n",Fdt->Header.Revision));
  DEBUG ((DEBUG_INFO, "Fit Fdt->Header.Length: 0x%x\n",Fdt->Header.Length));
  DEBUG ((DEBUG_INFO, "Fit Fdt->DeviceTreeAddress: 0x%x\n", Fdt->DeviceTreeAddress));

  return Status;
}

/**
  Notify ReadyToPayLoad signal.
  @param[in] PeiServices       An indirect pointer to the EFI_PEI_SERVICES table published by the PEI Foundation.
  @param[in] NotifyDescriptor  Address of the notification descriptor data structure.
  @param[in] Ppi               Address of the PPI that was installed.
  @retval EFI_SUCCESS          Hobs data is discovered.
  @return Others               No Hobs data is discovered.
**/
EFI_STATUS
EFIAPI
EndOfPeiPpiNotifyCallback (
  IN EFI_PEI_SERVICES           **PeiServices,
  IN EFI_PEI_NOTIFY_DESCRIPTOR  *NotifyDescriptor,
  IN VOID                       *Ppi
  )
 {
  EFI_STATUS                     Status;

  //
  // Ready to Payload phase signal
  //
  Status = PeiServicesInstallPpi (&gReadyToPayloadSignalPpi);

  return Status;
 }

EFI_PEI_NOTIFY_DESCRIPTOR  mEndOfPeiNotifyList[] = {
  {
    (EFI_PEI_PPI_DESCRIPTOR_NOTIFY_CALLBACK | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
    &gEfiEndOfPeiSignalPpiGuid,
    EndOfPeiPpiNotifyCallback
  }
};
/**
  The wrapper function of PeiLoadImageLoadImage().
  @param This            - Pointer to EFI_PEI_LOAD_FILE_PPI.
  @param FileHandle      - Pointer to the FFS file header of the image.
  @param ImageAddressArg - Pointer to PE/TE image.
  @param ImageSizeArg    - Size of PE/TE image.
  @param EntryPoint      - Pointer to entry point of specified image file for output.
  @param AuthenticationState - Pointer to attestation authentication state of image.
  @return Status of PeiLoadImageLoadImage().
**/
EFI_STATUS
EFIAPI
PeiLoadFileLoadPayload (
  IN     CONST EFI_PEI_LOAD_FILE_PPI  *This,
  IN     EFI_PEI_FILE_HANDLE          FileHandle,
  OUT    EFI_PHYSICAL_ADDRESS         *ImageAddressArg   OPTIONAL,
  OUT    UINT64                       *ImageSizeArg      OPTIONAL,
  OUT    EFI_PHYSICAL_ADDRESS         *EntryPoint,
  OUT    UINT32                       *AuthenticationState
  )
{
  EFI_STATUS              Status;
  FIT_IMAGE_CONTEXT       Context;
  UINTN                   Instance;
  VOID                    *Binary;
  //UNIVERSAL_PAYLOAD_BASE  *PayloadBase;
  FIT_RELOCATE_ITEM       *RelocateTable;
  //UINTN                   Length;
  UINTN                   Delta;
  UINTN                   Index;

  Instance = 0;
  do {
    Status = PeiServicesFfsFindSectionData3 (EFI_SECTION_RAW, Instance++, FileHandle, &Binary, AuthenticationState);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    ZeroMem (&Context, sizeof (Context));
    Status = ParseFitImage (Binary, &Context);
  } while (EFI_ERROR (Status));

  if (EFI_ERROR (Status)) {
    ASSERT_EFI_ERROR (Status);
    return Status;
  }

  DEBUG ((
    DEBUG_INFO,
    "Before Rebase Payload File Base: 0x%08x, File Size: 0x%08X, EntryPoint: 0x%08x\n",
    Context.PayloadBaseAddress,
    Context.PayloadSize,
    Context.PayloadEntryPoint
    ));
  Context.PayloadBaseAddress = (EFI_PHYSICAL_ADDRESS)AllocatePages (EFI_SIZE_TO_PAGES (Context.PayloadSize));

  RelocateTable = (FIT_RELOCATE_ITEM *)(UINTN)(Context.PayloadBaseAddress + Context.RelocateTableOffset);
  CopyMem ((VOID *)Context.PayloadBaseAddress, Binary, Context.PayloadSize);

  if (Context.PayloadBaseAddress > Context.PayloadLoadAddress) {
    Delta                      = Context.PayloadBaseAddress - Context.PayloadLoadAddress;
    Context.PayloadEntryPoint += Delta;
    for (Index = 0; Index < Context.RelocateTableCount; Index++) {
      if ((RelocateTable[Index].RelocateType == 10) || (RelocateTable[Index].RelocateType == 3)) {
        *((UINT64 *)(Context.PayloadBaseAddress + RelocateTable[Index].Offset)) = *((UINT64 *)(Context.PayloadBaseAddress + RelocateTable[Index].Offset)) + Delta;
      }
    }
  } else {
    Delta                      = Context.PayloadLoadAddress - Context.PayloadBaseAddress;
    Context.PayloadEntryPoint -= Delta;
    for (Index = 0; Index < Context.RelocateTableCount; Index++) {
      if ((RelocateTable[Index].RelocateType == 10) || (RelocateTable[Index].RelocateType == 3)) {
        *((UINT64 *)(Context.PayloadBaseAddress + RelocateTable[Index].Offset)) = *((UINT64 *)(Context.PayloadBaseAddress + RelocateTable[Index].Offset)) - Delta;
      }
    }
  }

  DEBUG ((
    DEBUG_INFO,
    "After Rebase Payload File Base: 0x%08x, File Size: 0x%08X, EntryPoint: 0x%08x\n",
    Context.PayloadBaseAddress,
    Context.PayloadSize,
    Context.PayloadEntryPoint
    ));
  /*Length    = sizeof (UNIVERSAL_PAYLOAD_BASE);
  PayloadBase = BuildGuidHob (
                &gUniversalPayloadBaseGuid,
                Length
                );
  PayloadBase->Entry = (EFI_PHYSICAL_ADDRESS)Context.PayloadBaseAddress;
  */
  *ImageAddressArg = Context.PayloadBaseAddress;
  *ImageSizeArg    = Context.PayloadSize;
  *EntryPoint      = Context.PayloadEntryPoint;

  Status = PeiServicesNotifyPpi (&mEndOfPeiNotifyList[0]);
  ASSERT_EFI_ERROR (Status);
  return EFI_SUCCESS;
}

EFI_PEI_LOAD_FILE_PPI  mPeiLoadFilePpi = {
  PeiLoadFileLoadPayload
};

EFI_PEI_PPI_DESCRIPTOR  gPpiLoadFilePpiList = {
  (EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
  &gEfiPeiLoadFilePpiGuid,
  &mPeiLoadFilePpi
};

/**
  Install Pei Load File PPI.
  @param  FileHandle  Handle of the file being invoked.
  @param  PeiServices Describes the list of possible PEI Services.
  @retval EFI_SUCESS  The entry point executes successfully.
  @retval Others      Some error occurs during the execution of this function.
**/
EFI_STATUS
EFIAPI
InitializeFitPayloadLoaderPeim (
  IN       EFI_PEI_FILE_HANDLE  FileHandle,
  IN CONST EFI_PEI_SERVICES     **PeiServices
  )
{
  EFI_STATUS  Status;

  Status = PeiServicesInstallPpi (&gPpiLoadFilePpiList);
  //
  // Build FDT in end of PEI notify callback.
  //
  Status = PeiServicesNotifyPpi (&mReadyToPayloadNotifyList);

  return Status;
}
