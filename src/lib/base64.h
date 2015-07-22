/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2006 Free Software Foundation Europe e.V.

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
/*
 * Generic base 64 input and output routines
 *
 * Written by Kern E. Sibbald, March MM.
 */

/* Maximum size of len bytes after base64 encoding */
#define BASE64_SIZE(len) ((4 * len + 2) / 3 + 1)

// #define BASE64_SIZE(len) (((len + 3 - (len % 3)) / 3) * 4)
