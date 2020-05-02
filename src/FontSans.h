/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
** Copyright (c) 2010, Monash University
** All rights reserved.
** Redistribution and use in source and binary forms, with or without modification,
** are permitted provided that the following conditions are met:
**
**       * Redistributions of source code must retain the above copyright notice,
**          this list of conditions and the following disclaimer.
**       * Redistributions in binary form must reproduce the above copyright
**         notice, this list of conditions and the following disclaimer in the
**         documentation and/or other materials provided with the distribution.
**       * Neither the name of the Monash University nor the names of its contributors
**         may be used to endorse or promote products derived from this software
**         without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
** THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
** PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
** BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
** CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
** SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
** OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
**
** Contact:
*%  Owen Kaluza - Owen.Kaluza(at)monash.edu
*%
*% Development Team :
*%  http://www.underworldproject.org/aboutus.html
**
**~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

/***
 * 3D vertex data in "font.bin" for first 95 characters from the Open Font License font "FreeUniversal":
 * (last char is a custom hard coded square glyph)
 *
 * Downloaded from:
 * https://fontlibrary.org/en/font/freeuniversal
 *
 * License details:
 * http://scripts.sil.org/cms/scripts/page.php?site_id=nrsi&id=OFL
 *
 * Charset: ASCII 32-127 (custom last char #96)
 * !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~■
 *
 * Font data exported using "Font23D" by Harishankar Narayanan
 * original project: https://github.com/codetiger/Font23D
 * modified version: https://github.com/OKaluza/Font23D
 *
 ***/

#ifndef FontSans__
#define FontSans__

#include "Include.h"

float font_charwidths[] = {9.000000, 11.046875, 13.531250, 22.359375, 16.765625, 27.828125, 22.968750, 11.046875, 10.781250, 10.781250, 19.687500, 30.000000, 8.265625, 9.578125, 8.265625, 11.890625, 16.765625, 16.765625, 16.765625, 16.765625, 16.765625, 16.765625, 16.765625, 16.765625, 16.765625, 16.765625, 8.265625, 8.265625, 30.000000, 30.000000, 30.000000, 15.500000, 30.000000, 22.500000, 20.187500, 20.390625, 21.593750, 18.187500, 17.078125, 22.828125, 21.265625, 7.828125, 17.046875, 19.921875, 15.875000, 26.921875, 21.687500, 23.203125, 18.656250, 23.531250, 18.875000, 19.828125, 18.375000, 22.093750, 19.593750, 29.156250, 21.093750, 19.953125, 18.453125, 11.890625, 11.890625, 11.890625, 30.000000, 15.000000, 6.890625, 16.546875, 17.703125, 15.609375, 17.718750, 16.703125, 9.875000, 17.703125, 17.171875, 6.890625, 8.578125, 14.937500, 6.890625, 26.859375, 17.203125, 17.750000, 17.953125, 17.953125, 10.875000, 15.093750, 10.484375, 17.171875, 16.031250, 24.406250, 16.406250, 16.031250, 14.031250, 14.625000, 10.343750, 14.625000, 16.000000, 21.0 };

std::vector<unsigned> font_tricounts = {0,14,14,62,367,517,357,5,33,33,33,21,5,5,5,5,318,39,151,211,26,139,328,11,347,330,14,14,11,14,11,254,398,20,171,145,122,21,17,155,21,5,61,19,9,23,17,198,126,320,176,329,13,113,11,23,21,15,17,13,5,13,11,5,5,244,138,129,186,212,69,262,93,14,110,19,5,123,85,126,128,128,33,193,69,69,11,23,21,29,17,227,5,227,81,2};

#endif //FontSans__
