/*
    bench.h - Demo program to benchmark open-source compression algorithm
    Copyright (C) Yann Collet 2012

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

	You can contact the author at :
	- LZ4 homepage : http://fastcompression.blogspot.com/p/lz4.html
	- LZ4 source repository : http://code.google.com/p/lz4/
*/
#pragma once

#if defined (__cplusplus)
extern "C" {
#endif


int BMK_benchFile(char** fileNamesTable, int nbFiles, int cLevel);

// Parameters
void BMK_SetBlocksize(int bsize);
void BMK_SetNbIterations(int nbLoops);
void BMK_SetPause();



#if defined (__cplusplus)
}
#endif
