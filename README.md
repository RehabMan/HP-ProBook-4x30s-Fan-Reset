## Fanreset.dylib/HPFanReset.efi by RehabMan

This repository contains the source code for both Fanreset.dylib (used with Chameleon) and HPFanReset.efi (used with Clover).

The purpose of the code is to reset the fan control (to BIOS control) in the EC on HP ProBook/EliteBook/Zbook laptops.

When using my "quiet fan patch" for the ProBook, it is necessary to do this in the bootloader to have a properly functioning fan in Windows, or in other cases where the OS X based fan control software (patches + ACPIPoller.kext) are not in play.


### How to Install

Fanreset.dylib should be copied to /Extra/modules.

HPFanReset.efi should be copied to the EFI partition at EFI/Clover/drivers64UEFI.

This code should not be used with laptops other than ProBook/EliteBook/Zbook, unless you've proven to yourself that the same fan control registers in the EC are used.

Distributions:

- RehabMan-Fanreset-2012-0913.dylib.zip: original Fanreset.dylib for Chameleon

- RehabMan-Fanreset-2012-0922.dylib.zip: same but with WiFi whitelist hack

- HPFanReset-2013-1205.efi.zip: HPFanReset.efi for Clover UEFI


### Downloads

Current location:
https://bitbucket.org/RehabMan/hp-probook-4x30s-fan-reset/downloads

Old location:
https://code.google.com/p/hp-probook-4x30s-fan-reset/downloads/list
