#include "mapping.hpp"
#include "telex.hpp"

#include <cassert>
#include <string>

int main(int argc, char* argv[]) {
    assert(qatt::compose_telex("xin") == "xin");
    assert(qatt::compose_telex("aw") == "ă");
    assert(qatt::compose_telex("uwow") == "ươ");
    assert(qatt::compose_telex("uongw") == "ương");
    assert(qatt::compose_telex("uowng") == "ương");
    assert(qatt::compose_telex("chuaw") == "chưa");
    assert(qatt::compose_telex("theme") == "thêm");
    assert(qatt::compose_telex("enter") == "enter");
    assert(qatt::compose_telex("thif") == "thì");
    assert(qatt::compose_telex("thiff") == "thif");
    assert(qatt::compose_telex("giar") == "giả");
    assert(qatt::compose_telex("xoas") == "xóa");
    assert(qatt::compose_telex("toaf") == "tòa");
    assert(qatt::compose_telex("hoaf") == "hòa");
    assert(qatt::compose_telex("toanf") == "toàn");
    assert(qatt::compose_telex("quyf") == "quỳ");
    assert(qatt::compose_telex("quaf") == "quà");
    assert(qatt::compose_telex("hoef") == "hòe");
    assert(qatt::compose_telex("hoanf") == "hoàn");
    assert(qatt::compose_telex("quets") == "quét");
    assert(qatt::compose_telex("quats") == "quát");
    assert(qatt::compose_telex("quytj") == "quỵt");
    assert(qatt::compose_telex("nguyj") == "ngụy");
    assert(qatt::compose_telex("suyst") == "suýt");
    assert(qatt::compose_telex("thuyr") == "thủy");
    assert(qatt::compose_telex("khuyur") == "khuỷu");
    assert(qatt::compose_telex("thuwowr") == "thưở");
    assert(qatt::compose_telex("chuyeenj") == "chuyện");
    assert(qatt::compose_telex("nghiax") == "nghĩa");
    assert(qatt::compose_telex("muoons") == "muốn");
    assert(qatt::compose_telex("tieens") == "tiến");
    assert(qatt::compose_telex("Vieetj") == "việt");
    assert(qatt::compose_telex("toois") == "tối");
    assert(qatt::compose_telex("chaof") == "chào");
    assert(qatt::compose_telex("ddang") == "đang");
    assert(qatt::compose_telex("ginf") == "gìn");
    assert(qatt::compose_telex("tuys") == "túy");
    assert(qatt::compose_telex("[") == "ơ");
    assert(qatt::compose_telex("]") == "ư");
    const auto vowel_edit = qatt::make_utf8_edit("a", "ă");
    assert(vowel_edit.backspaces == 1);
    assert(vowel_edit.inserted_text == "ă");
    const auto accented_edit = qatt::make_utf8_edit("chao", "chào");
    assert(accented_edit.backspaces == 2);
    assert(accented_edit.inserted_text == "ào");
    const auto append_edit = qatt::make_utf8_edit("chà", "chào");
    assert(append_edit.backspaces == 0);
    assert(append_edit.inserted_text == "o");
    assert(qatt::remove_last_utf8_codepoint("chào") == "chà");

    if (argc > 1) {
        const auto mapping = qatt::Mapping::load_tsv(argv[1]);
        assert(mapping.size() > 4000);
        assert(mapping.contains_word("việt"));
        assert(mapping.has_prefix("en"));
        assert(!mapping.has_prefix("ent"));
        assert(!mapping.contains_word("enter"));
        assert(mapping.convert_word("việt") != "việt");
        assert(mapping.convert_word("chào") == "𓃑");
        assert(mapping.convert_word("khongco") == "khongco");
    }

    assert(qatt::utf8_codepoint_count("việt") == 4);
    return 0;
}
