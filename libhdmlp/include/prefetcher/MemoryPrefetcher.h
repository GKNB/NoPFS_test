#ifndef HDMLP_MEMORYPREFETCHER_H
#define HDMLP_MEMORYPREFETCHER_H


#include <map>
#include <string>
#include <vector>
#include <condition_variable>
#include "PrefetcherBackend.h"
#include "../storage/StorageBackend.h"
#include "../utils/MetadataStore.h"
#include "../utils/Metrics.h"
#include "../utils/Sampler.h"

#include <queue>
#include <iostream>

class MemoryPrefetcher : public PrefetcherBackend {
public:
    MemoryPrefetcher(std::map<std::string, std::string>& backend_options, std::vector<int>::iterator prefetch_start,
                     std::vector<int>::iterator prefetch_end, unsigned long long int capacity, StorageBackend* backend, MetadataStore* metadata_store,
                     int storage_level, bool alloc_buffer, Metrics* metrics, int eviction_policy, Sampler* sampler);

    ~MemoryPrefetcher() override;

    void prefetch(int thread_id, int storage_class) override;

    void fetch(int file_id, char* dst) override;

    void fetch_and_cache(int file_id, char* dst) override;

    char* get_location(int file_id, unsigned long* len) override;

    int get_prefetch_offset() override;

    bool is_done() override;

    void fetch_and_rm_cache(int file_id, char* dst) override;

    bool cache_file_or_not(int file_id);
    
    int evict_last_from_cache();

    void add_file_priority(int file_id);

    void update_file_priority(int file_id, bool update_pf);

    int get_new_access_distance(int file_id, int old_priority);

protected:
    char* buffer;
    std::vector<unsigned long long int> file_ends;
    std::unordered_map<int, int> file_id_to_idx;
    // 0 = not cached; 1 == cached; 2 == caching
    std::unordered_map<int, char> file_cached;
    StorageBackend* backend;
    MetadataStore* metadata_store;
    Metrics* metrics;
    std::vector<int>::iterator prefetch_start;
    std::vector<int>::iterator prefetch_end;
    std::mutex prefetcher_mutex;
    std::condition_variable prefetch_cv;
    int num_elems;
    int prefetch_offset = 0;
    int storage_level;
    unsigned long long capacity;
    bool buffer_allocated = false;

    std::multimap<int, int, std::greater<int>> pfmap;
    std::unordered_map<int, int> fpmap;
    std::queue<int> file_id_cache;
    int eviction_policy;
    int buffer_offset;
    unsigned long max_file_size = 0;
    Sampler* sampler;
    std::vector<int> node_access_string;
};


#endif //HDMLP_MEMORYPREFETCHER_H
