# SearchEngine

A simple search engine for a single website. Crawls pages, builds an inverted index, and serves search queries via a web UI.

## Components

| File | Role |
|---|---|
| `crawler.cpp` | Fetches pages via HTTP, extracts visible text, saves index to disk |
| `server.cpp` | Loads index on startup, serves a web UI and handles search over HTTP |
| `client.cpp` | CLI client — sends a query, prints matching URLs |
| `text.h` | UTF-8-aware tokenizer (Russian and English) |
| `html.h` | HTML cleaning — strips script/style blocks, tags, entities |
| `thread_pool.h` | Thread pool used by the server |

## Dependencies

- `g++` with C++17
- `libcurl`

```bash
sudo apt install libcurl4-openssl-dev
```

## Build

```bash
make        # builds crawler, server, client
make test   # runs tests
make clean  # removes binaries and index files
```

## Usage

**Step 1 — crawl and index the site:**
```bash
./crawler
# [1] Indexed: https://rau.am
# [2] Indexed: https://rau.am/about
# ...
# Done. Pages: 30  Unique words: 4821
```

Add `--debug` to inspect extracted text per page:
```bash
./crawler --debug
```

**Step 2 — start the server:**
```bash
./server
# Index loaded: 4821 terms, 30 pages
# Server listening on http://localhost:8080
```

**Step 3 — open in browser:**
```
http://localhost:8080
```

Or use the CLI client:
```bash
./client
Search: ректор
```

Run both steps at once:
```bash
make run
```

## How it works

1. **Crawler** — BFS over same-domain links. Strips `<script>`, `<style>`, `<head>` blocks, then all tags and HTML entities. Tokenizes visible text into an inverted index.
2. **Index** — two plain text files:
   - `index.txt` — `word docId1 docId2 ...`
   - `urls.txt` — `docId url`
3. **Server** — speaks HTTP, serves a web UI with search form, removable filter chips, and pagination (5 results per page).
4. **Search** — AND logic: all query words must appear on the page. Case-insensitive, exact match.
