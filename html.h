#pragma once

#include <string>
#include <regex>

// Case-insensitive search for needle in haystack starting at pos
inline size_t findCI(const std::string& hay, const std::string& needle, size_t pos = 0) {
    for (size_t i = pos; i + needle.size() <= hay.size(); i++) {
        bool match = true;
        for (size_t j = 0; j < needle.size(); j++)
            if (std::tolower((unsigned char)hay[i + j]) != std::tolower((unsigned char)needle[j])) { match = false; break; }
        if (match) return i;
    }
    return std::string::npos;
}

// Remove all <tagName ...>...</tagName> blocks (case-insensitive)
inline std::string stripBlocks(const std::string& html, const std::string& tagName) {
    std::string result;
    size_t pos = 0;
    while (pos < html.size()) {
        size_t start = findCI(html, "<" + tagName, pos);
        if (start == std::string::npos) { result += html.substr(pos); break; }

        result += html.substr(pos, start - pos);

        size_t end = findCI(html, "</" + tagName, start);
        if (end == std::string::npos) break;

        size_t gt = html.find('>', end);
        pos = (gt == std::string::npos) ? html.size() : gt + 1;
    }
    return result;
}

// Extract visible text from HTML
inline std::string extractText(const std::string& html) {
    std::string text = html;
    text = stripBlocks(text, "script");
    text = stripBlocks(text, "style");
    text = stripBlocks(text, "head");
    text = std::regex_replace(text, std::regex("<[^>]*>"), " ");
    text = std::regex_replace(text, std::regex("&amp;"),  "&");
    text = std::regex_replace(text, std::regex("&lt;"),   "<");
    text = std::regex_replace(text, std::regex("&gt;"),   ">");
    text = std::regex_replace(text, std::regex("&nbsp;"), " ");
    text = std::regex_replace(text, std::regex("&#?[a-zA-Z0-9]+;"), " ");
    return text;
}
