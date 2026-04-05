#include <iostream>
#include <string>
#include <vector>
#include <regex>
#include <map>
#include <set>
#include <queue>
#include <fstream>
#include <curl/curl.h>
#include "text.h"
#include "html.h"

const std::string INDEX_FILE = "index.txt";
const std::string URLS_FILE  = "urls.txt";
const int         MAX_PAGES  = 30;

// ---------- network ----------

size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// ---------- crawling ----------

std::vector<std::string> extractLinks(const std::string& html,
                                      const std::string& baseUrl) {
    std::vector<std::string> links;
    std::regex hrefRe(R"(href\s*=\s*["']([^"'#][^"']*?)["'])",
                      std::regex::icase);
    auto it  = std::sregex_iterator(html.begin(), html.end(), hrefRe);
    auto end = std::sregex_iterator();

    for (; it != end; ++it) {
        std::string url = (*it)[1].str();
        if (url.rfind("http", 0) == 0) {
            if (url.rfind(baseUrl, 0) == 0)
                links.push_back(url);
        } else if (!url.empty() && url[0] == '/') {
            links.push_back(baseUrl + url);
        }
    }
    return links;
}

// ---------- persistence ----------

void saveIndex(const std::map<std::string, std::set<int>>& index,
               const std::map<int, std::string>& docUrls) {
    std::ofstream idx(INDEX_FILE);
    for (const auto& [word, docs] : index) {
        idx << word;
        for (int id : docs) idx << " " << id;
        idx << "\n";
    }

    std::ofstream urls(URLS_FILE);
    for (const auto& [id, url] : docUrls)
        urls << id << " " << url << "\n";

    std::cout << "Index saved to " << INDEX_FILE << " and " << URLS_FILE << "\n";
}

// ---------- main ----------

int main(int argc, char* argv[]) {
    const std::string startUrl = "https://rau.am";
    bool debug = (argc > 1 && std::string(argv[1]) == "--debug");

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        std::cerr << "curl_global_init failed\n";
        return 1;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "curl_easy_init failed\n";
        curl_global_cleanup();
        return 1;
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    std::map<std::string, std::set<int>> invertedIndex;
    std::map<int, std::string>           docUrls;
    std::set<std::string>                visited;
    std::queue<std::string>              toVisit;
    int docId = 1;

    toVisit.push(startUrl);

    while (!toVisit.empty() && docId <= MAX_PAGES) {
        std::string url = toVisit.front();
        toVisit.pop();

        if (visited.count(url)) continue;
        visited.insert(url);

        std::string html;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &html);

        if (curl_easy_perform(curl) != CURLE_OK) {
            std::cerr << "Failed to fetch: " << url << "\n";
            continue;
        }

        std::string text = extractText(html);
        for (const auto& token : tokenize(text))
            invertedIndex[token].insert(docId);
        docUrls[docId] = url;

        std::cout << "[" << docId << "] Indexed: " << url << "\n";
        if (debug) {
            std::cout << "  --- visible text (first 300 chars) ---\n";
            std::cout << "  " << text.substr(0, 300) << "\n";
            std::cout << "  ---\n";
        }

        docId++;

        for (const auto& link : extractLinks(html, startUrl))
            if (!visited.count(link))
                toVisit.push(link);
    }

    curl_easy_cleanup(curl);
    curl_global_cleanup();

    saveIndex(invertedIndex, docUrls);

    std::cout << "Done. Pages: " << (docId - 1)
              << "  Unique words: " << invertedIndex.size() << "\n";
    return 0;
}
