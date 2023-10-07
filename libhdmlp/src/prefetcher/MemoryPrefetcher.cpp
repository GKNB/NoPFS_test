#include <cstring>
#include <thread>
#include "../../include/prefetcher/MemoryPrefetcher.h"
#include "../../include/utils/MetadataStore.h"

MemoryPrefetcher::MemoryPrefetcher(std::map<std::string, std::string>& backend_options, std::vector<int>::iterator prefetch_start,
                                   std::vector<int>::iterator prefetch_end, unsigned long long int capacity, StorageBackend* backend,
                                   MetadataStore* metadata_store,
                                   int storage_level, bool alloc_buffer, Metrics* metrics, int eviction_policy, Sampler* sampler) {
    if (alloc_buffer) {
        buffer = new char[capacity];
        buffer_allocated = true;
    }
    this->prefetch_start = prefetch_start;
    this->prefetch_end = prefetch_end;
    this->backend = backend;
    this->metadata_store = metadata_store;
    this->storage_level = storage_level;
    this->capacity = capacity;
    this->metrics = metrics;
    num_elems = std::distance(prefetch_start, prefetch_end);
    file_ends.resize(num_elems, 0);
    this->eviction_policy = eviction_policy;
    this->sampler = new Sampler(*sampler);
    printf("eviction policy %d \n", eviction_policy);
    if (eviction_policy == 0)
    {
      int i = 0;
      // record locations of files
      // file_id_to_idx: file_id - no.
      // file_ends: no. -- location of the ending
      for (auto iter = prefetch_start; iter != prefetch_end; ++iter, ++i) {
        int file_id = *iter;
        file_id_to_idx[file_id] = i;
        if (i == 0) {
          file_ends[i] = backend->get_file_size(file_id);
        } else {
          file_ends[i] = file_ends[i-1] + backend->get_file_size(file_id);
        }
        file_cached[file_id] = 0;
      }
      this->metadata_store->store_planned_locations(prefetch_start, prefetch_end, storage_level);
    } else {
      this->buffer_offset = 0;
      for (int i = 0; i < backend->get_length(); i++) {
          // get sample size
          unsigned long size = backend->get_file_size(i);
          int label_size = backend->get_label_size(i) + 1;
          if (size > max_file_size) {
              max_file_size = size;
          }
      }
      std::cout << "memoryPrefetcher--max_file_size " << max_file_size << " capacity " << capacity << std::endl;
      for (int i = 0; i < backend->get_length(); i++) {
        if (buffer_offset + max_file_size >= capacity) break;
        if (i == 0) {
          file_ends[i] = max_file_size;
        } else {
          file_ends[i] = file_ends[i-1] + max_file_size;
        } 
        buffer_offset += max_file_size;
      }
      std::cout << "memoryPrefetcher--buffer_offset " << buffer_offset << std::endl;
      buffer_offset = 0;

      if (eviction_policy == 2) {
        sampler->get_fp(fpmap);
        for(auto it = fpmap.begin(); it != fpmap.end(); ++it) {
          std::cout << "Key: " << it->first << ", Value: " << it->second << '\n';
        }
      }
    }
}

MemoryPrefetcher::~MemoryPrefetcher() {
    if (buffer_allocated) {
        delete[] buffer;
    }
}

void MemoryPrefetcher::prefetch(int thread_id, int storage_class) {
  bool profiling = metrics != nullptr;
  std::chrono::time_point<std::chrono::high_resolution_clock> t1, t2;
  while (true) {
    std::unique_lock<std::mutex> lock(prefetcher_mutex);
    int idx = prefetch_offset;
    if (idx >= num_elems) {
      // Everything is prefetched.
      break;
    }
    ++prefetch_offset;  // Claim this file to prefetch.
    const int file_id = *(prefetch_start + idx);
    if (file_cached[file_id] >= 1) {
      // A different thread cached or is caching this file.
      lock.unlock();
      continue;
    }
    file_cached[file_id] = 2;  // Mark we're prefetching it.
    lock.unlock();

    // Fetch the file without the lock.
    if (profiling) {
      t1 = std::chrono::high_resolution_clock::now();
    }
    unsigned long long int start = (idx == 0) ? 0 : file_ends[idx-1];
    backend->fetch(file_id, buffer + start);
    if (profiling) {
      t2 = std::chrono::high_resolution_clock::now();
      metrics->read_locations[storage_level][thread_id].emplace_back(OPTION_PFS);
      metrics->read_times[storage_level][thread_id].emplace_back(std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count());
    }
    // Reclaim the lock to mark the file as fetched.
    lock.lock();
    file_cached[file_id] = 1;
    lock.unlock();
    metadata_store->insert_cached_file(storage_level, file_id);
    prefetch_cv.notify_all();
  }
}

void MemoryPrefetcher::fetch_and_cache(int file_id, char* dst) {
  std::unique_lock<std::mutex> lock(prefetcher_mutex);
  if (file_cached.count(file_id) == 0) {
    throw std::runtime_error("Trying fetch_and_cache with bad file_id");
  }
  if (file_cached[file_id] == 1) {
    // A different thread already fetched this file.
    lock.unlock();
    fetch(file_id, dst);
    return;
  } else if (file_cached[file_id] == 2) {
    // A different thread is currently caching this file, wait.
    prefetch_cv.wait(lock, [&]() { return file_cached[file_id] == 1; });
    lock.unlock();
    fetch(file_id, dst);
    return;
  }
  file_cached[file_id] = 2;  // Mark we're prefetching it.
  lock.unlock();

  // Fetch without the lock.
  int idx = file_id_to_idx[file_id];
  unsigned long long int start = (idx == 0) ? 0 : file_ends[idx-1];
  unsigned long long int end = file_ends[idx];
  backend->fetch(file_id, buffer + start);
  // Copy to the destination buffer.
  memcpy(dst, buffer + start, end - start);
  // Reclaim the lock to mark the file as cached.
  lock.lock();
  file_cached[file_id] = 1;
  lock.unlock();
  metadata_store->insert_cached_file(storage_level, file_id);
  prefetch_cv.notify_all();
}

void MemoryPrefetcher::fetch_and_rm_cache(int file_id, char* dst) {
  bool cache_file = false;
  int next_idx;
  std::unique_lock<std::mutex> lock(prefetcher_mutex);
  if (file_cached.count(file_id) == 0 || file_cached[file_id] == 0) {
    if (file_cached.size() * max_file_size + max_file_size >= capacity) {
      // cache is full, decide either to rm & cache or not cache
      cache_file = cache_file_or_not(file_id);
      // std::cout << "cache is full & cache_file is " << cache_file << std::endl;
      if (cache_file) {
        // rm one file and set next_idx to the file that just removed
        next_idx = evict_last_from_cache();
        // std::cout << "cache is full & file_id is " << file_id << "idx is " << next_idx << std::endl;
      }
    } else {
      // cache is not full, set next_idx to buffer_offset
      cache_file = true;
      next_idx = buffer_offset;
      buffer_offset += 1;
      file_cached[file_id] = 0;
      // std::cout << "cache is not full & file_id is " << file_id << "idx is " << next_idx << std::endl;
    }
  } else if (file_cached[file_id] == 1) {
    // A different thread already fetched this file.
    // std::cout << "file_id " << file_id << " is alreday fetched" << std::endl;
    lock.unlock();
    fetch(file_id, dst);
    return;
  } else if (file_cached[file_id] == 2) {
    // A different thread is currently caching this file, wait.
    // std::cout << "file_id " << file_id << " is being fetched" << std::endl;
    prefetch_cv.wait(lock, [&]() { return file_cached[file_id] == 1; });
    lock.unlock();
    fetch(file_id, dst);
    return;
  }
  if (cache_file) {
    // set current file status to prefetching
    file_cached[file_id] = 2;  // Mark we're prefetching it.
    lock.unlock();
  } else {
    lock.unlock();
  }

  // Fetch without the lock.
  if (cache_file) {
    // if cache this file
    // int idx = file_id_to_idx[file_id];
    std::cout << "file_id " << file_id << " next_idx" << next_idx << std::endl;
    unsigned long long int start = (next_idx == 0) ? 0 : file_ends[next_idx-1];
    // unsigned long long int end = file_ends[next_idx];
    unsigned long long int end = start + backend->get_file_size(file_id);
    std::cout << "file_id " << file_id << " start " << start << " end " << end << std::endl;
    backend->fetch(file_id, buffer + start);
    // Copy to the destination buffer.
    memcpy(dst, buffer + start, end - start);
  } else {
    // cache is full and this file will not be cached (just add to the sbf)
    backend->fetch(file_id, dst);
  }
  // Reclaim the lock to mark the file as cached.
  lock.lock();
  if (cache_file) {
    file_cached[file_id] = 1;
    file_id_to_idx[file_id] = next_idx;
    add_file_priority(file_id);
    // std::cout << "file_id " << file_id << " fetch done" << std::endl;
  }
  lock.unlock();
  if (cache_file) {
    metadata_store->insert_cached_file(storage_level, file_id);
    prefetch_cv.notify_all();
  }
}

void MemoryPrefetcher::fetch(int file_id, char* dst) {
    unsigned long len;
    if (eviction_policy > 0) {
      std::unique_lock<std::mutex> lock(prefetcher_mutex);
      if (file_id_to_idx.count(file_id) == 0) {
        std::cout << "file_id " << file_id << " is no longer in the cache." << std::endl;
        lock.unlock();
        // return false;
      } else {
        char* loc = get_location(file_id, &len);
        memcpy(dst, loc, len);
        update_file_priority(file_id);
        lock.unlock();
      }

    } else {
      char* loc = get_location(file_id, &len);

      memcpy(dst, loc, len);
    }
}

char* MemoryPrefetcher::get_location(int file_id, unsigned long* len) {
  // No lock: This is only called when the file is marked visible and cached.
  int idx = file_id_to_idx[file_id];
  unsigned long long int start = (idx == 0) ? 0 : file_ends[idx-1];
  unsigned long long int end = start + backend->get_file_size(file_id);
  *len = end - start;
  // std::cout << "file_id " << file_id << " start " << start << " end " << end << " len " << *len << std::endl;
  return buffer + start;
}

int MemoryPrefetcher::get_prefetch_offset() {
    // Unsynchronized access, as this value is only used for approximating if file should be fetched remotely and
    // stale values therefore aren't critical
    return prefetch_offset;
}

bool MemoryPrefetcher::is_done() {
    return prefetch_offset >= num_elems;
}

bool MemoryPrefetcher::cache_file_or_not(int file_id) {
  switch (eviction_policy) {
    case 1: {
      // FIFO
      return true;
      break;
    }
    case 2: {
      // access_frequency
      auto it = pfmap.rbegin();
      std::cout << "MP--cache or not? least priority in cache " << it->first << " compare to " << fpmap[file_id] << std::endl;
      if (it->first < fpmap[file_id]) {
        return true;
      } else {
        return false;
      }
      break;
    }
    default:
      throw std::runtime_error("Unsupported eviction_policy");
  }
}

int MemoryPrefetcher::evict_last_from_cache() {
  int file_to_evict, evict_file_idx;
  switch (eviction_policy) {
    case 1: {
      // FIFO
      file_to_evict = file_id_cache.front();
      file_id_cache.pop();
      break;
    }
    case 2: {
      auto it = pfmap.rbegin();
      file_to_evict = it->second;
      std::cout << "memoryPrefetcher--evict file " << file_to_evict << std::endl;
      pfmap.erase(it.base());
      break;
    }
    default:
      throw std::runtime_error("Unsupported eviction_policy");
  }
  evict_file_idx = file_id_to_idx[file_to_evict];
  file_id_to_idx.erase(file_to_evict);
  file_cached[file_to_evict] = 0;
  // file_cached.erase(file_to_evict);
  metadata_store->rm_cached_file(file_to_evict);
  return evict_file_idx;
}

void MemoryPrefetcher::add_file_priority(int file_id) {
  switch (eviction_policy) {
    case 1: {
      // FIFO
      file_id_cache.push(file_id);
      break;
    }
    case 2: {
      fpmap[file_id] -= 1;
      int priority = fpmap[file_id];
      std::cout << "memoryPrefetcher--add_file_priority get file_id " << file_id << " 's priority and reduce one " << priority<< std::endl;
      pfmap.insert({priority, file_id});
      break;
    }
    default:
      throw std::runtime_error("Unsupported eviction_policy");
  }
}

void MemoryPrefetcher::update_file_priority(int file_id) {
  // rm old file_id, priority pair
  auto it1 = fpmap.find(file_id);
  int priority = it1->second;
  auto range = pfmap.equal_range(priority);
  fpmap.erase(it1);
  auto it2 = range.first;
  while(it2 != range.second)
  {
    if(it2->second == file_id)
      break;
    it2++;
  }
  pfmap.erase(it2);
  std::cout << "MP--rm file " << file_id << " 's old priority " << priority << std::endl;
  priority -= 1;
  
  // add new file_id, priority pair
  pfmap.insert({priority, file_id});
  fpmap.insert({file_id, priority});
  std::cout << "MP--add file " << file_id << " 's new priority " << priority << std::endl;
}