#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace qatt {

struct Utf8Edit {
    std::size_t backspaces;
    std::string inserted_text;
};

[[nodiscard]] std::string compose_telex(std::string_view keystrokes);
[[nodiscard]] Utf8Edit make_utf8_edit(std::string_view current,
                                      std::string_view next);
[[nodiscard]] std::string remove_last_utf8_codepoint(std::string_view text);
[[nodiscard]] std::size_t utf8_codepoint_count(std::string_view text) noexcept;

}  // namespace qatt
