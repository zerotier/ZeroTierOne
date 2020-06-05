/*
 *  TAP-Windows -- A kernel driver to provide virtual tap
 *                 device functionality on Windows.
 *
 *  This code was inspired by the CIPE-Win32 driver by Damion K. Wilson.
 *
 *  This source code is Copyright (C) 2002-2014 OpenVPN Technologies, Inc.,
 *  and is released under the GPL version 2 (see below).
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program (see the file COPYING included with this
 *  distribution); if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include "tap.h"

int
HexStringToDecimalInt (const int p_Character)
{
    int l_Value = 0;

    if (p_Character >= 'A' && p_Character <= 'F')
        l_Value = (p_Character - 'A') + 10;
    else if (p_Character >= 'a' && p_Character <= 'f')
        l_Value = (p_Character - 'a') + 10;
    else if (p_Character >= '0' && p_Character <= '9')
        l_Value = p_Character - '0';

    return l_Value;
}

BOOLEAN
ParseMAC (MACADDR dest, const char *src)
{
    int c;
    int mac_index = 0;
    BOOLEAN high_digit = FALSE;
    int delim_action = 1;

    ASSERT (src);
    ASSERT (dest);

    CLEAR_MAC (dest);

    while (c = *src++)
    {
        if (IsMacDelimiter (c))
        {
            mac_index += delim_action;
            high_digit = FALSE;
            delim_action = 1;
        }
        else if (IsHexDigit (c))
        {
            const int digit = HexStringToDecimalInt (c);
            if (mac_index < sizeof (MACADDR))
            {
                if (!high_digit)
                {
                    dest[mac_index] = (char)(digit);
                    high_digit = TRUE;
                    delim_action = 1;
                }
                else
                {
                    dest[mac_index] = (char)(dest[mac_index] * 16 + digit);
                    ++mac_index;
                    high_digit = FALSE;
                    delim_action = 0;
                }
            }
            else
                return FALSE;
        }
        else
            return FALSE;
    }

    return (mac_index + delim_action) >= sizeof (MACADDR);
}

/*
 * Generate a MAC using the GUID in the adapter name.
 *
 * The mac is constructed as 00:FF:xx:xx:xx:xx where
 * the Xs are taken from the first 32 bits of the GUID in the
 * adapter name.  This is similar to the Linux 2.4 tap MAC
 * generator, except linux uses 32 random bits for the Xs.
 *
 * In general, this solution is reasonable for most
 * applications except for very large bridged TAP networks,
 * where the probability of address collisions becomes more
 * than infintesimal.
 *
 * Using the well-known "birthday paradox", on a 1000 node
 * network the probability of collision would be
 * 0.000116292153.  On a 10,000 node network, the probability
 * of collision would be 0.01157288998621678766.
 */

VOID
GenerateRandomMac(
    __in MACADDR mac,
    __in const unsigned char *adapter_name
    )
{
    unsigned const char *cp = adapter_name;
    unsigned char c;
    unsigned int i = 2;
    unsigned int byte = 0;
    int brace = 0;
    int state = 0;

    CLEAR_MAC (mac);

    mac[0] = 0x00;
    mac[1] = 0xFF;

    while (c = *cp++)
    {
        if (i >= sizeof (MACADDR))
            break;
        if (c == '{')
            brace = 1;
        if (IsHexDigit (c) && brace)
        {
            const unsigned int digit = HexStringToDecimalInt (c);
            if (state)
            {
                byte <<= 4;
                byte |= digit;
                mac[i++] = (unsigned char) byte;
                state = 0;
            }
            else
            {
                byte = digit;
                state = 1;
            }
        }
    }
}

VOID
GenerateRelatedMAC(
    __in MACADDR dest,
    __in const MACADDR src,
    __in const int delta
    )
{
    ETH_COPY_NETWORK_ADDRESS (dest, src);
    dest[2] += (UCHAR) delta;
}
