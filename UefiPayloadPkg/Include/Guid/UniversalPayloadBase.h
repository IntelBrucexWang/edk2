/** @file
  Universal Payload general definitions.

Copyright (c) 2021, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
    - Universal Payload Specification 0.75 (https://universalpayload.github.io/documentation/)
**/

#ifndef UNIVERSAL_PAYLOAD_BASE_H_
#define UNIVERSAL_PAYLOAD_BASE_H_

extern GUID  gUniversalPayloadBaseGuid;

typedef struct {
  UNIVERSAL_PAYLOAD_GENERIC_HEADER    Header;
  EFI_PHYSICAL_ADDRESS                Entry;
} UNIVERSAL_PAYLOAD_BASE;

#endif // UNIVERSAL_PAYLOAD_BASE_H_
