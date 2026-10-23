#include "mapping.hpp"

#include <charconv>
#include <fstream>
#include <stdexcept>
#include <string>

namespace qatt {

Mapping Mapping::load_tsv(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open mapping file: " + file_path);
    }

    Mapping mapping;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(file, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line.front() == '#') {
            continue;
        }

        const auto first_tab = line.find('\t');
        const auto second_tab =
            first_tab == std::string::npos ? std::string::npos
                                           : line.find('\t', first_tab + 1);
        const auto third_tab =
            second_tab == std::string::npos ? std::string::npos
                                            : line.find('\t', second_tab + 1);
        if (first_tab == std::string::npos || second_tab == std::string::npos ||
            third_tab != std::string::npos || first_tab == 0 ||
            second_tab == first_tab + 1 || second_tab + 1 == line.size()) {
            throw std::runtime_error(
                "Invalid TSV row " + std::to_string(line_number));
        }

        const auto priority_text =
            std::string_view(line).substr(second_tab + 1);
        int priority = 0;
        const auto [end, error] = std::from_chars(
            priority_text.data(), priority_text.data() + priority_text.size(),
            priority);
        if (error != std::errc{} ||
            end != priority_text.data() + priority_text.size()) {
            throw std::runtime_error(
                "Invalid priority on TSV row " + std::to_string(line_number));
        }

        const auto glyph = line.substr(0, first_tab);
        const auto word =
            line.substr(first_tab + 1, second_tab - first_tab - 1);
        mapping.entries_[word] = MappingItem{glyph, priority};
    }

    return mapping;
}

std::size_t Mapping::size() const noexcept {
    return entries_.size();
}

bool Mapping::contains_word(std::string_view word) const {
    return entries_.contains(std::string(word));
}

bool Mapping::has_prefix(std::string_view prefix) const {
    for (const auto& [word, item] : entries_) {
        (void)item;
        if (word.starts_with(prefix)) {
            return true;
        }
    }
    return false;
}

std::string Mapping::convert_word(std::string_view word) const {
    const auto found = entries_.find(std::string(word));
    return found == entries_.end() ? std::string(word) : found->second.glyph;
}

}  // namespace qatt
