/*
 WString.cpp - String library for Wiring & Arduino
 ...mostly rewritten by Paul Stoffregen...
 Copyright (c) 2009-10 Hernando Barragan.  All rights reserved.
 Copyright 2011, Paul Stoffregen, paul@pjrc.com
 Modified by Ivan Grokhotkov, 2014 - esp8266 support
 Modified by Michael C. Miller, 2015 - esp8266 progmem support

 Modified by Torsten Wolf, 2023 - remove arduino dependencies to use in framework native
 (find original version in "framework-arduinoespressif8266/cores/esp8266")

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <limits>
#include "WString.h"
#include "math.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"

#define __STRHELPER(x) #x
#define STR(x) __STRHELPER(x) // stringifier

extern "C" {
char * dtostrf(double number, signed char width, unsigned char prec, char *s) {
    bool negative = false;

    if (isnan(number)) {
        strcpy(s, "nan");
        return s;
    }
    if (isinf(number)) {
        strcpy(s, "inf");
        return s;
    }

    char* out = s;

    int fillme = width; // how many cells to fill for the integer part
    if (prec > 0) {
        fillme -= (prec+1);
    }

    // Handle negative numbers
    if (number < 0.0) {
        negative = true;
        fillme--;
        number = -number;
    }

    // Round correctly so that print(1.999, 2) prints as "2.00"
    // I optimized out most of the divisions
    double rounding = 2.0;
    for (uint8_t i = 0; i < prec; ++i)
        rounding *= 10.0;
    rounding = 1.0 / rounding;

    number += rounding;

    // Figure out how big our number really is
    double tenpow = 1.0;
    int digitcount = 1;
    double nextpow;
    while (number >= (nextpow = (10.0 * tenpow))) {
        tenpow = nextpow;
        digitcount++;
    }

    // minimal compensation for possible lack of precision (#7087 addition)
    number *= 1 + std::numeric_limits<decltype(number)>::epsilon();

    number /= tenpow;
    fillme -= digitcount;

    // Pad unused cells with spaces
    while (fillme-- > 0) {
        *out++ = ' ';
    }

    // Handle negative sign
    if (negative) *out++ = '-';

    // Print the digits, and if necessary, the decimal point
    digitcount += prec;
    int8_t digit = 0;
    while (digitcount-- > 0) {
        digit = (int8_t)number;
        if (digit > 9) digit = 9; // insurance
        *out++ = (char)('0' | digit);
        if ((digitcount == prec) && (prec > 0)) {
            *out++ = '.';
        }
        number -= digit;
        number *= 10.0;
    }

    // make sure the string is terminated
    *out = 0;
    return s;
}
};

/*********************************************/
/*  Constructors                             */
/*********************************************/

String::String(const char *cstr) {
    init();
    if (cstr)
        copy(cstr, strlen(cstr));
}

String::String(const String &value) {
    init();
    *this = value;
}

// String::String(const __FlashStringHelper *pstr) {
//     init();
//     *this = pstr; // see operator =
// }

String::String(String &&rval) noexcept {
    init();
    move(rval);
}

String::String(unsigned char value, unsigned char base) {
    init();
    char buf[1 + 8 * sizeof(unsigned char)];
    itoa((int)value, buf, base);
    *this = buf;
}

String::String(int value, unsigned char base) {
    init();
    char buf[2 + 8 * sizeof(int)];
    if (base == 10) {
        sprintf(buf, "%d", value);
    } else {
        itoa(value, buf, base);
    }
    *this = buf;
}

String::String(unsigned int value, unsigned char base) {
    init();
    char buf[1 + 8 * sizeof(unsigned int)];
    itoa((int)value, buf, base);
    *this = buf;
}

String::String(long value, unsigned char base) {
    init();
    char buf[2 + 8 * sizeof(long)];
    if (base == 10) {
        sprintf(buf, "%ld", value);
    } else {
        ltoa(value, buf, base);
    }
    *this = buf;
}

String::String(unsigned long value, unsigned char base) {
    init();
    char buf[1 + 8 * sizeof(unsigned long)];
    ltoa((long)value, buf, base);
    *this = buf;
}

String::String(long long value) {
    init();
    char buf[2 + 8 * sizeof(long long)];
    sprintf(buf, "%lld", value);
    *this = buf;
}

String::String(unsigned long long value) {
    init();
    char buf[1 + 8 * sizeof(unsigned long long)];
    sprintf(buf, "%llu", value);
    *this = buf;
}

String::String(long long value, unsigned char base) {
    init();
    char buf[2 + 8 * sizeof(long long)];
    *this = lltoa(value, buf, sizeof(buf));
}

String::String(unsigned long long value, unsigned char base) {
    init();
    char buf[1 + 8 * sizeof(unsigned long long)];
    *this = ulltoa(value, buf, sizeof(buf));
}

String::String(float value, unsigned char decimalPlaces) {
    init();
    char buf[33];
    *this = dtostrf(value, (decimalPlaces + 2), decimalPlaces, buf);
}

String::String(double value, unsigned char decimalPlaces) {
    init();
    char buf[33];
    *this = dtostrf(value, (decimalPlaces + 2), decimalPlaces, buf);
}

/*********************************************/
/*  Memory Management                        */
/*********************************************/

void String::invalidate(void) {
    if (!isSSO() && wbuffer())
        free(wbuffer());
    init();
}

bool String::reserve(unsigned int size) {
    if (buffer() && capacity() >= size)
        return true;
    if (changeBuffer(size)) {
        if (len() == 0)
            wbuffer()[0] = 0;
        return true;
    }
    return false;
}

bool String::changeBuffer(unsigned int maxStrLen) {
    // Can we use SSO here to avoid allocation?
    if (maxStrLen < sizeof(sso.buff) - 1) {
        if (isSSO() || !buffer()) {
            // Already using SSO, nothing to do
            uint16_t oldLen = len();
            setSSO(true);
            setLen(oldLen);
        } else { // if bufptr && !isSSO()
            // Using bufptr, need to shrink into sso.buff
            const char *temp = buffer();
            uint16_t oldLen = len();
            setSSO(true);
            setLen(oldLen);
            memcpy(wbuffer(), temp, maxStrLen);
            free((void *)temp);
        }
        return true;
    }
    // Fallthrough to normal allocator
    size_t newSize = (maxStrLen + 16) & (~0xf);
    // Make sure we can fit newsize in the buffer
    if (newSize > CAPACITY_MAX) {
        return false;
    }
    uint16_t oldLen = len();
    char *newbuffer = (char *)realloc(isSSO() ? nullptr : wbuffer(), newSize);
    if (newbuffer) {
        size_t oldSize = capacity() + 1; // include NULL.
        if (isSSO()) {
            // Copy the SSO buffer into allocated space
            memmove(newbuffer, sso.buff, sizeof(sso.buff));
        }
        if (newSize > oldSize) {
            memset(newbuffer + oldSize, 0, newSize - oldSize);
        }
        setSSO(false);
        setCapacity(newSize - 1);
        setLen(oldLen); // Needed in case of SSO where len() never existed
        setBuffer(newbuffer);
        return true;
    }
    return false;
}

/*********************************************/
/*  Copy and Move                            */
/*********************************************/

String &String::copy(const char *cstr, unsigned int length) {
    if (!reserve(length)) {
        invalidate();
        return *this;
    }
    setLen(length);
    memmove(wbuffer(), cstr, length + 1);
    return *this;
}

// String &String::copy(const __FlashStringHelper *pstr, unsigned int length) {
//     if (!reserve(length)) {
//         invalidate();
//         return *this;
//     }
//     setLen(length);
//     memcpy_P(wbuffer(), (PGM_P)pstr, length + 1); // We know wbuffer() cannot ever be in PROGMEM, so memcpy safe here
//     return *this;
// }

void String::move(String &rhs) noexcept {
    invalidate();
    sso = rhs.sso;
    rhs.init();
    }

String &String::operator =(const String &rhs) {
    if (this == &rhs)
        return *this;
    if (rhs.buffer())
        copy(rhs.buffer(), rhs.len());
    else
        invalidate();
    return *this;
}

String &String::operator =(String &&rval) noexcept {
    if (this != &rval)
        move(rval);
    return *this;
}

String &String::operator =(const char *cstr) {
    if (cstr)
        copy(cstr, strlen(cstr));
    else
        invalidate();
    return *this;
}

// String & String::operator = (const __FlashStringHelper *pstr)
// {
//     if (pstr) copy(pstr, strlen_P((PGM_P)pstr));
//     else invalidate();

//     return *this;
// }

/*********************************************/
/*  concat                                   */
/*********************************************/

bool String::concat(const String &s) {
    // Special case if we're concatting ourself (s += s;) since we may end up
    // realloc'ing the buffer and moving s.buffer in the method called
    if (&s == this) {
        unsigned int newlen = 2 * len();
        if (!s.buffer())
            return false;
        if (s.len() == 0)
            return true;
        if (!reserve(newlen))
            return false;
        memmove(wbuffer() + len(), buffer(), len());
        setLen(newlen);
        wbuffer()[newlen] = 0;
        return true;
    } else {
        return concat(s.buffer(), s.len());
    }
}

bool String::concat(const char *cstr, unsigned int length) {
    unsigned int newlen = len() + length;
    if (!cstr)
        return false;
    if (length == 0)
        return true;
    if (!reserve(newlen))
        return false;
    memmove(wbuffer() + len(), cstr, length);
    setLen(newlen);
    wbuffer()[newlen] = 0;
    return true;
}

bool String::concat(const char *cstr) {
    if (!cstr)
        return false;
    return concat(cstr, strlen(cstr));
}

bool String::concat(char c) {
    return concat(&c, 1);
}

bool String::concat(unsigned char num) {
    char buf[1 + 3 * sizeof(unsigned char)];
    return concat(buf, sprintf(buf, "%d", num));
}

bool String::concat(int num) {
    char buf[2 + 3 * sizeof(int)];
    return concat(buf, sprintf(buf, "%d", num));
}

bool String::concat(unsigned int num) {
    char buf[1 + 3 * sizeof(unsigned int)];
    itoa((int)num, buf, 10);
    return concat(buf, strlen(buf));
}

bool String::concat(long num) {
    char buf[2 + 3 * sizeof(long)];
    return concat(buf, sprintf(buf, "%ld", num));
}

bool String::concat(unsigned long num) {
    char buf[1 + 3 * sizeof(unsigned long)];
    ltoa((long)num, buf, 10);
    return concat(buf, strlen(buf));
}

bool String::concat(long long num) {
    char buf[2 + 3 * sizeof(long long)];
    return concat(buf, sprintf(buf, "%lld", num));
}

bool String::concat(unsigned long long num) {
    char buf[1 + 3 * sizeof(unsigned long long)];
    return concat(buf, sprintf(buf, "%llu", num));
}

bool String::concat(float num) {
    char buf[20];
    char *string = dtostrf(num, 4, 2, buf);
    return concat(string, strlen(string));
}

bool String::concat(double num) {
    char buf[20];
    char *string = dtostrf(num, 4, 2, buf);
    return concat(string, strlen(string));
}

//bool String::concat(const __FlashStringHelper *str) {
//    if (!str)
//        return false;
//    int length = strlen_P((PGM_P)str);
//    if (length == 0)
//        return true;
//    unsigned int newlen = len() + length;
//    if (!reserve(newlen))
//        return false;
//    memcpy_P(wbuffer() + len(), (PGM_P)str, length + 1);
//    setLen(newlen);
//    return true;
//}

/*********************************************/
/*  Insert                                   */
/*********************************************/

String &String::insert(size_t position, const char *other, size_t other_length) {
    if (position > length())
        return *this;

    auto len = length();
    auto total = len + other_length;
    if (!reserve(total))
        return *this;

    auto left = len - position;
    setLen(total);

    auto *start = wbuffer() + position;
    memmove(start + other_length, start, left);
    memmove(start, other, other_length);
    wbuffer()[total] = '\0';

    return *this;
}

//String &String::insert(size_t position, const __FlashStringHelper *other) {
//    auto *p = reinterpret_cast<const char*>(other);
//    return insert(position, p, strlen_P(p));
//}

String &String::insert(size_t position, char other) {
    char tmp[2] { other, '\0' };
    return insert(position, tmp, 1);
}

String &String::insert(size_t position, const char *other) {
    return insert(position, other, strlen(other));
}

String &String::insert(size_t position, const String &other) {
    return insert(position, other.c_str(), other.length());
}

String operator +(const String &lhs, String &&rhs) {
    String res;
    auto total = lhs.length() + rhs.length();
    if (rhs.capacity() > total) {
        rhs.insert(0, lhs);
        res = std::move(rhs);
    } else {
        res.reserve(total);
        res += lhs;
        res += rhs;
        rhs.invalidate();
}

    return res;
}

String operator +(String &&lhs, String &&rhs) {
    String res;
    auto total = lhs.length() + rhs.length();
    if ((total > lhs.capacity()) && (total < rhs.capacity())) {
        rhs.insert(0, lhs);
        res = std::move(rhs);
    } else {
        lhs += rhs;
        rhs.invalidate();
        res = std::move(lhs);
}

    return res;
}

String operator +(char lhs, const String &rhs) {
    String res;
    res.reserve(rhs.length() + 1);
    res += lhs;
    res += rhs;
    return res;
}

String operator +(const char *lhs, const String &rhs) {
    String res;
    res.reserve(strlen(lhs) + rhs.length());
    res += lhs;
    res += rhs;
    return res;
}

/*********************************************/
/*  Comparison                               */
/*********************************************/

int String::compareTo(const String &s) const {
    if (!buffer() || !s.buffer()) {
        if (s.buffer() && s.len() > 0)
            return 0 - *(unsigned char *)s.buffer();
        if (buffer() && len() > 0)
            return *(unsigned char *)buffer();
        return 0;
    }
    return strcmp(buffer(), s.buffer());
}

bool String::equals(const String &s2) const {
    return (len() == s2.len() && compareTo(s2) == 0);
}

bool String::equals(const char *cstr) const {
    if (len() == 0)
        return (cstr == NULL || *cstr == 0);
    if (cstr == NULL)
        return buffer()[0] == 0;
    return strcmp(buffer(), cstr) == 0;
}

bool String::operator<(const String &rhs) const {
    return compareTo(rhs) < 0;
}

bool String::operator>(const String &rhs) const {
    return compareTo(rhs) > 0;
}

bool String::operator<=(const String &rhs) const {
    return compareTo(rhs) <= 0;
}

bool String::operator>=(const String &rhs) const {
    return compareTo(rhs) >= 0;
}

bool String::equalsIgnoreCase(const String &s2) const {
    if (this == &s2)
        return true;
    if (len() != s2.len())
        return false;
    if (len() == 0)
        return true;
    const char *p1 = buffer();
    const char *p2 = s2.buffer();
    while (*p1) {
        if (tolower(*p1++) != tolower(*p2++))
            return false;
    }
    return true;
}

unsigned char String::equalsConstantTime(const String &s2) const {
    // To avoid possible time-based attacks present function
    // compares given strings in a constant time.
    if (len() != s2.len())
        return 0;
    //at this point lengths are the same
    if (len() == 0)
        return 1;
    //at this point lengths are the same and non-zero
    const char *p1 = buffer();
    const char *p2 = s2.buffer();
    unsigned int equalchars = 0;
    unsigned int diffchars = 0;
    while (*p1) {
        if (*p1 == *p2)
            ++equalchars;
        else
            ++diffchars;
        ++p1;
        ++p2;
    }
    //the following should force a constant time eval of the condition without a compiler "logical shortcut"
    unsigned char equalcond = (equalchars == len());
    unsigned char diffcond = (diffchars == 0);
    return (equalcond & diffcond); //bitwise AND
}

bool String::startsWith(const String &s2) const {
    if (len() < s2.len())
        return false;
    return startsWith(s2, 0);
}

bool String::startsWith(const String &s2, unsigned int offset) const {
    if (offset > (unsigned)(len() - s2.len()) || !buffer() || !s2.buffer())
        return false;
    return strncmp(&buffer()[offset], s2.buffer(), s2.len()) == 0;
}

bool String::endsWith(const String &s2) const {
    if (len() < s2.len() || !buffer() || !s2.buffer())
        return false;
    return strcmp(&buffer()[len() - s2.len()], s2.buffer()) == 0;
}

/*********************************************/
/*  Character Access                         */
/*********************************************/

void String::setCharAt(unsigned int loc, char c) {
    if (loc < len())
        wbuffer()[loc] = c;
}

char &String::operator[](unsigned int index) {
    static char dummy_writable_char;
    if (index >= len() || !buffer()) {
        dummy_writable_char = 0;
        return dummy_writable_char;
    }
    return wbuffer()[index];
}

char String::operator[](unsigned int index) const {
    if (index >= len() || !buffer())
        return '\0';
    return buffer()[index];
}

void String::getBytes(unsigned char *buf, unsigned int bufsize, unsigned int index) const {
    if (!bufsize || !buf)
        return;
    if (index >= len()) {
        buf[0] = 0;
        return;
    }
    unsigned int n = bufsize - 1;
    if (n > len() - index)
        n = len() - index;
    strncpy((char *)buf, buffer() + index, n);
    buf[n] = 0;
}

/*********************************************/
/*  Search                                   */
/*********************************************/

int String::indexOf(char ch, unsigned int fromIndex) const {
    if (fromIndex >= len())
        return -1;
    const char *temp = strchr(buffer() + fromIndex, ch);
    if (temp == NULL)
        return -1;
    return temp - buffer();
}

int String::indexOf(const char *s2, unsigned int fromIndex) const {
    if (fromIndex >= len())
        return -1;
    const char *found = strstr(buffer() + fromIndex, s2);
    if (found == NULL)
        return -1;
    return found - buffer();
}

int String::indexOf(const String &s2, unsigned int fromIndex) const {
    return indexOf(s2.c_str(), fromIndex);
}

int String::lastIndexOf(char ch) const {
    return lastIndexOf(ch, len() - 1);
}

int String::lastIndexOf(char ch, unsigned int fromIndex) const {
    if (fromIndex >= len())
        return -1;
    char *writeTo = wbuffer();
    char tempchar = writeTo[fromIndex + 1]; // save the replaced character
    writeTo[fromIndex + 1] = '\0';
    char *temp = strrchr(writeTo, ch);
    writeTo[fromIndex + 1] = tempchar; // restore character
    if (temp == NULL)
        return -1;
    return temp - writeTo;
}

int String::lastIndexOf(const String &s2) const {
    return lastIndexOf(s2, len() - s2.len());
}

int String::lastIndexOf(const String &s2, unsigned int fromIndex) const {
    if (s2.len() == 0 || len() == 0 || s2.len() > len())
        return -1;
    if (fromIndex >= len())
        fromIndex = len() - 1;
    int found = -1;
    for (const char *p = buffer(); p <= buffer() + fromIndex; p++) {
        p = strstr(p, s2.buffer());
        if (!p)
            break;
        if ((unsigned int)(p - buffer()) <= fromIndex)
            found = p - buffer();
    }
    return found;
}

String String::substring(unsigned int left, unsigned int right) const {
    if (left > right) {
        unsigned int temp = right;
        right = left;
        left = temp;
    }
    String out;
    if (left >= len())
        return out;
    if (right > len())
        right = len();
    char *writeTo = wbuffer();
    char tempchar = writeTo[right]; // save the replaced character
    writeTo[right] = '\0';
    out = writeTo + left; // pointer arithmetic
    writeTo[right] = tempchar; // restore character
    return out;
}

/*********************************************/
/*  Modification                             */
/*********************************************/

void String::replace(char find, char replace) {
    if (!buffer())
        return;
    for (char *p = wbuffer(); *p; p++) {
        if (*p == find)
            *p = replace;
    }
}

void String::replace(const String &find, const String &replace) {
    if (len() == 0 || find.len() == 0)
        return;
    int diff = replace.len() - find.len();
    char *readFrom = wbuffer();
    char *foundAt;
    if (diff == 0) {
        while ((foundAt = strstr(readFrom, find.buffer())) != NULL) {
            memmove(foundAt, replace.buffer(), replace.len());
            readFrom = foundAt + replace.len();
        }
    } else if (diff < 0) {
        char *writeTo = wbuffer();
        while ((foundAt = strstr(readFrom, find.buffer())) != NULL) {
            unsigned int n = foundAt - readFrom;
            memmove(writeTo, readFrom, n);
            writeTo += n;
            memmove(writeTo, replace.buffer(), replace.len());
            writeTo += replace.len();
            readFrom = foundAt + find.len();
            setLen(len() + diff);
        }
        memmove(writeTo, readFrom, strlen(readFrom)+1);
    } else {
        unsigned int size = len(); // compute size needed for result
        while ((foundAt = strstr(readFrom, find.buffer())) != NULL) {
            readFrom = foundAt + find.len();
            size += diff;
        }
        if (size == len())
            return;
        if (size > capacity() && !changeBuffer(size))
            return; // XXX: tell user!
        int index = len() - 1;
        while (index >= 0 && (index = lastIndexOf(find, index)) >= 0) {
            readFrom = wbuffer() + index + find.len();
            memmove(readFrom + diff, readFrom, len() - (readFrom - buffer()));
            int newLen = len() + diff;
            memmove(wbuffer() + index, replace.buffer(), replace.len());
            setLen(newLen);
            wbuffer()[newLen] = 0;
            index--;
        }
    }
}

void String::remove(unsigned int index, unsigned int count) {
    if (index >= len()) {
        return;
    }
    if (count <= 0) {
        return;
    }
    if (count > len() - index) {
        count = len() - index;
    }
    char *writeTo = wbuffer() + index;
    unsigned int newlen = len() - count;
    setLen(newlen);
    memmove(writeTo, wbuffer() + index + count, newlen - index);
    wbuffer()[newlen] = 0;
}

void String::toLowerCase(void) {
    if (!buffer())
        return;
    for (char *p = wbuffer(); *p; p++) {
        *p = tolower(*p);
    }
}

void String::toUpperCase(void) {
    if (!buffer())
        return;
    for (char *p = wbuffer(); *p; p++) {
        *p = toupper(*p);
    }
}

void String::trim(void) {
    if (!buffer() || len() == 0)
        return;
    char *begin = wbuffer();
    while (isspace(*begin))
        begin++;
    char *end = wbuffer() + len() - 1;
    while (isspace(*end) && end >= begin)
        end--;
    unsigned int newlen = end + 1 - begin;
    setLen(newlen);
    if (begin > buffer())
        memmove(wbuffer(), begin, newlen);
    wbuffer()[newlen] = 0;
}

/*********************************************/
/*  Parsing / Conversion                     */
/*********************************************/

long String::toInt(void) const {
    if (buffer())
        return atol(buffer());
    return 0;
}

float String::toFloat(void) const {
    if (buffer())
        return atof(buffer());
    return 0.0F;
}

double String::toDouble(void) const {
    if (buffer())
        return atof(buffer());
    return 0.0;
}

// global empty string to allow returning const String& with nothing

const String emptyString;
