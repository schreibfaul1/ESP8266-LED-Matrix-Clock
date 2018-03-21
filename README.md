# ESP8266-LED-Matrix-Clock

Entwickelt mit Sloeber Eclipse

benötigt werden:
- 6 MAX7219 incl 8x LED-Matrix8x8
- RTC DS3231
- ein ESP12 Board

Kurzbeschreibung:

Beim Setup wird die aktuelle Zeit über WLAN von einem NTP geholt. Diese wird in sie MEZ bzw. MESZ umgerechnet und damit die
RTC syncornisiert. Wenn die Uhr in Betrieb ist wird dieser Vorgang täglich wiederholt.

![Display](https://github.com/schreibfaul1/)


