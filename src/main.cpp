#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include "mapping.hpp"
#include "telex.hpp"

int main(int argc, char* argv[]) {
    const std::filesystem::path mapping_path =
        argc > 1
            ? argv[1]
            : std::filesystem::path(argv[0]).parent_path() / "mapping.tsv";

    try {
        const auto mapping = qatt::Mapping::load_tsv(mapping_path.string());
        if (argc > 2) {
            const auto composed = qatt::compose_telex(argv[2]);
            std::cout << mapping.convert_word(composed) << '\n';
        } else {
            std::cout << "Loaded " << mapping.size()
                      << " mapping entries from " << mapping_path << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
