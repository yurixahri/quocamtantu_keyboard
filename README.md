![Loading](resources/qatt.ico)

# Quốc Âm Tân Tự (國音新字)

Prototype bộ gõ Quốc Âm Tân Tự dùng C++20/CMake. Hiện có tính năng Telex/mapping và app tray cho Windows; phần giao diện web và pipeline xử lý ảnh không nằm trong project con này. Dự án này 100% là vibe code vì mình thấy khá hứng thú với nội dung này.

## Dữ liệu

- `config/alphabet.json`: danh sách thanh, phụ âm đầu, vần và các quy tắc tạo biến thể. Thứ tự danh sách theo dữ liệu nguồn.
- `data/mapping.tsv`: bảng tra được sao từ `public/mapping.yaml`, có thêm mục `chào` trỏ tới glyph của âm tiết `ch` + `ao` + thanh huyền vì bảng nguồn đặt dấu sai thành `chaò`. Nội dung file nguồn thực tế là TSV, mỗi dòng gồm `glyph<TAB>từ tiếng Việt<TAB>độ ưu tiên`; tên `.yaml` ở project cũ không phản ánh định dạng.
- `src/telex.cpp`: chuyển chuỗi phím Telex thành âm tiết tiếng Việt NFC.
- `src/mapping.cpp`: nạp TSV và đổi chính xác một âm tiết có trong bảng thành glyph.
- `src/windows_tray.cpp`: app nền Windows, menu khay hệ thống chọn chế độ Quốc Âm, Telex hoặc tắt; cập nhật phần đuôi thay đổi thay vì xóa/gõ lại toàn âm tiết ở mỗi phím.
- `config.ini`: cấu hình phím tắt chuyển chế độ (mặc định Alt+Z) và bật/tắt âm thanh thông báo.
- `resources/qatt.ico`: biểu tượng nền trắng, dùng glyph Quốc Âm Tân Tự cho âm `qu` từ font QATT. Font hiện tại không có bảng chữ cái Latin, nên icon dùng glyph QATT thay vì chữ `Q` Latin.
- `src/main.cpp`: CLI để kiểm tra mapping hoặc thử chuyển một âm tiết.

JSON phù hợp cho cấu hình có cấu trúc và dễ đọc bằng công cụ phổ biến. Khi cần parse trực tiếp trong C++, có thể dùng `nlohmann/json`; hiện bộ khung chưa thêm thư viện ngoài. Bảng chuyển đổi lớn được giữ ở TSV để dễ sinh, diff và đọc tuần tự, không cần nhúng toàn bộ vào JSON.

## Build trên Windows

Yêu cầu CMake 3.20 trở lên và MinGW-w64 hoặc MSVC hỗ trợ C++20. Mặc định CMake build `qatt_core` dạng static và liên kết runtime C++ tĩnh vào các executable, để không cần phân phối DLL runtime của MinGW/MSVC. Các DLL hệ thống Windows vẫn được sử dụng. Có thể tắt liên kết runtime tĩnh bằng `-DQATT_STATIC_RUNTIME=OFF`.

```powershell
cmake -S . -B build -G Ninja -DQATT_STATIC_RUNTIME=ON
cmake --build build
ctest --test-dir build --output-on-failure
.\build\qatt_tray.exe
```

Với MSVC, có thể chọn runtime tĩnh tương đương bằng tùy chọn trên. Để build runtime động, cấu hình lại với `-DQATT_STATIC_RUNTIME=OFF`.

CLI để xem số mục hoặc thử Telex:

```powershell
.\build\qatt_cli.exe
.\build\qatt_cli.exe .\build\mapping.tsv vieetj
```

Tray app khởi động ở chế độ chuyển sang Quốc Âm Tân Tự. Chạy `qatt_tray.exe` cùng `mapping.tsv`, `qatt.ico` và `config.ini` (CMake tự copy các file vào thư mục build). Bấm chuột phải vào biểu tượng tray để chọn chế độ; nhấn phím tắt (mặc định **Alt+Z**, có thể chỉnh trong `config.ini`) để luân phiên Quốc Âm Tân Tự → chỉ Telex → tắt → Quốc Âm Tân Tự.

- **Chuyển sang Quốc Âm Tân Tự**: gõ Telex; khi chốt từ thì chuyển từ có trong mapping thành glyph.
- **Chỉ gõ Telex**: gõ tiếng Việt bằng Telex, không tra mapping/glyph.
- **Tắt bộ gõ**: chuyển phím trực tiếp cho ứng dụng như bình thường.
- **Thoát**: đóng ứng dụng.

Ở hai chế độ nhập, phím chữ được giữ trong từ hiện tại và hiển thị thành Telex; nhấn cách, Enter hoặc dấu câu để chốt từ. Trong chế độ Quốc Âm, chỉ các từ có trong mapping mới đổi glyph; chuỗi không có mapping sẽ giữ nguyên phím gốc. Engine cũng chỉ áp dụng Telex khi âm tiết có vần và phụ âm cuối hợp lệ, nên `enter` được giữ nguyên thay vì thành `ểnt`.

Phím `W` nhận diện hai nguyên âm cuối của âm tiết: `uo` + `w` thành `ươ` (ví dụ `uongw` → `ương`), `ua` + `w` thành `ưa` (ví dụ `chuaw` → `chưa`); các trường hợp khác thì đổi nguyên âm a/o/u gần nhất thành ă/ơ/ư. Phím `a`, `e`, `o` lặp lại cũng có thể đổi nguyên âm trước đó dù đã gõ phụ âm cuối, ví dụ `them` rồi `e` → `thêm`.

Nhấn Alt+Z để chuyển chế độ; mỗi lần đổi chế độ phát âm thông báo Windows.
Enter và Tab chốt âm tiết rồi chuyển phím gốc tới ứng dụng. Các ký tự cần Shift như `<` và `>` cũng được chuyển phím gốc, không bị thay bằng ký tự Unicode giả lập.
Nhấn lại cùng phím thanh sẽ hủy thanh và giữ phím đó như chữ thường; ví dụ `thiff` hiển thị `thif`.

Vị trí dấu theo quy tắc kiểu cũ: bỏ qua `qu`/`gi` đầu khi chúng đóng vai trò phụ âm; nếu chỉ có một nguyên âm thì đặt dấu ở đó. Với hai nguyên âm, đặt dấu ở nguyên âm đầu, trừ khi có phụ âm cuối; với ba nguyên âm hoặc hai nguyên âm kèm phụ âm cuối, đặt ở nguyên âm thứ hai. `ê` và `ơ` luôn được ưu tiên. Ví dụ `toaf` → `tòa`, `toanf` → `toàn`, `quyf` → `quỳ`, `xoas` → `xóa`, `thuwowr` → `thưở`, `chuyeenj` → `chuyện`.

### Giới hạn của prototype hook

- Đây là hook bàn phím toàn cục dùng `WH_KEYBOARD_LL` và `SendInput`, không phải Windows TSF. Một số ứng dụng, màn hình bảo mật hoặc ứng dụng chạy quyền cao hơn có thể không nhận hoặc không cho chèn phím.
- Nên thử trong trình soạn thảo thông thường trước; tránh dùng khi nhập mật khẩu hoặc trong ứng dụng nhạy cảm.
- Phiên bản đầu chỉ xử lý nhập Telex bằng chữ cái ASCII, phím xoá, dấu câu, cách, Enter và Tab. Backspace vật lý được chuyển thẳng tới ứng dụng để xóa một ký tự hiển thị; engine cố đồng bộ lại buffer Telex, và nếu không thể thì để phần còn lại của từ đi qua nguyên bản đến dấu phân cách tiếp theo. Điều hướng con trỏ giữa một từ chưa chốt chưa được xử lý như IME hoàn chỉnh.
- Key-repeat khi giữ `W` được bỏ qua để không chèn thêm chữ `w` sau khi tạo dấu/mũ bằng Telex.
- Hiện không có giao diện cài đặt/khởi động cùng hệ thống.
- Tính năng telex vẫn còn chưa được mượt mà, đôi lúc thêm dấu không như ý muốn như khi gõ song song tiếng anh/việt.

## Linux

`qatt_core` và `qatt_cli` không phụ thuộc Windows. Để build trên Linux, dùng CMake với compiler Linux native; MinGW-w64 tạo chương trình Windows, không phải Linux. Tray/nhận phím Linux cần adapter riêng (ví dụ IBus/Fcitx) và chưa được triển khai.

## Lưu ý Unicode

Mapping chứa ký tự UTF-8 có codepoint ngoài BMP. Các codepoint `U+010000` trở đi trong dữ liệu hiện tại không thuộc vùng Private Use Area. Engine cần giữ dữ liệu UTF-8 và xử lý codepoint Unicode đúng; không nên giả định một glyph luôn tương ứng với một byte hay một `wchar_t`.

## References
- Source dựa theo: https://github.com/hungtq1016/quocamtantu
- Quy tắc đặt dấu thanh (kiểu cũ): https://vi.wikipedia.org/wiki/Quy_tắc_đặt_dấu_thanh_của_chữ_Quốc_ngữ
