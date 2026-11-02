#include "telex.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace qatt {
namespace {

enum class Tone : std::size_t {
    grave,
    acute,
    hook,
    tilde,
    dot,
    none
};

constexpr std::array<std::pair<char, Tone>, 5> kToneKeys{{
    {'f', Tone::grave},
    {'s', Tone::acute},
    {'r', Tone::hook},
    {'x', Tone::tilde},
    {'j', Tone::dot},
}};

struct VowelForms {
    std::string_view base;
    std::array<std::string_view, 5> toned;
};

constexpr std::array<VowelForms, 12> kVowels{{
    {"a", {"à", "á", "ả", "ã", "ạ"}},
    {"ă", {"ằ", "ắ", "ẳ", "ẵ", "ặ"}},
    {"â", {"ầ", "ấ", "ẩ", "ẫ", "ậ"}},
    {"e", {"è", "é", "ẻ", "ẽ", "ẹ"}},
    {"ê", {"ề", "ế", "ể", "ễ", "ệ"}},
    {"i", {"ì", "í", "ỉ", "ĩ", "ị"}},
    {"o", {"ò", "ó", "ỏ", "õ", "ọ"}},
    {"ô", {"ồ", "ố", "ổ", "ỗ", "ộ"}},
    {"ơ", {"ờ", "ớ", "ở", "ỡ", "ợ"}},
    {"u", {"ù", "ú", "ủ", "ũ", "ụ"}},
    {"ư", {"ừ", "ứ", "ử", "ữ", "ự"}},
    {"y", {"ỳ", "ý", "ỷ", "ỹ", "ỵ"}},
}};

bool is_vowel(std::string_view character) {
    for (const auto& vowel : kVowels) {
        if (character == vowel.base) {
            return true;
        }
    }
    return false;
}

bool has_vowel_within_tone_range(const std::vector<std::string>& chars) {
    std::size_t consonants_after_vowel = 0;
    for (auto it = chars.rbegin(); it != chars.rend(); ++it) {
        if (is_vowel(*it)) {
            return consonants_after_vowel <= 2;
        }
        if (it->size() != 1 ||
            std::isalpha(static_cast<unsigned char>((*it)[0])) == 0) {
            return false;
        }
        ++consonants_after_vowel;
        if (consonants_after_vowel > 2) {
            return false;
        }
    }
    return false;
}

Tone tone_for_key(char key) {
    for (const auto& [tone_key, tone] : kToneKeys) {
        if (key == tone_key) {
            return tone;
        }
    }
    return Tone::none;
}

std::string tone_character(std::string_view character, Tone tone) {
    if (tone == Tone::none) {
        return std::string(character);
    }
    for (const auto& vowel : kVowels) {
        if (character == vowel.base) {
            return std::string(vowel.toned[static_cast<std::size_t>(tone)]);
        }
    }
    return std::string(character);
}

std::size_t find_tone_index(const std::vector<std::string>& chars) {
    std::size_t start = 0;
    if (chars.size() >= 3 && chars[0] == "q" && chars[1] == "u") {
        start = 2;
    } else if (chars.size() >= 3 && chars[0] == "g" && chars[1] == "i" &&
               is_vowel(chars[2])) {
        start = 2;
    }

    std::vector<std::size_t> vowels;
    for (std::size_t i = start; i < chars.size(); ++i) {
        if (is_vowel(chars[i])) {
            vowels.push_back(i);
        }
    }
    if (vowels.empty()) {
        for (std::size_t i = 0; i < chars.size(); ++i) {
            if (is_vowel(chars[i])) {
                vowels.push_back(i);
            }
        }
        if (vowels.empty()) {
            return chars.size();
        }
    }

    for (const auto index : vowels) {
        if (chars[index] == "ê" || chars[index] == "ơ") {
            return index;
        }
    }

    for (const auto index : vowels) {
        if (chars[index] == "ô") {
            return index;
        }
    }

    for (const auto index : vowels) {
        if (chars[index] == "ă" || chars[index] == "â" || chars[index] == "ư") {
            return index;
        }
    }

    if (vowels.size() == 1) {
        return vowels.front();
    }

    const auto last_vowel = vowels.back();
    const bool has_coda = last_vowel + 1 < chars.size();
    if (has_coda) {
        if (vowels.size() >= 2) {
            return vowels[1];
        }
        return vowels.front();
    }

    if (vowels.size() == 2) {
        return vowels.front();
    }

    if (vowels.size() >= 3) {
        return vowels[1];
    }

    return vowels.front();
}

bool has_valid_final_consonant(const std::vector<std::string>& chars) {
    std::size_t last_vowel = chars.size();
    for (std::size_t i = 0; i < chars.size(); ++i) {
        if (is_vowel(chars[i])) {
            last_vowel = i;
        }
    }
    if (last_vowel == chars.size()) {
        return false;
    }

    std::string coda;
    for (std::size_t i = last_vowel + 1; i < chars.size(); ++i) {
        coda += chars[i];
    }
    constexpr std::array<std::string_view, 9> valid_codas{
        "", "n", "m", "ng", "p", "c", "t", "ch", "nh"};
    return std::find(valid_codas.begin(), valid_codas.end(), coda) !=
           valid_codas.end();
}

std::string join(const std::vector<std::string>& chars) {
    std::string result;
    for (const auto& character : chars) {
        result += character;
    }
    return result;
}

}  // namespace

std::string compose_telex(std::string_view keystrokes) {
    std::vector<std::string> chars;
    Tone tone = Tone::none;
    bool cancelled_tone = false;
    bool vietnamese_off = false;

    struct RepeatAction {
        char key;
        std::vector<std::pair<std::size_t, std::string>> previous_chars;
    };
    std::optional<RepeatAction> last_action;

    for (const char input : keystrokes) {
        const auto key = static_cast<char>(
            std::tolower(static_cast<unsigned char>(input)));

        if (vietnamese_off) {
            chars.emplace_back(1, key);
            continue;
        }

        if (last_action && last_action->key == key) {
            for (const auto& [index, previous] : last_action->previous_chars) {
                chars[index] = previous;
            }
            chars.emplace_back(1, key);
            vietnamese_off = true;
            last_action.reset();
            continue;
        }
        last_action.reset();

        if (key == 'd') {
            const auto first = chars.size() > 6 ? chars.size() - 6 : 0;
            for (std::size_t i = chars.size(); i > first; --i) {
                const auto index = i - 1;
                if (chars[index] == "d") {
                    chars[index] = "đ";
                    last_action = RepeatAction{key, {{index, "d"}}};
                    break;
                }
                if (chars[index] == "đ") {
                    chars[index] = "d";
                    chars.emplace_back(1, key);
                    vietnamese_off = true;
                    break;
                }
            }
            if (vietnamese_off || last_action) {
                continue;
            }
        }

        if (key == 'a' || key == 'e' || key == 'o') {
            const auto base = std::string(1, key);
            const auto marked = key == 'a' ? "â" : key == 'e' ? "ê" : "ô";
            const auto first = chars.size() > 6 ? chars.size() - 6 : 0;
            for (std::size_t i = chars.size(); i > first; --i) {
                const auto index = i - 1;
                if (chars[index] == marked) {
                    chars[index] = base;
                    chars.emplace_back(1, key);
                    vietnamese_off = true;
                    break;
                }
                if (chars[index] == base) {
                    const bool is_oeo =
                        key == 'o' && index + 1 < chars.size() &&
                        chars[index + 1] == "e";
                    if (!is_oeo) {
                        chars[index] = marked;
                        last_action = RepeatAction{key, {{index, base}}};
                    }
                    break;
                }
            }
            if (vietnamese_off || last_action) {
                continue;
            }
        }

        if (key == '[') {
            chars.emplace_back("ơ");
            continue;
        }
        if (key == ']') {
            chars.emplace_back("ư");
            continue;
        }

        if (key == 'w') {
            std::vector<std::size_t> vowel_indices;
            for (std::size_t i = 0; i < chars.size(); ++i) {
                if (is_vowel(chars[i])) {
                    vowel_indices.push_back(i);
                }
            }

            if (vowel_indices.size() >= 2) {
                const auto first = vowel_indices[vowel_indices.size() - 2];
                const auto second = vowel_indices.back();
                if (chars[first] == "u" && chars[second] == "o") {
                    chars[first] = "ư";
                    chars[second] = "ơ";
                    last_action =
                        RepeatAction{key, {{first, "u"}, {second, "o"}}};
                    continue;
                }
                if (chars[first] == "u" && chars[second] == "a") {
                    chars[first] = "ư";
                    last_action = RepeatAction{key, {{first, "u"}}};
                    continue;
                }
            }

            const auto first = chars.size() > 6 ? chars.size() - 6 : 0;
            for (std::size_t i = chars.size(); i > first; --i) {
                const auto index = i - 1;
                if (chars[index] == "a" || chars[index] == "o" ||
                    chars[index] == "u") {
                    const auto base = chars[index];
                    chars[index] =
                        base == "a" ? "ă" : base == "o" ? "ơ" : "ư";
                    last_action = RepeatAction{key, {{index, base}}};
                    break;
                }
                if (chars[index] == "ă" || chars[index] == "ơ" ||
                    chars[index] == "ư") {
                    chars[index] = chars[index] == "ă" ? "a" :
                                   chars[index] == "ơ" ? "o" : "u";
                    chars.emplace_back(1, key);
                    vietnamese_off = true;
                    break;
                }
            }
            if (vietnamese_off || last_action) {
                continue;
            }
        }

        const auto key_tone = tone_for_key(key);
        if (key_tone != Tone::none && has_vowel_within_tone_range(chars)) {
            if (tone == key_tone) {
                tone = Tone::none;
                chars.emplace_back(1, key);
                cancelled_tone = true;
                vietnamese_off = true;
            } else {
                tone = key_tone;
                cancelled_tone = false;
            }
            continue;
        }

        if (key == 'z' && tone != Tone::none &&
            has_vowel_within_tone_range(chars)) {
            tone = Tone::none;
            cancelled_tone = true;
            continue;
        }

        chars.emplace_back(1, key);
    }

    const bool contains_vowel =
        std::any_of(chars.begin(), chars.end(), is_vowel);
    if (contains_vowel && !has_valid_final_consonant(chars)) {
        if (cancelled_tone || vietnamese_off) {
            const auto tone_index = find_tone_index(chars);
            if (tone != Tone::none && tone_index < chars.size()) {
                chars[tone_index] = tone_character(chars[tone_index], tone);
            }
            return join(chars);
        }
        std::string raw;
        raw.reserve(keystrokes.size());
        for (const auto input : keystrokes) {
            raw.push_back(static_cast<char>(
                std::tolower(static_cast<unsigned char>(input))));
        }
        return raw;
    }

    const auto tone_index = find_tone_index(chars);
    if (tone != Tone::none && tone_index < chars.size()) {
        chars[tone_index] = tone_character(chars[tone_index], tone);
    }
    return join(chars);
}

std::string telex_keystrokes_from_text(std::string_view text) {
    constexpr std::array<char, 5> tone_keys{'f', 's', 'r', 'x', 'j'};
    std::string keystrokes;
    Tone tone = Tone::none;

    for (std::size_t offset = 0; offset < text.size();) {
        const auto lead = static_cast<unsigned char>(text[offset]);
        const std::size_t length =
            lead < 0x80 ? 1 : (lead & 0xE0U) == 0xC0U
                                ? 2
                                : (lead & 0xF0U) == 0xE0U ? 3 : 4;
        if (offset + length > text.size()) {
            keystrokes.append(text.substr(offset));
            break;
        }
        const auto character = text.substr(offset, length);
        offset += length;

        if (character == "đ" || character == "Đ") {
            keystrokes += "dd";
            continue;
        }

        bool encoded_vowel = false;
        for (const auto& vowel : kVowels) {
            if (character == vowel.base) {
                if (vowel.base == "ă") {
                    keystrokes += "aw";
                } else if (vowel.base == "â") {
                    keystrokes += "aa";
                } else if (vowel.base == "ê") {
                    keystrokes += "ee";
                } else if (vowel.base == "ô") {
                    keystrokes += "oo";
                } else if (vowel.base == "ơ") {
                    keystrokes += "ow";
                } else if (vowel.base == "ư") {
                    keystrokes += "uw";
                } else {
                    keystrokes.append(vowel.base);
                }
                encoded_vowel = true;
                break;
            }
            for (std::size_t index = 0; index < vowel.toned.size(); ++index) {
                if (character == vowel.toned[index]) {
                    if (vowel.base == "ă") {
                        keystrokes += "aw";
                    } else if (vowel.base == "â") {
                        keystrokes += "aa";
                    } else if (vowel.base == "ê") {
                        keystrokes += "ee";
                    } else if (vowel.base == "ô") {
                        keystrokes += "oo";
                    } else if (vowel.base == "ơ") {
                        keystrokes += "ow";
                    } else if (vowel.base == "ư") {
                        keystrokes += "uw";
                    } else {
                        keystrokes.append(vowel.base);
                    }
                    tone = static_cast<Tone>(index);
                    encoded_vowel = true;
                    break;
                }
            }
            if (encoded_vowel) {
                break;
            }
        }
        if (!encoded_vowel) {
            if (length == 1) {
                keystrokes.push_back(static_cast<char>(
                    std::tolower(static_cast<unsigned char>(character[0]))));
            } else {
                keystrokes.append(character);
            }
        }
    }

    if (tone != Tone::none) {
        keystrokes.push_back(tone_keys[static_cast<std::size_t>(tone)]);
    }
    return keystrokes;
}

Utf8Edit make_utf8_edit(std::string_view current, std::string_view next) {
    std::size_t common_bytes = 0;
    const auto max_common = std::min(current.size(), next.size());
    while (common_bytes < max_common &&
           current[common_bytes] == next[common_bytes]) {
        ++common_bytes;
    }

    while (common_bytes > 0 &&
           ((common_bytes < current.size() &&
             (static_cast<unsigned char>(current[common_bytes]) & 0xC0U) ==
                 0x80U) ||
            (common_bytes < next.size() &&
             (static_cast<unsigned char>(next[common_bytes]) & 0xC0U) ==
                 0x80U))) {
        --common_bytes;
    }

    return {
        utf8_codepoint_count(current.substr(common_bytes)),
        std::string(next.substr(common_bytes)),
    };
}

std::string remove_last_utf8_codepoint(std::string_view text) {
    if (text.empty()) {
        return {};
    }

    auto start = text.size() - 1;
    while (start > 0 &&
           (static_cast<unsigned char>(text[start]) & 0xC0U) == 0x80U) {
        --start;
    }
    return std::string(text.substr(0, start));
}

std::size_t utf8_codepoint_count(std::string_view text) noexcept {
    std::size_t count = 0;
    for (const auto byte : text) {
        if ((static_cast<unsigned char>(byte) & 0xC0U) != 0x80U) {
            ++count;
        }
    }
    return count;
}

}  // namespace qatt
