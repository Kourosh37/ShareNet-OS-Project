# ShareNet-Phase1-C

ShareNet یک پروژه ساده اشتراک فایل در شبکه داخلی است که با زبان C و سوکت‌های POSIX روی لینوکس پیاده‌سازی شده است. در فاز اول، سرور به صورت single-process و single-threaded کار می‌کند و در هر لحظه فقط یک کلاینت را پردازش می‌کند.

## ویژگی‌ها

- ارتباط TCP با معماری Client-Server
- پروتکل binary با `MessageHeader` و `ChunkHeader`
- ارسال و دریافت امن داده با `send_all` و `recv_all`
- نمایش لیست فایل‌های سرور
- آپلود فایل از کلاینت به `server_files`
- دانلود chunk شده فایل در `downloads`
- جلوگیری ساده از path traversal در نام فایل
- رابط گرافیکی raylib برای سرور و کلاینت
- لایه socket سازگار با Linux، Windows و macOS

## پیش‌نیازها

- Linux
- gcc
- make
- برای نسخه گرافیکی: raylib

روی Ubuntu/Debian در صورت آماده بودن بسته‌ها:

```sh
sudo apt install build-essential libraylib-dev
```

روی macOS با Homebrew:

```sh
brew install raylib pkg-config
```

روی Windows باید MinGW-w64 یا gcc سازگار و raylib نصب باشد. برای ساخت GUI ویندوزی، متغیر `RAYLIB_PATH` را به پوشه نصب raylib تنظیم کنید؛ این پوشه باید `include` و `lib` داشته باشد.

## ساختار پروژه

```text
ShareNet-Phase1-C/
├── Makefile
├── README.md
├── DOCUMENTATION.md
├── include/
├── src/
├── server_files/
├── client_files/
├── downloads/
└── test_files/
```

## کامپایل

```sh
make
```

خروجی کامپایل دو فایل اجرایی است:

```sh
./sharenet_server
./sharenet_client
```

برای ساخت نسخه گرافیکی با raylib:

```sh
make gui
```

خروجی گرافیکی:

```sh
./sharenet_server_gui
./sharenet_client_gui
```

## اجرای سرور

```sh
make run-server
```

یا:

```sh
./sharenet_server
```

## اجرای کلاینت

در ترمینال جداگانه:

```sh
make run-client
```

یا:

```sh
./sharenet_client
```

## اجرای نسخه گرافیکی

در یک ترمینال:

```sh
./sharenet_server_gui
```

در ترمینال یا پنجره جدا:

```sh
./sharenet_client_gui
```

در GUI سرور ابتدا روی `Start Server` کلیک کنید. در GUI کلاینت می‌توانید مسیر آپلود، نام فایل دانلود و عملیات لیست/آپلود/دانلود را از طریق دکمه‌ها انجام دهید.

## ساخت خروجی برای سیستم‌عامل‌ها

Linux:

```sh
./scripts/build-linux.sh
```

خروجی در `dist/linux` قرار می‌گیرد.

Windows:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build-windows.ps1
```

یا:

```bat
scripts\build-windows.bat
```

خروجی در `dist/windows` قرار می‌گیرد. اگر `RAYLIB_PATH` تنظیم نشده باشد، خروجی‌های ترمینالی ویندوز ساخته می‌شوند و خروجی‌های GUI ویندوزی skip می‌شوند.

macOS:

```sh
./scripts/build-macos.sh
```

خروجی در `dist/macos` قرار می‌گیرد.

## سناریوی دمو

1. سرور را اجرا کنید.
2. کلاینت را اجرا کنید.
3. گزینه آپلود را انتخاب کنید و مسیر `client_files/sample.txt` را وارد کنید.
4. گزینه نمایش لیست فایل‌ها را انتخاب کنید.
5. گزینه دانلود را انتخاب کنید و نام `sample.txt` را وارد کنید.

## بررسی صحت فایل

```sh
diff client_files/sample.txt downloads/sample.txt
sha256sum client_files/sample.txt downloads/sample.txt
```

اگر `diff` خروجی نداشته باشد و hashها یکسان باشند، فایل دانلود شده دقیقا با فایل اصلی برابر است.
