-- Features
Asynchronous-ready Crawler: Fetches web content efficiently using libcurl.

HTML Processing: Automated tag removal and content cleaning.

Elasticsearch Integration: Direct indexing of processed pages via REST API.

Robust Error Handling: Validates HTTP statuses (e.g., 404 Not Found) and follows redirects.

Dockerized Infrastructure: Easy environment setup using containerization.

-- Tech Stack
Language: C++17

Networking: libcurl

Search Engine: Elasticsearch 7.17

Environment: Docker / macOS (Apple Silicon compatible)

Build System: CMake

-- Installation & Setup
1. Prerequisites
Ensure you have Docker Desktop installed and running on your machine.

2. Start Elasticsearch
Run the following command to start the database in a Docker container:

Bash
docker run -d --name elasticsearch -p 9200:9200 -p 9300:9300 -e "discovery.type=single-node" docker.elastic.co/elasticsearch/elasticsearch:7.17.10
3. Build the Project
Clone the repository and build using CMake:

Bash
mkdir build && cd build
cmake ..
make
🖥 Usage
The engine takes a list of URLs and processes them into the pages index.

Run the application:

Bash
./elasticsearch_crawler_project
Verify the index:
Check if the data is saved correctly by querying the local Elasticsearch node:

Bash
curl -X GET "localhost:9200/pages/_cat/indices?v"

-- Roadmap
[ ] Multithreading: Implement a Thread Pool for concurrent crawling.

[ ] Recursive Crawling: Add BFS/DFS logic to discover new links automatically.

[ ] Search Interface: Build a CLI or Web UI to perform full-text searches.

[ ] Advanced Ranking: Implement basic relevance scoring for search results.
