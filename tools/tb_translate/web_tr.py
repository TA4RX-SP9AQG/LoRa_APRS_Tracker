"""Turkish version of the web configuration page (data_embed/index.html), applied at build time.

tools/compress.py calls translate_html() when the firmware is built with -D TB_LANG_TR. Only whole text nodes and a
few attributes that match a key exactly are replaced, so ids, names, scripts and styles can never be changed.
"""
import re

TEXT = {
    "LoRa Tracker Software &minus; Ricardo Guzman CA2RXU": "LoRa Tracker Yazılımı &minus; Ricardo Guzman CA2RXU",
    "Save": "Kaydet",
    "Configuration": "Yapılandırma",
    "Beaconing": "Beacon Gönderimi",
    "Station": "İstasyon",
    "Connectivity": "Bağlantı",
    "Hardware": "Donanım",
    "Telemetry": "Telemetri",
    "Administration": "Yönetim",
    "About / Info": "Hakkında / Bilgi",
    "Beacons": "Beacon Profilleri",
    "Add your personal configuration for each Callsign of Tracker Station.": "Tracker istasyonunun her çağrı işareti için kendi ayarlarınızı girin.",
    "Path and timing used to send the GPS beacons.": "GPS beacon'larını göndermek için kullanılan yol (path) ve zamanlama.",
    "Path": "Yol (Path)",
    "Send Comment After X Beacons": "Her X beacon'da bir yorum gönder",
    "GPS Beacons": "GPS Beacon'ları",
    "Fixed Beacon Rate": "Sabit Beacon Aralığı",
    "minutes": "dakika",
    "Standing Update Time": "Hareketsizken Güncelleme Süresi",
    "Station Config": "İstasyon Ayarları",
    "Additional station configuration.": "Ek istasyon ayarları.",
    "Email": "E-posta",
    "(to send GPS position)": "(GPS konumunu göndermek için)",
    "Remember Station Time": "İstasyonu Hatırlama Süresi",
    "Send Encoded Altitude": "Kodlanmış Yüksekliği Gönder",
    "(Instead of Encoded Velocity+Course)": "(Kodlanmış hız+yön yerine)",
    "Disable GPS": "GPS'i Devre Dışı Bırak",
    "(For non moving Stations/TNC)": "(Hareket etmeyen istasyonlar/TNC için)",
    "Simplified Tracker Mode": "Basit Tracker Modu",
    "(Disables Buttons + Menu and only sends GPS Beacon)": "(Düğmeleri ve menüyü kapatır, yalnızca GPS beacon'ı gönderir)",
    "Add your password to access Winlink.": "Winlink'e erişmek için parolanızı girin.",
    "Password": "Parola",
    "Remember to match your boards posible Frequencies, SF, CR4.": "Kartınızın desteklediği frekans, SF ve CR4 değerleriyle eşleştirmeyi unutmayın.",
    "Power: SX127X Max 20dBm / SX126X Max 22dBm.": "Güç: SX127X en çok 20 dBm / SX126X en çok 22 dBm.",
    "Channel Activity Detection (CAD)": "Kanal Etkinliği Algılama (CAD)",
    "(Listen before transmitting to avoid collisions)": "(Çakışmayı önlemek için göndermeden önce dinler)",
    "BT Classic or BLE Configuration.": "BT Classic veya BLE ayarı.",
    "Enable Bluetooth": "Bluetooth'u Etkinleştir",
    "Bluetooth Device Name": "Bluetooth Cihaz Adı",
    "Bluetooth Type": "Bluetooth Türü",
    "BT Classic is only available on boards that support it, otherwise BLE is always used.": "BT Classic yalnızca onu destekleyen kartlarda vardır, aksi halde her zaman BLE kullanılır.",
    "Protocol": "Protokol",
    "WiFi AP": "WiFi Erişim Noktası",
    "Add your password to access WiFi AP.": "WiFi erişim noktasına bağlanmak için parola girin.",
    "Display": "Ekran",
    "Screen configuration.": "Ekran ayarları.",
    "Enable Display EcoMode": "Ekran Eko Modunu Etkinleştir",
    "Turn 180º": "180º Döndür",
    "Display Timeout (for EcoMode)": "Ekran Zaman Aşımı (Eko Mod için)",
    "seconds": "saniye",
    "Show Symbol Drawing on Screen": "Ekranda Sembol Çizimini Göster",
    "Battery": "Pil",
    "BATTERY Information on GPS Beacon Configuration": "GPS beacon'ında pil bilgisi ayarları",
    "Send Voltage in GPS Beacon": "GPS Beacon'ında Gerilimi Gönder",
    "Send as Encoded Telemetry": "Kodlanmış Telemetri Olarak Gönder",
    "Force Sending Voltage Info on every GPS Beacon": "Gerilim bilgisini her GPS beacon'ında zorla gönder",
    "Monitor Battery Voltage and power-off/sleep if low": "Pil gerilimini izle, düşükse kapat/uyut",
    "Voltage power-off/sleep": "Kapanma/uyku gerilimi",
    "(per cell, T-Beam 1W has 2 cells)": "(hücre başına, T-Beam 1W'ta 2 hücre var)",
    "Volts": "Volt",
    "Notifications": "Bildirimler",
    "Tx Led Active": "Tx Ledi Etkin",
    "Messsage Rx Led Active": "Mesaj Rx Ledi Etkin",
    "Pin used for Tx Led": "Tx Ledi için pin",
    "Pin": "Pin",
    "Pin used for Message Rx Led": "Mesaj Rx Ledi için pin",
    "Buzzer Active": "Buzzer Etkin",
    "Flashlight Pin Active": "Fener Pini Etkin",
    "Buzzer Tone Pin": "Buzzer Ton Pini",
    "Pin used for Flashlight Pin": "Fener için pin",
    "Buzzer Vcc Pin": "Buzzer Vcc Pini",
    "Boot Up Beep": "Açılış Sesi",
    "Tx Beep": "Tx Sesi",
    "Message Rx Beep": "Mesaj Alma Sesi",
    "Station Rx Beep": "İstasyon Alma Sesi",
    "Low Battery Beep": "Düşük Pil Sesi",
    "Shutdown Beep": "Kapanış Sesi",
    "PTT Trigger": "PTT Tetikleyici",
    "Enable PTT Trigger": "PTT Tetikleyiciyi Etkinleştir",
    "PTT in Reverse Mode": "Ters Modda PTT",
    "Time to enable trigger before Tx": "Tx'ten önce tetiği açma süresi",
    "miliseconds": "milisaniye",
    "Time to disable trigger after Tx": "Tx'ten sonra tetiği kapatma süresi",
    "Pin used for external Amp trigger": "Harici yükselteç tetiği için pin",
    "Telemetry data to be available on Screen, as Telemetry APRS Packet or as APRS Wx Packet": "Telemetri verisi ekranda, telemetri APRS paketi olarak ya da APRS Wx paketi olarak kullanılabilir",
    "Activate Sensor": "Sensörü Etkinleştir",
    "(Requires BME280/BMP280/BME680/SHTC3 sensor)": "(BME280/BMP280/BME680/SHTC3 sensörü gerekir)",
    "Send Telemetry via LoRa APRS": "Telemetriyi LoRa APRS ile Gönder",
    "Temperature Correction": "Sıcaklık Düzeltmesi",
    "Celsius": "Santigrat",
    "Backup": "Yedek",
    "Download the current configuration as a JSON file, or restore a previous one.": "Geçerli ayarları JSON dosyası olarak indirin ya da önceki bir yedeği geri yükleyin.",
    "Download": "İndir",
    "Restore": "Geri Yükle",
    "Reboot": "Yeniden Başlat",
    "Restart the Tracker.": "Tracker'ı yeniden başlatır.",
    "Reboot Device": "Cihazı Yeniden Başlat",
    "LoRa APRS Tracker Firmware": "LoRa APRS Tracker Yazılımı",
    "Support this project": "Bu projeyi destekleyin",
    "Saving settings": "Ayarlar kaydediliyor",
    "Settings will be saved and Tracker will reboot": "Ayarlar kaydedilecek ve Tracker yeniden başlayacak",
    "Stopping WiFiAP...": "WiFi erişim noktası kapatılıyor...",
    "Settings": "Ayarlar",
    "Settings saved!": "Ayarlar kaydedildi!",
    "Close": "Kapat",
    "System Message": "Sistem Mesajı",
    # blocks the page script builds itself (data_embed/script.js)
    "Callsign": "Çağrı İşareti",
    "Symbol": "Sembol",
    "Overlay": "Sembol Katmanı",
    "Comment": "Yorum",
    "Smart Beacon Active": "Akıllı Beacon Etkin",
    "GPS Eco Mode": "GPS Eko Modu",
    "Smart Beacon Setting": "Akıllı Beacon Profili",
    "Human/Runner (Slow Speed)": "İnsan/Koşucu (Düşük Hız)",
    "Bicycle (Mid Speed)": "Bisiklet (Orta Hız)",
    "Car/Motorcycle (Fast Speed)": "Araba/Motosiklet (Yüksek Hız)",
    "Status": "Durum",
    "Profile Label": "Profil Etiketi",
    "Frequency": "Frekans",
    "Power (dBm)": "Güç (dBm)",
    "Save to keep them.": "Kalıcı olması için Kaydet'e basın.",
}

ATTRIBUTES = {
    "Toggle light/dark": "Açık/koyu temayı değiştir",
    "Close": "Kapat",
}

NODE = re.compile(">([^<>]{2,300})<")


def translate_html(html: str) -> str:
    def node(m):
        raw = m.group(1)
        core = " ".join(raw.split())
        tr = TEXT.get(core)
        if tr is None:
            return m.group(0)
        lead = raw[: len(raw) - len(raw.lstrip())]
        trail = raw[len(raw.rstrip()) :]
        return ">" + lead + tr + trail + "<"

    out = NODE.sub(node, html)
    for en, tr in ATTRIBUTES.items():
        for attr in ("title", "aria-label"):
            out = out.replace(attr + "=" + chr(34) + en + chr(34), attr + "=" + chr(34) + tr + chr(34))
    return out.replace('<html lang="en"', '<html lang="tr"', 1)


def untranslated(html: str):
    """Text nodes of the page that have no Turkish entry (for the build report / tests)."""
    missing = []
    for m in NODE.finditer(html):
        core = " ".join(m.group(1).split())
        if re.search("[A-Za-z]{3,}", core) and ";" not in core.replace("&minus;", "").replace("&gt;", "") and "{" not in core:
            if core not in TEXT and core not in missing:
                missing.append(core)
    return missing
