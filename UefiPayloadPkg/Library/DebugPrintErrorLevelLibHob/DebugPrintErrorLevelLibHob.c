/** @file
  Debug Print Error Level library instance that retrieves
  the DebugPrintErrorLevel from bootloader.

  Copyright (c) 2022, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <Uefi.h>
#include <PiDxe.h>
#include <Library/PcdLib.h>
#include <Library/HobLib.h>
#include <Guid/DebugPrintErrorLevel.h>
#include <Library/DebugPrintErrorLevelLib.h>
#include <UniversalPayload/UniversalPayload.h>
#include <Library/IoLib.h>

STATIC UINT32   gDebugPrintErrorLevel;
STATIC BOOLEAN  gDebugPrintErrorLevelInitialized = FALSE;

/**
  Returns the debug print error level mask for the current module.

  @return  Debug print error level mask for the current module.

**/
UINT32
EFIAPI
GetDebugPrintErrorLevel (
  VOID
  )
{
  VOID                                  *GuidHob;
  UNIVERSAL_PAYLOAD_GENERIC_HEADER      *GenericHeader;
  UEFI_PAYLOAD_DEBUG_PRINT_ERROR_LEVEL  *DebugPrintErrorLevel;
  IoWrite8(0x80,0xA0);
  /*
  if (GetHobList() == NULL) {
     IoWrite8(0x80,0xCC);
    return PcdGet32 (PcdDebugPrintErrorLevel);
  }
*/
  if (!gDebugPrintErrorLevelInitialized) {
    IoWrite8(0x80,0xA1);
    gDebugPrintErrorLevelInitialized = TRUE;
    gDebugPrintErrorLevel            = PcdGet32 (PcdDebugPrintErrorLevel);
    GuidHob                          = GetFirstGuidHob (&gEdkiiDebugPrintErrorLevelGuid);
    if (GuidHob != NULL) {
      IoWrite8(0x80,0xA2);
      GenericHeader = (UNIVERSAL_PAYLOAD_GENERIC_HEADER *)GET_GUID_HOB_DATA (GuidHob);
      if ((sizeof (UNIVERSAL_PAYLOAD_GENERIC_HEADER) < GET_GUID_HOB_DATA_SIZE (GuidHob)) &&
          (GenericHeader->Length <= GET_GUID_HOB_DATA_SIZE (GuidHob)))
      {
        IoWrite8(0x80,0xA3);
        if (GenericHeader->Revision == UEFI_PAYLOAD_DEBUG_PRINT_ERROR_LEVEL_REVISION) {
          IoWrite8(0x80,0xA4);
          DebugPrintErrorLevel =  (UEFI_PAYLOAD_DEBUG_PRINT_ERROR_LEVEL *)GET_GUID_HOB_DATA (GuidHob);
          if (DebugPrintErrorLevel->Header.Length >= UNIVERSAL_PAYLOAD_SIZEOF_THROUGH_FIELD (UEFI_PAYLOAD_DEBUG_PRINT_ERROR_LEVEL, ErrorLevel)) {
            IoWrite8(0x80,0xA5);
            gDebugPrintErrorLevel = DebugPrintErrorLevel->ErrorLevel;
          }
        }
      }
    }
  }
  IoWrite8(0x80,0xA6);
  return gDebugPrintErrorLevel;
}

/**
  Sets the global debug print error level mask fpr the entire platform.

  @param   ErrorLevel     Global debug print error level.

  @retval  TRUE           The debug print error level mask was sucessfully set.
  @retval  FALSE          The debug print error level mask could not be set.

**/
BOOLEAN
EFIAPI
SetDebugPrintErrorLevel (
  UINT32  ErrorLevel
  )
{
  //
  // This library uinstance does not support setting the global debug print error
  // level mask.
  //
  return FALSE;
}
