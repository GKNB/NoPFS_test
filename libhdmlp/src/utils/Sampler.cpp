#include <iostream>
#include <algorithm>
#include "../../include/utils/Sampler.h"

Sampler::Sampler(StorageBackend* backend, // NOLINT(cert-msc32-c,cert-msc51-cpp)
                 int n,
                 int batch_size,
                 int epochs,
                 int distr_scheme,
                 bool drop_last_batch,
                 int seed,
                 int node_id,
                 int eviction_policy) {
    count = backend->get_length();
    access_sequence.resize(count);
    for (int i = 0; i < count; i++) {
        access_sequence[i] = i;
    }
    this->backend = backend;
    this->n = n;
    this->batch_size = batch_size;
    this->distr_scheme = distr_scheme;
    this->epochs = epochs;
    if (drop_last_batch) {
        batch_no = count / batch_size;
        node_local_batch_size = batch_size / n + (batch_size % n != 0);
        // std::cout << "rank " << node_id << "erase: " << (count - node_local_batch_size * batch_no * n) << std::endl;
        access_sequence.erase(access_sequence.end() - (count - node_local_batch_size * batch_no * n), access_sequence.end());
    } else {
        batch_no = count / batch_size + (count % batch_size != 0);
        node_local_batch_size = batch_size / n + (batch_size % n != 0);
    }

    random_engine.seed(seed);
    shuffle_sequence(access_sequence);
    this->eviction_policy = eviction_policy;
    this->node_id = node_id;
    switch (distr_scheme) {
        case 1: {
            break;
        }
        case 2: {
            // local shuffling
            int offset = (node_local_batch_size * batch_no) * node_id;
            std::vector<int> temp (access_sequence.begin() + offset, access_sequence.begin() + node_local_batch_size * batch_no + offset);
            access_sequence = temp;
            // std::cout << "rank " << node_id << "local shuffling access_sequence size: " << access_sequence.size() << std::endl;
            // std::cout << "rank " << node_id << "access_sequence: ";
            // for (auto elem : access_sequence)
            //     std::cout << elem << " ";
            // std::cout << std::endl;
            break;
        }
        case 3: {
            partial_shuffle_size = 4;
            int offset = (node_local_batch_size * batch_no * partial_shuffle_size) * (node_id / partial_shuffle_size);
            std::vector<int> temp (access_sequence.begin() + offset, access_sequence.begin() + node_local_batch_size * batch_no * partial_shuffle_size + offset);
            access_sequence = temp;
            // std::cout << "rank " << node_id << "access_sequence size: " << access_sequence.size() << std::endl;
            // std::cout << "rank " << node_id << "access_sequence: ";
            // for (auto elem : access_sequence)
            //     std::cout << elem << " ";
            // std::cout << std::endl;
            break;
        }
        default:
            throw std::runtime_error("Unsupported distr_scheme");
    }
}

/**
 * Shuffle the provided sequence (vector).
 *
 * @param vec Pointer to vector that is shuffled
 */
void Sampler::shuffle_sequence(std::vector<int>& vec) {
    std::shuffle(vec.begin(), vec.end(), random_engine);
}

/**
 * Gets the access string for a given node in the current epoch
 * @param node_id
 * @param access_string
 */
void Sampler::get_node_access_string(int node_id, std::vector<int>& access_string) {
    get_node_access_string_for_seq(access_sequence, node_id, access_string);
    // std::cout << "rank " << node_id << "access_sequence: "; 
    // for (auto elem : access_sequence)
    //     std::cout << elem << " ";
    // std::cout << std::endl;
}

void Sampler::get_node_access_string_for_seq(std::vector<int>& seq, int node_id, std::vector<int>& access_string) {
    switch (distr_scheme) {
        case 1: {
            // get the access string of this process based on seq 
            // (access sequence of all training samples (containing batches in order)). 
            int offset = node_local_batch_size * node_id;
            for (int j = 0; j < batch_no; j++) {
                for (int k = j * batch_size + offset;
                     k < std::min(j * batch_size + std::min(offset + node_local_batch_size, batch_size), count);
                     k++) {
                    int file_id = seq[k];
                    access_string.push_back(file_id);
                }
            }
            break;
        }
        case 2: {
            access_string = seq;
            // std::cout << "rank " << node_id << "access_string size: " << access_string.size() << std::endl;
            break;
        }
        case 3: {
            int offset = node_local_batch_size * (node_id % partial_shuffle_size);
            for (int j = 0; j < batch_no; j++) {
                for (int k = j * batch_size / partial_shuffle_size + offset;
                     k < std::min(j * batch_size / partial_shuffle_size + std::min(offset + node_local_batch_size, batch_size / partial_shuffle_size), count);
                     k++) {
                    int file_id = seq[k];
                    access_string.push_back(file_id);
                }
            }
            break;
        }
        default:
            throw std::runtime_error("Unsupported distr_scheme");
    }
}

void Sampler::get_access_frequency(std::vector<int>& access_freq, int node_id, int lookahead) {
    std::default_random_engine engine_copy = random_engine;
    std::vector<int> curr_access_seq = access_sequence;
    get_access_frequency_for_seq(curr_access_seq, access_freq, node_id);
    for (int i = 1; i < lookahead; i++) {
        shuffle_sequence(curr_access_seq);
        get_access_frequency_for_seq(curr_access_seq, access_freq, node_id);
    }
    random_engine = engine_copy;
}


void Sampler::get_prefetch_string(int node_id, const std::vector<unsigned long long int>& capacities,
                                  std::vector<int>& prefetch_string,
                                  std::vector<std::vector<int>::iterator>& storage_class_ends, bool in_order) {
    int num_storage_classes = capacities.size();
    if (num_storage_classes == 1) {
        // Only staging buffer
        return;
    }
    std::vector<int> access_freq;
    // collect sample frequency based on the access sequence of all training epochs
    get_access_frequency(access_freq, node_id, epochs);

    // std::cout << "rank " << node_id << "get access_frequency with size: " << access_freq.size() << std::endl;
    // std::cout << "rank " << node_id << "access_frequency ";
    // for (auto elem : access_freq)
    //     std::cout << elem << " ";
    // std::cout << std::endl;
    // Build list of file IDs this node accesses at least once, sorted
    // by access frequency.
    std::vector<int> file_ids_in_freq_order;
    for (size_t i = 0; i < access_freq.size(); ++i) {
      if (access_freq[i] > 0) {
        file_ids_in_freq_order.push_back(i);
      }
    }
    // std::cout << "rank " << node_id << "file_ids_in_freq_order size: " << file_ids_in_freq_order.size() << std::endl;
    std::sort(file_ids_in_freq_order.begin(), file_ids_in_freq_order.end(),
              [&access_freq](int& a, int& b) {
                return access_freq[a] > access_freq[b];
              });
    prefetch_string.reserve(file_ids_in_freq_order.size());
    unsigned long long curr_size = 0;
    int curr_storage_class = 1;
    for (const auto& file_id : file_ids_in_freq_order) {
      unsigned long size = backend->get_file_size(file_id);
      if (curr_size + size > capacities[curr_storage_class]) {
        storage_class_ends.push_back(prefetch_string.end());
        if (curr_storage_class < num_storage_classes - 1) {
          ++curr_storage_class;
          curr_size = 0;
        } else {
          break;
        }
      }
      prefetch_string.emplace_back(file_id);
      curr_size += size;
    }
    // std::cout << "rank " << node_id << "prefetch_string with size: " << prefetch_string.size() << std::endl;
    // std::cout << "rank " << node_id << "prefetch_string ";
    // for (auto elem : prefetch_string)
    //     std::cout << elem << " ";
    // std::cout << std::endl;
    if ((int) storage_class_ends.size() < num_storage_classes - 1) {
        storage_class_ends.push_back(prefetch_string.end());
    }
    if (in_order) {
        std::vector<int> first_accesses;
        get_first_accesses(first_accesses, node_id, epochs, access_freq);
        // std::cout << "rank " << node_id << "get first_accesses with size: " << first_accesses.size() << std::endl;
        auto storage_class_begin = prefetch_string.begin();
        for (auto& storage_class_end : storage_class_ends) {
            std::sort(storage_class_begin, storage_class_end, [&first_accesses](int& a, int& b) {
                          if (first_accesses[a] == 0) {
                              return false;
                          }
                          if (first_accesses[b] == 0) {
                              return true;
                          }
                          return first_accesses[a] < first_accesses[b];
                      }
            );
            storage_class_begin = storage_class_end;
        }
    }
}

void Sampler::get_access_frequency_for_seq(std::vector<int>& seq, std::vector<int>& access_freq, int node_id) {
    std::vector<int> access_string;
    get_node_access_string_for_seq(seq, node_id, access_string);
    // Fill with 0s if elements not present.
    switch (distr_scheme) {
        case 1: {
            access_freq.resize(access_sequence.size(), 0);
            break;
        }
        case 2: {
            access_freq.resize(access_sequence.size() * n, 0);
            break;
        }
        case 3: {
            access_freq.resize(access_sequence.size() * (n / partial_shuffle_size), 0);
            break;
        }
    }
    for (const auto& file_id : access_string) {
      access_freq[file_id]++;
    }
}

void Sampler::advance_batch() {
    shuffle_sequence(access_sequence);
}

void Sampler::get_first_accesses(std::vector<int>& first_accesses, int node_id, int lookahead, std::vector<int>& access_freq) {
    std::default_random_engine engine_copy = random_engine;
    std::vector<int> curr_access_seq = access_sequence;
    int offset = 0;
    // Fill with 0s if elements not present.
    // store the number of epoch that one specific sample will be accessed by current process
    first_accesses.resize(access_freq.size(), 0);
    for (int i = 0; i < lookahead; i++) {
        std::vector<int> access_string;
        get_node_access_string_for_seq(curr_access_seq, node_id, access_string);
        for (int file_id : access_string) {
          if (first_accesses[file_id] == 0) {
            first_accesses[file_id] = offset;
          }
          offset++;
        }
        if (i != lookahead - 1) {
            shuffle_sequence(curr_access_seq);
        }
    }
    random_engine = engine_copy;
}

int Sampler::get_batch_size() {
    return batch_size;
}

int Sampler::get_node_local_batch_size() {
    return node_local_batch_size;
}

void Sampler::get_fp(std::unordered_map<int, int>& fpmap) {
    std::vector<int> access_freq;
    // collect sample frequency based on the access sequence of all training epochs
    printf("in get_fq before get_access_freq: node_id %d epochs %d\n", node_id, epochs);
    get_access_frequency(access_freq, node_id, epochs);

    for (size_t i = 0; i < access_freq.size(); ++i) {
      if (access_freq[i] > 0) {
        fpmap[i] = access_freq[i];
        printf("Sampler -- add file_id %d with access freq %d\n", i, access_freq[i]);
      }
    }
}

void Sampler::store_node_access_string(std::vector<int>& node_access_string, std::unordered_map<int, int>& fpmap) {
    std::default_random_engine engine_copy = random_engine;
    std::vector<int> curr_access_seq = access_sequence;
    int lookahead = epochs;
    
    node_access_string.resize(access_sequence.size() / n * lookahead, 0);
    for (int i = 0; i < lookahead; i++) {
        std::vector<int> access_string;
        get_node_access_string_for_seq(curr_access_seq, node_id, access_string);
        for (size_t j = 0; j < access_string.size(); j++) {
          node_access_string[i * (access_sequence.size() / n) + j] = access_string[j];
          if (fpmap.count(access_string[j]) == 0) {
            fpmap[access_string[j]] = i * (access_sequence.size() / n) + j;
          }
        }

        if (i != lookahead - 1) {
            shuffle_sequence(curr_access_seq);
        }
    }
    random_engine = engine_copy;
}    
