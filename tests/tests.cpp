#include "mapping.hpp"
#include "telex.hpp"

#include <cassert>
#include <string>

int main(int argc, char* argv[]) {
    assert(qatt::compose_telex("xin") == "xin");
    assert(qatt::compose_telex("aw") == "ă");
    assert(qatt::compose_telex("aa") == "â");
    assert(qatt::compose_telex("aaa") == "aa");
    assert(qatt::compose_telex("aaaa") == "aaa");
    assert(qatt::compose_telex("ee") == "ê");
    assert(qatt::compose_telex("eee") == "ee");
    assert(qatt::compose_telex("oo") == "ô");
    assert(qatt::compose_telex("ooo") == "oo");
    assert(qatt::compose_telex("dd") == "đ");
    assert(qatt::compose_telex("ddd") == "dd");
    assert(qatt::compose_telex("aww") == "aw");
    assert(qatt::compose_telex("uoww") == "uow");
    assert(qatt::compose_telex("uaww") == "uaw");
    assert(qatt::compose_telex("oeo") == "oeo");
    assert(qatt::compose_telex("as") == "á");
    assert(qatt::compose_telex("ass") == "as");
    assert(qatt::compose_telex("asz") == "a");
    assert(qatt::compose_telex("aszz") == "az");
    assert(qatt::compose_telex("aasz") == "â");
    assert(qatt::compose_telex("asww") == "áw");
    assert(qatt::compose_telex("angf") == "àng");
    assert(qatt::compose_telex("thengf") == "thèng");
    assert(qatt::compose_telex("themf") == "thèm");
    assert(qatt::compose_telex("thenghf") == "thenghf");
    assert(qatt::telex_keystrokes_from_text("thê") == "thee");
    assert(qatt::compose_telex(
               qatt::telex_keystrokes_from_text("thê")) == "thê");
    assert(qatt::compose_telex(
               qatt::telex_keystrokes_from_text("thê") + "f") == "thề");
    assert(qatt::compose_telex(
               qatt::telex_keystrokes_from_text("thêm")) == "thêm");
    assert(qatt::compose_telex(
               qatt::telex_keystrokes_from_text("thềm")) == "thềm");
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
