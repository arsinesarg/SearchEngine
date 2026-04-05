#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <chrono>
#include "thread_pool.h"
#include "text.h"

const std::string INDEX_FILE = "index.txt";
const std::string URLS_FILE  = "urls.txt";

// ---------- index ----------

struct SearchIndex {
    std::map<std::string, std::set<int>> terms;  // word -> doc IDs
    std::map<int, std::string>           urls;   // doc ID -> URL
};

bool loadIndex(SearchIndex& idx) {
    std::ifstream idxFile(INDEX_FILE);
    if (!idxFile) {
        std::cerr << "Cannot open " << INDEX_FILE << " — run crawler first\n";
        return false;
    }
    std::string line;
    while (std::getline(idxFile, line)) {
        std::istringstream ss(line);
        std::string word;
        ss >> word;
        int id;
        while (ss >> id)
            idx.terms[word].insert(id);
    }

    std::ifstream urlFile(URLS_FILE);
    if (!urlFile) {
        std::cerr << "Cannot open " << URLS_FILE << " — run crawler first\n";
        return false;
    }
    while (std::getline(urlFile, line)) {
        std::istringstream ss(line);
        int id;
        std::string url;
        if (ss >> id >> url)
            idx.urls[id] = url;
    }

    std::cout << "Index loaded: " << idx.terms.size() << " terms, "
              << idx.urls.size() << " pages\n";
    return true;
}

// ---------- search ----------

std::set<int> search(const SearchIndex& idx, const std::string& query,
                     std::vector<std::string>& outTokens) {
    outTokens = tokenize(query);
    if (outTokens.empty()) return {};

    auto it = idx.terms.find(outTokens[0]);
    if (it == idx.terms.end()) return {};
    std::set<int> result = it->second;

    for (size_t i = 1; i < outTokens.size() && !result.empty(); i++) {
        it = idx.terms.find(outTokens[i]);
        if (it == idx.terms.end()) return {};
        std::set<int> inter;
        for (int id : result)
            if (it->second.count(id)) inter.insert(id);
        result = inter;
    }
    return result;
}

// ---------- HTTP helpers ----------

std::string urlDecode(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size()) {
            int val = 0;
            std::sscanf(s.substr(i + 1, 2).c_str(), "%x", &val);
            out += (char)val;
            i += 2;
        } else if (s[i] == '+') {
            out += ' ';
        } else {
            out += s[i];
        }
    }
    return out;
}

// Extract value of ?key=... from query string
std::string getParam(const std::string& qs, const std::string& key) {
    std::string prefix = key + "=";
    size_t pos = qs.find(prefix);
    if (pos == std::string::npos) return "";
    pos += prefix.size();
    size_t end = qs.find('&', pos);
    return urlDecode(qs.substr(pos, end == std::string::npos ? end : end - pos));
}

std::string httpResponse(const std::string& status, const std::string& type,
                         const std::string& body) {
    return "HTTP/1.1 " + status + "\r\n"
           "Content-Type: " + type + "; charset=utf-8\r\n"
           "Content-Length: " + std::to_string(body.size()) + "\r\n"
           "Connection: close\r\n\r\n" + body;
}

// ---------- UI ----------

const int PER_PAGE = 5;

const std::string PAGE = R"html(<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>RAU Search</title>
<style>
  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

  body {
    font-family: 'Segoe UI', system-ui, sans-serif;
    background: linear-gradient(160deg, #ffd6e7 0%, #ffb3d1 50%, #f99bbf 100%);
    min-height: 100vh;
    padding: 48px 20px 60px;
  }

  /* ── search area ── */
  .search-wrap {
    max-width: 680px;
    margin: 0 auto 28px;
  }

  .search-row {
    display: flex;
    align-items: center;
    gap: 12px;
  }

  form {
    flex: 1;
    display: flex;
    align-items: center;
    background: rgba(0,0,0,.08);
    border: 1.5px solid rgba(0,0,0,.2);
    border-radius: 14px;
    padding: 0 16px;
    backdrop-filter: blur(6px);
    transition: border-color .2s;
  }
  form:focus-within { border-color: #fff; background: rgba(0,0,0,.12); }

  form svg { flex-shrink: 0; opacity: .75; }

  input[type=text] {
    flex: 1;
    padding: 13px 12px;
    font-size: 16px;
    background: transparent;
    border: none;
    outline: none;
    color: #1a1a1a;
  }
  input[type=text]::placeholder { color: rgba(0,0,0,.38); }

  .btn-search {
    padding: 0;
    background: none;
    border: none;
    cursor: pointer;
    color: rgba(0,0,0,.55);
    display: flex;
    align-items: center;
  }
  .btn-search:hover { color: #1a1a1a; }

  .btn-filter {
    width: 50px; height: 50px;
    flex-shrink: 0;
    background: rgba(0,0,0,.08);
    border: 1.5px solid rgba(0,0,0,.2);
    border-radius: 14px;
    display: flex; align-items: center; justify-content: center;
    cursor: pointer;
    color: #1a1a1a;
    backdrop-filter: blur(6px);
    transition: background .2s;
  }
  .btn-filter:hover { background: rgba(0,0,0,.12); }

  /* ── chips ── */
  .chips {
    display: flex;
    flex-wrap: wrap;
    gap: 8px;
    margin-top: 14px;
  }

  .chip {
    display: inline-flex;
    align-items: center;
    gap: 6px;
    background: rgba(0,0,0,.08);
    border: 1.5px solid rgba(0,0,0,.2);
    border-radius: 20px;
    padding: 5px 12px;
    color: #1a1a1a;
    font-size: 13px;
    text-decoration: none;
    transition: background .15s;
  }
  .chip:hover { background: rgba(0,0,0,.15); }
  .chip .x { font-size: 15px; opacity: .7; line-height: 1; }

  /* ── results ── */
  .container {
    max-width: 680px;
    margin: 0 auto;
  }

  .stats {
    font-size: 13px;
    color: rgba(0,0,0,.45);
    margin-bottom: 16px;
  }

  .result {
    background: rgba(0,0,0,.07);
    border: 1.5px solid rgba(0,0,0,.12);
    border-radius: 14px;
    padding: 15px 20px;
    margin-bottom: 10px;
    backdrop-filter: blur(4px);
    transition: background .2s, transform .15s;
  }
  .result:hover {
    background: rgba(0,0,0,.12);
    transform: translateY(-1px);
  }

  .result a {
    font-size: 15px;
    color: #1a1a1a;
    text-decoration: none;
    word-break: break-all;
    font-weight: 500;
  }
  .result a:hover { text-decoration: underline; }

  .result .url {
    font-size: 12px;
    color: rgba(0,0,0,.4);
    margin-top: 4px;
    word-break: break-all;
  }

  .empty { color: rgba(0,0,0,.55); font-size: 15px; margin-top: 24px; }
  .hint  { color: rgba(0,0,0,.45);  font-size: 14px; margin-top: 12px; }

  /* ── pagination ── */
  .pagination {
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 6px;
    margin-top: 32px;
  }

  .page-btn {
    width: 36px; height: 36px;
    display: flex; align-items: center; justify-content: center;
    border-radius: 50%;
    font-size: 14px;
    text-decoration: none;
    color: #1a1a1a;
    background: rgba(0,0,0,.08);
    border: 1.5px solid rgba(0,0,0,.15);
    transition: background .15s;
  }
  .page-btn:hover   { background: rgba(0,0,0,.15); }
  .page-btn.active  { background: #fff; color: #c2607a; font-weight: 700; border-color: #fff; }
  .page-btn.arrow   { font-size: 16px; }
</style>
</head>
<body>
<div class="search-wrap">
  <div class="search-row">
    <form action="/search" method="get">
      <input type="text" name="q" value="{QUERY}" placeholder="Поиск по сайту rau.am..." autofocus>
      <input type="hidden" name="p" value="1">
      <button class="btn-search" type="submit">
        <svg width="20" height="20" fill="none" stroke="currentColor" stroke-width="2.2"
             viewBox="0 0 24 24">
          <polyline points="9 18 15 12 9 6"/>
        </svg>
      </button>
    </form>
    <div class="btn-filter">
      <svg width="20" height="20" fill="none" stroke="currentColor" stroke-width="2"
           viewBox="0 0 24 24">
        <line x1="4" y1="6"  x2="20" y2="6"/><line x1="4" y1="12" x2="20" y2="12"/>
        <line x1="4" y1="18" x2="20" y2="18"/>
        <circle cx="8"  cy="6"  r="2" fill="white"/>
        <circle cx="16" cy="12" r="2" fill="white"/>
        <circle cx="10" cy="18" r="2" fill="white"/>
      </svg>
    </div>
  </div>
{CHIPS}
</div>
<div class="container">
{BODY}
</div>
</body>
</html>)html";

std::string buildPage(const std::string& query, const std::string& chips,
                      const std::string& body) {
    std::string page = PAGE;
    auto rep = [&](const std::string& k, const std::string& v) {
        size_t p = page.find(k);
        if (p != std::string::npos) page.replace(p, k.size(), v);
    };
    rep("{QUERY}", query);
    rep("{CHIPS}", chips);
    rep("{BODY}",  body);
    return page;
}

// Build removable chip for each token: clicking removes that word from query
std::string buildChips(const std::string& query,
                        const std::vector<std::string>& tokens) {
    if (tokens.empty()) return "";
    std::string html = "<div class='chips'>";
    for (const auto& tok : tokens) {
        // Build query without this token
        std::string reduced;
        std::istringstream ss(query);
        std::string w;
        while (ss >> w) {
            if (toLower(w) != tok) {
                if (!reduced.empty()) reduced += '+';
                reduced += w;
            }
        }
        std::string href = reduced.empty() ? "/" : "/search?q=" + reduced + "&p=1";
        html += "<a class='chip' href='" + href + "'>" + tok
             + " <span class='x'>&#x2715;</span></a>";
    }
    html += "</div>";
    return html;
}

std::string buildResults(const std::set<int>& hits, const SearchIndex& idx,
                         const std::string& query, int ms, int page) {
    if (query.empty())
        return "<p class='hint'>Введите запрос для поиска по сайту rau.am</p>";
    if (hits.empty())
        return "<p class='empty'>По запросу \"" + query + "\" ничего не найдено.</p>";

    std::vector<int> ids(hits.begin(), hits.end());
    int total = (int)ids.size();
    int pages = (total + PER_PAGE - 1) / PER_PAGE;
    page = std::max(1, std::min(page, pages));
    int from = (page - 1) * PER_PAGE;
    int to   = std::min(from + PER_PAGE, total);

    std::string stats = "<p class='stats'>Найдено: " + std::to_string(total)
                      + " стр. (" + std::to_string(ms) + " мс)</p>";
    std::string items;
    for (int i = from; i < to; i++) {
        auto it = idx.urls.find(ids[i]);
        if (it == idx.urls.end()) continue;
        const std::string& url = it->second;
        items += "<div class='result'>"
                 "<a href='" + url + "' target='_blank'>" + url + "</a>"
                 "<div class='url'>" + url + "</div>"
                 "</div>";
    }

    // Pagination
    std::string base = "/search?q=";
    for (char c : query) base += (c == ' ') ? '+' : c;
    base += "&p=";

    std::string pager = "<div class='pagination'>";
    if (page > 1)
        pager += "<a class='page-btn arrow' href='" + base + std::to_string(page-1) + "'>&#8249;</a>";
    for (int i = 1; i <= pages; i++) {
        std::string cls = (i == page) ? "page-btn active" : "page-btn";
        pager += "<a class='" + cls + "' href='" + base + std::to_string(i)
               + "'>" + std::to_string(i) + "</a>";
    }
    if (page < pages)
        pager += "<a class='page-btn arrow' href='" + base + std::to_string(page+1) + "'>&#8250;</a>";
    pager += "</div>";

    return stats + items + (pages > 1 ? pager : "");
}

// ---------- request handling ----------

void process_request(int client_fd, const SearchIndex& idx) {
    char buf[4096] = {0};
    if (read(client_fd, buf, sizeof(buf) - 1) <= 0) { close(client_fd); return; }

    std::string req(buf);
    // Parse first line: "GET /path?qs HTTP/1.1"
    std::string path, qs;
    {
        size_t s = req.find(' '), e = req.find(' ', s + 1);
        if (s == std::string::npos || e == std::string::npos) { close(client_fd); return; }
        std::string full = req.substr(s + 1, e - s - 1);
        size_t q = full.find('?');
        path = full.substr(0, q);
        qs   = (q != std::string::npos) ? full.substr(q + 1) : "";
    }

    std::string response;
    if (path == "/" || path == "/search") {
        std::string query = getParam(qs, "q");
        int page = 1;
        std::string ps = getParam(qs, "p");
        if (!ps.empty()) page = std::stoi(ps);

        auto t0 = std::chrono::steady_clock::now();
        std::vector<std::string> tokens;
        std::set<int> hits = search(idx, query, tokens);
        int ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now() - t0).count();

        std::cout << "Query: \"" << query << "\"  hits: " << hits.size() << "\n";

        std::string chips = buildChips(query, tokens);
        std::string body  = buildResults(hits, idx, query, ms, page);
        response = httpResponse("200 OK", "text/html", buildPage(query, chips, body));
    } else {
        response = httpResponse("404 Not Found", "text/plain", "Not found");
    }

    send(client_fd, response.c_str(), response.size(), 0);
    close(client_fd);
}

// ---------- main ----------

int main() {
    SearchIndex idx;
    if (!loadIndex(idx)) return 1;

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) { std::cerr << "socket() failed\n"; return 1; }

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "bind() failed\n"; return 1;
    }
    if (listen(server_sock, 5) < 0) {
        std::cerr << "listen() failed\n"; return 1;
    }

    std::cout << "Server listening on http://localhost:8080\n";

    SimplePool pool(4);
    while (true) {
        int client_fd = accept(server_sock, nullptr, nullptr);
        if (client_fd < 0) { std::cerr << "accept() failed\n"; continue; }
        pool.addJob([client_fd, &idx]() { process_request(client_fd, idx); });
    }
    return 0;
}
