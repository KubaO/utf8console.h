// The newest version of this file is always available at
// https://github.com/KubaO/utf8console.h

// utf8console.h
// Copyright (c) 2018 Kuba Ober.
// All rights reserved.
//
// This code is licensed under the MIT License.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// This header has one purpose only:
// Your code uses UTF-8 everywhere. You want console I/O to just work, in UTF-8.
// In fact, you want your entire project to simply be written in UTF-8.
// This is problematic on Windows, and perhaps on other systems.
//
// All you have to do:
//    int main() {
//      auto con = utf8con::make_utf8_io();
//      ...
//    }

// Note: At the moment, not all functionality is implemented. This code uses
// wcout and wcerr. That is not optimal. It should use Windows console I/O instead.

#ifndef UTF8_CONSOLE
#define UTF8_CONSOLE
#ifdef _WIN32
#include <io.h>
#include <stdio.h>
#endif

#include <clocale>
#include <cstdint>
#include <iostream>
#include <locale>
#include <string>

namespace utf8con {

#ifdef _WIN32
static constexpr bool translate = true;
void prepare_console() {
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);
}
#else
static constexpr bool translate = false;
void prepare_console() {
    const char *curLocale = setlocale(LC_ALL, nullptr);
    std::wcout.imbue(std::locale(curLocale));
    std::wcerr.imbue(std::locale(curLocale));
}
#endif

class utf8_on_wide_out : public std::streambuf {
public:
    explicit utf8_on_wide_out(std::wstreambuf *wrapped) : m_out(wrapped) {}
protected:
    int sync() override {
        return m_out->pubsync();
    }
    int_type overflow(int_type ch) override {
        if (ch != traits_type::eof()) {
            auto c = traits_type::to_char_type(ch);
            if ((c & 0x80) == 0x00) {
                left = -1;
                return output((unsigned char)c) ? 0 : traits_type::eof();
            }
            return (output_raw(&c, &c+1) == 1) ? 0 : traits_type::eof();
        }
        return 0;
    }
    std::streamsize xsputn(const char_type* s, std::streamsize count) override {
        return output_raw(s, s+count);
    }
private:
    using wtraits_type = std::wstreambuf::traits_type;
    int left = -1;
    uint32_t point;
    std::wstreambuf *m_out;
    bool output(wtraits_type::int_type ch) {
        return m_out->sputc(wtraits_type::to_char_type(ch)) != wtraits_type::eof();
    }
    int output_raw(const char_type *start, const char_type *end) {
        const char_type *const first = start;
        int left = this->left;
        uint32_t point = this->point;
        while (start != end) {
            unsigned char ch = *start++;
            if ((ch & 0x80) == 0) {
                left = -1; // reset the decoder
                if (output(ch))
                    continue;
                break;
            }
            else if ((ch & 0xC0) == 0x80 && left > 0) {
                point = (ch & 0x3F) | (point << 6);
                left --;
            }
            else if ((ch & 0xE0) == 0xC0) {
                point = ch & 0x1F; left = 1;
            }
            else if ((ch & 0xF0) == 0xE0) {
                point = ch & 0x0F; left = 2;
            }
            else if ((ch & 0xF8) == 0xF0) {
                point = ch & 0x07, left = 3;
            }
            else {
                left = -1;
            }
            if (left == 0) {
                left = -1;
                if (sizeof(wtraits_type::char_type) >= 4 ||
                        point <= 0xD7FF || (point >= 0xE000 && point <= 0xFFFF)) {
                    if (!output(point)) break;
                }
                else if (point >= 0x10000 && point <= 0x10FFFF) {
                    point -= 0x10000;
                    if (!output((point >> 10) + 0xD800)) break;
                    if (!output((point & 0x3FF) + 0xDC00)) break;
                }
            }
        }
        this->point = point;
        this->left = left;
        return start - first;
    }
};

class con_transcoder {
    std::streambuf *m_coutBuf = {}, *m_cerrBuf = {};
public:
    con_transcoder() {
        static bool prepared;
        if (!translate) return;
        if (!prepared)
            prepare_console(), prepared = true;
        if (!dynamic_cast<utf8_on_wide_out*>(std::cout.rdbuf())) {
            std::cout.rdbuf()->pubsync();
            m_coutBuf = std::cout.rdbuf(new utf8_on_wide_out(std::wcout.rdbuf()));
        }
        if (!dynamic_cast<utf8_on_wide_out*>(std::cerr.rdbuf())) {
            std::cerr.rdbuf()->pubsync();
            m_cerrBuf = std::cerr.rdbuf(new utf8_on_wide_out(std::wcerr.rdbuf()));
        }
    }
    ~con_transcoder() {
        if (m_coutBuf)
            std::cout.rdbuf(m_coutBuf);
        if (m_cerrBuf)
            std::cerr.rdbuf(m_cerrBuf);
    }
    con_transcoder(con_transcoder &&) = default;
    con_transcoder(const con_transcoder &) = delete;
    con_transcoder &operator=(con_transcoder &&) = default;
    con_transcoder &operator=(const con_transcoder &) = delete;
};

con_transcoder make_utf8_output() {
    return con_transcoder();
}

}

#endif // UTF8_CONSOLE
