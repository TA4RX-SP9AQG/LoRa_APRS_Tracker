/* Copyright (C) 2026 Ricardo Guzman - CA2RXU
 *
 * This file is part of LoRa APRS Tracker.
 *
 * LoRa APRS Tracker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LoRa APRS Tracker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LoRa APRS Tracker. If not, see <https://www.gnu.org/licenses/>.
 */

#include <APRSPacketLib.h>
#include "tb_lang.h"
#include <TinyGPS++.h>
#include <vector>
#include "notification_utils.h"
#include "custom_characters.h"
#include "station_utils.h"
#include "configuration.h"
#include "battery_utils.h"
#include "board_pinout.h"
#include "power_utils.h"
#include "menu_utils.h"
#include "msg_utils.h"
#include "gps_utils.h"
#include "wx_utils.h"
#include "display.h"
#include "utils.h"


extern int                  menuDisplay;
extern Beacon               *currentBeacon;
extern Configuration        Config;
extern TinyGPSPlus          gps;
extern std::vector<String>  loadedAPRSMessages;
extern std::vector<String>  loadedWLNKMails;
extern int                  messagesIterator;
extern uint8_t              loraIndex;
extern uint32_t             menuTime;
extern bool                 symbolAvailable;
extern bool                 keyDetected;
extern bool                 keyboardConnected;
extern String               messageCallsign;
extern String               messageText;
extern bool                 flashlight;
extern bool                 digipeaterActive;
extern bool                 sosActive;
extern bool                 bluetoothActive;
extern bool                 displayEcoMode;
extern bool                 screenBrightness;
extern bool                 disableGPS;
extern bool                 showHumanHeading;
extern APRSPacket           lastReceivedPacket;

extern uint8_t              winlinkStatus;
extern String               winlinkMailNumber;
extern String               winlinkAddressee;
extern String               winlinkSubject;
extern String               winlinkBody;
extern String               winlinkAlias;
extern String               winlinkAliasComplete;
extern bool                 winlinkCommentState;

extern bool                 batteryConnected;
extern int                  wxModuleType;
extern bool                 gpsIsActive;

String      freqChangeWarning;
uint8_t     lowBatteryPercent       = 21;

#if defined(TTGO_T_DECK_PLUS) || defined(TTGO_T_DECK_GPS)
    String topHeader1   = "";
    String topHeader1_1 = "";
    String topHeader1_2 = "";
    String topHeader1_3 = "";
    String topHeader2   = "";
#endif


namespace MENU_Utils {

    const String checkBTType() {
        if (Config.bluetooth.useBLE) {
            return "BLE";
        } else {
            return "BT Classic";
        }
    }

    const String checkProcessActive(const bool process) {
        if (process) {
            return "ON";
        } else {
            return "OFF";
        }
    }

    const String screenBrightnessAsString(const uint8_t bright) {
        #ifdef HAS_TFT
            if (bright == 255) {
                return "Max";
            } else if (bright == 70) {
                return "Low";
            } else {
                return "Mid";
            }
        #else
            if (bright == 255) {
                return "Max";
            } else if (bright == 1) {
                return "Low";
            } else {
                return "Mid";
            }
        #endif
    }

    String padForDisplay(const String& text) {      // left-aligned, 9 chars wide (callsign column on screen)
        char buffer[10];
        snprintf(buffer, sizeof(buffer), "%-9s", text.c_str());
        return String(buffer);
    }

    void showOnScreen() {
        String lastLine;
        uint32_t lastMenuTime = millis() - menuTime;
        if (!(menuDisplay==0) && !(menuDisplay==400) && !(menuDisplay==410) && !(menuDisplay==300) && !(menuDisplay>=500 && menuDisplay<=5100) && lastMenuTime > 30*1000) {
            menuDisplay     = 0;
            messageCallsign = "";
            messageText     = "";
        }
        if (keyDetected) {
            lastLine = TB_T("<Back Up/Down Select>", "<Geri Yuk/Asg Sec>");
        } else {
            lastLine = TB_T("1P=Down 2P=Back LP=Go", "1x=Asagi 2x=Geri Uz=Git");
        }

        #if defined(TTGO_T_DECK_PLUS) || defined(TTGO_T_DECK_GPS)
            topHeader1      = currentBeacon->callsign;
            const auto time_now = now();
            topHeader1_1    = Utils::createDateString(time_now);
            topHeader1_2    = Utils::createTimeString(time_now);
            topHeader1_3    = "";
            topHeader2      = String(gps.location.lat(), 4);
            topHeader2      += " ";
            topHeader2      += String(gps.location.lng(), 4);

            for (int i = topHeader2.length(); i < 19; i++) {
                topHeader2 += " ";
            }
            if (gps.satellites.value() <= 9) topHeader2 += " ";
            topHeader2 += TB_T("SAT:", "UYDU:");
            topHeader2 += String(gps.satellites.value());
            if (gps.hdop.hdop() > 5) {
                topHeader2 += "X";
            } else if (gps.hdop.hdop() > 2 && gps.hdop.hdop() < 5) {
                topHeader2 += "-";
            } else if (gps.hdop.hdop() <= 2) {
                topHeader2 += "+";
            }
        #endif

        switch (menuDisplay) { // Graphic Menu is in here!!!!
            case 1:     // 1. Messages
                displayShow("<< MENU >>",TB_T("  6.Extras", "  6.Ekstralar"), TB_T("> 1.Messages", "> 1.Mesajlar"), TB_T("  2.Configuration", "  2.Ayarlar"), TB_T("  3.Reports", "  3.Raporlar"), lastLine);
                break;
            case 2:     // 2. Configuration
                displayShow("<< MENU >>", TB_T("  1.Messages", "  1.Mesajlar"), TB_T("> 2.Configuration", "> 2.Ayarlar"), TB_T("  3.Reports", "  3.Raporlar"), TB_T("  4.Stations", "  4.Istasyonlar"), lastLine);
                break;
            case 3:     //3. Reports
                displayShow("<< MENU >>", TB_T("  2.Configuration", "  2.Ayarlar"), TB_T("> 3.Reports", "> 3.Raporlar"), TB_T("  4.Stations", "  4.Istasyonlar"), TB_T("  5.Winlink/Mail", "  5.Winlink/Posta"), lastLine);
                break;
            case 4:     //4. Stations
                displayShow("<< MENU >>", TB_T("  3.Reports", "  3.Raporlar"), TB_T("> 4.Stations", "> 4.Istasyonlar"), TB_T("  5.Winlink/Mail", "  5.Winlink/Posta"), TB_T("  6.Extras", "  6.Ekstralar"), lastLine);
                break;
            case 5:     //5. Winlink
                displayShow("<< MENU >>", TB_T("  4.Stations", "  4.Istasyonlar"), TB_T("> 5.Winlink/Mail", "> 5.Winlink/Posta"), TB_T("  6.Extras", "  6.Ekstralar"), TB_T("  1.Messages", "  1.Mesajlar"), lastLine);
                break;
            case 6:     //6. Extras
                displayShow("<< MENU >>", TB_T("  5.Winlink/Mail", "  5.Winlink/Posta"), TB_T("> 6.Extras", "> 6.Ekstralar"), TB_T("  1.Messages", "  1.Mesajlar"), TB_T("  2.Configuration", "  2.Ayarlar"), lastLine);
                break;

//////////
            case 10:    // 1.Messages ---> Messages Read
                displayShow(TB_T(" MESSAGES>", " MESAJLAR>"), TB_T("> Read (", "> Oku (") + String(MSG_Utils::getNumAPRSMessages()) + ")", TB_T("  Write", "  Yaz"), TB_T("  Delete", "  Sil"), "  APRSThursday", lastLine);
                break;
            case 100:   // 1.Messages ---> Messages Read ---> Display Received/Saved APRS Messages
                {
                    String msgSender    = loadedAPRSMessages[messagesIterator].substring(0, loadedAPRSMessages[messagesIterator].indexOf(","));
                    String msgText      = loadedAPRSMessages[messagesIterator].substring(loadedAPRSMessages[messagesIterator].indexOf(",") + 1);

                    #ifdef HAS_TFT
                        #if defined(HELTEC_WIRELESS_TRACKER)
                            displayShow(TB_T(" MSG APRS>", " APRS MSJ>"), TB_T("From --> ", "Kimden --> ") + msgSender, msgText,TB_T("                 Next=Down", "               Ileri=Asagi"), "", "");
                        #else   // T-Deck
                            displayShow(TB_T("MSG APRS>", "APRS MSJ>"), TB_T("From --> ", "Kimden --> ") + msgSender, msgText,TB_T("             Next=Down", "           Ileri=Asagi"), "", "");
                        #endif
                    #else
                        displayShow(TB_T(" MSG APRS>", " APRS MSJ>"), TB_T("From --> ", "Kimden --> ") + msgSender, msgText, "", "", TB_T("           Next=Down", "         Ileri=Asagi"));
                    #endif
                }
                break;
            case 11:    // 1.Messages ---> Messages Write
                displayShow(TB_T(" MESSAGES>", " MESAJLAR>"), TB_T("  Read (", "  Oku (") + String(MSG_Utils::getNumAPRSMessages()) + ")", TB_T("> Write", "> Yaz"), TB_T("  Delete", "  Sil"), "  APRSThursday", lastLine);
                break;
            case 110:   // 1.Messages ---> Messages Write ---> Write
                if (keyDetected || keyboardConnected) {
                    #ifdef HAS_TFT
                        #if defined(HELTEC_WIRELESS_TRACKER)
                            displayShow(TB_T("WRITE MSG>", "MESAJ YAZ>"), "", TB_T("CALLSIGN = ", "CAGRI ISR. = ") + String(messageCallsign), "", "", TB_T("<Back               Enter>", "<Geri               Tamam>"));
                        #else   //  T-DECK
                            displayShow(TB_T("WRITE MSG>", "MESAJ YAZ>"), "", TB_T("CALLSIGN = ", "CAGRI ISR. = ") + String(messageCallsign), "", "", TB_T("<Back           Enter>", "<Geri           Tamam>"));
                        #endif
                    #else
                        displayShow(TB_T("WRITE MSG>", "MESAJ YAZ>"), "", TB_T("CALLSIGN = ", "CAGRI ISR. = ") + String(messageCallsign), "", "", TB_T("<Back          Enter>", "<Geri          Tamam>"));
                    #endif
                } else {
                    displayShow(TB_T("WRITE MSG>", "MESAJ YAZ>"), "", TB_T("No Keyboard Detected", "Klavye bulunamadi"), TB_T("Can't write Message", "Mesaj yazilamaz"), "", TB_T("1P = Back", "1x = Geri"));
                }
                break;
            case 111:
                if (messageText.length() <= 67) {
                    #ifdef HAS_TFT
                        #if defined(HELTEC_WIRELESS_TRACKER)
                            if (messageText.length() < 10) {
                                displayShow(TB_T("WRITE MSG>", "MESAJ YAZ>"), TB_T("CALLSIGN -> ", "CAGRI ISR. -> ") + messageCallsign, TB_T("MSG -> ", "MSJ -> ") + messageText, TB_T("<Back      (0", "<Geri      (0") + String(messageText.length()) + TB_T(")     Enter>", ")     Tamam>"), "", "");
                            } else {
                                displayShow(TB_T("WRITE MSG>", "MESAJ YAZ>"), TB_T("CALLSIGN -> ", "CAGRI ISR. -> ") + messageCallsign, TB_T("MSG -> ", "MSJ -> ") + messageText, TB_T("<Back      (", "<Geri      (") + String(messageText.length()) + TB_T(")     Enter>", ")     Tamam>"), "", "");
                            }
                        #else   // T-Deck
                            if (messageText.length() < 10) {
                                displayShow(TB_T("WRITE MSG>", "MESAJ YAZ>"), TB_T("CALLSIGN -> ", "CAGRI ISR. -> ") + messageCallsign, TB_T("MSG -> ", "MSJ -> ") + messageText, TB_T("<Back    (0", "<Geri    (0") + String(messageText.length()) + TB_T(")   Enter>", ")   Tamam>"), "", "");
                            } else {
                                displayShow(TB_T("WRITE MSG>", "MESAJ YAZ>"), TB_T("CALLSIGN -> ", "CAGRI ISR. -> ") + messageCallsign, TB_T("MSG -> ", "MSJ -> ") + messageText, TB_T("<Back    (", "<Geri    (") + String(messageText.length()) + TB_T(")   Enter>", ")   Tamam>"), "", "");
                            }
                        #endif
                    #else
                        if (messageText.length() < 10) {
                            displayShow(TB_T("WRITE MSG>", "MESAJ YAZ>"), TB_T("CALLSIGN -> ", "CAGRI ISR. -> ") + messageCallsign, TB_T("MSG -> ", "MSJ -> ") + messageText, "", "", TB_T("<Back   (0", "<Geri   (0") + String(messageText.length()) + TB_T(")   Enter>", ")   Tamam>"));
                        } else {
                            displayShow(TB_T("WRITE MSG>", "MESAJ YAZ>"), TB_T("CALLSIGN -> ", "CAGRI ISR. -> ") + messageCallsign, TB_T("MSG -> ", "MSJ -> ") + messageText, "", "", TB_T("<Back   (", "<Geri   (") + String(messageText.length()) + TB_T(")   Enter>", ")   Tamam>"));
                        }
                    #endif
                } else {
                    #ifdef HAS_TFT
                        #if defined(HELTEC_WIRELESS_TRACKER)
                            displayShow(TB_T("WRITE MSG>", "MESAJ YAZ>"), TB_T("  --- MSG TOO LONG! ---", "  -- MSJ COK UZUN! --"), " -> " + messageText, TB_T("<Back   (", "<Geri   (") + String(messageText.length()) + ")",  "", "");
                        #else   // T-Deck
                            displayShow(TB_T("WRITE MSG>", "MESAJ YAZ>"), TB_T("  --- MSG TOO LONG! ---", "  -- MSJ COK UZUN! --"), " -> " + messageText, TB_T("<Back   (", "<Geri   (") + String(messageText.length()) + ")", "", "");
                        #endif
                    #else
                        displayShow(TB_T("WRITE MSG>", "MESAJ YAZ>"), TB_T("--- MSG TOO LONG! ---", "-- MSJ COK UZUN! --"), " -> " + messageText, "", "", TB_T("<Back   (", "<Geri   (") + String(messageText.length()) + ")");
                    #endif
                }
                break;
            case 12:    // 1.Messages ---> Messages Delete
                displayShow(TB_T(" MESSAGES>", " MESAJLAR>"), TB_T("  Read (", "  Oku (") + String(MSG_Utils::getNumAPRSMessages()) + ")", TB_T("  Write", "  Yaz"), TB_T("> Delete", "> Sil"), "  APRSThursday", lastLine);
                break;
            case 120:   // 1.Messages ---> Messages Delete ---> Delete: ALL
                displayShow(TB_T("DELETE MSG", "MESAJ SIL"), "", TB_T("  DELETE APRS MSG?", "  APRS MSJ SIL?"), "", "", TB_T(" Confirm = LP or '>'", " Onay = Uz veya '>'"));
                break;
            case 13:    // 1.Messages ---> APRSThursday
                displayShow(TB_T(" MESSAGES>", " MESAJLAR>"), TB_T("  Read (", "  Oku (") + String(MSG_Utils::getNumAPRSMessages()) + ")", TB_T("  Write", "  Yaz"), TB_T("  Delete", "  Sil"), "> APRSThursday", lastLine);
                break;
            case 130:   // 1.Messages ---> APRSThursday ---> Delete: ALL
                displayShow(TB_T(" APRS Thu.", " APRS Per."), "> Check In", TB_T("  Join", "  Katil"), TB_T("  Unsubscribe", "  Abonelik Iptal"), TB_T("  KeepSubscribed+12h", "  AboneKal+12s"), lastLine);
                break;
            case 1300:
                if (messageText.length() <= 67) {
                    #ifdef HAS_TFT
                        #if defined(HELTEC_WIRELESS_TRACKER)
                            if (messageText.length() < 10) {
                                displayShow(TB_T("WRITE MSG>", "MESAJ YAZ>"), "    - APRSThursday -", TB_T("MSG -> ", "MSJ -> ") + messageText, TB_T("<Back      (0", "<Geri      (0") + String(messageText.length()) + TB_T(")     Enter>", ")     Tamam>"), "", "");
                            } else {
                                displayShow(TB_T("WRITE MSG>", "MESAJ YAZ>"), "    - APRSThursday -", TB_T("MSG -> ", "MSJ -> ") + messageText, TB_T("<Back      (", "<Geri      (") + String(messageText.length()) + TB_T(")     Enter>", ")     Tamam>"), "", "");
                            }
                        #else   // T-Deck
                            if (messageText.length() < 10) {
                                displayShow(TB_T("WRITE MSG>", "MESAJ YAZ>"), "    - APRSThursday -", TB_T("MSG -> ", "MSJ -> ") + messageText, TB_T("<Back    (0", "<Geri    (0") + String(messageText.length()) + TB_T(")   Enter>", ")   Tamam>"), "", "");
                            } else {
                                displayShow(TB_T("WRITE MSG>", "MESAJ YAZ>"), "    - APRSThursday -", TB_T("MSG -> ", "MSJ -> ") + messageText, TB_T("<Back    (", "<Geri    (") + String(messageText.length()) + TB_T(")   Enter>", ")   Tamam>"), "", "");
                            }
                        #endif
                    #else
                        if (messageText.length() < 10) {
                            displayShow(TB_T("WRITE MSG>", "MESAJ YAZ>"), "  - APRSThursday -", TB_T("MSG -> ", "MSJ -> ") + messageText, "", "", TB_T("<Back   (0", "<Geri   (0") + String(messageText.length()) + TB_T(")   Enter>", ")   Tamam>"));
                        } else {
                            displayShow(TB_T("WRITE MSG>", "MESAJ YAZ>"), "  - APRSThursday -", TB_T("MSG -> ", "MSJ -> ") + messageText, "", "", TB_T("<Back   (", "<Geri   (") + String(messageText.length()) + TB_T(")   Enter>", ")   Tamam>"));
                        }
                    #endif
                } else {
                    #ifdef HAS_TFT
                        #if defined(HELTEC_WIRELESS_TRACKER)
                            displayShow(TB_T("WRITE MSG>", "MESAJ YAZ>"), TB_T("  --- MSG TOO LONG! ---", "  -- MSJ COK UZUN! --"), " -> " + messageText, TB_T("<Back   (", "<Geri   (") + String(messageText.length()) + ")",  "", "");
                        #else   // T-Deck
                            displayShow(TB_T("WRITE MSG>", "MESAJ YAZ>"), TB_T("  --- MSG TOO LONG! ---", "  -- MSJ COK UZUN! --"), " -> " + messageText, TB_T("<Back   (", "<Geri   (") + String(messageText.length()) + ")", "", "");
                        #endif
                    #else
                        displayShow(TB_T("WRITE MSG>", "MESAJ YAZ>"), TB_T("--- MSG TOO LONG! ---", "-- MSJ COK UZUN! --"), " -> " + messageText, "", "", TB_T("<Back   (", "<Geri   (") + String(messageText.length()) + ")");
                    #endif
                }
                break;
            case 131:   // 1.Messages ---> APRSThursday ---> Delete: ALL
                displayShow(TB_T(" APRS Thu.", " APRS Per."), "  Check In", TB_T("> Join", "> Katil"), TB_T("  Unsubscribe", "  Abonelik Iptal"), TB_T("  KeepSubscribed+12h", "  AboneKal+12s"), lastLine);
                break;
            case 1310:
                if (messageText.length() <= 67) {
                    #ifdef HAS_TFT
                        #if defined(HELTEC_WIRELESS_TRACKER)
                            if (messageText.length() < 10) {
                                displayShow(TB_T("WRITE MSG>", "MESAJ YAZ>"), "    - APRSThursday -", TB_T("MSG -> ", "MSJ -> ") + messageText, TB_T("<Back      (0", "<Geri      (0") + String(messageText.length()) + TB_T(")     Enter>", ")     Tamam>"), "", "");
                            } else {
                                displayShow(TB_T("WRITE MSG>", "MESAJ YAZ>"), "    - APRSThursday -", TB_T("MSG -> ", "MSJ -> ") + messageText, TB_T("<Back      (", "<Geri      (") + String(messageText.length()) + TB_T(")     Enter>", ")     Tamam>"), "", "");
                            }
                        #else   // T-Deck
                            if (messageText.length() < 10) {
                                displayShow(TB_T("WRITE MSG>", "MESAJ YAZ>"), "    - APRSThursday -", TB_T("MSG -> ", "MSJ -> ") + messageText, TB_T("<Back    (0", "<Geri    (0") + String(messageText.length()) + TB_T(")   Enter>", ")   Tamam>"), "", "");
                            } else {
                                displayShow(TB_T("WRITE MSG>", "MESAJ YAZ>"), "    - APRSThursday -", TB_T("MSG -> ", "MSJ -> ") + messageText, TB_T("<Back    (", "<Geri    (") + String(messageText.length()) + TB_T(")   Enter>", ")   Tamam>"), "", "");
                            }
                        #endif
                    #else
                        if (messageText.length() < 10) {
                            displayShow(TB_T("WRITE MSG>", "MESAJ YAZ>"), "  - APRSThursday -", TB_T("MSG -> ", "MSJ -> ") + messageText, "", "", TB_T("<Back   (0", "<Geri   (0") + String(messageText.length()) + TB_T(")   Enter>", ")   Tamam>"));
                        } else {
                            displayShow(TB_T("WRITE MSG>", "MESAJ YAZ>"), "  - APRSThursday -", TB_T("MSG -> ", "MSJ -> ") + messageText, "", "", TB_T("<Back   (", "<Geri   (") + String(messageText.length()) + TB_T(")   Enter>", ")   Tamam>"));
                        }
                    #endif
                } else {
                    #ifdef HAS_TFT
                        #if defined(HELTEC_WIRELESS_TRACKER)
                            displayShow(TB_T("WRITE MSG>", "MESAJ YAZ>"), TB_T("  --- MSG TOO LONG! ---", "  -- MSJ COK UZUN! --"), " -> " + messageText, TB_T("<Back   (", "<Geri   (") + String(messageText.length()) + ")",  "", "");
                        #else   // T-Deck
                            displayShow(TB_T("WRITE MSG>", "MESAJ YAZ>"), TB_T("  --- MSG TOO LONG! ---", "  -- MSJ COK UZUN! --"), " -> " + messageText, TB_T("<Back   (", "<Geri   (") + String(messageText.length()) + ")", "", "");
                        #endif
                    #else
                        displayShow(TB_T("WRITE MSG>", "MESAJ YAZ>"), TB_T("--- MSG TOO LONG! ---", "-- MSJ COK UZUN! --"), " -> " + messageText, "", "", TB_T("<Back   (", "<Geri   (") + String(messageText.length()) + ")");
                    #endif
                }
                break;
            case 132:   // 1.Messages ---> APRSThursday ---> Delete: ALL
                displayShow(TB_T(" APRS Thu.", " APRS Per."), "  Check In", TB_T("  Join", "  Katil"), TB_T("> Unsubscribe", "> Abonelik Iptal"), TB_T("  KeepSubscribed+12h", "  AboneKal+12s"), lastLine);
                break;
            case 133:   // 1.Messages ---> APRSThursday ---> Delete: ALL
                displayShow(TB_T(" APRS Thu.", " APRS Per."), "  Check In", TB_T("  Join", "  Katil"), TB_T("  Unsubscribe", "  Abonelik Iptal"), TB_T("> KeepSubscribed+12h", "> AboneKal+12s"), lastLine);
                break;

//////////
            case 20:    // 2.Configuration ---> Callsign
                displayShow(TB_T(" CONFIG>", " AYARLAR>"), TB_T("  Power Off", "  Kapat"), TB_T("> Change Callsign ", "> Cagri Degistir "), TB_T("  Change Frequency", "  Frekans Degistir"), TB_T("  Display", "  Ekran"),lastLine);
                break;
            case 21:    // 2.Configuration ---> Change Freq
                displayShow(TB_T(" CONFIG>", " AYARLAR>"), TB_T("  Change Callsign ", "  Cagri Degistir "), TB_T("> Change Frequency", "> Frekans Degistir"), TB_T("  Display", "  Ekran"), "  " + checkBTType() + " (" + checkProcessActive(bluetoothActive) + ")",lastLine);
                break;
            case 22:    // 2.Configuration ---> Display
                displayShow(TB_T(" CONFIG>", " AYARLAR>"), TB_T("  Change Frequency", "  Frekans Degistir"), TB_T("> Display", "> Ekran"), "  " + checkBTType() + " (" + checkProcessActive(bluetoothActive) + ")", TB_T("  Status", "  Durum"),lastLine);
                break;
            case 23:    // 2.Configuration ---> Bluetooth
                displayShow(TB_T(" CONFIG>", " AYARLAR>"), TB_T("  Display", "  Ekran"),  "> " + checkBTType() + " (" + checkProcessActive(bluetoothActive) + ")", TB_T("  Status", "  Durum"), TB_T("  Notifications", "  Bildirimler"), lastLine);
                break;
            case 24:    // 2.Configuration ---> Status
                displayShow(TB_T(" CONFIG>", " AYARLAR>"), "  " + checkBTType() + " (" + checkProcessActive(bluetoothActive) + ")", TB_T("> Status", "> Durum"),TB_T("  Notifications", "  Bildirimler"), TB_T("  Reboot", "  Reset"),lastLine);
                break;
            case 25:    // 2.Configuration ---> Notifications
                displayShow(TB_T(" CONFIG>", " AYARLAR>"), TB_T("  Status", "  Durum"), TB_T("> Notifications", "> Bildirimler"), TB_T("  Reboot", "  Reset"), TB_T("  Power Off", "  Kapat"),lastLine);
                break;
            case 26:    // 2.Configuration ---> Reboot
                displayShow(TB_T(" CONFIG>", " AYARLAR>"), TB_T("  Notifications", "  Bildirimler"), TB_T("> Reboot", "> Reset"), TB_T("  Power Off", "  Kapat"), TB_T("  Change Callsign", "  Cagri Degistir"),lastLine);
                break;
            case 27:    // 2.Configuration ---> Power Off
                displayShow(TB_T(" CONFIG>", " AYARLAR>"), TB_T("  Reboot", "  Reset"), TB_T("> Power Off", "> Kapat"), TB_T("  Change Callsign", "  Cagri Degistir"), TB_T("  Change Frequency", "  Frekans Degistir"),lastLine);
                break;


            case 200:   // 2.Configuration ---> Change Callsign
                displayShow(TB_T(" CALLSIGN>", " CAGRI ISR.>"), "",TB_T("  Confirm Change?", "  Degisiklik onay?"),"","",TB_T("<Back         Select>", "<Geri         Sec>   "));
                break;

            case 210:   // 2.Configuration ---> Change Frequency
                switch (loraIndex) {
                    case 0: freqChangeWarning = "      EU --> PL"; break;
                    case 1: freqChangeWarning = "      PL --> UK"; break;
                    case 2: freqChangeWarning = "      UK --> US"; break;
                    case 3: freqChangeWarning = "      US --> EU"; break;
                }
                displayShow(TB_T("LORA FREQ>", "LORA FREK.>"), "",TB_T("   Confirm Change?", "   Degisiklik onay?"), freqChangeWarning, "", TB_T("<Back         Select>", "<Geri         Sec>   "));
                break;

            case 220:   // 2.Configuration ---> Display ---> ECO Mode
                displayShow(TB_T(" DISPLAY>", " EKRAN>"), "", TB_T("> ECO Mode    (", "> ECO Modu    (") + checkProcessActive(displayEcoMode) + ")", TB_T("  Brightness  (", "  Parlaklik   (") + screenBrightnessAsString(screenBrightness) + ")","",lastLine);
                break;

            case 221:   // 2.Configuration ---> Display ---> Brightness
                displayShow(TB_T(" DISPLAY>", " EKRAN>"), "", TB_T("  ECO Mode    (", "  ECO Modu    (") + checkProcessActive(displayEcoMode) + ")", TB_T("> Brightness  (", "> Parlaklik   (") + screenBrightnessAsString(screenBrightness) + ")","",lastLine);
                break;
            case 2210:   // 2.Configuration ---> Display ---> Brightness: MIN
                displayShow(TB_T("BRIGHTNESS", "PARLAKLIK"), "", "> Low", "  Mid","  Max",lastLine);
                break;
            case 2211:   // 2.Configuration ---> Display ---> Brightness: MID
                displayShow(TB_T("BRIGHTNESS", "PARLAKLIK"), "", "  Low", "> Mid","  Max",lastLine);
                break;
            case 2212:   // 2.Configuration ---> Display ---> Brightness: MAX
                displayShow(TB_T("BRIGHTNESS", "PARLAKLIK"), "", "  Low", "  Mid","> Max",lastLine);
                break;

            case 230:
                if (bluetoothActive) {
                    bluetoothActive = false;
                    displayShow("BLUETOOTH>", "", " Bluetooth --> OFF", 1000);
                } else {
                    bluetoothActive = true;
                    displayShow("BLUETOOTH>", "", " Bluetooth --> ON", 1000);
                }
                menuDisplay = 23;
                break;

            case 240:    // 2.Configuration ---> Status
                displayShow(TB_T(" STATUS>", " DURUM>"), "", TB_T("> Write", "> Yaz"),TB_T("  Select", "  Sec"),"",lastLine);
                break;
            case 241:    // 2.Configuration ---> Status
                displayShow(TB_T(" STATUS>", " DURUM>"), "", TB_T("  Write", "  Yaz"),TB_T("> Select", "> Sec"),"",lastLine);
                break;

            case 250:    // 2.Configuration ---> Notifications
                displayShow(TB_T(" NOTIFIC>", " BILDIRIM>"), TB_T("> Turn Off Sound/Led", "> Ses/Led Kapat"),"","","",lastLine);
                break;

            case 260:   // 2.Configuration ---> Reboot
                if (keyDetected) {
                    displayShow(TB_T(" REBOOT?", " RESET?"), "",TB_T("Confirm Reboot...", "Resetleniyor..."),"","",TB_T("<Back   Enter=Confirm", "<Geri   Enter=Confirm"));
                } else {
                    displayShow(TB_T(" REBOOT?", " RESET?"), TB_T("no Keyboard Detected", "Klavye bulunamadi"),TB_T(" Use RST Button to", " RST dugmesiyle"),TB_T("Reboot Tracker", "resetleyin"),"",lastLine);
                }
                break;
            case 270:   // 2.Configuration ---> Power Off
                displayShow(TB_T("POWER OFF?", "KAPAT?"), "",TB_T("Confirm Power Off...", "Kapaniyor..."),"","",TB_T("<Back Enter/>=Confirm", "<Geri Enter/>=Confirm"));
                break;

//////////
            case 30:     // 3. Reports : Wx Report
                displayShow(TB_T(" REPORTS >", " RAPORLAR >"),TB_T("> 1.Wx Report", "> 1.Hava Raporu"), TB_T("  2.Hospital QTH", "  2.Hastane QTH"), TB_T("  3.Police QTH", "  3.Polis QTH"), TB_T("  4.Fire Station QTH", "  4.Itfaiye QTH"), lastLine);
                break;
            case 31:     // 3. Reports : Nearest Hospital
                displayShow(TB_T(" REPORTS >", " RAPORLAR >"),TB_T("  1.Wx Report", "  1.Hava Raporu"), TB_T("> 2.Hospital QTH", "> 2.Hastane QTH"), TB_T("  3.Police QTH", "  3.Polis QTH"), TB_T("  4.Fire Station QTH", "  4.Itfaiye QTH"), lastLine);
                break;
            case 32:     // 3. Reports : Nearest Police Station
                displayShow(TB_T(" REPORTS >", " RAPORLAR >"),TB_T("  1.Wx Report", "  1.Hava Raporu"), TB_T("  2.Hospital QTH", "  2.Hastane QTH"), TB_T("> 3.Police QTH", "> 3.Polis QTH"), TB_T("  4.Fire Station QTH", "  4.Itfaiye QTH"), lastLine);
                break;
            case 33:     // 3. Reports : Nearest Fire Station
                displayShow(TB_T(" REPORTS >", " RAPORLAR >"),TB_T("  1.Wx Report", "  1.Hava Raporu"), TB_T("  2.Hospital QTH", "  2.Hastane QTH"), TB_T("  3.Police QTH", "  3.Polis QTH"), TB_T("> 4.Fire Station QTH", "> 4.Itfaiye QTH"), lastLine);
                break;

            case 300:
                // waiting for Report
                break;

//////////
            case 40:    //4.Stations ---> Packet Decoder
                displayShow(TB_T(" STATIONS>", " ISTASYON>"), "", TB_T("> Packet Decoder", "> Paket Cozucu"), TB_T("  Near By Stations", "  Yakin Istasyonlar"), "", TB_T("<Back", "<Geri"));
                break;
            case 41:    //4.Stations ---> Near By Stations
                displayShow(TB_T(" STATIONS>", " ISTASYON>"), "", TB_T("  Packet Decoder", "  Paket Cozucu"), TB_T("> Near By Stations", "> Yakin Istasyonlar"), "", TB_T("<Back", "<Geri"));
                break;

            case 400:   //4.Stations ---> Packet Decoder
                if (lastReceivedPacket.sender != currentBeacon->callsign) {
                    String firstLineDecoder = padForDisplay(lastReceivedPacket.sender);
                    firstLineDecoder += lastReceivedPacket.symbol;

                    if (lastReceivedPacket.type == 0 || lastReceivedPacket.type == 4) {      // gps and Mic-E gps

                        char bufferCourseSpeedAltitude[24];
                        sprintf(bufferCourseSpeedAltitude, "A=%04dm %3dkm/h %3d", lastReceivedPacket.altitude, lastReceivedPacket.speed, lastReceivedPacket.course);
                        String courseSpeedAltitude = String(bufferCourseSpeedAltitude);

                        double distanceKm = TinyGPSPlus::distanceBetween(gps.location.lat(), gps.location.lng(), lastReceivedPacket.latitude, lastReceivedPacket.longitude) / 1000.0;
                        double courseTo   = TinyGPSPlus::courseTo(gps.location.lat(), gps.location.lng(), lastReceivedPacket.latitude, lastReceivedPacket.longitude);

                        String pathDec = (lastReceivedPacket.path.length() > 14) ? "P:" : TB_T("PATH:  ", "YOL:  ");
                        pathDec += lastReceivedPacket.path;

                        displayShow(firstLineDecoder, "GPS " + String(lastReceivedPacket.latitude,3) + " " + String(lastReceivedPacket.longitude,3), courseSpeedAltitude, "D:" + String(distanceKm) + "km    " + String(courseTo,0), pathDec, "< RSSI:" + String(lastReceivedPacket.rssi) + " SNR:" + String(lastReceivedPacket.snr));
                    } else if (lastReceivedPacket.type == 1) {    // message
                        displayShow(firstLineDecoder, TB_T("ADDRESSEE: ", "ALICI: ") + lastReceivedPacket.addressee, TB_T("MSG:  ", "MSJ:  ") + lastReceivedPacket.payload, "", "", "< RSSI:" + String(lastReceivedPacket.rssi) + " SNR:" + String(lastReceivedPacket.snr));
                    } else if (lastReceivedPacket.type == 2) {    // status
                        displayShow(firstLineDecoder, TB_T("-------STATUS-------", "-------DURUM--------"), lastReceivedPacket.payload, "", "", "< RSSI:" + String(lastReceivedPacket.rssi) + " SNR:" + String(lastReceivedPacket.snr));
                    } else if (lastReceivedPacket.type == 3) {    // telemetry
                        displayShow(firstLineDecoder, TB_T("------TELEMETRY------", "-----TELEMETRI------"), "", "", "", "< RSSI:" + String(lastReceivedPacket.rssi) + " SNR:" + String(lastReceivedPacket.snr));
                    } else if (lastReceivedPacket.type == 5) {    // object
                        displayShow(firstLineDecoder, TB_T("-------OBJECT-------", "-------NESNE--------"), "", "", "", "< RSSI:" + String(lastReceivedPacket.rssi) + " SNR:" + String(lastReceivedPacket.snr));
                    }
                }
                break;
            case 410:    //4.Stations ---> Near By Stations
                displayShow(TB_T(" NEAR BY>", " YAKIN>"), STATION_Utils::getNearStation(0), STATION_Utils::getNearStation(1), STATION_Utils::getNearStation(2), STATION_Utils::getNearStation(3), TB_T("<Back", "<Geri"));
                break;

//////////
            case 50:    // 5.Winlink MENU
                if (winlinkStatus == 5) {
                    menuDisplay = 5000;
                } else {
                    displayShow(" WINLINK>", TB_T("> Login", "> Giris") , TB_T("  Read SavedMails(", "  Kayitli Posta Oku(") + String(MSG_Utils::getNumWLNKMails()) + ")", TB_T("  Delete SavedMails", "  Kayitli Posta Sil"), TB_T("  Wnlk Comment (", "  Wnlk Yorum (") + checkProcessActive(winlinkCommentState) + ")" , lastLine);
                }
                break;
            case 51:    // 5.Winlink
                displayShow(" WINLINK>", TB_T("  Login", "  Giris") , TB_T("> Read SavedMails(", "> Kayitli Posta Oku(") + String(MSG_Utils::getNumWLNKMails()) + ")", TB_T("  Delete SavedMails", "  Kayitli Posta Sil"), TB_T("  Wnlk Comment (", "  Wnlk Yorum (") + checkProcessActive(winlinkCommentState) + ")" , lastLine);
                break;
            case 52:    // 5.Winlink
                displayShow(" WINLINK>", TB_T("  Login", "  Giris") , TB_T("  Read SavedMails(", "  Kayitli Posta Oku(") + String(MSG_Utils::getNumWLNKMails()) + ")", TB_T("> Delete SavedMails", "> Kayitli Posta Sil"), TB_T("  Wnlk Comment (", "  Wnlk Yorum (") + checkProcessActive(winlinkCommentState) + ")" , lastLine);
                break;
            case 53:    // 5.Winlink
                displayShow(" WINLINK>", TB_T("  Login", "  Giris") , TB_T("  Read SavedMails(", "  Kayitli Posta Oku(") + String(MSG_Utils::getNumWLNKMails()) + ")", TB_T("  Delete SavedMails", "  Kayitli Posta Sil"), TB_T("> Wnlk Comment (", "> Wnlk Yorum (") + checkProcessActive(winlinkCommentState) + ")" , lastLine);
                break;

            case 500:    // 5.Winlink ---> Login
                displayShow(" WINLINK>", "" , TB_T("Login Initiation ...", "Giris basliyor ..."), TB_T("Challenge -> waiting", "Challenge -> bekliyor"), "" , "");
                break;
            case 501:    // 5.Winlink ---> Login
                displayShow(" WINLINK>", "" , TB_T("Login Initiation ...", "Giris basliyor ..."), TB_T("Challenge -> sent", "Challenge -> gitti"), "" , "");
                break;
            case 502:    // 5.Winlink ---> Login
                displayShow(" WINLINK>", "" , TB_T("Login Initiation ...", "Giris basliyor ..."), TB_T("Challenge -> ack ...", "Challenge -> onay ..."), "" , "");
                break;

            case 5000:   // WINLINK: List Pend. Mail //
                displayShow("WLNK MENU>", TB_T("  Write Mail", "  Posta Yaz") , TB_T("> List Pend. Mails", "> Bekleyen Postalar"), TB_T("  Downloaded Mails", "  Inen Postalar"), TB_T("  Read Mail    (R#)", "  Posta Oku   (R#)"), lastLine);
                break;

            case 5010:    // WINLINK: Downloaded Mails //
                displayShow("WLNK MENU>", TB_T("  List Pend. Mails", "  Bekleyen Postalar"), TB_T("> Downloaded Mails", "> Inen Postalar"), TB_T("  Read Mail    (R#)", "  Posta Oku   (R#)"), TB_T("  Reply Mail   (Y#)", "  Posta Yanit (Y#)"), lastLine);
                break;
            case 50100:    // WINLINK: Downloaded Mails //
                displayShow(" WINLINK>", "" , TB_T("> Read SavedMails(", "> Kayitli Posta Oku(") + String(MSG_Utils::getNumWLNKMails()) + ")", TB_T("  Delete SavedMails", "  Kayitli Posta Sil"), "" , lastLine);
                break;
            case 50101:    // WINLINK: Downloaded Mails //
                {
                    String mailText = loadedWLNKMails[messagesIterator];

                    #ifdef HAS_TFT
                        #if defined(HELTEC_WIRELESS_TRACKER)
                            displayShow("WLNK MAIL>", mailText, TB_T("                 Next=Down", "               Ileri=Asagi"), "", "", "");
                        #else   // T-Deck
                            displayShow("WLNK MAIL>", mailText, TB_T("             Next=Down", "           Ileri=Asagi"), "", "", "");
                        #endif
                    #else
                        displayShow("WLNK MAIL>", "", mailText, "", "", TB_T("           Next=Down", "         Ileri=Asagi"));
                    #endif

                }
                break;
            case 50110:    // WINLINK: Downloaded Mails //
                displayShow(" WINLINK>", "" , TB_T("  Read SavedMails(", "  Kayitli Posta Oku(") + String(MSG_Utils::getNumWLNKMails()) + ")", TB_T("> Delete SavedMails", "> Kayitli Posta Sil"), "" , lastLine);
                break;
            case 50111:    // WINLINK: Downloaded Mails //
                displayShow("WLNK DEL>", "", TB_T("  DELETE ALL MAILS?", "  TUM POSTA SIL?"), "", "", TB_T(" Confirm = LP or '>'", " Onay = Uz veya '>'"));
                break;

            case 5020:    // WINLINK: Read Mail //
                displayShow("WLNK MENU>", TB_T("  Downloaded Mails", "  Inen Postalar"), TB_T("> Read Mail    (R#)", "> Posta Oku   (R#)"), TB_T("  Reply Mail   (Y#)", "  Posta Yanit (Y#)"), TB_T("  Forward Mail (F#)", "  Posta Ilet  (F#)"), lastLine);
                break;
            case 5021:
                displayShow("WLNK READ>", "", TB_T("    READ MAIL N.", "    POSTA OKU N.") + winlinkMailNumber, "", "", TB_T("<Back          Enter>", "<Geri          Tamam>"));
                break;

            case 5030:    // WINLINK: Reply Mail //
                displayShow("WLNK MENU>", TB_T("  Read Mail    (R#)", "  Posta Oku   (R#)"), TB_T("> Reply Mail   (Y#)", "> Posta Yanit (Y#)"), TB_T("  Forward Mail (F#)", "  Posta Ilet  (F#)"), TB_T("  Delete Mail  (K#)", "  Posta Sil   (K#)"), lastLine);
                break;
            case 5031:
                displayShow("WLNK REPLY", "", TB_T("   REPLY MAIL N.", "   POSTA YANIT N.") + winlinkMailNumber , "", "", TB_T("<Back          Enter>", "<Geri          Tamam>"));
                break;

            case 5040:    // WINLINK: Foward Mail //
                displayShow("WLNK MENU>", TB_T("  Reply Mail   (Y#)", "  Posta Yanit (Y#)"), TB_T("> Forward Mail (F#)", "> Posta Ilet  (F#)"), TB_T("  Delete Mail  (K#)", "  Posta Sil   (K#)"), "  Alias Menu", lastLine);
                break;
            case 5041:    // WINLINK: Forward Mail //
                displayShow("WLNK FORW>", "", TB_T("  FORWARD MAIL N.", "  POSTA ILET N.") + winlinkMailNumber , "", "", TB_T("<Back          Enter>", "<Geri          Tamam>"));
                break;
            case 5042:    // WINLINK: Forward Mail //
                displayShow("WLNK FORW>", TB_T("  FORWARD MAIL N.", "  POSTA ILET N.") + winlinkMailNumber , "To = " + winlinkAddressee, "", "", TB_T("<Back          Enter>", "<Geri          Tamam>"));
                break;

            case 5050:    // WINLINK: Delete Mail //
                displayShow("WLNK MENU>", TB_T("  Forward Mail (F#)", "  Posta Ilet  (F#)"), TB_T("> Delete Mail  (K#)", "> Posta Sil   (K#)"), "  Alias Menu", TB_T("  Log Out", "  Cikis"), lastLine);
                break;
            case 5051:    // WINLINK: Delete Mail //
                displayShow("WLNK DEL>", "", TB_T("   DELETE MAIL N.", "   POSTA SIL N.")  + winlinkMailNumber, "", "", TB_T("<Back          Enter>", "<Geri          Tamam>"));
                break;

            case 5060:    // WINLINK: Alias Menu //
                displayShow("WLNK MENU>", TB_T("  Delete Mail  (K#)", "  Posta Sil   (K#)"), "> Alias Menu", TB_T("  Log Out", "  Cikis"), TB_T("  Write Mail", "  Posta Yaz"), lastLine);
                break;
            case 5061:    // WINLINK: Alias Menu : Create Alias //
                displayShow("WLNK ALIAS", TB_T("> Create Alias", "> Alias Olustur") , TB_T("  Delete Alias ", "  Alias Sil "), TB_T("  List All Alias", "  Tum Alias"), "", lastLine);
                break;
            case 50610:   // WINLINK: Alias Menu : Create Alias //
                displayShow("WLNK ALIAS", "", TB_T("Write Alias to Create", "Olusturulacak Alias"), "     -> " + winlinkAlias, "", TB_T("<Back          Enter>", "<Geri          Tamam>"));
                break;
            case 50611:   // WINLINK: Alias Menu : Create Alias //
                displayShow("WLNK ALIAS", "", "      " + winlinkAlias + " =", winlinkAliasComplete, "", TB_T("<Back          Enter>", "<Geri          Tamam>"));
                break;
            case 5062:    // WINLINK: Alias Menu : Delete Alias //
                displayShow("WLNK ALIAS", TB_T("  Create Alias", "  Alias Olustur") , TB_T("> Delete Alias ", "> Alias Sil "), TB_T("  List All Alias", "  Tum Alias"), "", lastLine);
                break;
            case 50620:   // WINLINK: Alias Menu : Delete Alias //
                displayShow("WLNK ALIAS", TB_T("Write Alias to Delete", "Silinecek Alias"), "", "     -> " + winlinkAlias, "", TB_T("<Back          Enter>", "<Geri          Tamam>"));
                break;
            case 5063:    // WINLINK: Alias Menu : List Alias//
                displayShow("WLNK ALIAS", TB_T("  Create Alias", "  Alias Olustur") , TB_T("  Delete Alias ", "  Alias Sil "), TB_T("> List All Alias", "> Tum Alias"), "", lastLine);
                break;

            case 5070:    // WINLINK: Log Out MAIL //
                displayShow("WLNK MENU>", "  Alias Menu", TB_T("> Log Out", "> Cikis"), TB_T("  Write Mail", "  Posta Yaz"), TB_T("  List Pend. Mails", "  Bekleyen Postalar"), lastLine);
                break;

            case 5080:    // WINLINK: WRITE MAIL //
                displayShow("WLNK MENU>", TB_T("  Log Out", "  Cikis"), TB_T("> Write Mail", "> Posta Yaz"), TB_T("  List Pend. Mails", "  Bekleyen Postalar"), TB_T("  Downloaded Mails", "  Inen Postalar"), lastLine);
                break;
            case 5081:    // WINLINK: WRITE MAIL: Addressee //
                #ifdef HAS_TFT
                    #if defined(HELTEC_WIRELESS_TRACKER)
                        displayShow("WLNK MAIL>", TB_T("   --- Send Mail to ---", "   -- Posta alicisi --"), "-> " + winlinkAddressee, TB_T("<Back               Enter>", "<Geri               Tamam>"), "", "");
                    #else   // T-Deck?
                        displayShow("WLNK MAIL>", TB_T("   --- Send Mail to ---", "   -- Posta alicisi --"), "-> " + winlinkAddressee, TB_T("<Back           Enter>", "<Geri           Tamam>"), "", "");
                    #endif
                #else
                    displayShow("WLNK MAIL>", TB_T("--- Send Mail to ---", "-- Posta alicisi --"), "-> " + winlinkAddressee, "", "", TB_T("<Back          Enter>", "<Geri          Tamam>"));
                #endif
                break;
            case 5082:    // WINLINK: WRITE MAIL: Subject //
                #ifdef HAS_TFT
                    #if defined(HELTEC_WIRELESS_TRACKER)
                        displayShow("WLNK MAIL>", TB_T("   --- Write Subject ---", "   -- Konu yaz --"), "-> " + winlinkSubject, TB_T("<Back               Enter>", "<Geri               Tamam>"), "", "");
                    #else   // T-Deck?
                        displayShow("WLNK MAIL>", TB_T("   --- Write Subject ---", "   -- Konu yaz --"), "-> " + winlinkSubject, TB_T("<Back           Enter>", "<Geri           Tamam>"), "", "");
                    #endif
                #else
                    displayShow("WLNK MAIL>", TB_T("--- Write Subject ---", "-- Konu yaz --"), "-> " + winlinkSubject, "", "", TB_T("<Back          Enter>", "<Geri          Tamam>"));
                #endif
                break;
            case 5083:    // WINLINK: WRITE MAIL: Body //
                if (winlinkBody.length() <= 67) {
                    #ifdef HAS_TFT
                        #if defined(HELTEC_WIRELESS_TRACKER)
                            displayShow("WLNK MAIL>", TB_T("-- Body (Lenght =", "-- Govde (Uzun. =") + String(winlinkBody.length()) + ")", "-> " + winlinkBody, TB_T("<Clear Body         Enter>", "<Govde Sil          Tamam>"), "", "");
                        #else   // T-Deck
                            displayShow("WLNK MAIL>", TB_T("-- Body (Lenght =", "-- Govde (Uzun. =") + String(winlinkBody.length()) + ")", "-> " + winlinkBody, TB_T("<Clear Body     Enter>", "<Govde Sil      Tamam>"), "", "");
                        #endif
                    #else
                        displayShow("WLNK MAIL>", TB_T("-- Body (Lenght =", "-- Govde (Uzun. =") + String(winlinkBody.length()) + ")", "-> " + winlinkBody, "", "", TB_T("<Clear Body    Enter>", "<Govde Sil     Tamam>"));
                    #endif
                } else {
                    #ifdef HAS_TFT
                        #if defined(HELTEC_WIRELESS_TRACKER)
                            displayShow("WLNK MAIL>", TB_T("-- Body Too Long = ", "-- Govde Cok Uzun = ") + String(winlinkBody.length()), "-> " + winlinkBody, TB_T("<Clear Body", "<Govde Sil "), "", "");
                        #else   // T-Deck
                            displayShow("WLNK MAIL>", TB_T("-- Body Too Long = ", "-- Govde Cok Uzun = ") + String(winlinkBody.length()), "-> " + winlinkBody, TB_T("<Clear Body", "<Govde Sil "), "", "");
                        #endif
                    #else
                        displayShow("WLNK MAIL>", TB_T("-- Body Too Long = ", "-- Govde Cok Uzun = ") + String(winlinkBody.length()), "-> " + winlinkBody, "", "", TB_T("<Clear Body", "<Govde Sil "));
                    #endif
                }
                break;
            case 5084:    // WINLINK: WRITE MAIL: End Mail? //
                displayShow("WLNK MAIL>", "", TB_T("> End Mail", "> Bitir"), TB_T("  1 More Line", "  1 Satir Daha"), "", "      Up/Down Select>");
                break;
            case 5085:    // WINLINK: WRITE MAIL: One More Line(Body) //
                displayShow("WLNK MAIL>", "", TB_T("  End Mail", "  Bitir"), TB_T("> 1 More Line", "> 1 Satir Daha"), "", "      Up/Down Select>");
                break;

                // validar winlinkStatus = 0
                // check si no esta logeado o si

//////////
            case 60:    // 6. Extras ---> Send Email with GPS info
                displayShow(TB_T(" EXTRAS>", " EKSTRA>"), TB_T("  Flashlight    (", "  Fener         (") + checkProcessActive(flashlight) + ")", TB_T("> Send Email(GPS)", "> E-posta (GPS)"), "  Digipeater    (" + checkProcessActive(digipeaterActive) + ")", "  S.O.S.        (" + checkProcessActive(sosActive) + ")", lastLine);
                break;
            case 61:    // 6. Extras ---> Digipeater
                displayShow(TB_T(" EXTRAS>", " EKSTRA>"), TB_T("  Send Email(GPS)", "  E-posta (GPS)"), "> Digipeater    (" + checkProcessActive(digipeaterActive) + ")", "  S.O.S.        (" + checkProcessActive(sosActive) + ")", TB_T("  Beacon(GPS)+Comment", "  Beacon(GPS)+Yorum"), lastLine);
                break;
            case 62:    // 6. Extras ---> S.O.S.
                displayShow(TB_T(" EXTRAS>", " EKSTRA>"), "  Digipeater    (" + checkProcessActive(digipeaterActive) + ")", "> S.O.S.        (" + checkProcessActive(sosActive) + ")", TB_T("  Beacon(GPS)+Comment", "  Beacon(GPS)+Yorum"), TB_T("  Flashlight    (", "  Fener         (") + checkProcessActive(flashlight) + ")", lastLine);
                break;
            case 63:    // 6. Extras ---> Beacon(GPS) + Comment
                displayShow(TB_T(" EXTRAS>", " EKSTRA>"), "  S.O.S.        (" + checkProcessActive(sosActive) + ")", TB_T("> Beacon(GPS)+Comment", "> Beacon(GPS)+Yorum"), TB_T("  Flashlight    (", "  Fener         (") + checkProcessActive(flashlight) + ")", TB_T("  Send Email(GPS)", "  E-posta (GPS)"), lastLine);
                break;
            case 64:    // 6. Extras ---> Flashlight
                displayShow(TB_T(" EXTRAS>", " EKSTRA>"), TB_T("  Beacon(GPS)+Comment", "  Beacon(GPS)+Yorum"), TB_T("> Flashlight    (", "> Fener         (") + checkProcessActive(flashlight) + ")", TB_T("  Send Email(GPS)", "  E-posta (GPS)"), "  Digipeater    (" + checkProcessActive(digipeaterActive) + ")", lastLine);
                break;

            case 630:
                if (keyDetected) {
                    if (messageText.length() <= 67) {
                        String lengthStr = (messageText.length() < 10) ? "0" + String(messageText.length()) : String(messageText.length());
                        displayShow(TB_T(" COMMENT>", " YORUM>"), TB_T("Send this Comment in", "Bu yorum sonraki"), TB_T("the next GPS Beacon :", "GPS beacon'da gider:"), messageText, "", TB_T("<Back   (", "<Geri   (") + lengthStr + TB_T(")   Enter>", ")   Tamam>"));
                    } else {
                        displayShow(TB_T(" COMMENT>", " YORUM>"), TB_T("Comment is too long! ", "Yorum cok uzun! "), TB_T("Comment too long:", "Yorum cok uzun:") + messageText, "", "", TB_T("<Back   (", "<Geri   (") + String(messageText.length()) + ")>");
                    }
                } else {
                    displayShow(TB_T(" COMMENT>", " YORUM>"), TB_T("No Keyboard Detected", "Klavye bulunamadi"), "", "", "", lastLine);
                }
                break;

//////////
            case 9000:  //  9. multiPress Menu ---> Turn ON WiFi AP
                displayShow(TB_T(" CONFIG>", " AYARLAR>"), TB_T("> Turn Tracker Off", "> Tracker Kapat"),TB_T("  Config. WiFi AP", "  WiFi AP Ayari"),  "","",lastLine);
                break;
            case 9001:  //  9. multiPress Menu
                displayShow(TB_T(" CONFIG>", " AYARLAR>"), TB_T("  Turn Tracker Off", "  Tracker Kapat"),TB_T("> Config. WiFi AP", "> WiFi AP Ayari"),  "","",lastLine);
                break;


//////////
            case 0:       ///////////// MAIN MENU //////////////
                String hdopState, firstRowMainMenu, secondRowMainMenu, thirdRowMainMenu, fourthRowMainMenu, fifthRowMainMenu, sixthRowMainMenu;

                firstRowMainMenu = currentBeacon->callsign;
                if (Config.display.showSymbol) {
                    firstRowMainMenu = padForDisplay(firstRowMainMenu);
                    if (!symbolAvailable) {
                        firstRowMainMenu += currentBeacon->symbol;
                    }
                }

                if (disableGPS) {
                    secondRowMainMenu = "";
                    thirdRowMainMenu = "    LoRa APRS TNC";
                    fourthRowMainMenu = "";
                } else {
                    const auto time_now = now();
                    secondRowMainMenu = Utils::createDateString(time_now) + "   " + Utils::createTimeString(time_now);
                    if (time_now % 10 < 5) {
                        if (gps.satellites.value() == 0) {
                            thirdRowMainMenu = TB_T("WAITING FOR GPS:", "GPS BEKLENIYOR:");
                        } else {
                            thirdRowMainMenu = String(gps.location.lat(), 4);
                            thirdRowMainMenu += " ";
                            thirdRowMainMenu += String(gps.location.lng(), 4);
                        }
                    } else {
                        thirdRowMainMenu = String(Utils::getMaidenheadLocator(gps.location.lat(), gps.location.lng(), 8));
                        thirdRowMainMenu += " LoRa[";
                        switch (loraIndex) {
                            case 0: thirdRowMainMenu += "Eu]"; break;
                            case 1: thirdRowMainMenu += "PL]"; break;
                            case 2: thirdRowMainMenu += "UK]"; break;
                            case 3: thirdRowMainMenu += "US]"; break;
                        }
                    }

                    for (int i = thirdRowMainMenu.length(); i < 18; i++) {
                        thirdRowMainMenu += " ";
                    }

                    if (gps.hdop.hdop() > 5) {
                        hdopState = "X";
                    } else if (gps.hdop.hdop() > 2 && gps.hdop.hdop() < 5) {
                        hdopState = "-";
                    } else if (gps.hdop.hdop() <= 2) {
                        hdopState = "+";
                    }

                    if (gps.satellites.value() <= 9) thirdRowMainMenu += " ";
                    if (gpsIsActive) {
                        thirdRowMainMenu += String(gps.satellites.value());
                        thirdRowMainMenu += hdopState;
                    } else {
                        thirdRowMainMenu += "--";
                    }

                    String fourthRowAlt = String(gps.altitude.meters(),0);
                    fourthRowAlt.trim();
                    for (int a = fourthRowAlt.length(); a < 4; a++) {
                        fourthRowAlt = " " + fourthRowAlt;
                    }
                    String fourthRowSpeed = String(gps.speed.kmph(),0);
                    fourthRowSpeed.trim();
                    for (int b = fourthRowSpeed.length(); b < 3; b++) {
                        fourthRowSpeed = " " + fourthRowSpeed;
                    }
                    String fourthRowCourse = String(gps.course.deg(),0);
                    if (fourthRowSpeed == "  0") {
                        fourthRowCourse = "---";
                    } else {
                        fourthRowCourse.trim();
                        for (int c = fourthRowCourse.length(); c < 3; c++) {
                            fourthRowCourse = "0" + fourthRowCourse;
                        }
                    }
                    fourthRowMainMenu = "A=";
                    fourthRowMainMenu += fourthRowAlt;
                    fourthRowMainMenu += "m  ";
                    fourthRowMainMenu += fourthRowSpeed;
                    fourthRowMainMenu += "km/h  ";
                    fourthRowMainMenu += fourthRowCourse;
                    if (Config.telemetry.active && (time_now % 10 < 5) && wxModuleType != 0) {
                        fourthRowMainMenu = WX_Utils::readDataSensor(1);
                    }
                    if (MSG_Utils::getNumWLNKMails() > 0) {
                        fourthRowMainMenu = TB_T("** WLNK MAIL: ", "** WLNK POSTA: ");
                        fourthRowMainMenu += String(MSG_Utils::getNumWLNKMails());
                        fourthRowMainMenu += " **";
                    }
                    if (MSG_Utils::getNumAPRSMessages() > 0) {
                        fourthRowMainMenu = TB_T("*** MESSAGES: ", "*** MESAJLAR: ");
                        fourthRowMainMenu += String(MSG_Utils::getNumAPRSMessages());
                        fourthRowMainMenu += " ***";
                    }
                    if (!gpsIsActive) {
                        fourthRowMainMenu = TB_T("*** GPS  SLEEPING ***", "*** GPS UYKUDA ***");
                    }
                }

                if (showHumanHeading) {
                    fifthRowMainMenu = GPS_Utils::getCardinalDirection(gps.course.deg());
                } else {
                    fifthRowMainMenu = TB_T("LAST Rx = ", "SON Rx = ");
                    fifthRowMainMenu += MSG_Utils::getLastHeardTracker();
                }

                if (batteryConnected) {
                    String batteryVoltage = BATTERY_Utils::getBatteryInfoVoltage();
                    #if defined(TTGO_T_Beam_V0_7) || defined(TTGO_LORA32_V2_1_GPS) || defined(TTGO_LORA32_V2_1_915_GPS) || defined(TTGO_LORA32_V2_1_TNC) || defined(TTGO_LORA32_V2_1_915_TNC) || defined(HELTEC_V2_GPS) || defined(HELTEC_V2_GPS_915) || defined(HELTEC_V2_TNC) || defined(HELTEC_V3_GPS) || defined(HELTEC_V3_TNC) || defined(HELTEC_V3_2_GPS) || defined(HELTEC_V3_2_TNC) || defined(HELTEC_WIRELESS_TRACKER) || defined(HELTEC_WSL_V3_GPS_DISPLAY) || defined(HELTEC_WSL_V3_TNC) || defined(TTGO_T_DECK_GPS) || defined(TTGO_T_DECK_PLUS) || defined(LIGHTTRACKER_PLUS_1_0) || defined(TTGO_LORA32_T3S3_V1_2_GPS) || defined(TTGO_T_BEAM_1W) || defined(Hydra_1W_LoRa_E22_400M30S_GPS)
                        sixthRowMainMenu = TB_T("Battery: ", "Pil: ");
                        sixthRowMainMenu += batteryVoltage;
                        sixthRowMainMenu += "V   ";
                        sixthRowMainMenu += BATTERY_Utils::getPercentVoltageBattery(batteryVoltage.toFloat());
                        sixthRowMainMenu += "%";
                    #endif
                    #if defined(HAS_AXP192) || defined(HAS_AXP2101)
                        String batteryCharge = POWER_Utils::getBatteryInfoCurrent();
                        #ifdef HAS_AXP192
                            if (batteryCharge.toInt() == 0) {
                                sixthRowMainMenu = TB_T("Battery Charged ", "Pil Dolu ");
                                sixthRowMainMenu += batteryVoltage;
                                sixthRowMainMenu += "V";
                            } else if (batteryCharge.toInt() > 0) {
                                sixthRowMainMenu = TB_T("Bat: ", "Pil: ");
                                sixthRowMainMenu += batteryVoltage;
                                sixthRowMainMenu += TB_T("V (charging)", "V (sarj)");
                            } else {
                                sixthRowMainMenu = TB_T("Battery ", "Pil ");
                                sixthRowMainMenu += batteryVoltage;
                                sixthRowMainMenu += "V ";
                                sixthRowMainMenu += batteryCharge;
                                sixthRowMainMenu += "mA";
                            }
                        #endif
                        #ifdef HAS_AXP2101
                            if (Config.notification.lowBatteryBeep && !POWER_Utils::isCharging() && batteryCharge.toInt() < lowBatteryPercent) {
                                lowBatteryPercent = batteryCharge.toInt();
                                NOTIFICATION_Utils::lowBatteryBeep();
                                if (batteryCharge.toInt() < 6) {
                                    NOTIFICATION_Utils::lowBatteryBeep();
                                }
                            }
                            if (POWER_Utils::isCharging()) {
                                lowBatteryPercent = 21;
                            }
                            if (POWER_Utils::isCharging() && batteryCharge != "100") {
                                sixthRowMainMenu = TB_T("Bat: ", "Pil: ");
                                sixthRowMainMenu += String(batteryVoltage);
                                sixthRowMainMenu += TB_T("V (charging)", "V (sarj)");
                            } else if (!POWER_Utils::isCharging() && batteryCharge == "100") {
                                sixthRowMainMenu = TB_T("Battery Charged ", "Pil Dolu ");
                                sixthRowMainMenu += String(batteryVoltage);
                                sixthRowMainMenu += "V";
                            } else {
                                sixthRowMainMenu = TB_T("Battery  ", "Pil  ");
                                sixthRowMainMenu += String(batteryVoltage);
                                sixthRowMainMenu += "V   ";
                                sixthRowMainMenu += batteryCharge;
                                sixthRowMainMenu += "%";
                            }
                        #endif
                    #endif
                } else {
                    sixthRowMainMenu = TB_T("No Battery Connected", "Pil bagli degil");
                }
                displayShow(firstRowMainMenu,
                            secondRowMainMenu,
                            thirdRowMainMenu,
                            fourthRowMainMenu,
                            fifthRowMainMenu,
                            sixthRowMainMenu);
                break;
        }
    }

}