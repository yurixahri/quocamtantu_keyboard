#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>

namespace qatt {

struct MappingItem {
    std::string glyph;
    int priority = 0;
};

class Mapping {
public:
    static Mapping load_tsv(const std::string& file_path);

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool contains_word(std::string_view word) const;
    [[nodiscard]] bool has_prefix(std::string_view prefix) const;
    [[nodiscard]] std::string convert_word(std::string_view word) const;

private:
    std::unordered_map<std::string, MappingItem> entries_;
};

}  // namespace qatt
