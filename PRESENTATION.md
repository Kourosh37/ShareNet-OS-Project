<div dir="rtl" align="right" style="direction: rtl; text-align: right;">

# ارائه پروژه ShareNet

## ۱. هدف پروژه

ShareNet یک سامانه ساده برای اشتراک‌گذاری فایل در شبکه داخلی است. طبق Phase 1، هدف ساخت یک سیستم Client-Server است که کلاینت بتواند به سرور وصل شود، لیست فایل‌ها را ببیند، فایل آپلود کند و فایل دانلود کند.

طبق Phase 2، سیستم باید از نسخه ساده به نسخه همزمان توسعه پیدا کند؛ یعنی چند کلاینت و چند درخواست upload/download بتوانند همزمان مدیریت شوند، بدون اینکه فایل‌ها خراب یا ناقص شوند.

## ۲. معماری سیستم

<p>
• <b>Server:</b> نگهداری فایل‌ها، ساخت لیست فایل‌ها، پاسخ به upload و download.<br>
• <b>Client:</b> اتصال به سرور، انتخاب فایل، آپلود، مشاهده لیست و دانلود.
</p>

مسیرهای اصلی پیاده‌سازی:

<p>
• منطق شبکه و فایل: <span dir="ltr"><code>internal/netshare</code></span><br>
• برنامه سرور: <span dir="ltr"><code>cmd/server/main.go</code></span><br>
• برنامه کلاینت: <span dir="ltr"><code>cmd/client/main.go</code></span>
</p>

## ۳. اتصال Client و Server

سرور با TCP listener اجرا می‌شود و منتظر اتصال کلاینت‌ها می‌ماند.

<p>
• ساخت listener: <span dir="ltr"><code>internal/netshare/server.go:26-40</code></span><br>
• حلقه دریافت اتصال‌ها: <span dir="ltr"><code>internal/netshare/server.go:59-68</code></span><br>
• اتصال کلاینت با timeout و keep-alive: <span dir="ltr"><code>internal/netshare/protocol.go:157-167</code></span>
</p>

این بخش requirement اصلی Phase 1 یعنی ارتباط پایه بین کلاینت و سرور را پوشش می‌دهد.

## ۴. پروتکل ارتباطی

در PDFها گفته شده پیام‌ها باید ساختار مشخص داشته باشند و فیلد <span dir="ltr"><code>type</code></span> نوع عملیات را مشخص کند. در این پروژه، header هر پیام به‌صورت یک خط JSON ارسال می‌شود.

<p>
• ساختار request و response: <span dir="ltr"><code>internal/netshare/protocol.go:43-54</code></span><br>
• نوشتن JSON: <span dir="ltr"><code>internal/netshare/protocol.go:78-86</code></span><br>
• خواندن JSON: <span dir="ltr"><code>internal/netshare/protocol.go:88-94</code></span>
</p>

نوع پیام‌های اصلی:

<p>
• <span dir="ltr"><code>REQUEST_LIST</code></span><br>
• <span dir="ltr"><code>UPLOAD_START</code></span><br>
• <span dir="ltr"><code>DOWNLOAD_REQUEST</code></span><br>
• <span dir="ltr"><code>DELETE_REQUEST</code></span>
</p>

تعریف این پیام‌ها در <span dir="ltr"><code>internal/netshare/protocol.go:25-34</code></span> قرار دارد.

## ۵. لیست فایل‌ها

یکی از خواسته‌های اصلی Phase 1 و Phase 2، مشاهده فایل‌های موجود روی سرور است.

<p>
• خواندن فایل‌ها از پوشه سرور: <span dir="ltr"><code>internal/netshare/protocol.go:128-155</code></span><br>
• حذف فایل‌های موقت upload از لیست: <span dir="ltr"><code>internal/netshare/protocol.go:134-144</code></span><br>
• ارسال نام، اندازه و زمان تغییر فایل: <span dir="ltr"><code>internal/netshare/protocol.go:145-149</code></span><br>
• پاسخ سرور به درخواست لیست: <span dir="ltr"><code>internal/netshare/server.go:81-87</code></span>
</p>

## ۶. آپلود فایل

در Phase 1 کلاینت باید بتواند فایل را در سیستم ثبت کند. در این پیاده‌سازی، آپلود به‌صورت مرحله‌ای و امن انجام می‌شود.

<p>
• شروع آپلود در کلاینت: <span dir="ltr"><code>internal/netshare/protocol.go:188-236</code></span><br>
• ارسال درخواست <span dir="ltr"><code>UPLOAD_START</code></span>: <span dir="ltr"><code>internal/netshare/protocol.go:210-212</code></span><br>
• گرفتن تأیید اولیه از سرور: <span dir="ltr"><code>internal/netshare/protocol.go:213-219</code></span><br>
• ارسال داده فایل به‌صورت chunk: <span dir="ltr"><code>internal/netshare/protocol.go:220-222</code></span><br>
• اعلام پایان stream با <span dir="ltr"><code>CloseWrite</code></span>: <span dir="ltr"><code>internal/netshare/protocol.go:223-225</code></span>
</p>

در سمت سرور:

<p>
• handler آپلود: <span dir="ltr"><code>internal/netshare/server.go:99-154</code></span><br>
• ساخت فایل موقت: <span dir="ltr"><code>internal/netshare/server.go:113-125</code></span><br>
• دریافت داده فایل: <span dir="ltr"><code>internal/netshare/server.go:133</code></span><br>
• انتقال فایل موقت به نام نهایی بعد از کامل شدن آپلود: <span dir="ltr"><code>internal/netshare/server.go:146-153</code></span>
</p>

نتیجه: فایل ناقص وارد لیست فایل‌های اصلی نمی‌شود.

## ۷. دانلود فایل

کلاینت باید بتواند فایل را از لیست انتخاب کند و فایل نهایی بدون خرابی بازسازی شود.

<p>
• شروع دانلود در کلاینت: <span dir="ltr"><code>internal/netshare/protocol.go:238-270</code></span><br>
• ارسال درخواست دانلود: <span dir="ltr"><code>internal/netshare/protocol.go:251-253</code></span><br>
• دریافت اندازه فایل: <span dir="ltr"><code>internal/netshare/protocol.go:254-260</code></span><br>
• ساخت فایل خروجی و نوشتن داده: <span dir="ltr"><code>internal/netshare/protocol.go:261-270</code></span><br>
• ارسال فایل توسط سرور: <span dir="ltr"><code>internal/netshare/server.go:156-182</code></span>
</p>

## ۸. انتقال Chunk-Based

طبق صورت‌مسئله، فایل باید به بخش‌های کوچک‌تر تقسیم شود. در این پروژه فایل کامل وارد حافظه نمی‌شود و انتقال با buffer ثابت انجام می‌شود.

<p>
• اندازه chunk/buffer برابر <span dir="ltr"><code>64 KiB</code></span>: <span dir="ltr"><code>internal/netshare/protocol.go:21</code></span><br>
• کپی مرحله‌ای همراه با progress: <span dir="ltr"><code>internal/netshare/protocol.go:96-126</code></span><br>
• دریافت مرحله‌ای upload در سرور: <span dir="ltr"><code>internal/netshare/server.go:237-273</code></span>
</p>

## ۹. همزمانی در Phase 2

مهم‌ترین requirement فاز ۲، پذیرش چند کلاینت و پاسخ‌گویی همزمان است. در این پروژه، سرور برای هر اتصال یک goroutine جدا اجرا می‌کند.

<p>
• دریافت اتصال‌ها: <span dir="ltr"><code>internal/netshare/server.go:59-68</code></span><br>
• اجرای هر connection در goroutine: <span dir="ltr"><code>internal/netshare/server.go:66</code></span>
</p>

در PDF از چندپردازه بودن صحبت شده است. در این نسخه Go، به‌جای <span dir="ltr"><code>fork()</code></span> از goroutine استفاده شده؛ هدف عملی فاز ۲ یعنی سرویس‌دهی همزمان به چند کلاینت پوشش داده می‌شود.

## ۱۰. جلوگیری از تداخل فایل‌ها

برای جلوگیری از race condition، برای هر فایل یک قفل جدا استفاده شده است.

<p>
• نگهداری map قفل‌ها: <span dir="ltr"><code>internal/netshare/server.go:21-23</code></span><br>
• ساخت یا دریافت قفل فایل: <span dir="ltr"><code>internal/netshare/server.go:213-225</code></span><br>
• قفل نوشتن برای upload: <span dir="ltr"><code>internal/netshare/server.go:109-111</code></span><br>
• قفل خواندن برای download: <span dir="ltr"><code>internal/netshare/server.go:162-164</code></span><br>
• قفل نوشتن برای delete: <span dir="ltr"><code>internal/netshare/server.go:190-192</code></span>
</p>

نتیجه: چند دانلود می‌توانند همزمان انجام شوند، اما upload یا delete با عملیات ناسازگار تداخل پیدا نمی‌کند.

## ۱۱. حفظ صحت فایل

برای اینکه فایل دریافت‌شده با فایل اصلی یکسان باشد، چند کار انجام شده است:

<p>
• انتقال دقیق تعداد byte اعلام‌شده.<br>
• نوشتن upload در فایل موقت.<br>
• انتقال به نام نهایی فقط بعد از کامل شدن upload.<br>
• حذف فایل موقت در صورت شکست.<br>
• قفل‌گذاری برای جلوگیری از تداخل.
</p>

ارجاع‌های مهم:

<p>
• cleanup فایل موقت: <span dir="ltr"><code>internal/netshare/server.go:120-125</code></span><br>
• بررسی تعداد byteهای دریافت‌شده: <span dir="ltr"><code>internal/netshare/server.go:133-140</code></span><br>
• جایگزینی فایل نهایی: <span dir="ltr"><code>internal/netshare/server.go:146-153</code></span>
</p>

## ۱۲. مدیریت خطا

خطاهای سرور به‌صورت JSON response برگردانده می‌شوند و در UI با modal اختصاصی نمایش داده می‌شوند.

<p>
• ارسال خطای سرور: <span dir="ltr"><code>internal/netshare/server.go:202-205</code></span><br>
• modal خطا: <span dir="ltr"><code>internal/appui/dialog.go:14-26</code></span><br>
• تبدیل خطای فنی به پیام قابل‌فهم: <span dir="ltr"><code>internal/appui/dialog.go:28-42</code></span>
</p>

## ۱۳. رابط کاربری

کلاینت تب‌بندی شده است:

<p>
• Server: تنظیم IP و Port و وضعیت اتصال.<br>
• Storage: مسیر ذخیره دانلودها.<br>
• Upload: انتخاب و آپلود فایل.<br>
• Files: مشاهده و دانلود فایل‌های سرور.
</p>

ارجاع‌ها:

<p>
• تب‌های کلاینت: <span dir="ltr"><code>cmd/client/main.go:254-260</code></span><br>
• بخش upload: <span dir="ltr"><code>cmd/client/main.go:237-244</code></span><br>
• لیست فایل‌ها و دکمه دانلود: <span dir="ltr"><code>cmd/client/main.go:101-130</code></span>
</p>

سرور نیز تب‌بندی شده است:

<p>
• Server: Start/Stop و تنظیم endpoint.<br>
• Storage: پوشه فایل‌های سرور.<br>
• Files: مشاهده، حذف و کپی فایل‌ها.<br>
• Activity: لاگ عملیات.
</p>

ارجاع‌ها:

<p>
• تب‌های سرور: <span dir="ltr"><code>cmd/server/main.go:201-207</code></span><br>
• Start سرور: <span dir="ltr"><code>cmd/server/main.go:93-108</code></span><br>
• Stop سرور: <span dir="ltr"><code>cmd/server/main.go:109-115</code></span>
</p>

## ۱۴. تست‌ها

برای اثبات پوشش requirementها، تست‌های اصلی نوشته شده‌اند:

<p>
• انتقال همزمان و صحت فایل: <span dir="ltr"><code>internal/netshare/protocol_test.go:14-74</code></span><br>
• upload همزمان با نام یکسان: <span dir="ltr"><code>internal/netshare/protocol_test.go:76-126</code></span><br>
• نام فایل Unicode و فارسی: <span dir="ltr"><code>internal/netshare/protocol_test.go:128-168</code></span><br>
• upload و download فایل بزرگ: <span dir="ltr"><code>internal/netshare/protocol_test.go:170-198</code></span>
</p>

## ۱۵. جمع‌بندی

این پیاده‌سازی requirements اصلی Phase 1 را پوشش می‌دهد:

<p>
• اتصال کلاینت به سرور.<br>
• مشاهده لیست فایل‌ها.<br>
• آپلود فایل.<br>
• دانلود فایل.<br>
• انتقال chunk-based.<br>
• بازسازی صحیح فایل.
</p>

همچنین requirements اصلی Phase 2 را پوشش می‌دهد:

<p>
• پذیرش چند کلاینت همزمان.<br>
• پاسخ‌گویی همزمان به چند درخواست.<br>
• مدیریت چند فایل.<br>
• جلوگیری از race condition.<br>
• حفظ صحت فایل.<br>
• مدیریت خطاها.
</p>

در نتیجه ShareNet یک دمو کامل از سیستم اشتراک‌گذاری فایل Client-Server در شبکه داخلی است.

</div>
