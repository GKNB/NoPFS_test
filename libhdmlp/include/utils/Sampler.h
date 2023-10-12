#ifndef HDMLP_SAMPLER_H
#define HDMLP_SAMPLER_H


#include <vector>
#include <random>
#include "../storage/StorageBackend.h"

class Sampler {
public:
    int epochs;

    Sampler(StorageBackend* backend,
            int n,
            int batch_size,
            int epochs,
            int distr_scheme,
            bool drop_last_batch,
            int seed, 
            int node_id,
            int eviction_policy);

    void get_node_access_string(int node_id, std::vector<int>& access_string);

    void get_prefetch_string(int node_id, const std::vector<unsigned long long int>& capacities,
                             std::vector<int>& prefetch_string,
                             std::vector<std::vector<int>::iterator>& storage_class_ends, bool in_order);

    void advance_batch();

    int get_batch_size();

    int get_node_local_batch_size();

    void get_fp(std::unordered_map<int, int>& fpmap);

    void store_node_access_string(std::vector<int>& node_access_string, std::unordered_map<int, int>& fpmap);

    int node_id;

private:
    std::default_random_engine random_engine;
    std::vector<int> access_sequence;
    StorageBackend* backend;
    int n;
    int count;
    int batch_size;
    int distr_scheme;
    int node_local_batch_size;
    int batch_no;
    int partial_shuffle_size;
    int eviction_policy;


  void shuffle_sequence(std::vector<int>& vec);

  void get_access_frequency(std::vector<int>& access_freq, int node_id, int lookahead);

  void get_access_frequency_for_seq(std::vector<int>& seq, std::vector<int>& access_freq, int node_id);

  void get_node_access_string_for_seq(std::vector<int>& seq, int node_id, std::vector<int>& access_string);

  void get_first_accesses(std::vector<int>& first_accesses, int node_id, int lookahead, std::vector<int>& access_freq);

};


#endif //HDMLP_SAMPLER_H
