# Smart Greenhouse Firmware (Mono-File)

Firmware ESP32 untuk smart greenhouse cabai merah dalam **satu file Arduino**
(`firmware-mono/firmware-mono.ino`). Perangkat membaca suhu/kelembapan udara dengan **DHT22**,
tiga probe kelembapan tanah kapasitif, memutuskan misting/blower/irigasi lewat
fuzzy logic, lalu mengirim telemetri ke Firebase Realtime Database.

Repo ini adalah varian mono-file dari
[smartgh-firmware](https://github.com/will-zx/smartgh-firmware). Logika kontrol
utama sama; perbedaannya adalah seluruh kode digabung ke satu `.ino` agar bisa
dibuka langsung di Arduino IDE.

## Fitur

- DHT22 untuk suhu dan kelembapan udara (GPIO digital, bukan I2C).
- Tiga probe soil kapasitif di ADC1 dengan validasi per probe.
- Filter EMA untuk suhu, kelembapan, dan soil.
- Perhitungan VPD (Tetens) dan ETc (Blaney-Criddle + Kc fase).
- Kontrol fuzzy Mamdani untuk misting, blower, dan izin irigasi.
- Override manual per aktuator dari dashboard: auto, paksa nyala, paksa mati.
- Siklus fertigasi empat pompa: FILL -> DOSE_A -> DOSE_B -> SETTLE -> DIST.
- Telemetri Firebase RTDB dan polling perintah.
- LCD 16x2 I2C dengan baris status berjalan (marquee).
- Backup log MicroSD dan antrean saat push Firebase gagal.
- Dua task FreeRTOS: `controlTask` (core 1) dan `netTask` (core 0).

## Kebutuhan library (Arduino IDE)

Pasang lewat **Library Manager**:

| Library | Kegunaan |
| --- | --- |
| **DHT sensor library** (Adafruit) | Driver DHT22 |
| **Adafruit Unified Sensor** | Dependensi DHT sensor library |
| **ArduinoJson** (Benoit Blanchon, v7) | Serialisasi JSON Firebase |
| **LiquidCrystal_I2C** (marcoschwartz) | LCD I2C |

Library berikut sudah termasuk dalam board package ESP32: `WiFi`,
`WiFiClientSecure`, `HTTPClient`, `Preferences`, `SD`, `Wire`, `SPI`.

Board package: **esp32 by Espressif Systems** (ESP32 Dev Module).

## Hardware dan wiring

Mikrokontroler: ESP32 DevKit v1.

### Sensor

| Sensor | Antarmuka | Pin | Catatan |
| --- | --- | --- | --- |
| DHT22 (suhu/RH) | Digital one-wire | GPIO 4 | Butuh pull-up 4,7-10 kOhm ke 3,3 V bila modul belum punya; jeda baca minimal 2 detik |
| Probe soil 1 | Analog ADC1 | GPIO 34 | Kapasitif, input-only |
| Probe soil 2 | Analog ADC1 | GPIO 33 | Kapasitif |
| Probe soil 3 | Analog ADC1 | GPIO 39 | Kapasitif, input-only |

DHT22 disuplai **3,3 V**. DATA ke GPIO 4. Jangan gunakan pin ini untuk input
analog saat WiFi aktif karena GPIO 4 termasuk ADC2.

Semua probe soil wajib di **ADC1**. Saat WiFi aktif, ADC2 (GPIO 0, 2, 4,
12-15, 25-27) tidak dapat dipakai untuk pembacaan analog. Probe soil menempati
GPIO 34/33/39; pompa DIST memakai GPIO 32 sebagai output digital.

### Aktuator

| Aktuator | Pin | Sinyal | Fungsi |
| --- | --- | --- | --- |
| Misting | GPIO 26 | MOSFET (XY-MOS) | Penyiram kabut |
| Blower (2 kipas paralel) | GPIO 25 | MOSFET PWM (XY-MOS) | Ventilasi |
| FILL | GPIO 27 | MOSFET | Tandon air -> tandon pencampuran |
| DOSE_A | GPIO 13 | MOSFET | Stok pupuk A -> tandon pencampuran |
| DOSE_B | GPIO 14 | MOSFET | Stok pupuk B -> tandon pencampuran |
| DIST | GPIO 32 | MOSFET | Tandon pencampuran -> tanaman |

Aktuator memakai modul **XY-MOS/MOSFET**. Blower memakai PWM LEDC 1 kHz
(`BLOWER_FREQ`, `BLOWER_RES 8`) dengan duty minimum 20 % (`BLOWER_MIN_DUTY`).
Ubah `MOSFET_INVERTED` menjadi `1` di bagian config bila modul aktif-LOW.

### LCD, I2C, dan MicroSD

| Perangkat | Pin / alamat |
| --- | --- |
| LCD I2C 16x2 | SDA GPIO 21, SCL GPIO 22, alamat `0x27` |
| MicroSD (SPI) | CS GPIO 5, SCK GPIO 18, MISO GPIO 19, MOSI GPIO 23 |

LCD dan DHT22 tidak berbagi bus: DHT22 memakai GPIO digital, sedangkan I2C hanya
untuk LCD. Kartu MicroSD sebaiknya FAT32. Tanpa kartu SD, firmware tetap jalan
tetapi tidak menyimpan backup log lokal.

## Konfigurasi

Semua konfigurasi berada di bagian atas `firmware-mono/firmware-mono.ino`.

### WiFi dan Firebase

Nilai yang sekarang ada di source:

```cpp
#define WIFI_SSID       "jon"
#define WIFI_PASSWORD   "kkkkkkkk"
```

Kredensial Firebase masih kosong dan harus diisi sebelum perangkat dapat
mengirim data atau menerima perintah:

```cpp
#define FIREBASE_API_KEY ""
#define DEVICE_ID        ""
#define DEVICE_EMAIL     ""
#define DEVICE_PASSWORD  ""
```

`WIFI_SSID` harus jaringan **2,4 GHz**; ESP32 tidak mendukung 5 GHz.

### Kalibrasi soil

Urutan nilai mengikuti `SOIL_PINS = {34, 33, 39}`. Modul kapasitif membaca
terbalik: kondisi kering menghasilkan nilai ADC lebih tinggi, basah lebih rendah.

Nilai awal di source:

```cpp
#define SOIL_DRY_CAL    {2509, 3231, 3255}
#define SOIL_WET_CAL    {913, 1012, 1050}
#define SOIL_RAW_TOL    300
```

Prosedur kalibrasi per probe:

1. Biarkan probe di udara kering selama kurang lebih 3 menit, lalu baca nilai
   `raw` pada log serial atau LCD.
2. Rendam probe penuh di air, lalu baca nilai `raw` lagi.
3. Isi `SOIL_DRY_CAL` dengan bacaan kering dan `SOIL_WET_CAL` dengan bacaan
   basah untuk probe yang sama.
4. Ulangi untuk setiap probe. Jangan memakai satu angka untuk semua probe karena
   dua modul dengan model sama dapat berbeda ratusan count.

`SOIL_RAW_TOL` adalah jaring pengaman: pembacaan di luar rentang kalibrasi
ditandai tidak valid, sehingga probe tercabut tidak terbaca sebagai 0 % atau
100 % palsu.

### Debit pompa dan fertigasi

Nilai debit saat ini:

| Pompa | Debit |
| --- | --- |
| FILL | 125,36 L/jam |
| DOSE_A | 15,79 L/jam |
| DOSE_B | 15,79 L/jam |
| DIST | 125,36 L/jam |

Koreksi per pompa tersedia lewat `FILL_FLOW_CORRECTION`,
`DOSE_A_FLOW_CORRECTION`, `DOSE_B_FLOW_CORRECTION`, dan
`DIST_FLOW_CORRECTION`. Kalibrasi ulang dengan mengukur volume keluaran nyata
pada instalasi akhir.

Dosis nutrisi Hydro J aktif (`FERTILIZER_DOSE_ENABLED 1`) dengan panduan produk:
Semai/Awal 3 mL/L, Vegetatif 5 mL/L, Generatif/Pembungaan/Buah/Panen 7 mL/L.
Verifikasi produk dan stok sebelum dipakai. Batas perangkat lunak: volume siklus
maksimum 18.000 mL, dosis stok A/B maksimum 500 mL, kapasitas tangki campuran
18.000 mL, dan kapasitas stok A/B masing-masing 6.216 mL. Tidak ada sensor level
fisik, jadi luapan harus dicegah secara mekanis oleh operator.

### Area dan fase tanaman

- Luas area irigasi: `IRRIG_AREA_CM2 9000.0f` (10 polybag 30x30 cm).
- Siklus irigasi per hari: `IRRIG_CYCLES_PER_DAY 2`.
- Gate irigasi otomatis: `IRRIG_AUTO_ENABLED 1`.
- Kc fase: Awal 0,40; Vegetatif 0,75; Pembungaan 1,10; Pemuliaan buah 1,00;
  Panen 0,90.
- Fase default: Vegetatif, VPD target 1,0 kPa.
- Batas override manual: 10 menit (`MANUAL_MAX_MS`).

## Cara kerja

```text
controlTask (core 1, tiap ~2 detik)
  baca DHT22 + 3 probe soil
  -> filter EMA
  -> VPD dan ETc
  -> fuzzy decision
  -> koreksi setpoint VPD fase
  -> override manual
  -> MOSFET aktuator + state machine fertigasi
  -> LCD + log MicroSD
  -> snapshot untuk netTask

netTask (core 0)
  -> WiFi + login Firebase
  -> push sensor_logs tiap 20 detik
  -> poll commands tiap 5 detik
  -> flush backlog MicroSD
```

Volume irigasi dihitung dari ETc:

```text
ETc = Kc x p x (0,46 x Tmean + 8)
anggaran_harian_ml = ETc x luas_cm2 x 0,1 / efisiensi
dosis_ml = anggaran_harian_ml / siklus_per_hari
durasi_pompa_ms = dosis_ml / debit_ml_per_detik x 1000
```

`Tmean` adalah rata-rata suhu 24 jam dalam ember 1 menit. Sebelum jendela
24 jam terisi penuh, ETc memakai suhu EMA terakhir yang masih segar sebagai
Tmean sementara dan menandainya `tmean_fallback`; fallback kedaluwarsa setelah
5 menit tanpa sampel valid. Permintaan irigasi manual yang ditahan gate
otomatis dilepas kembali ke AUTO.

## Fail-safe

- DHT22 tidak valid atau data di luar rentang membuat status menjadi
  `Sensor Error`.
- Saat sensor tidak valid, keputusan otomatis (fuzzy) dimatikan, tetapi override
  manual misting/blower/irigasi tetap dihormati sampai kedaluwarsa.
- Irigasi manual saat sensor error tetap melewati gate keamanan `irrigationUpdate`
  (sinkronisasi waktu, Tmean, kapasitas tangki, volume, dan dosis).
- Override manual kedaluwarsa setelah 10 menit dan aktuator kembali ke fuzzy.
- Task watchdog dinonaktifkan pada varian mono ini untuk simulasi
  (`esp_task_wdt_deinit()` di `setup()`).

## Firebase

Telemetri ditulis ke:

```text
/zones/{DEVICE_ID}/sensor_logs
```

Perintah dibaca dari:

```text
/commands/{DEVICE_ID}
```

Aksi perintah yang dikenali: `mist`, `irigasi`, `blower`, `set_blower_duty`, `auto`,
`restart`, `set_vpd`, `set_fase`, `set_durasi`, dan `fase_reset`. Untuk aktuator,
field `mode` menerima `on`, `off`, atau `auto`. `set_blower_duty` memakai
`value` angka 0-100 (persen). Field `value` lama tetap didukung.

Field telemetri utama: `temperature`, `humidity`, `soil`, `soil2`, `soil3`,
`soil_ok`, `soil_n`, `vpd`, `etc`, `tmean_c`, `blower_level`, `misting`,
`irigasi`, `status_sistem`, `manual_mode`, status override per aktuator, fase,
VPD target, serta detail dosis dan durasi irigasi. Nilai yang tidak valid tidak
dikirim ke Firebase.

## Build dan upload

1. Buka `firmware-mono/firmware-mono.ino` di Arduino IDE.
2. Pilih board **ESP32 Dev Module**.
3. Pasang library pada bagian kebutuhan library.
4. Isi kredensial Firebase dan pastikan WiFi benar.
5. Verifikasi/compile, lalu upload.
6. Buka Serial Monitor pada **115200 baud**.

Saat boot, cek baris berikut:

```text
=== Smart Watering System ===
[sensor] DHT22 mulai pada GPIO 4
[fase] ...
```

Jika DHT22 gagal dibaca, firmware akan menampilkan pesan untuk memeriksa wiring
dan pull-up.

## Troubleshooting

| Gejala | Pemeriksaan |
| --- | --- |
| `Sensor Error` terus muncul | Cek DHT22 di GPIO 4, suplai 3,3 V, dan pull-up DATA |
| Suhu/RH kosong di dashboard | DHT22 gagal dibaca; field tidak dikirim saat nilai tidak valid |
| Soil selalu 0 % atau 100 % | Cek polaritas dan kalibrasi `SOIL_DRY_CAL`/`SOIL_WET_CAL` |
| LCD menyala tetapi teks kosong | Atur potensiometer kontras; cek alamat I2C `0x27` |
| Gagal konek WiFi | Gunakan SSID 2,4 GHz, cek password, dan jangkauan sinyal |
| Data tidak masuk Firebase | Isi `FIREBASE_API_KEY`, `DEVICE_ID`, `DEVICE_EMAIL`, `DEVICE_PASSWORD` |
| Tidak ada backup log | Pastikan kartu MicroSD FAT32 dan wiring SPI benar |
| Pompa/aktuator terbalik | Ubah `MOSFET_INVERTED` sesuai modul MOSFET |

## Struktur repo

```text
firmware-mono/firmware-mono.ino   Firmware lengkap dalam satu file untuk Arduino IDE
README.md           Dokumentasi ini
```

## Keamanan

SSID dan password WiFi saat ini ditulis langsung di source sesuai kebutuhan
repo ini. Kredensial Firebase akun perangkat juga akan berada di file yang sama
setelah diisi. Siapa pun yang dapat membaca repo dapat melihat nilai tersebut,
jadi gunakan akun perangkat khusus dan batasi izinnya di Firebase.
