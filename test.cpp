#include <iostream>
#include <cassert>
#include <map>
#include <set>
#include "text.h"
#include "html.h"

int passed = 0;
int failed = 0;

#define CHECK(expr) do { \
    if (expr) { passed++; } \
    else { std::cerr << "FAIL: " << #expr << " (" << __FILE__ << ":" << __LINE__ << ")\n"; failed++; } \
} while(0)

// ---------- toLower ----------

void test_toLower() {
    CHECK(toLower("Hello") == "hello");
    CHECK(toLower("МЕРИ") == "мери");
    CHECK(toLower("РАУ")  == "рау");
    CHECK(toLower("Ректор") == "ректор");
    CHECK(toLower("abc123") == "abc123");
    CHECK(toLower("") == "");
}

// ---------- tokenize ----------

void test_tokenize() {
    // Basic ASCII
    auto t = tokenize("Hello world");
    CHECK(t.size() == 2);
    CHECK(t[0] == "hello");
    CHECK(t[1] == "world");

    // Russian
    t = tokenize("Привет мир");
    CHECK(t.size() == 2);
    CHECK(t[0] == "привет");
    CHECK(t[1] == "мир");

    // Mixed
    t = tokenize("RAU Ереван 2024");
    CHECK(t.size() == 3);
    CHECK(t[0] == "rau");
    CHECK(t[1] == "ереван");
    CHECK(t[2] == "2024");

    // Single-char tokens filtered out
    t = tokenize("а в и a");
    CHECK(t.empty());

    // Punctuation as separator
    t = tokenize("один,два.три");
    CHECK(t.size() == 3);

    // Empty input
    t = tokenize("");
    CHECK(t.empty());

    // Case folding
    t = tokenize("МЕРИ");
    CHECK(t.size() == 1);
    CHECK(t[0] == "мери");
}

// ---------- findCI ----------

void test_findCI() {
    CHECK(findCI("Hello World", "world") == 6);
    CHECK(findCI("Hello World", "HELLO") == 0);
    CHECK(findCI("abcdef", "xyz") == std::string::npos);
    CHECK(findCI("", "a") == std::string::npos);
    CHECK(findCI("<SCRIPT>", "<script") == 0);
}

// ---------- stripBlocks ----------

void test_stripBlocks() {
    std::string html = "<p>text</p><script>var x=1;</script><p>more</p>";
    std::string result = stripBlocks(html, "script");
    CHECK(result.find("var x") == std::string::npos);
    CHECK(result.find("text") != std::string::npos);
    CHECK(result.find("more") != std::string::npos);

    // Case-insensitive tag matching
    html = "<SCRIPT>secret</SCRIPT>visible";
    result = stripBlocks(html, "script");
    CHECK(result.find("secret") == std::string::npos);
    CHECK(result.find("visible") != std::string::npos);

    // Multiple blocks
    html = "<style>css</style>mid<style>more css</style>end";
    result = stripBlocks(html, "style");
    CHECK(result.find("css") == std::string::npos);
    CHECK(result.find("mid") != std::string::npos);
    CHECK(result.find("end") != std::string::npos);
}

// ---------- extractText ----------

void test_extractText() {
    std::string html =
        "<head><title>Title</title></head>"
        "<body>"
        "<script>var x = 'hidden';</script>"
        "<style>.cls { color: red; }</style>"
        "<p>Visible text</p>"
        "<p>Ректор университета</p>"
        "</body>";

    std::string text = extractText(html);
    CHECK(text.find("hidden") == std::string::npos);
    CHECK(text.find("color") == std::string::npos);
    CHECK(text.find("Title") == std::string::npos);
    CHECK(text.find("Visible text") != std::string::npos);
    CHECK(text.find("Ректор") != std::string::npos);

    // Entity decoding
    std::string entities = "<p>a &amp; b &lt;c&gt; &nbsp;d</p>";
    text = extractText(entities);
    CHECK(text.find('&') != std::string::npos);
    CHECK(text.find('<') != std::string::npos);
}

// ---------- search (AND logic) ----------

using Index = std::map<std::string, std::set<int>>;

std::set<int> search(const Index& idx, const std::string& query) {
    auto tokens = tokenize(query);
    if (tokens.empty()) return {};

    auto it = idx.find(tokens[0]);
    if (it == idx.end()) return {};
    std::set<int> result = it->second;

    for (size_t i = 1; i < tokens.size() && !result.empty(); i++) {
        it = idx.find(tokens[i]);
        if (it == idx.end()) return {};
        std::set<int> inter;
        for (int id : result)
            if (it->second.count(id)) inter.insert(id);
        result = inter;
    }
    return result;
}

void test_search() {
    Index idx;
    // doc 1: главная страница
    // doc 2: страница ректора
    // doc 3: страница факультета математики
    // doc 4: новость о новом корпусе
    idx["ректор"]      = {1, 2};
    idx["университет"] = {1, 2, 3, 4};
    idx["математика"]  = {3};
    idx["новый"]       = {4};
    idx["корпус"]      = {4};

    // "ректор" — есть на двух страницах
    CHECK(search(idx, "ректор") == std::set<int>({1, 2}));

    // "университет" — есть на всех страницах
    CHECK(search(idx, "университет") == std::set<int>({1, 2, 3, 4}));

    // "математика" — только на странице факультета
    CHECK(search(idx, "математика") == std::set<int>({3}));

    // "новый корпус" — AND: обе слова только на doc 4
    CHECK(search(idx, "новый корпус") == std::set<int>({4}));

    // "ректор университет" — AND: только страницы где есть оба
    CHECK(search(idx, "ректор университет") == std::set<int>({1, 2}));

    // "математика ректор" — AND: нет пересечения
    CHECK(search(idx, "математика ректор").empty());

    // регистр не важен
    CHECK(search(idx, "Ректор")      == std::set<int>({1, 2}));
    CHECK(search(idx, "УНИВЕРСИТЕТ") == std::set<int>({1, 2, 3, 4}));
    CHECK(search(idx, "Новый Корпус")== std::set<int>({4}));

    // слово которого нет
    CHECK(search(idx, "декан").empty());

    // пустой запрос
    CHECK(search(idx, "").empty());
}

// ---------- main ----------

int main() {
    test_toLower();
    test_tokenize();
    test_findCI();
    test_stripBlocks();
    test_extractText();
    test_search();

    std::cout << passed << " passed, " << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}
