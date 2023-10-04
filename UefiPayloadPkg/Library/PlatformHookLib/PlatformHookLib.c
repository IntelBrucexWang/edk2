/** @file
  Platform Hook Library instance for UART device.

  Copyright (c) 2021, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <PiDxe.h>
#include <UniversalPayload/SerialPortInfo.h>
#include <Library/PlatformHookLib.h>
#include <Library/PcdLib.h>
#include <Library/HobLib.h>
#include <Library/IoLib.h>
/** Library Constructor

  @retval RETURN_SUCCESS  Success.
**/
EFI_STATUS
EFIAPI
PlatformHookSerialPortConstructor (
  VOID
  )
{
  // Nothing to do here. This constructor is added to
  // enable the chain of constructor invocation for
  // dependent libraries.
  return RETURN_SUCCESS;
}

/**
  Performs platform specific initialization required for the CPU to access
  the hardware associated with a SerialPortLib instance.  This function does
  not initialize the serial port hardware itself.  Instead, it initializes
  hardware devices that are required for the CPU to access the serial port
  hardware.  This function may be called more than once.

  @retval RETURN_SUCCESS       The platform specific initialization succeeded.
  @retval RETURN_DEVICE_ERROR  The platform specific initialization could not be completed.

**/
RETURN_STATUS
EFIAPI
PlatformHookSerialPortInitialize (
  VOID
  )
{
  RETURN_STATUS                       Status;
  UNIVERSAL_PAYLOAD_SERIAL_PORT_INFO  *SerialPortInfo;
  UINT8                               *GuidHob;
  UNIVERSAL_PAYLOAD_GENERIC_HEADER    *GenericHeader;
IoWrite8(0x80,0xD0);

  if (GetHobList() == NULL) {
IoWrite8(0x80,0xD2);
    Status = PcdSetBoolS (PcdSerialUseMmio, FALSE);
IoWrite8(0x80,0xD3);
    if (RETURN_ERROR (Status)) {
IoWrite8(0x80,0xD4);
      return Status;
    }
IoWrite8(0x80,0xD6);
    Status = PcdSet64S (PcdSerialRegisterBase, 0x3f8);
IoWrite8(0x80,0xD7);
    if (RETURN_ERROR (Status)) {
IoWrite8(0x80,0xD8);
      return Status;
    }
IoWrite8(0x80,0xDA);
    Status = PcdSet32S (PcdSerialRegisterStride, 1);
IoWrite8(0x80,0xDB);
    if (RETURN_ERROR (Status)) {
IoWrite8(0x80,0xDC);
      return Status;
    }
IoWrite8(0x80,0xDE);
    Status = PcdSet32S (PcdSerialBaudRate, 115200);
IoWrite8(0x80,0xDF);
    if (RETURN_ERROR (Status)) {
IoWrite8(0x80,0xF0);
      return Status;
    }
IoWrite8(0x80,0xF2);
    return RETURN_SUCCESS;
  }

  GuidHob = GetFirstGuidHob (&gUniversalPayloadSerialPortInfoGuid);
  if (GuidHob == NULL) {
    return EFI_NOT_FOUND;
  }

  GenericHeader = (UNIVERSAL_PAYLOAD_GENERIC_HEADER *)GET_GUID_HOB_DATA (GuidHob);
  if ((sizeof (UNIVERSAL_PAYLOAD_GENERIC_HEADER) > GET_GUID_HOB_DATA_SIZE (GuidHob)) || (GenericHeader->Length > GET_GUID_HOB_DATA_SIZE (GuidHob))) {
    return EFI_NOT_FOUND;
  }

  if (GenericHeader->Revision == UNIVERSAL_PAYLOAD_SERIAL_PORT_INFO_REVISION) {
    SerialPortInfo = (UNIVERSAL_PAYLOAD_SERIAL_PORT_INFO *)GET_GUID_HOB_DATA (GuidHob);
    if (GenericHeader->Length < UNIVERSAL_PAYLOAD_SIZEOF_THROUGH_FIELD (UNIVERSAL_PAYLOAD_SERIAL_PORT_INFO, RegisterBase)) {
      //
      // Return if can't find the Serial Port Info Hob with enough length
      //
      return EFI_NOT_FOUND;
    }

    Status = PcdSetBoolS (PcdSerialUseMmio, SerialPortInfo->UseMmio);
    if (RETURN_ERROR (Status)) {
      return Status;
    }

    Status = PcdSet64S (PcdSerialRegisterBase, SerialPortInfo->RegisterBase);
    if (RETURN_ERROR (Status)) {
      return Status;
    }

    Status = PcdSet32S (PcdSerialRegisterStride, SerialPortInfo->RegisterStride);
    if (RETURN_ERROR (Status)) {
      return Status;
    }

    Status = PcdSet32S (PcdSerialBaudRate, SerialPortInfo->BaudRate);
    if (RETURN_ERROR (Status)) {
      return Status;
    }

    return RETURN_SUCCESS;
  }

  return EFI_NOT_FOUND;
}
