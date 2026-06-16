# مستندات ShareNet-Phase1-C

## 1. معرفی پروژه

ShareNet یک سیستم اشتراک فایل با معماری Client-Server است. هسته انتقال فایل با زبان C و TCP socket پیاده‌سازی شده و رابط گرافیکی اصلی پروژه با Qt ساخته شده است.

## 2. هدف فاز اول

هدف فاز اول، انتقال درست فایل در شبکه داخلی با پروتکل binary، آپلود، دانلود، لیست فایل‌ها و بررسی صحت فایل نهایی است.

## 3. معماری کلی

سیستم شامل یک سرور مرکزی و کلاینت‌ها است. سرور فایل‌ها را در `server_files` نگهداری می‌کند. کلاینت فایل آپلود می‌کند، لیست فایل‌ها را دریافت می‌کند و فایل انتخابی را دانلود می‌کند.

## 4. ابزارهای اجرایی

- `sharenet_server`: سرور CLI
- `sharenet_client`: کلاینت CLI
- `sharenet_qt_server`: سرور Qt
- `sharenet_qt_client`: کلاینت Qt

## 5. نقش سرور

سرور روی IP و port مشخص listen می‌کند. در نسخه Qt، فایل‌های موجود در `server_files` داخل جدول نمایش داده می‌شوند و امکان حذف یا copy/download فایل انتخاب‌شده وجود دارد.

## 6. نقش کلاینت

کلاینت Qt امکان انتخاب فایل آپلود، انتخاب پوشه دانلود، مشاهده فایل‌های سرور، دانلود فایل انتخاب‌شده و مشاهده progress bar را فراهم می‌کند.

## 7. پروتکل ارتباطی

پروتکل binary است. هر پیام با `MessageHeader` شروع می‌شود:

```c
typedef struct {
    uint32_t type;
    uint32_t payload_size;
} MessageHeader;
```

همه فیلدهای عددی ارسالی روی شبکه با network byte order منتقل می‌شوند.

## 8. ساختار ChunkHeader

```c
typedef struct {
    uint32_t chunk_index;
    uint32_t total_chunks;
    uint32_t chunk_size;
} ChunkHeader;
```

هر chunk شامل header و داده واقعی فایل است.

## 9. پیام‌های پروتکل

- `MSG_REQUEST_LIST`
- `MSG_LIST_RESPONSE`
- `MSG_REQUEST_UPLOAD`
- `MSG_UPLOAD_RESPONSE`
- `MSG_DOWNLOAD_REQUEST`
- `MSG_CHUNK_DATA`
- `MSG_DONE_TRANSFER`
- `MSG_ERROR`
- `MSG_EXIT`

## 10. روند آپلود

کلاینت metadata فایل را ارسال می‌کند. سپس فایل را به chunkهای پشت سر هم تقسیم می‌کند و با `MSG_CHUNK_DATA` می‌فرستد. در پایان `MSG_DONE_TRANSFER` ارسال می‌شود.

## 11. روند دانلود

کلاینت نام فایل را ارسال می‌کند. سرور فایل را از `server_files` پیدا می‌کند و chunk به chunk برای کلاینت می‌فرستد. کلاینت فایل را در پوشه دانلود انتخاب‌شده بازسازی می‌کند.

## 12. امنیت ساده

نام فایل نباید خالی باشد و نباید شامل `/`، `\` یا `..` باشد. این کنترل از path traversal ساده جلوگیری می‌کند.

## 13. Build CLI

Linux/WSL:

```sh
make
```

Windows:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build-windows.ps1
```

## 14. Build Qt

Windows:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build-qt-windows.ps1
```

Linux:

```sh
./scripts/build-qt-linux.sh
```

macOS:

```sh
./scripts/build-qt-macos.sh
```

## 15. تست صحت فایل

```sh
diff client_files/sample.txt downloads/sample.txt
sha256sum client_files/sample.txt downloads/sample.txt
```

اگر `diff` خروجی نداشته باشد و hashها برابر باشند، انتقال صحیح است.

## 16. بسته Portable ویندوز

```powershell
powershell -ExecutionPolicy Bypass -File scripts\package-windows-portable.ps1
```

این اسکریپت بسته portable شامل خروجی‌های لازم و runtimeها را در `dist` می‌سازد.

## 17. محدودیت‌های فاز اول

- احراز هویت وجود ندارد.
- رمزنگاری ارتباط وجود ندارد.
- resume برای انتقال ناقص پیاده‌سازی نشده است.
- پروتکل برای نیازهای فاز اول طراحی شده است.
