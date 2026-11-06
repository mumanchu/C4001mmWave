///////////////////////////////////////////////////////////////////////////
// Floating point conversion routines
// Copyright (C) https://muman.ch + https://github.com/mumanchu, 2026.06.26
// If you re-use this software, please include the above copyright notice
/*
NOTE On 8-bit processors double and float are both 32-bits (both float).

atof() takes 5192 bytes, and sprintf("%f") is even worse. That's a lot 
of code if you only have 32KB flash. These functions are optimized to be 
as fast as possible, way faster and smaller than Arduino's dtostrf().

The AVR Arduinos do not have full support for converting floating point
numbers to strings. Functions like sprintf() and sscanf() will not 
convert floating point. 

'psz' should should point to a buffer of at least 22 characters including
the '\0' terminator. Buffer overflow is NOT checked! I use 32, just to be 
safe.

The maximum value accepted for ****ToString() is +-4294967000.0
(unsigned long minus rounding errors) if the value is larger it returns
false with "NAN" in *result (NAN = not a number).

The output format is 'n.n'. Exponent format ('e+04' etc) is not handled.
The max. number of decimal places is 6 for float. Float holds only 6 or 
7 significant digits so the value may be truncated and rounded up.
*/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <stdlib.h>

static const char utils_nan[] = "NAN";		// or "-E-" for 7-segment display
static const char utils_format1[] = "-%lu";
static const char utils_format2[] = "%lu";
static const char utils_format3[] = "-%lu.%0?lu";
static const char utils_format4[] = "%lu.%0?lu";


// Converts a float to a string with the format [-]1234567890[.123456]
// with max. 6 decimal places with 6 or 7 significant digits.
// The allowed range is +-4294967000.999999.
// Exponent format (e.g. "1e7") is not supported.
bool floatToString(float value, char* psz, unsigned int decimalPlaces)
{
	const float MAX_FORMAT_FLOAT = 4294967000.0f;

	bool negative = value < 0.0f;
	if (negative)
		value = -value;

	// no decimal places, integer part only
	if (decimalPlaces == 0) {
		float v = value + 0.5f;		// round up
		if (v > MAX_FORMAT_FLOAT) {
			strcpy(psz, utils_nan);
			return false;
		}
		sprintf(psz, negative ? utils_format1 : utils_format2, (unsigned long)v);
		return true;
	}

	// max 6 decimal places for float
	if (decimalPlaces > 6)
		decimalPlaces = 6;

	// integer part
	if (value > MAX_FORMAT_FLOAT) {
		strcpy(psz, utils_nan);
		return false;
	}
	unsigned long left = (unsigned long)value;

	// decimal part, multiply by 10^decimalPlaces to get an integer
	float v = value - (float)left;
	static const float pwr10[6] =
		{ 10.0f, 100.0f, 1000.0f, 10000.0f, 100000.0f, 1000000.0f };
	float pwr = pwr10[decimalPlaces - 1];
	v *= pwr;
	v += 0.5f;			// round up
	if (v > pwr) {		// handle overflow 
		v -= pwr;
		left += 1;
	}
	unsigned long right = (unsigned long)v;

	// format 'left.right' integers according to decimalPlaces
	char dp = '0' + decimalPlaces;
	char format[20];
	if (negative) {
		strcpy(format, utils_format3);
		format[7] = dp;
	}
	else {
		strcpy(format, utils_format4);
		format[6] = dp;
	}
	sprintf(psz, format, left, right);

	return true;
}

// Convert a string to a float
// String format: "[+|-]4294967295.123456"
// Exponents ('e+04' etc) are not handled.
// The integer part can be up to +-4294967295, the decimal part can be 
// up to 6 digits, but note that float holds only 6 or 7 significant 
// digits so it may be rounded/truncated.
// Returns false with *result = NAN if it fails to convert (bad format  
// or overflow).
// *pszEnd points to the next unprocessed character.
bool stringToFloat(const char* psz, float* result, char** pszEnd)
{
	*result = NAN;
	errno = 0;

	// skip leading spaces
	while (*psz == ' ')
		++psz;

	// sign
	char ch = *psz;
	bool negative = ch == '-';
	if (negative || ch == '+')
		ch = *++psz;
	*pszEnd = (char*)psz;

	// must be at least 1 digit
	if (ch < '0' || ch > '9')
		return false;

	// integer part
	char* endPtr1;
	unsigned long left = strtoul(psz, &endPtr1, 10);
	if (errno)		// 'errno' is the only way to detect arithmetic overflow
		return false;
	float f = (float)left;
	ch = *endPtr1;
	if (ch != '.') {
		*pszEnd = endPtr1;
	}
	else {
		// decimal part
		ch = *++endPtr1;
		// must be at least 1 digit
		if (ch < '0' || ch > '9') {
			*pszEnd = endPtr1;
			return false;
		}
		char* endPtr2;
		unsigned long right = strtoul(endPtr1, &endPtr2, 10);
		*pszEnd = endPtr2;
		if (errno)
			return false;
		if (right != 0) {
			unsigned int decimalPlaces = (unsigned int)(endPtr2 - endPtr1);
			if (decimalPlaces > 6)
				return false;
			static const float pwr10[6] =
				{ 0.1f, 0.01f, 0.001f, 0.0001f, 0.00001f, 0.000001f };
			f += (float)right * pwr10[decimalPlaces - 1];
		}
	}
	*result = (negative && f != 0.0f) ? -f : f;
	return true;
}

