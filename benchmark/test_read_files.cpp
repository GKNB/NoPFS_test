
#include <dirent.h>
#include <codecvt>
#include <locale>
#include <algorithm>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <fstream>
#include <sstream>
#include <string.h>
#include <iostream>

struct FileInformation {
        std::string label;
        std::string file_name;
        int file_size;
    };

std::string path = "/files2/scratch/kwf5687/imagenet/train/";
std::vector<std::string> label_mappings;
std::vector<int> size_mappings;
std::vector<std::string> id_mappings;

std::string abs_path(const std::string* rel_path);

std::string abs_path(const std::string* rel_path) {
    return path + *rel_path;
}

int main() {
    int node_id = 0;
    std::vector<FileInformation> file_information;
    struct dirent* entry = nullptr;
    DIR* dp = nullptr;
    bool checkpoint = false;
    std::string checkpoint_path = "";

    dp = opendir(path.c_str());
    std::cout << "filesystembackend -- init_mappings: print dp " << path << std::endl;
    if (dp == nullptr) {
        throw std::runtime_error("Invalid path specified");
    }
    while ((entry = readdir(dp))) {
        if (entry->d_name[0] != '.') {
            std::string dir_name = entry->d_name;
            std::cout << "dir_name " << dir_name << std::endl;
            struct dirent* subentry = nullptr;
            DIR* subp = nullptr;

            subp = opendir((path + dir_name).c_str());
            std::cout << "path + dir_name " << path + dir_name << std::endl;
            if (subp == nullptr) {
                // Not a directory
                continue;
            }

            while ((subentry = readdir(subp))) {
                std::string rel_path = entry->d_name;
                rel_path += '/';
                rel_path += subentry->d_name;
                std::cout << "rel_path" << rel_path  << std::endl;
                std::string file_name = abs_path(&rel_path);
                std::cout << "file_name" << file_name  << std::endl;
                int fd = open(file_name.c_str(), O_RDONLY);
                struct stat stbuf; // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
                fstat(fd, &stbuf);
                close(fd);
                if (!S_ISREG(stbuf.st_mode)) { // NOLINT(hicpp-signed-bitwise)
                    // Not a regular file
                    continue;
                }
                struct FileInformation fi{};
                fi.label = entry->d_name;
                fi.file_name = subentry->d_name;
                fi.file_size = stbuf.st_size;
                std::cout << "fi " << fi.label << " " << fi.file_name << " " << fi.file_size << " " << std::endl;
                file_information.push_back(fi);
            }

            closedir(subp);


        }
    }
    closedir(dp);
    // Ensure that all nodes have same file ids by sorting them
    std::sort(file_information.begin(), file_information.end(), [](FileInformation& a, FileInformation& b) {
                  return a.label + a.file_name > b.label + b.file_name;
              }
    );
    std::ofstream checkpoint_stream;
    if (checkpoint && node_id == 0) {
        checkpoint_stream.open(checkpoint_path + "/hdmlp_metadata_",  std::ofstream::out | std::ofstream::trunc);
    }
    for (const auto& fi : file_information) {
        id_mappings.push_back(fi.file_name);
        label_mappings.push_back(fi.label);
        size_mappings.push_back(fi.file_size);
        if (checkpoint && node_id == 0) {
            checkpoint_stream << fi.file_name << "," << fi.label << "," << fi.file_size << std::endl;
        }
    }
    std::cout << "id_mappings size: " << id_mappings.size() << std::endl;

    if (checkpoint && node_id == 0) {
        checkpoint_stream.close();
        // Rename after file was completely written to ensure no nodes see partially written files:
        std::rename((checkpoint_path + "/hdmlp_metadata_").c_str(), (checkpoint_path + "/hdmlp_metadata").c_str());
    }
    return 0;
}