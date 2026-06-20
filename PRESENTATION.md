<div dir="rtl" align="right" style="direction:rtl;text-align:right;font-family:Tahoma,Arial,sans-serif;line-height:1.9;">

<style>
body, div, p, h1, h2, h3, li, table, th, td { direction: rtl !important; text-align: right !important; }
table { width: 100%; border-collapse: collapse; margin: 12px 0; }
th, td { border: 1px solid #999; padding: 8px; vertical-align: top; }
th { background: #f2f2f2; }
.ltr, code { direction: ltr !important; unicode-bidi: isolate !important; display: inline-block; text-align: left !important; }
</style>

# گزارش  تطبیق ShareNet با فاز ۱ و فاز ۲

این گزارش فقط روی موارد خواسته‌شده در داکیومنت‌ها تمرکز دارد و برای هر مورد، محل پیاده‌سازی در کد را با **فایل، تابع و شماره خط** می‌آورد.

**نکته‌های مهم برای دفاع:**

- کد فعلی نسخه نهایی و ارتقایافته است؛ بنابراین منطق فاز ۱ داخل همین نسخه وجود دارد، اما سرور دیگر ترتیبی نیست و در فاز ۲ همزمان شده است.
- در فاز ۲، همزمانی با <span class="ltr">goroutine</span> انجام شده، نه با <span class="ltr">fork/process</span> واقعی سیستم‌عاملی.
- فایل‌ها به‌صورت stream و با buffer ثابت ۶۴KiB منتقل می‌شوند؛ یعنی chunking عملی انجام شده، ولی header جدا با <span class="ltr">chunk_index</span> و <span class="ltr">total_chunks</span> پیاده نشده است.

---

# فاز ۱: نسخه پایه Client-Server

در فاز ۱ باید یک سیستم Client-Server پایه وجود داشته باشد: سرور فایل‌ها را نگه دارد، کلاینت وصل شود، لیست فایل‌ها را بگیرد، فایل آپلود کند، فایل دانلود کند، انتقال به بخش‌های کوچک انجام شود و فایل نهایی سالم بازسازی شود.

<table>
<thead>
<tr>
<th>خواسته فاز ۱</th>
<th>نحوه پیاده‌سازی</th>
<th>ارجاع دقیق به کد</th>
</tr>
</thead>
<tbody>
<tr>
<td>اجرای برنامه روی لینوکس</td>
<td>برای ساخت نسخه لینوکس، اسکریپت build وجود دارد و خروجی سرور و کلاینت ساخته می‌شود.</td>
<td><span class="ltr">scripts/build-linux.sh:1-18</span><br>خروجی‌ها: <span class="ltr">scripts/build-linux.sh:16-17</span><br>راهنما: <span class="ltr">README.md:10-17</span> و <span class="ltr">README.md:25-37</span></td>
</tr>
<tr>
<td>معماری Server-Client و اتصال کلاینت</td>
<td>سرور با TCP listener اجرا می‌شود و کلاینت با آدرس IP/Port به آن وصل می‌شود.</td>
<td>تابع سرور: <span class="ltr">Server.Start</span> در <span class="ltr">internal/netshare/server.go:26-40</span><br>تابع اتصال کلاینت: <span class="ltr">dial</span> در <span class="ltr">internal/netshare/protocol.go:157-167</span><br>ساخت آدرس در UI: <span class="ltr">cmd/client/main.go:53-55</span></td>
</tr>
<tr>
<td>مشاهده لیست فایل‌ها</td>
<td>کلاینت درخواست لیست می‌فرستد؛ سرور پوشه فایل‌ها را می‌خواند و نام، اندازه و زمان تغییر فایل را برمی‌گرداند.</td>
<td>ساختار فایل: <span class="ltr">FileInfo</span> در <span class="ltr">internal/netshare/protocol.go:36-41</span><br>تابع خواندن فایل‌ها: <span class="ltr">ListLocalFiles</span> در <span class="ltr">internal/netshare/protocol.go:128-155</span><br>درخواست کلاینت: <span class="ltr">List</span> در <span class="ltr">internal/netshare/protocol.go:169-186</span><br>پاسخ سرور: <span class="ltr">server.go:81-87</span><br>نمایش در UI: <span class="ltr">cmd/client/main.go:133-152</span></td>
</tr>
<tr>
<td>اشتراک‌گذاری / آپلود فایل</td>
<td>کاربر فایل را انتخاب می‌کند، کلاینت آن را به سرور می‌فرستد، سرور ابتدا فایل موقت می‌سازد و بعد از کامل شدن آن را جایگزین فایل نهایی می‌کند.</td>
<td>انتخاب فایل: <span class="ltr">cmd/client/main.go:154-177</span><br>فراخوانی آپلود: <span class="ltr">cmd/client/main.go:179-203</span><br>تابع کلاینت: <span class="ltr">Upload</span> در <span class="ltr">internal/netshare/protocol.go:188-236</span><br>تابع سرور: <span class="ltr">handleUpload</span> در <span class="ltr">internal/netshare/server.go:99-154</span><br>فایل موقت: <span class="ltr">server.go:113-125</span><br>جایگزینی نهایی: <span class="ltr">server.go:146-153</span></td>
</tr>
<tr>
<td>انتخاب و دانلود فایل</td>
<td>در لیست فایل‌ها برای هر فایل دکمه دانلود وجود دارد؛ با کلیک، تابع دانلود کلاینت اجرا می‌شود و سرور فایل را ارسال می‌کند.</td>
<td>دکمه دانلود در UI: <span class="ltr">cmd/client/main.go:101-130</span><br>تابع داخلی دانلود UI: <span class="ltr">downloadFile</span> در <span class="ltr">cmd/client/main.go:80-99</span><br>تابع کلاینت: <span class="ltr">Download</span> در <span class="ltr">internal/netshare/protocol.go:238-270</span><br>تابع سرور: <span class="ltr">handleDownload</span> در <span class="ltr">internal/netshare/server.go:156-182</span></td>
</tr>
<tr>
<td>تقسیم فایل به chunk</td>
<td>کد فایل را یک‌جا در حافظه نمی‌خواند؛ انتقال با buffer ثابت ۶۴KiB انجام می‌شود. این همان chunking عملی است، اما header پیشنهادی داکیومنت برای هر chunk وجود ندارد.</td>
<td>اندازه buffer: <span class="ltr">chunkSize</span> در <span class="ltr">internal/netshare/protocol.go:21</span><br>کپی مرحله‌ای: <span class="ltr">copyWithProgress</span> در <span class="ltr">internal/netshare/protocol.go:96-126</span><br>دریافت مرحله‌ای آپلود در سرور: <span class="ltr">receiveUpload</span> در <span class="ltr">internal/netshare/server.go:237-273</span></td>
</tr>
<tr>
<td>بازسازی فایل نهایی در سمت کلاینت</td>
<td>کلاینت هنگام دانلود فایل خروجی را می‌سازد و دقیقاً به اندازه اعلام‌شده از stream می‌خواند و روی دیسک می‌نویسد.</td>
<td>ساخت مسیر و فایل خروجی: <span class="ltr">internal/netshare/protocol.go:261-266</span><br>خواندن محدود و نوشتن فایل: <span class="ltr">internal/netshare/protocol.go:267-270</span><br>نمایش مسیر ذخیره‌شده: <span class="ltr">cmd/client/main.go:93-96</span></td>
</tr>
<tr>
<td>صحت فایل</td>
<td>در دانلود از محدودیت اندازه استفاده شده، در آپلود تعداد بایت‌ها بررسی می‌شود، و تست‌ها فایل نهایی را byte-to-byte با فایل اصلی مقایسه می‌کنند.</td>
<td>دانلود با <span class="ltr">io.LimitReader</span>: <span class="ltr">internal/netshare/protocol.go:267</span><br>بررسی تعداد بایت آپلود: <span class="ltr">internal/netshare/server.go:133-140</span><br>commit بعد از کامل شدن: <span class="ltr">server.go:146-153</span><br>تست فایل بزرگ: <span class="ltr">internal/netshare/protocol_test.go:170-198</span></td>
</tr>
<tr>
<td>مدیریت خطاها</td>
<td>سرور خطا را در پاسخ JSON برمی‌گرداند و کلاینت آن را در dialog نشان می‌دهد.</td>
<td>ساختار پاسخ: <span class="ltr">response</span> در <span class="ltr">internal/netshare/protocol.go:49-54</span><br>ارسال خطا: <span class="ltr">writeError</span> در <span class="ltr">internal/netshare/server.go:202-205</span><br>نمایش خطا: <span class="ltr">internal/appui/dialog.go:14-42</span></td>
</tr>
<tr>
<td>رفتار ترتیبی سرور در فاز ۱</td>
<td>این مورد در نسخه فعلی مطابق فاز ۱ نیست، چون کد نهایی برای فاز ۲ همزمان شده است. خط کلیدی همزمانی، اجرای <span class="ltr">go s.handle(conn)</span> است. برای حالت ترتیبی باید همین خط بدون <span class="ltr">go</span> اجرا شود.</td>
<td>حلقه پذیرش اتصال: <span class="ltr">acceptLoop</span> در <span class="ltr">internal/netshare/server.go:59-68</span><br>خط همزمان‌سازی: <span class="ltr">internal/netshare/server.go:66</span></td>
</tr>
<tr>
<td>دمو ساده</td>
<td>مراحل اجرای سرور و کلاینت در Runbook آمده است.</td>
<td>سرور: <span class="ltr">RUNBOOK.md:23-35</span><br>کلاینت: <span class="ltr">RUNBOOK.md:36-44</span></td>
</tr>
</tbody>
</table>

## پروتکل فاز ۱ در کد

پروتکل به‌صورت JSON line پیاده شده است. هر درخواست فیلد <span class="ltr">type</span> دارد و پاسخ سرور شامل وضعیت موفقیت، خطا، لیست فایل‌ها یا اندازه فایل است.

<table>
<thead>
<tr>
<th>مفهوم پروتکل</th>
<th>ارجاع</th>
</tr>
</thead>
<tbody>
<tr>
<td>فیلد <span class="ltr">type</span> در درخواست</td>
<td><span class="ltr">request</span> در <span class="ltr">internal/netshare/protocol.go:43-47</span></td>
</tr>
<tr>
<td>ساختار پاسخ سرور</td>
<td><span class="ltr">response</span> در <span class="ltr">internal/netshare/protocol.go:49-54</span></td>
</tr>
<tr>
<td>نوشتن و خواندن JSON</td>
<td><span class="ltr">writeJSONLine</span>: <span class="ltr">protocol.go:78-86</span><br><span class="ltr">readJSONLine</span>: <span class="ltr">protocol.go:88-94</span></td>
</tr>
<tr>
<td>درخواست لیست، آپلود و دانلود</td>
<td>ثابت‌ها: <span class="ltr">protocol.go:25-34</span><br>لیست: <span class="ltr">protocol.go:169-186</span><br>آپلود: <span class="ltr">protocol.go:188-236</span><br>دانلود: <span class="ltr">protocol.go:238-270</span></td>
</tr>
</tbody>
</table>

---

# فاز ۲: نسخه همزمان و چندکلاینتی

در فاز ۲ باید سرور بتواند چند کلاینت و چند درخواست را همزمان مدیریت کند، چند فایل مختلف را کنترل کند و در عین همزمانی، صحت فایل‌ها حفظ شود.

<table>
<thead>
<tr>
<th>خواسته فاز ۲</th>
<th>نحوه پیاده‌سازی</th>
<th>ارجاع دقیق به کد</th>
</tr>
</thead>
<tbody>
<tr>
<td>اتصال همزمان چند کلاینت</td>
<td>سرور هر connection جدید را قبول می‌کند و پردازش آن را در یک goroutine جدا انجام می‌دهد.</td>
<td>تابع <span class="ltr">acceptLoop</span>: <span class="ltr">internal/netshare/server.go:59-68</span><br>خط اصلی: <span class="ltr">go s.handle(conn)</span> در <span class="ltr">server.go:66</span></td>
</tr>
<tr>
<td>چندپردازه بودن سرور</td>
<td>از نظر رفتاری، سرور همزمان است؛ اما از نظر دقیق سیستم‌عامل، چندپردازه واقعی نیست و از goroutine استفاده می‌کند.</td>
<td>ایجاد goroutine برای هر کلاینت: <span class="ltr">internal/netshare/server.go:66</span></td>
</tr>
<tr>
<td>پاسخ‌گویی همزمان به چند درخواست</td>
<td>هر connection در <span class="ltr">handle</span> جدا dispatch می‌شود و عملیات list، upload، download یا delete را اجرا می‌کند.</td>
<td>تابع <span class="ltr">handle</span>: <span class="ltr">internal/netshare/server.go:70-97</span><br>تشخیص نوع درخواست: <span class="ltr">server.go:78-95</span></td>
</tr>
<tr>
<td>آپلود همزمان و جلوگیری از خراب شدن فایل</td>
<td>برای هر نام فایل lock جدا وجود دارد. در upload قفل نوشتن گرفته می‌شود، فایل ابتدا موقت ذخیره می‌شود و بعد replace می‌شود.</td>
<td>قفل upload: <span class="ltr">internal/netshare/server.go:109-111</span><br>فایل موقت: <span class="ltr">server.go:113-125</span><br>replace نهایی: <span class="ltr">server.go:146-153</span><br>تابع lock: <span class="ltr">fileLock</span> در <span class="ltr">server.go:213-225</span><br>تست upload هم‌نام: <span class="ltr">protocol_test.go:76-126</span></td>
</tr>
<tr>
<td>دانلود همزمان</td>
<td>در دانلود از <span class="ltr">RLock</span> استفاده شده؛ بنابراین چند دانلود از یک فایل همزمان مجاز است، ولی با upload/delete همان فایل تداخل نمی‌کند.</td>
<td>قفل خواندن: <span class="ltr">internal/netshare/server.go:162-164</span><br>ارسال فایل: <span class="ltr">server.go:177-181</span><br>تست دانلود همزمان: <span class="ltr">protocol_test.go:48-64</span></td>
</tr>
<tr>
<td>مدیریت چند فایل</td>
<td>برای هر نام فایل، lock جدا ساخته می‌شود؛ بنابراین عملیات روی فایل‌های مختلف مستقل‌تر اجرا می‌شوند.</td>
<td>فیلدهای lock: <span class="ltr">internal/netshare/server.go:21-23</span><br>تابع <span class="ltr">fileLock</span>: <span class="ltr">server.go:213-225</span><br>استفاده در upload: <span class="ltr">server.go:109-111</span><br>استفاده در download: <span class="ltr">server.go:162-164</span><br>استفاده در delete: <span class="ltr">server.go:190-192</span></td>
</tr>
<tr>
<td>لیست فایل‌ها برای هر کلاینت</td>
<td>هر کلاینت مستقل می‌تواند <span class="ltr">REQUEST_LIST</span> بفرستد و پاسخ لیست فایل‌ها را بگیرد.</td>
<td>تابع کلاینت: <span class="ltr">List</span> در <span class="ltr">internal/netshare/protocol.go:169-186</span><br>پاسخ سرور: <span class="ltr">internal/netshare/server.go:81-87</span><br>Refresh UI: <span class="ltr">cmd/client/main.go:133-152</span><br>تست list همزمان: <span class="ltr">protocol_test.go:37-47</span></td>
</tr>
<tr>
<td>حفظ صحت فایل در همزمانی</td>
<td>قفل فایل‌ها، فایل موقت، بررسی تعداد بایت و تست‌های همزمانی باعث می‌شوند فایل نهایی خراب یا ترکیبی نشود.</td>
<td>تست اصلی همزمانی و صحت: <span class="ltr">TestConcurrentTransfersKeepFileIntegrity</span> در <span class="ltr">internal/netshare/protocol_test.go:14-74</span><br>مقایسه byte-to-byte: <span class="ltr">protocol_test.go:56-63</span><br>بررسی تعداد بایت آپلود: <span class="ltr">server.go:133-140</span></td>
</tr>
<tr>
<td>دموی نهایی</td>
<td>Runbook مراحل اجرای سرور، کلاینت، آپلود، لیست و دانلود را توضیح می‌دهد.</td>
<td>سرور: <span class="ltr">RUNBOOK.md:23-35</span><br>کلاینت: <span class="ltr">RUNBOOK.md:36-44</span></td>
</tr>
</tbody>
</table>

## جمع‌بندی   

در این پروژه، بخش‌های اصلی فاز ۱ یعنی اتصال Client-Server، لیست فایل‌ها، آپلود، دانلود، انتقال مرحله‌ای فایل و بازسازی صحیح پیاده‌سازی شده‌اند. سپس برای فاز ۲، سرور همزمان شده و برای هر اتصال، <span class="ltr">go s.handle(conn)</span> اجرا می‌شود. برای جلوگیری از race condition نیز برای هر فایل یک <span class="ltr">RWMutex</span> جدا استفاده شده است. تنها نکته‌هایی که باید شفاف گفته شوند این است که پیاده‌سازی فاز ۲ با goroutine انجام شده، نه process واقعی، و chunking با stream/buffer ثابت پیاده شده، نه با header جداگانه برای هر chunk.

</div>
