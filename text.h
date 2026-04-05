#pragma once

#include <string>
#include <vector>

// Check if byte pair is a lowercase Cyrillic letter (а–я, ё)
inline bool isCyrillicLower(unsigned char c1, unsigned char c2) {
    return (c1 == 0xD0 && c2 >= 0xB0 && c2 <= 0xBF)   // а–п
        || (c1 == 0xD1 && c2 >= 0x80 && c2 <= 0x8F)   // р–я
        || (c1 == 0xD1 && c2 == 0x91);                 // ё
}

// UTF-8-safe lowercase: ASCII + Cyrillic А–Я → а–я
inline std::string toLower(const std::string& str) {
    std::string out;
    out.reserve(str.size());
    for (size_t i = 0; i < str.size(); ) {
        unsigned char c = str[i];
        if (c < 0x80) {
            out += (char)std::tolower(c);
            i++;
        } else if (c == 0xD0 && i + 1 < str.size()) {
            unsigned char c2 = str[i + 1];
            if (c2 >= 0x90 && c2 <= 0x9F) {        // А–П → а–п
                out += (char)0xD0;
                out += (char)(c2 + 0x20);
            } else if (c2 >= 0xA0 && c2 <= 0xAF) { // Р–Я → р–я
                out += (char)0xD1;
                out += (char)(c2 - 0x20);
            } else {
                out += (char)c;
                out += (char)c2;
            }
            i += 2;
        } else {
            out += (char)c;
            i++;
        }
    }
    return out;
}

// Lowercase, split on non-word chars, drop single-codepoint tokens
inline std::vector<std::string> tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string current;
    const std::string lower = toLower(text);

    auto flush = [&]() {
        if (current.empty()) return;
        // count codepoints: skip UTF-8 continuation bytes
        size_t n = 0;
        for (unsigned char c : current) if ((c & 0xC0) != 0x80) n++;
        if (n > 1) tokens.push_back(current);
        current.clear();
    };

    for (size_t i = 0; i < lower.size(); ) {
        unsigned char c = lower[i];
        if (c < 0x80) {
            if (std::isalnum(c)) current += (char)c;
            else flush();
            i++;
        } else if ((c == 0xD0 || c == 0xD1) && i + 1 < lower.size()) {
            unsigned char c2 = lower[i + 1];
            if (isCyrillicLower(c, c2)) { current += (char)c; current += (char)c2; }
            else flush();
            i += 2;
        } else {
            flush();
            i += (c >= 0xE0) ? 3 : 2;
        }
    }
    flush();
    return tokens;
}
