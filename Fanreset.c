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


#include "libsaio.h"
#include "modules.h"
#include "Fanreset.h"

typedef unsigned char byte;

// Registers of the embedded controller
#define EC_DATAPORT		(0x62)	    // EC data io-port
#define EC_CTRLPORT		(0x66)	    // EC control io-port

// Embedded controller status register bits
#define EC_STAT_OBF		(0x01)      // Output buffer full 
#define EC_STAT_IBF		(0x02)      // Input buffer full 
#define EC_STAT_CMD		(0x08)      // Last write was a command write (0=data) 

// Embedded controller commands
// (write to EC_CTRLPORT to initiate read/write operation)
#define EC_CTRLPORT_READ    ((byte)0x80)
#define EC_CTRLPORT_WRITE	((byte)0x81)
#define EC_CTRLPORT_QUERY	((byte)0x84)

// Embedded controller offsets
#define EC_ZONE_SEL     (0x22)      // CRZN in DSDT
#define EC_ZONE_TEMP    (0x26)      // TEMP in DSDT
#define EC_FAN_CONTROL  (0x2F)      // FTGC in DSDT

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
	if (!ok) return false;
	    
    // tell 'em we want to "WRITE"
    ok = writeport(EC_CTRLPORT, EC_CTRLPORT_WRITE);
    if (!ok) return false;
    
    // wait for IBF to clear (command byte removed from EC's input queue)
    ok = waitportstatus(EC_STAT_IBF, 0);
    if (!ok) return false;

    // tell 'em where we want to write to
    ok = writeport(EC_DATAPORT, offset);
    if (!ok) return false;

    // wait for IBF to clear (address byte removed from EC's input queue)
    ok = waitportstatus(EC_STAT_IBF, 0);
    if (!ok) return false;
    
    // tell 'em what we want to write there
    ok = writeport(EC_DATAPORT, data);
    if (!ok) return false;
    
    // wait for IBF to clear (data byte removed from EC's input queue)
    ok = waitportstatus(EC_STAT_IBF, 0);
	return ok;
}

//-------------------------------------------------------------------------
// entry point of module -
//-------------------------------------------------------------------------

void Fanreset_start()
{
    // reset FAN control EC registers for HP ProBook 4x30s

    // first get rid of fake temp setting
    WriteByteToEC(EC_ZONE_SEL, 1);  // select CPU zone
    WriteByteToEC(EC_ZONE_TEMP, 0); // zero causes fake temp to reset
    
    // next set fan to automatic
    WriteByteToEC(EC_FAN_CONTROL, 0xFF); // 0xFF is "automatic mode"

/*
    // quick little hack for BigDonkey
    // clear bit 3 at 0xFED1F41A
    *((unsigned char*)0xFED1F41A) &= (unsigned char)((1 << 3) ^ 0xFF);
*/
}

