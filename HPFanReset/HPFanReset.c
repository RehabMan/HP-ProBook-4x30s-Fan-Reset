/*
 * Copyright (c) 2012 RehabMan. All rights reserved.
 *
 *  Released under "The GNU General Public License (GPL-2.0)"
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

/*
 * Please Note:
 *
 * The specific code to read and write to the EC was copied and modified from
 * original code written to control the fan on Thinkpads.
 *
 * I modified it to compile/run using the Mac's Xcode Tools and run under
 * the Chimera boot loader as a 'module'
 *
 * I could not find a specific open-source license provided by that code, so
 * my version here is released under GPLv2.
 *
 * The original source is here: http://sourceforge.net/projects/tp4xfancontrol/
 *
 */

/*
 * 2013-12-05
 *
 * This version ported for use in the Clover bootloader.
 *
 */

#include "HPFanReset.h"
#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>

#include <Library/UefiBootServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Library/MemLogLib.h>

#define DEBUG_ME 0
#if DEBUG_ME
#define DBG(...) do { MemLog(TRUE, 1, __VA_ARGS__); } while (0)
#else
#define DBG(...) do { } while (0)
#endif

#define BOOTLOG(...) do { MemLog(TRUE, 1, __VA_ARGS__); } while (0)

///////////////////////////////////////////////////////////////////////////////

typedef unsigned char byte;

#define true 1
#define false 0

// Registers of the embedded controller
#define EC_DATAPORT         (0x62)      // EC data io-port
#define EC_CTRLPORT         (0x66)      // EC control io-port

// Embedded controller status register bits
#define EC_STAT_OBF         (0x01)      // Output buffer full
#define EC_STAT_IBF         (0x02)      // Input buffer full
#define EC_STAT_CMD         (0x08)      // Last write was a command write (0=data)

// Embedded controller commands
// (write to EC_CTRLPORT to initiate read/write operation)
#define EC_CTRLPORT_READ    ((byte)0x80)
#define EC_CTRLPORT_WRITE   ((byte)0x81)
#define EC_CTRLPORT_QUERY   ((byte)0x84)

// Embedded controller offsets
#define EC_ZONE_SEL         (0x22)      // CRZN in DSDT
#define EC_ZONE_TEMP        (0x26)      // TEMP in DSDT
#define EC_FAN_CONTROL      (0x2F)      // FTGC in DSDT

#define RT_INLINE_ASM_GNU_STYLE 1

//-------------------------------------------------------------------------
//  asm inline port io
//-------------------------------------------------------------------------

inline void ASMOutU8(int Port, byte u8)
{
    __asm__ __volatile__("outb %b1, %w0\n\t"
                         :: "Nd" (Port),
                         "a" (u8));
}

inline byte ASMInU8(int Port)
{
    byte u8;
    __asm__ __volatile__("inb %w1, %b0\n\t"
                         : "=a" (u8)
                         : "Nd" (Port));
    return u8;
}

#define inb(port) ASMInU8((port))
#define outb(port, val) ASMOutU8((port), (val))

//-------------------------------------------------------------------------
//  delay by milliseconds
//-------------------------------------------------------------------------

void delay(int tick)
{
    //does not work... MicroSecondDelay(1000*tick);
    gBS->Stall(1000*tick);
}

//-------------------------------------------------------------------------
//  read control port and wait for set/clear of a status bit
//-------------------------------------------------------------------------
int waitportstatus(int mask, int wanted)
{
    int timeout = 1000;
    int tick = 10;
    int port = EC_CTRLPORT;
    // wait until input on control port has desired state or times out
    int time;
    for (time = 0; time < timeout; time += tick)
    {
        byte data = (byte)inb(port);
        // check for desired result
        if (wanted == (data & mask))
            return true;
        // try again after a moment
        delay(tick);
    }
    return false;
}

//-------------------------------------------------------------------------
//  write a character to an io port through WinIO device
//-------------------------------------------------------------------------
int writeport(int port, byte data)
{
    // write byte
    outb(port, data);
    return true;
}

//-------------------------------------------------------------------------
//  read a character from an io port through WinIO device
//-------------------------------------------------------------------------
int readport(int port, byte *pdata)
{
    // read byte
    byte data = inb(port);
    *pdata = data;
    return true;
}

//-------------------------------------------------------------------------
//  read a byte from the embedded controller (EC) via port io
//-------------------------------------------------------------------------
int ReadByteFromEC(int offset, byte *pdata)
{
    int ok;
    
    // wait for IBF and OBF to clear
    ok = waitportstatus(EC_STAT_IBF|EC_STAT_OBF, 0);
    if (!ok) return false;
    
    // tell 'em we want to "READ"
    ok = writeport(EC_CTRLPORT, EC_CTRLPORT_READ);
    if (!ok) return false;
    
    // wait for IBF to clear (command byte removed from EC's input queue)
    ok = waitportstatus(EC_STAT_IBF, 0);
    if (!ok) return false;
    
    // tell 'em where we want to read from
    ok = writeport(EC_DATAPORT, offset);
    if (!ok) return false;
    
    // wait for IBF to clear (address byte removed from EC's input queue)
    // Note: Techically we should waitportstatus(IBF|OBF,OBF) here. (a byte
    //  being in the EC's output buffer being ready to read). For some reason
    //  this never seems to happen
    ok = waitportstatus(EC_STAT_IBF, 0);
    if (!ok) return false;
    
    // read result (EC byte at offset)
    byte data;
    ok = readport(EC_DATAPORT, &data);
    if (ok) *pdata= data;
    
    return ok;
}

//-------------------------------------------------------------------------
//  write a byte to the embedded controller (EC) via port io
//-------------------------------------------------------------------------
int WriteByteToEC(int offset, byte data)
{
    int ok;
    
    // wait for IBF and OBF to clear
    ok = waitportstatus(EC_STAT_IBF| EC_STAT_OBF, 0);
    if (!ok)
    {
        DBG("HPFanReset:WriteByteToEC(1): waitportstatus IBF|OBF didn't clear\n");
        return false;
    }
    
    // tell 'em we want to "WRITE"
    ok = writeport(EC_CTRLPORT, EC_CTRLPORT_WRITE);
    if (!ok)
    {
        DBG("HPFanReset:WriteByteToEC(2): writeport EC_CTRLPORT failed\n");
        return false;
    }
    
    // wait for IBF to clear (command byte removed from EC's input queue)
    ok = waitportstatus(EC_STAT_IBF, 0);
    if (!ok)
    {
        DBG("HPFanReset:WriteByteToEC(3): waitportstatus IBF didn't clear\n");
        return false;
    }
    
    // tell 'em where we want to write to
    ok = writeport(EC_DATAPORT, offset);
    if (!ok)
    {
        DBG("HPFanReset:WriteByteToEC(4): writeport EC_DATAPORT offset failed\n");
        return false;
    }
    
    // wait for IBF to clear (address byte removed from EC's input queue)
    ok = waitportstatus(EC_STAT_IBF, 0);
    if (!ok)
    {
        DBG("HPFanReset:WriteByteToEC(5): waitportstatus IBF didn't clear\n");
        return false;
    }
    
    // tell 'em what we want to write there
    ok = writeport(EC_DATAPORT, data);
    if (!ok)
    {
        DBG("HPFanReset:WriteByteToEC(6): writeport EC_DATAPORT data\n");
        return false;
    }
    
    // wait for IBF to clear (data byte removed from EC's input queue)
    ok = waitportstatus(EC_STAT_IBF, 0);
    if (!ok)
    {
        DBG("HPFanReset:WriteByteToEC(7): waitportstatus IBF didn't clear\n");
        return false;
    }
    return ok;
}

//-------------------------------------------------------------------------
// entry point of module -
//-------------------------------------------------------------------------

int Fanreset_start()
{
    // reset FAN control EC registers for HP ProBook 4x30s
    
    // first get rid of fake temp setting
    int result = WriteByteToEC(EC_ZONE_SEL, 1); // select CPU zone
    DBG("HPFanReset:Fanreset_start: EC_ZONE_SEL result = %d\n", result);
    if (!result)
        return false;
    
    result = WriteByteToEC(EC_ZONE_TEMP, 0); // zero causes fake temp to reset
    DBG("HPFanReset:Fanreset_start: EC_ZONE_TEMP result = %d\n", result);
    if (!result)
        return false;
    
    // next set fan to automatic
    result = WriteByteToEC(EC_FAN_CONTROL, 0xFF); // 0xFF is "automatic mode"
    DBG("HPFanReset:Fanreset_start: EC_FAN_CONTROL result = %d\n", result);
    if (!result)
        return false;
    
    /* Note: BIOS whitelist hack below is based on this bit of info in ProBook DSDT:
     
     OperationRegion (RCRB, SystemMemory, 0xFED1C000, 0x4000)
     Field (RCRB, DWordAcc, Lock, Preserve)
     {
     Offset (0x1A8),
     APMC,   2,
     Offset (0x1000),
     Offset (0x3000),
     Offset (0x3404),
     HPAS,   2,
     ,   5,
     HPAE,   1,
     Offset (0x3418),
     ,   1,
     ,   1,
     SATD,   1,
     SMBD,   1,
     HDAD,   1,
     Offset (0x341A),
     RP1D,   1,
     RP2D,   1,
     RP3D,   1,
     RP4D,   1,    //!!!!! target bit (set to 1 to disable PCIe card in that slot)
     RP5D,   1,
     RP6D,   1,
     RP7D,   1,
     RP8D,   1
     }
     
     The code below is writing Zero to RP4D.  Bit 3 at address 0xFED1C000+0x341A.
     */
    
    /*
     // quick little hack for BigDonkey
     // clear bit 3 at 0xFED1F41A
     *((unsigned char*)0xFED1F41A) &= (unsigned char)((1 << 3) ^ 0xFF);
     */
    
    return true;
}

/** Entry point. just calls original Fanreset_start() from Chameleon module */
EFI_STATUS
EFIAPI
HPFanResetEntry (
	IN EFI_HANDLE				ImageHandle,
	IN EFI_SYSTEM_TABLE			*SystemTable
	)
{
    DBG("HPFanReset: entry point called\n");
    
    if (Fanreset_start())
        BOOTLOG("HPFanReset: successfully set fan control to BIOS mode.\n");
    else
        BOOTLOG("HPFanReset: Error! Unable to set fan control to BIOS mode.\n");
    
	return EFI_SUCCESS;
}

