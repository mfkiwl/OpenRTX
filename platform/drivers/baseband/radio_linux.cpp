/***************************************************************************
 *   Copyright (C) 2021 by Federico Amedeo Izzo IU2NUO,                    *
 *                         Niccolò Izzo IU2KIN                             *
 *                         Frederik Saraci IU2NRO                          *
 *                         Silvano Seva IU2KWO                             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   As a special exception, if other files instantiate templates or use   *
 *   macros or inline functions from this file, or you compile this file   *
 *   and link it with other works to produce a work based on this file,    *
 *   this file does not by itself cause the resulting work to be covered   *
 *   by the GNU General Public License. However the source code for this   *
 *   file must still be made available in accordance with the GNU General  *
 *   Public License. This exception does not invalidate any other reasons  *
 *   why a work based on this file might be covered by the GNU General     *
 *   Public License.                                                       *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#include <interfaces/radio.h>
#include <cstdio>
#include <string>

void radio_init(const rtxStatus_t *rtxState)
{
    (void) rtxState;
    puts("radio_linux: init() called");
}

void radio_terminate()
{
    puts("radio_linux: terminate() called");
}

void radio_setOpmode(const enum opmode mode)
{
    std::string mStr(" ");
    if(mode == NONE) mStr = "NONE";
    if(mode == FM)   mStr = "FM";
    if(mode == DMR)  mStr = "DMR";
    if(mode == M17)  mStr = "M17";

    printf("radio_linux: setting opmode to %s\n", mStr.c_str());
}

bool radio_checkRxDigitalSquelch()
{
    puts("radio_linux: radio_checkRxDigitalSquelch(), returning 'true'");
    return true;
}

void radio_enableRx()
{
    puts("radio_linux: enableRx() called");
}

void radio_enableTx()
{
    puts("radio_linux: enableTx() called");
}

void radio_disableRtx()
{
    puts("radio_linux: disableRtx() called");
}

void radio_updateConfiguration()
{
    puts("radio_linux: updateConfiguration() called");
}

float radio_getRssi()
{
    // Commented to reduce verbosity on Linux
    // printf("radio_linux: requested RSSI at freq %d, returning -100dBm\n", rxFreq);
    return -100.0f;
}

enum opstatus radio_getStatus()
{
    return OFF;
}
