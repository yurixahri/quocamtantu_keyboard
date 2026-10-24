#define UNICODE
#define _UNICODE

#include <windows.h>
#include <shellapi.h>
#include <mmsystem.h>

#include "mapping.hpp"
#include "telex.hpp"

#include <algorithm>
#include <cctype>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr wchar_t kWindowClass[] = L"QattTrayWindow";
constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kQattModeCommand = 1001;
constexpr UINT kTelexModeCommand = 1002;
constexpr UINT kOffModeCommand = 1003;
constexpr UINT kExitCommand = 1004;
constexpr int kToggleModeHotkeyId = 1;

enum class InputMode {
    qatt,
    telex,
    off
};

HWND g_window = nullptr;
HHOOK g_keyboard_hook = nullptr;
NOTIFYICONDATAW g_tray_icon{};
qatt::Mapping g_mapping;
InputMode g_mode = InputMode::qatt;
bool g_w_key_held = false;
bool g_passthrough_until_boundary = false;
std::string g_keystrokes;
std::string g_visible_word;
HWND g_target_window = nullptr;
bool g_uppercase_first = false;
bool g_sound_enabled = true;
bool g_game_auto_detect = true;
bool g_fullscreen_detect = true;
std::vector<std::wstring> g_game_exclude_apps;
HWND g_cached_foreground_window = nullptr;
bool g_is_foreground_game = false;

constexpr std::wstring_view kDefaultGameApps[] = {
    L"league of legends.exe",
    L"leagueclient.exe",
    L"leagueclientux.exe",
    L"cs2.exe",
    L"csgo.exe",
    L"dota2.exe",
    L"valorant.exe",
    L"valorant-win64-shipping.exe",
    L"genshinimpact.exe",
    L"gta5.exe",
    L"minecraft.exe",
    L"javaw.exe",
    L"robloxplayerbeta.exe",
    L"naraka.exe",
    L"overwatch.exe",
    L"fortniteclient-win64-shipping.exe",
    L"apex.exe",
    L"r5apex.exe",
    L"tslgame.exe",
    L"fifa.exe",
    L"fc24.exe",
    L"fc25.exe",
    L"pubg.exe",
    L"crossfire.exe",
    L"warframe.x64.exe"
};

void update_tray_tooltip();
void disable_with_error(const wchar_t* message);
void set_mode(InputMode mode);

std::filesystem::path app_dir() {
    std::vector<wchar_t> path(32768);
    const auto length = GetModuleFileNameW(
        nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length == path.size()) {
        throw std::runtime_error("Cannot determine executable path");
    }
    return std::filesystem::path(std::wstring(path.data(), length))
        .parent_path();
}

std::filesystem::path mapping_path() {
    return app_dir() / L"mapping.tsv";
}

std::filesystem::path icon_path() {
    return app_dir() / L"qatt.ico";
}

std::filesystem::path config_path() {
    return app_dir() / L"config.ini";
}

struct HotkeyConfig {
    UINT modifiers = MOD_ALT;
    UINT vk = 'Z';
};

HotkeyConfig load_hotkey_config(const std::filesystem::path& config_file) {
    HotkeyConfig cfg;
    wchar_t mod_buf[64]{};
    wchar_t key_buf[32]{};
    GetPrivateProfileStringW(L"hotkey", L"modifiers", L"Alt", mod_buf,
                             static_cast<DWORD>(std::size(mod_buf)),
                             config_file.c_str());
    GetPrivateProfileStringW(L"hotkey", L"key", L"Z", key_buf,
                             static_cast<DWORD>(std::size(key_buf)),
                             config_file.c_str());

    std::wstring mods = mod_buf;
    for (auto& c : mods) {
        c = static_cast<wchar_t>(std::towlower(c));
    }

    UINT m = 0;
    if (mods.find(L"alt") != std::wstring::npos) m |= MOD_ALT;
    if (mods.find(L"ctrl") != std::wstring::npos ||
        mods.find(L"control") != std::wstring::npos) m |= MOD_CONTROL;
    if (mods.find(L"shift") != std::wstring::npos) m |= MOD_SHIFT;
    if (mods.find(L"win") != std::wstring::npos) m |= MOD_WIN;
    if (m != 0) {
        cfg.modifiers = m;
    }

    std::wstring key_str = key_buf;
    for (auto& c : key_str) {
        c = static_cast<wchar_t>(std::towupper(c));
    }
    while (!key_str.empty() && std::iswspace(key_str.front())) {
        key_str.erase(key_str.begin());
    }
    while (!key_str.empty() && std::iswspace(key_str.back())) {
        key_str.pop_back();
    }

    if (key_str.size() == 1) {
        cfg.vk = static_cast<UINT>(key_str[0]);
    } else if (key_str.rfind(L"F", 0) == 0 && key_str.size() <= 3) {
        try {
            const int f_num = std::stoi(key_str.substr(1));
            if (f_num >= 1 && f_num <= 24) {
                cfg.vk = VK_F1 + static_cast<UINT>(f_num - 1);
            }
        } catch (...) {
        }
    } else if (key_str == L"SPACE") {
        cfg.vk = VK_SPACE;
    } else if (key_str == L"TAB") {
        cfg.vk = VK_TAB;
    } else if (key_str == L"GRAVE" || key_str == L"`" || key_str == L"TILDE") {
        cfg.vk = VK_OEM_3;
    }
    return cfg;
}

void load_game_config(const std::filesystem::path& config_file) {
    g_game_auto_detect = GetPrivateProfileIntW(
        L"game", L"auto_detect", 1, config_file.c_str()) != 0;
    g_fullscreen_detect = GetPrivateProfileIntW(
        L"game", L"fullscreen_detect", 1, config_file.c_str()) != 0;

    wchar_t buf[2048]{};
    GetPrivateProfileStringW(L"game", L"exclude_apps", L"", buf,
                             static_cast<DWORD>(std::size(buf)),
                             config_file.c_str());
    g_game_exclude_apps.clear();
    std::wstring str = buf;
    std::size_t start = 0;
    while (start < str.size()) {
        auto end = str.find(L',', start);
        if (end == std::wstring::npos) {
            end = str.size();
        }
        std::wstring item = str.substr(start, end - start);
        while (!item.empty() && std::iswspace(item.front())) {
            item.erase(item.begin());
        }
        while (!item.empty() && std::iswspace(item.back())) {
            item.pop_back();
        }
        for (auto& c : item) {
            c = static_cast<wchar_t>(std::towlower(c));
        }
        if (!item.empty()) {
            g_game_exclude_apps.push_back(item);
        }
        start = end + 1;
    }
}

bool is_game_or_fullscreen(HWND hwnd) {
    if (hwnd == nullptr || hwnd == GetDesktopWindow() || hwnd == GetShellWindow()) {
        return false;
    }

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != 0 && pid != GetCurrentProcessId()) {
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (process != nullptr) {
            wchar_t path_buf[MAX_PATH]{};
            DWORD size = MAX_PATH;
            if (QueryFullProcessImageNameW(process, 0, path_buf, &size)) {
                std::wstring exe_name = std::filesystem::path(path_buf).filename().wstring();
                for (auto& c : exe_name) {
                    c = static_cast<wchar_t>(std::towlower(c));
                }
                for (const auto& app : g_game_exclude_apps) {
                    if (exe_name == app) {
                        CloseHandle(process);
                        return true;
                    }
                }
                for (const auto& game : kDefaultGameApps) {
                    if (exe_name == game) {
                        CloseHandle(process);
                        return true;
                    }
                }
            }
            CloseHandle(process);
        }
    }

    if (g_fullscreen_detect) {
        RECT rect{};
        if (GetWindowRect(hwnd, &rect)) {
            const int w = GetSystemMetrics(SM_CXSCREEN);
            const int h = GetSystemMetrics(SM_CYSCREEN);
            if (rect.left <= 0 && rect.top <= 0 && rect.right >= w && rect.bottom >= h) {
                const auto style = GetWindowLongW(hwnd, GWL_STYLE);
                if ((style & WS_CAPTION) != WS_CAPTION) {
                    return true;
                }
            }
        }
    }

    return false;
}

bool is_current_window_game() {
    HWND fg = GetForegroundWindow();
    if (fg == nullptr) {
        return false;
    }
    if (fg == g_cached_foreground_window) {
        return g_is_foreground_game;
    }
    g_cached_foreground_window = fg;
    g_is_foreground_game = is_game_or_fullscreen(fg);
    return g_is_foreground_game;
}

std::wstring to_wide(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    if (text.size() > static_cast<std::size_t>(
                          (std::numeric_limits<int>::max)())) {
        throw std::runtime_error("Text is too long to insert");
    }

    const auto length = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
        static_cast<int>(text.size()), nullptr, 0);
    if (length == 0) {
        throw std::runtime_error("Invalid UTF-8 text in mapping");
    }

    std::wstring result(static_cast<std::size_t>(length), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                            static_cast<int>(text.size()), result.data(),
                            length) == 0) {
        throw std::runtime_error("Cannot convert UTF-8 text for Windows");
    }
    return result;
}

std::string display_word(std::string_view word, bool uppercase_first) {
    auto result = to_wide(word);
    if (uppercase_first && !result.empty()) {
        CharUpperBuffW(result.data(), 1);
    }
    const auto length = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, result.data(),
        static_cast<int>(result.size()), nullptr, 0, nullptr, nullptr);
    if (length <= 0) {
        return std::string(word);
    }
    std::string utf8(static_cast<std::size_t>(length), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, result.data(),
                            static_cast<int>(result.size()), utf8.data(),
                            length, nullptr, nullptr) <= 0) {
        return std::string(word);
    }
    return utf8;
}

bool send_backspaces(std::size_t count) {
    if (count > static_cast<std::size_t>((std::numeric_limits<UINT>::max)() / 2)) {
        return false;
    }
    const auto scan_code = MapVirtualKeyW(VK_BACK, MAPVK_VK_TO_VSC);
    if (scan_code == 0) {
        return false;
    }

    std::vector<INPUT> inputs(count * 2);
    for (std::size_t i = 0; i < count; ++i) {
        inputs[i * 2].type = INPUT_KEYBOARD;
        inputs[i * 2].ki.wScan = static_cast<WORD>(scan_code);
        inputs[i * 2].ki.dwFlags = KEYEVENTF_SCANCODE;
        inputs[i * 2 + 1].type = INPUT_KEYBOARD;
        inputs[i * 2 + 1].ki.wScan = static_cast<WORD>(scan_code);
        inputs[i * 2 + 1].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
    }
    if (inputs.empty()) {
        return true;
    }

    const auto sent = SendInput(static_cast<UINT>(inputs.size()), inputs.data(),
                                sizeof(INPUT));
    return sent == static_cast<UINT>(inputs.size());
}

bool send_text(std::string_view text) {
    const auto wide = to_wide(text);
    std::vector<INPUT> inputs(wide.size() * 2);
    for (std::size_t i = 0; i < wide.size(); ++i) {
        inputs[i * 2].type = INPUT_KEYBOARD;
        inputs[i * 2].ki.wScan = wide[i];
        inputs[i * 2].ki.dwFlags = KEYEVENTF_UNICODE;
        inputs[i * 2 + 1].type = INPUT_KEYBOARD;
        inputs[i * 2 + 1].ki.wScan = wide[i];
        inputs[i * 2 + 1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    }
    return inputs.empty() ||
           SendInput(static_cast<UINT>(inputs.size()), inputs.data(),
                     sizeof(INPUT)) == inputs.size();
}

void update_preview() {
    const auto composed = qatt::compose_telex(g_keystrokes);
    const auto preview =
        g_mode == InputMode::qatt && !g_mapping.has_prefix(composed)
            ? g_keystrokes
            : composed;
    const auto next_word = display_word(preview, g_uppercase_first);
    const auto edit = qatt::make_utf8_edit(g_visible_word, next_word);
    if (!send_backspaces(edit.backspaces)) {
        disable_with_error(L"Không thể cập nhật nội dung trong ứng dụng hiện tại.");
        return;
    }
    if (!send_text(edit.inserted_text)) {
        disable_with_error(L"Không thể chèn nội dung vào ứng dụng hiện tại.");
        return;
    }
    g_visible_word = next_word;
}

void clear_buffer() {
    g_keystrokes.clear();
    g_visible_word.clear();
    g_target_window = nullptr;
    g_uppercase_first = false;
}

void commit_word(std::string_view boundary = {}) {
    if (!g_keystrokes.empty()) {
        const auto composed = qatt::compose_telex(g_keystrokes);
        const auto is_mapped =
            g_mode == InputMode::qatt && g_mapping.contains_word(composed);
        const auto output = is_mapped ? g_mapping.convert_word(composed)
                                      : g_mode == InputMode::qatt
                                            ? g_keystrokes
                                            : composed;
        const auto display =
            is_mapped ? output : display_word(output, g_uppercase_first);
        const auto edit = qatt::make_utf8_edit(g_visible_word, display);

        if (!send_backspaces(edit.backspaces)) {
            disable_with_error(
                L"Không thể cập nhật nội dung trong ứng dụng hiện tại.");
            return;
        }
        if (!send_text(edit.inserted_text)) {
            disable_with_error(L"Không thể chèn nội dung vào ứng dụng hiện tại.");
            return;
        }
        clear_buffer();
    }
    if (!boundary.empty() && !send_text(boundary)) {
        disable_with_error(L"Không thể chèn nội dung vào ứng dụng hiện tại.");
        return;
    }
}

bool key_to_text(const KBDLLHOOKSTRUCT& key, std::wstring& text) {
    BYTE keyboard_state[256]{};
    if (!GetKeyboardState(keyboard_state)) {
        return false;
    }
    if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) {
        keyboard_state[VK_SHIFT] |= 0x80;
    }
    if ((GetAsyncKeyState(VK_CAPITAL) & 0x0001) != 0) {
        keyboard_state[VK_CAPITAL] |= 0x01;
    }
    WCHAR buffer[8]{};
    const auto length = ToUnicodeEx(
        key.vkCode, key.scanCode, keyboard_state, buffer,
        static_cast<int>(std::size(buffer)), 0, GetKeyboardLayout(0));
    if (length <= 0) {
        return false;
    }
    text.assign(buffer, static_cast<std::size_t>(length));
    return true;
}

void flush_if_target_changed() {
    if (g_keystrokes.empty() && !g_passthrough_until_boundary) {
        return;
    }
    if (g_target_window != GetForegroundWindow()) {
        clear_buffer();
        g_passthrough_until_boundary = false;
    }
}

LRESULT CALLBACK keyboard_proc(int code, WPARAM message, LPARAM data) {
    if (code < 0) {
        return CallNextHookEx(g_keyboard_hook, code, message, data);
    }

    const auto& key = *reinterpret_cast<const KBDLLHOOKSTRUCT*>(data);
    if (message == WM_KEYUP || message == WM_SYSKEYUP) {
        if (key.vkCode == 'W') {
            g_w_key_held = false;
        }
        return CallNextHookEx(g_keyboard_hook, code, message, data);
    }

    if (message != WM_KEYDOWN && message != WM_SYSKEYDOWN) {
        return CallNextHookEx(g_keyboard_hook, code, message, data);
    }

    if ((key.flags & LLKHF_INJECTED) != 0) {
        return CallNextHookEx(g_keyboard_hook, code, message, data);
    }

    const bool control_down =
        (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool alt_down = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    const bool windows_down =
        (GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 ||
        (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0;

    if (key.vkCode == VK_BACK && g_keystrokes.empty()) {
        return CallNextHookEx(g_keyboard_hook, code, message, data);
    }

    if (g_mode == InputMode::off) {
        return CallNextHookEx(g_keyboard_hook, code, message, data);
    }

    if (g_game_auto_detect && is_current_window_game()) {
        if (!g_keystrokes.empty()) {
            clear_buffer();
        }
        return CallNextHookEx(g_keyboard_hook, code, message, data);
    }

    if (key.vkCode == VK_SHIFT || key.vkCode == VK_LSHIFT ||
        key.vkCode == VK_RSHIFT || key.vkCode == VK_CAPITAL) {
        return CallNextHookEx(g_keyboard_hook, code, message, data);
    }

    flush_if_target_changed();

    if (control_down || alt_down || windows_down) {
        commit_word();
        return CallNextHookEx(g_keyboard_hook, code, message, data);
    }

    const bool shift_down = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    const bool letter_key = key.vkCode >= 'A' && key.vkCode <= 'Z';
    if (shift_down && !letter_key && key.vkCode != VK_SHIFT &&
        key.vkCode != VK_LSHIFT && key.vkCode != VK_RSHIFT) {
        commit_word();
        return CallNextHookEx(g_keyboard_hook, code, message, data);
    }

    if (g_passthrough_until_boundary) {
        if (key.vkCode >= 'A' && key.vkCode <= 'Z') {
            return CallNextHookEx(g_keyboard_hook, code, message, data);
        }
        g_passthrough_until_boundary = false;
        g_target_window = nullptr;
    }

    if (key.vkCode == 'W' && g_w_key_held) {
        return 1;
    }

    if (key.vkCode == VK_BACK && !g_keystrokes.empty()) {
        const auto shortened =
            qatt::remove_last_utf8_codepoint(g_visible_word);
        bool rebased = shortened.empty();
        std::string next_keystrokes;

        for (std::size_t length = 1;
             !rebased && length <= g_keystrokes.size(); ++length) {
            for (std::size_t begin = 0;
                 begin + length <= g_keystrokes.size(); ++begin) {
                auto candidate = g_keystrokes;
                candidate.erase(begin, length);
                if (display_word(qatt::compose_telex(candidate),
                                 g_uppercase_first) == shortened) {
                    next_keystrokes = std::move(candidate);
                    rebased = true;
                    break;
                }
            }
        }

        if (shortened.empty()) {
            clear_buffer();
        } else if (rebased) {
            g_keystrokes = std::move(next_keystrokes);
            g_visible_word = shortened;
        } else {
            clear_buffer();
            g_passthrough_until_boundary = true;
            g_target_window = GetForegroundWindow();
        }
        return CallNextHookEx(g_keyboard_hook, code, message, data);
    }

    if (key.vkCode >= 'A' && key.vkCode <= 'Z') {
        std::wstring text;
        if (key_to_text(key, text) && text.size() == 1 &&
            text.front() < 128 &&
            std::isalpha(static_cast<unsigned char>(text.front())) != 0) {
            if (g_keystrokes.empty()) {
                g_target_window = GetForegroundWindow();
                g_uppercase_first =
                    std::isupper(static_cast<unsigned char>(text.front())) != 0;
            }
            if (key.vkCode == 'W') {
                g_w_key_held = true;
            }
            g_keystrokes.push_back(static_cast<char>(
                std::tolower(static_cast<unsigned char>(text.front()))));
            update_preview();
            return 1;
        }
    }

    if (key.vkCode == VK_SPACE) {
        if (g_mode == InputMode::qatt && !g_keystrokes.empty()) {
            commit_word();
        } else {
            commit_word(" ");
        }
        return 1;
    }
    if (key.vkCode == VK_RETURN) {
        commit_word();
        return CallNextHookEx(g_keyboard_hook, code, message, data);
    }
    if (key.vkCode == VK_TAB) {
        commit_word();
        return CallNextHookEx(g_keyboard_hook, code, message, data);
    }

    std::wstring text;
    if (key_to_text(key, text) && text.size() == 1 && text.front() < 128 &&
        !std::iswalnum(text.front()) && !std::iswspace(text.front())) {
        const auto boundary_length = WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), 1, nullptr, 0, nullptr,
            nullptr);
        if (boundary_length > 0) {
            std::string boundary(static_cast<std::size_t>(boundary_length), '\0');
            if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
                                    1, boundary.data(), boundary_length,
                                    nullptr, nullptr) > 0) {
                commit_word(boundary);
                return 1;
            }
        }
    }

    commit_word();
    return CallNextHookEx(g_keyboard_hook, code, message, data);
}

void update_tray_tooltip() {
    const wchar_t* tooltip = L"QATT - Quốc Âm Tân Tự";
    if (g_mode == InputMode::telex) {
        tooltip = L"QATT - chỉ gõ Telex";
    } else if (g_mode == InputMode::off) {
        tooltip = L"QATT - đã tắt";
    }
    lstrcpynW(g_tray_icon.szTip,
              tooltip,
              static_cast<int>(std::size(g_tray_icon.szTip)));
    Shell_NotifyIconW(NIM_MODIFY, &g_tray_icon);
}

void set_mode(InputMode mode) {
    if (g_mode == mode) {
        return;
    }
    if (g_mode != InputMode::off) {
        commit_word();
    } else {
        clear_buffer();
    }
    g_mode = mode;
    g_w_key_held = false;
    update_tray_tooltip();
    if (g_sound_enabled) {
        PlaySoundW(L"SystemAsterisk", nullptr,
                   SND_ALIAS | SND_ASYNC | SND_NODEFAULT);
    }
}

void disable_with_error(const wchar_t* message) {
    clear_buffer();
    g_mode = InputMode::off;
    update_tray_tooltip();

    lstrcpynW(g_tray_icon.szInfoTitle, L"QATT", ARRAYSIZE(g_tray_icon.szInfoTitle));
    lstrcpynW(g_tray_icon.szInfo, message, ARRAYSIZE(g_tray_icon.szInfo));
    g_tray_icon.dwInfoFlags = NIIF_ERROR;
    g_tray_icon.uFlags = NIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &g_tray_icon);
    g_tray_icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
}

void finish_word_for_tray() {
    if (!g_keystrokes.empty() && g_target_window != GetForegroundWindow()) {
        clear_buffer();
        return;
    }
    commit_word();
}

void show_tray_menu() {
    finish_word_for_tray();
    const auto menu = CreatePopupMenu();
    if (menu == nullptr) {
        return;
    }
    AppendMenuW(menu, MF_STRING |
                          (g_mode == InputMode::qatt ? MF_CHECKED : MF_UNCHECKED),
                kQattModeCommand, L"Chuyển sang Quốc Âm Tân Tự");
    AppendMenuW(menu, MF_STRING |
                          (g_mode == InputMode::telex ? MF_CHECKED : MF_UNCHECKED),
                kTelexModeCommand, L"Chỉ gõ Telex");
    AppendMenuW(menu, MF_STRING |
                          (g_mode == InputMode::off ? MF_CHECKED : MF_UNCHECKED),
                kOffModeCommand, L"Tắt bộ gõ");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kExitCommand, L"Thoát");

    POINT cursor{};
    GetCursorPos(&cursor);
    SetForegroundWindow(g_window);
    const auto command = TrackPopupMenu(
        menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, cursor.x,
        cursor.y, 0, g_window, nullptr);
    DestroyMenu(menu);

    if (command == kQattModeCommand) {
        set_mode(InputMode::qatt);
    } else if (command == kTelexModeCommand) {
        set_mode(InputMode::telex);
    } else if (command == kOffModeCommand) {
        set_mode(InputMode::off);
    } else if (command == kExitCommand) {
        DestroyWindow(g_window);
    }
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam,
                             LPARAM lparam) {
    if (message == kTrayMessage) {
        if (lparam == WM_RBUTTONUP || lparam == WM_CONTEXTMENU ||
            lparam == WM_LBUTTONUP) {
            show_tray_menu();
        }
        return 0;
    }
    if (message == WM_DESTROY) {
        finish_word_for_tray();
        UnregisterHotKey(window, kToggleModeHotkeyId);
        if (g_keyboard_hook != nullptr) {
            UnhookWindowsHookEx(g_keyboard_hook);
            g_keyboard_hook = nullptr;
        }
        Shell_NotifyIconW(NIM_DELETE, &g_tray_icon);
        if (g_tray_icon.hIcon != nullptr) {
            DestroyIcon(g_tray_icon.hIcon);
            g_tray_icon.hIcon = nullptr;
        }
        PostQuitMessage(0);
        return 0;
    }
    if (message == WM_HOTKEY && wparam == kToggleModeHotkeyId) {
        const auto next_mode =
            g_mode == InputMode::qatt
                ? InputMode::telex
                : g_mode == InputMode::telex ? InputMode::off
                                             : InputMode::qatt;
        set_mode(next_mode);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

int run_app(HINSTANCE instance) {
    const auto mapping = qatt::Mapping::load_tsv(mapping_path().string());
    g_mapping = mapping;

    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = kWindowClass;
    if (RegisterClassExW(&window_class) == 0 &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        throw std::runtime_error("Cannot register tray window");
    }

    g_window = CreateWindowExW(0, kWindowClass, L"QATT", 0, 0, 0, 0, 0,
                               nullptr, nullptr, instance, nullptr);
    if (g_window == nullptr) {
        throw std::runtime_error("Cannot create tray message window");
    }

    const auto config_file = config_path();
    g_sound_enabled = GetPrivateProfileIntW(
        L"sound", L"enabled", 1, config_file.c_str()) != 0;
    load_game_config(config_file);

    const auto hotkey = load_hotkey_config(config_file);
    if (!RegisterHotKey(g_window, kToggleModeHotkeyId,
                        hotkey.modifiers | MOD_NOREPEAT, hotkey.vk)) {
        DestroyWindow(g_window);
        throw std::runtime_error("Cannot register toggle hotkey shortcut");
    }

    g_tray_icon.cbSize = sizeof(g_tray_icon);
    g_tray_icon.hWnd = g_window;
    g_tray_icon.uID = 1;
    g_tray_icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_tray_icon.uCallbackMessage = kTrayMessage;
    g_tray_icon.hIcon = static_cast<HICON>(LoadImageW(
        nullptr, icon_path().c_str(), IMAGE_ICON, 0, 0,
        LR_LOADFROMFILE | LR_DEFAULTSIZE));
    if (g_tray_icon.hIcon == nullptr) {
        UnregisterHotKey(g_window, kToggleModeHotkeyId);
        throw std::runtime_error("Cannot load qatt.ico");
    }
    update_tray_tooltip();
    if (!Shell_NotifyIconW(NIM_ADD, &g_tray_icon)) {
        UnregisterHotKey(g_window, kToggleModeHotkeyId);
        throw std::runtime_error("Cannot create notification area icon");
    }

    g_keyboard_hook =
        SetWindowsHookExW(WH_KEYBOARD_LL, keyboard_proc, instance, 0);
    if (g_keyboard_hook == nullptr) {
        Shell_NotifyIconW(NIM_DELETE, &g_tray_icon);
        UnregisterHotKey(g_window, kToggleModeHotkeyId);
        throw std::runtime_error("Cannot install keyboard hook");
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    try {
        return run_app(instance);
    } catch (const std::exception& error) {
        const auto message = to_wide(error.what());
        MessageBoxW(nullptr, message.c_str(), L"QATT", MB_OK | MB_ICONERROR);
        return 1;
    }
}
