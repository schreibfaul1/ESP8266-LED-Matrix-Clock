// MatrixClock V1

#include <SPI.h>
#include <Ticker.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <wire.h>
#include "time.h"

#define SDA    	5      // Pin sda (I2C)
#define SCL    	4      // Pin scl (I2C)
#define CS     	15     // Pin cs  (SPI)
#define anzMAX 	6      // Anzahl der kaskadierten Module

char ssid[] = "Wolles-POWERLINE";         				// your network SSID (name)
char pass[] = "86472097922942014909";     				// your network password

unsigned short maxPosX = anzMAX * 8 - 1; 					//calculated maxposition
unsigned short LEDarr[anzMAX][8];						//character matrix to display (40*8)
unsigned short helpArrMAX[anzMAX * 8];    				//helperarray for chardecoding
unsigned short helpArrPos[anzMAX * 8];				//helperarray pos of chardecoding
unsigned int z_PosX = 0;									//xPosition im Display für Zeitanzeige
unsigned int d_PosX = 0;                 //xPosition im Display für Datumanzeige
bool f_tckr1s = false;
bool f_tckr50ms = false;
bool f_tckr24h = false;
unsigned long epoch = 0;
unsigned int localPort = 2390;           // local port to listen for UDP packets
const char* ntpServerName = "time.nist.gov";
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
IPAddress timeServerIP;                     // time.nist.gov NTP server address
tm *tt, ttm;

//Variablen für RTC DS3231
const unsigned char DS3231_ADDRESS = 0x68;
const unsigned char secondREG = 0x00;
const unsigned char minuteREG = 0x01;
const unsigned char hourREG = 0x02;
const unsigned char WTREG = 0x03;      //Wochentag
const unsigned char dateREG = 0x04;
const unsigned char monthREG = 0x05;
const unsigned char yearREG = 0x06;
const unsigned char alarm_min1secREG = 0x07;
const unsigned char alarm_min1minREG = 0x08;
const unsigned char alarm_min1hrREG = 0x09;
const unsigned char alarm_min1dateREG = 0x0A;
const unsigned char alarm_min2minREG = 0x0B;
const unsigned char alarm_min2hrREG = 0x0C;
const unsigned char alarm_min2dateREG = 0x0D;
const unsigned char controlREG = 0x0E;
const unsigned char statusREG = 0x0F;
const unsigned char ageoffsetREG = 0x10;
const unsigned char tempMSBREG = 0x11;
const unsigned char tempLSBREG = 0x12;
const unsigned char _24_hour_format = 0;
const unsigned char _12_hour_format = 1;
const unsigned char AM = 0;
const unsigned char PM = 1;

struct DateTime {
	unsigned short sek1, sek2, sek12, min1, min2, min12, std1, std2, std12;
	unsigned short tag1, tag2, tag12, mon1, mon2, mon12, jahr1, jahr2, jahr12, WT;
} MEZ;

// The object for the Ticker
Ticker tckr;
// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;

//Monatswerte
char M_arr[12][5] = { { '.', 'J', 'a', 'n', '.' }, { '.', 'F', 'e', 'b', '.' },
		{ '.', 'M', 'a', 'e', '.' }, { '.', 'A', 'p', 'r', '.' }, { '.', 'M', 'a',
				'i', ' ' }, { '.', 'J', 'u', 'n', 'i' }, { '.', 'J', 'u', 'l', 'i' }, {
				'.', 'A', 'u', 'g', ' ' }, { '.', 'S', 'e', 'p', 't' }, { '.', 'O', 'k',
				't', '.' }, { '.', 'N', 'o', 'v', ' ' }, { '.', 'D', 'e', 'z', '.' } };
//Tage
char WT_arr[7][3] = { { 'S', 'o', ',' }, { 'M', 'o', ',' }, { 'D', 'i', ',' }, {
		'M', 'i', ',' }, { 'D', 'o', ',' }, { 'F', 'r', ',' }, { 'S', 'a', ',' } };

// Zeichensatz 5x8 in einer 8x8 Matrix, 0,0 ist rechts oben
unsigned short const font1[96][9] = { { 0x07, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00 },   // 0x20, Space
		{ 0x07, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04, 0x00 },   // 0x21, !
		{ 0x07, 0x09, 0x09, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0x22, "
		{ 0x07, 0x0a, 0x0a, 0x1f, 0x0a, 0x1f, 0x0a, 0x0a, 0x00 },   // 0x23, #
		{ 0x07, 0x04, 0x0f, 0x14, 0x0e, 0x05, 0x1e, 0x04, 0x00 },   // 0x24, $
		{ 0x07, 0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13, 0x00 },   // 0x25, %
		{ 0x07, 0x04, 0x0a, 0x0a, 0x0a, 0x15, 0x12, 0x0d, 0x00 },   // 0x26, &
		{ 0x07, 0x04, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0x27, '
		{ 0x07, 0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02, 0x00 },   // 0x28, (
		{ 0x07, 0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08, 0x00 },   // 0x29, )
		{ 0x07, 0x04, 0x15, 0x0e, 0x1f, 0x0e, 0x15, 0x04, 0x00 },   // 0x2a, *
		{ 0x07, 0x00, 0x04, 0x04, 0x1f, 0x04, 0x04, 0x00, 0x00 },   // 0x2b, +
		{ 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x02 },   // 0x2c, ,
		{ 0x07, 0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00, 0x00 },   // 0x2d, -
		{ 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x00 },   // 0x2e, .
		{ 0x07, 0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10, 0x00 },   // 0x2f, /
		{ 0x07, 0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e, 0x00 },   // 0x30, 0
		{ 0x07, 0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e, 0x00 },   // 0x31, 1
		{ 0x07, 0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f, 0x00 },   // 0x32, 2
		{ 0x07, 0x0e, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0e, 0x00 },   // 0x33, 3
		{ 0x07, 0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02, 0x00 },   // 0x34, 4
		{ 0x07, 0x1f, 0x10, 0x1e, 0x01, 0x01, 0x11, 0x0e, 0x00 },   // 0x35, 5
		{ 0x07, 0x06, 0x08, 0x10, 0x1e, 0x11, 0x11, 0x0e, 0x00 },   // 0x36, 6
		{ 0x07, 0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08, 0x00 },   // 0x37, 7
		{ 0x07, 0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e, 0x00 },   // 0x38, 8
		{ 0x07, 0x0e, 0x11, 0x11, 0x0f, 0x01, 0x02, 0x0c, 0x00 },   // 0x39, 9
		{ 0x04, 0x00, 0x03, 0x03, 0x00, 0x03, 0x03, 0x00, 0x00 },   // 0x3a, :
		{ 0x07, 0x00, 0x0c, 0x0c, 0x00, 0x0c, 0x04, 0x08, 0x00 },   // 0x3b, ;
		{ 0x07, 0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02, 0x00 },   // 0x3c, <
		{ 0x07, 0x00, 0x00, 0x1f, 0x00, 0x1f, 0x00, 0x00, 0x00 },   // 0x3d, =
		{ 0x07, 0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08, 0x00 },   // 0x3e, >
		{ 0x07, 0x0e, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04, 0x00 },   // 0x3f, ?
		{ 0x07, 0x0e, 0x11, 0x17, 0x15, 0x17, 0x10, 0x0f, 0x00 },   // 0x40, @
		{ 0x07, 0x04, 0x0a, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x00 },   // 0x41, A
		{ 0x07, 0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e, 0x00 },   // 0x42, B
		{ 0x07, 0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e, 0x00 },   // 0x43, C
		{ 0x07, 0x1e, 0x09, 0x09, 0x09, 0x09, 0x09, 0x1e, 0x00 },   // 0x44, D
		{ 0x07, 0x1f, 0x10, 0x10, 0x1c, 0x10, 0x10, 0x1f, 0x00 },   // 0x45, E
		{ 0x07, 0x1f, 0x10, 0x10, 0x1f, 0x10, 0x10, 0x10, 0x00 },   // 0x46, F
		{ 0x07, 0x0e, 0x11, 0x10, 0x10, 0x13, 0x11, 0x0f, 0x00 },   // 0x37, G
		{ 0x07, 0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11, 0x00 },   // 0x48, H
		{ 0x07, 0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e, 0x00 },   // 0x49, I
		{ 0x07, 0x1f, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0c, 0x00 },   // 0x4a, J
		{ 0x07, 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11, 0x00 },   // 0x4b, K
		{ 0x07, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f, 0x00 },   // 0x4c, L
		{ 0x07, 0x11, 0x1b, 0x15, 0x11, 0x11, 0x11, 0x11, 0x00 },   // 0x4d, M
		{ 0x07, 0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x00 },   // 0x4e, N
		{ 0x07, 0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e, 0x00 },   // 0x4f, O
		{ 0x07, 0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10, 0x00 },   // 0x50, P
		{ 0x07, 0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d, 0x00 },   // 0x51, Q
		{ 0x07, 0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11, 0x00 },   // 0x52, R
		{ 0x07, 0x0e, 0x11, 0x10, 0x0e, 0x01, 0x11, 0x0e, 0x00 },   // 0x53, S
		{ 0x07, 0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00 },   // 0x54, T
		{ 0x07, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e, 0x00 },   // 0x55, U
		{ 0x07, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04, 0x00 },   // 0x56, V
		{ 0x07, 0x11, 0x11, 0x11, 0x15, 0x15, 0x1b, 0x11, 0x00 },   // 0x57, W
		{ 0x07, 0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11, 0x00 },   // 0x58, X
		{ 0x07, 0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04, 0x00 },   // 0x59, Y
		{ 0x07, 0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f, 0x00 },   // 0x5a, Z
		{ 0x07, 0x0e, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0e, 0x00 },   // 0x5b, [
		{ 0x07, 0x10, 0x10, 0x08, 0x04, 0x02, 0x01, 0x01, 0x00 },   // 0x5c, '\'
		{ 0x07, 0x0e, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0e, 0x00 },   // 0x5d, ]
		{ 0x07, 0x04, 0x0a, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0x5e, ^
		{ 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x00 },   // 0x5f, _
		{ 0x07, 0x04, 0x04, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0x60, `
		{ 0x07, 0x00, 0x0e, 0x01, 0x0d, 0x13, 0x13, 0x0d, 0x00 },   // 0x61, a
		{ 0x07, 0x10, 0x10, 0x10, 0x1c, 0x12, 0x12, 0x1c, 0x00 },   // 0x62, b
		{ 0x07, 0x00, 0x00, 0x00, 0x0e, 0x10, 0x10, 0x0e, 0x00 },   // 0x63, c
		{ 0x07, 0x01, 0x01, 0x01, 0x07, 0x09, 0x09, 0x07, 0x00 },   // 0x64, d
		{ 0x07, 0x00, 0x00, 0x0e, 0x11, 0x1f, 0x10, 0x0f, 0x00 },   // 0x65, e
		{ 0x07, 0x06, 0x09, 0x08, 0x1c, 0x08, 0x08, 0x08, 0x00 },   // 0x66, f
		{ 0x07, 0x00, 0x0e, 0x11, 0x13, 0x0d, 0x01, 0x01, 0x0e },   // 0x67, g
		{ 0x07, 0x10, 0x10, 0x10, 0x16, 0x19, 0x11, 0x11, 0x00 },   // 0x68, h
		{ 0x05, 0x00, 0x02, 0x00, 0x06, 0x02, 0x02, 0x07, 0x00 },   // 0x69, i
		{ 0x07, 0x00, 0x02, 0x00, 0x06, 0x02, 0x02, 0x12, 0x0c },   // 0x6a, j
		{ 0x07, 0x10, 0x10, 0x12, 0x14, 0x18, 0x14, 0x12, 0x00 },   // 0x6b, k
		{ 0x05, 0x06, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x00 },   // 0x6c, l
		{ 0x07, 0x00, 0x00, 0x0a, 0x15, 0x15, 0x11, 0x11, 0x00 },   // 0x6d, m
		{ 0x07, 0x00, 0x00, 0x16, 0x19, 0x11, 0x11, 0x11, 0x00 },   // 0x6e, n
		{ 0x07, 0x00, 0x00, 0x0e, 0x11, 0x11, 0x11, 0x0e, 0x00 },   // 0x6f, o
		{ 0x07, 0x00, 0x00, 0x1c, 0x12, 0x12, 0x1c, 0x10, 0x10 },   // 0x70, p
		{ 0x07, 0x00, 0x00, 0x07, 0x09, 0x09, 0x07, 0x01, 0x01 },   // 0x71, q
		{ 0x07, 0x00, 0x00, 0x16, 0x19, 0x10, 0x10, 0x10, 0x00 },   // 0x72, r
		{ 0x07, 0x00, 0x00, 0x0f, 0x10, 0x0e, 0x01, 0x1e, 0x00 },   // 0x73, s
		{ 0x07, 0x08, 0x08, 0x1c, 0x08, 0x08, 0x09, 0x06, 0x00 },   // 0x74, t
		{ 0x07, 0x00, 0x00, 0x11, 0x11, 0x11, 0x13, 0x0d, 0x00 },   // 0x75, u
		{ 0x07, 0x00, 0x00, 0x11, 0x11, 0x11, 0x0a, 0x04, 0x00 },   // 0x76, v
		{ 0x07, 0x00, 0x00, 0x11, 0x11, 0x15, 0x15, 0x0a, 0x00 },   // 0x77, w
		{ 0x07, 0x00, 0x00, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x00 },   // 0x78, x
		{ 0x07, 0x00, 0x00, 0x11, 0x11, 0x0f, 0x01, 0x11, 0x0e },   // 0x79, y
		{ 0x07, 0x00, 0x00, 0x1f, 0x02, 0x04, 0x08, 0x1f, 0x00 },   // 0x7a, z
		{ 0x07, 0x06, 0x08, 0x08, 0x10, 0x08, 0x08, 0x06, 0x00 },   // 0x7b, {
		{ 0x07, 0x04, 0x04, 0x04, 0x00, 0x04, 0x04, 0x04, 0x00 },   // 0x7c, |
		{ 0x07, 0x0c, 0x02, 0x02, 0x01, 0x02, 0x02, 0x0c, 0x00 },   // 0x7d, }
		{ 0x07, 0x08, 0x15, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0x7e, ~
		{ 0x07, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x00 }    // 0x7f, DEL
};
//**************************************************************************************************
void connect_to_WiFi() {  // We start by connecting to a WiFi network
	Serial.println("");
	Serial.print("Connecting to ");
	Serial.println(ssid);
	WiFi.begin(ssid, pass);

	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}
	Serial.println("");

	Serial.println("WiFi connected");
	Serial.print("IP address: ");
	Serial.println(WiFi.localIP());

	Serial.println("Starting UDP");
	udp.begin(localPort);
	Serial.print("Local port: ");
	Serial.println(udp.localPort());
}
//**************************************************************************************************
tm* connectNTP() { //if response from NTP was succesfull return *tm else return a nullpointer
	WiFi.hostByName(ntpServerName, timeServerIP);
	Serial.println(timeServerIP);
	Serial.println("sending NTP packet...");
	// set all bytes in the buffer to 0
	memset(packetBuffer, 0, NTP_PACKET_SIZE);
	// Initialize values needed to form NTP request
	// (see URL above for details on the packets)
	packetBuffer[0] = 0b11100011;   // LI, Version, Mode
	packetBuffer[1] = 0;     // Stratum, or type of clock
	packetBuffer[2] = 6;     // Polling Interval
	packetBuffer[3] = 0xEC;  // Peer Clock Precision
	// 8 bytes of zero for Root Delay & Root Dispersion
	packetBuffer[12] = 49;
	packetBuffer[13] = 0x4E;
	packetBuffer[14] = 49;
	packetBuffer[15] = 52;
	// all NTP fields have been given values, now
	// you can send a packet requesting a timestamp:
	udp.beginPacket(timeServerIP, 123); //NTP requests are to port 123
	udp.write(packetBuffer, NTP_PACKET_SIZE);
	udp.endPacket();
	delay(1000);                 // wait to see if a reply is available
	int cb = udp.parsePacket();
	udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
	//the timestamp starts at byte 40 of the received packet and is four bytes,
	// or two words, long. First, esxtract the two words:
	unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
	unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
	// combine the four bytes (two words) into a long integer
	// this is NTP time (seconds since Jan 1 1900):
	unsigned long secsSince1900 = highWord << 16 | lowWord;
	// now convert NTP time into everyday time:
	const unsigned long seventyYears = 2208988800UL;
	// subtract seventy years:
	epoch = secsSince1900 - seventyYears +2; //+2000ms Verarbeitungszeit

	time_t t;
	t = epoch;
	tm* tt;
	tt = localtime(&t);
	if (cb == 48)
		return (tt);
	else
		return (NULL);
}
//**************************************************************************************************
void rtc_init(unsigned char sda, unsigned char scl) {
	Wire.begin(sda, scl);
	rtc_Write(controlREG, 0x00);
}
//**************************************************************************************************
// BCD Code
//**************************************************************************************************
unsigned char dec2bcd(unsigned char x) { //value 0...99
	unsigned char z, e, r;
	e = x % 10;
	z = x / 10;
	z = z << 4;
	r = e | z;
	return (r);
}
unsigned char bcd2dec(unsigned char x) { //value 0...99
	int z, e;
	e = x & 0x0F;
	z = x & 0xF0;
	z = z >> 4;
	z = z * 10;
	return (z + e);
}
//**************************************************************************************************
// RTC I2C Code
//**************************************************************************************************
unsigned char rtc_Read(unsigned char regaddress) {
	Wire.beginTransmission(DS3231_ADDRESS);
	Wire.write(regaddress);
	Wire.endTransmission();
	Wire.requestFrom((unsigned char) DS3231_ADDRESS, (unsigned char) 1);
	return (Wire.read());
}
void rtc_Write(unsigned char regaddress, unsigned char value) {
	Wire.beginTransmission(DS3231_ADDRESS);
	Wire.write(regaddress);
	Wire.write(value);
	Wire.endTransmission();
}
//**************************************************************************************************
unsigned char rtc_sekunde() {
	return (bcd2dec(rtc_Read(secondREG)));
}
unsigned char rtc_minute() {
	return (bcd2dec(rtc_Read(minuteREG)));
}
unsigned char rtc_stunde() {
	return (bcd2dec(rtc_Read(hourREG)));
}
unsigned char rtc_wochentag() {
	return (bcd2dec(rtc_Read(WTREG)));
}
unsigned char rtc_tag() {
	return (bcd2dec(rtc_Read(dateREG)));
}
unsigned char rtc_monat() {
	return (bcd2dec(rtc_Read(monthREG)));
}
unsigned char rtc_jahr() {
	return (bcd2dec(rtc_Read(yearREG)));
}
void rtc_sekunde(unsigned char sek) {
	rtc_Write(secondREG, (dec2bcd(sek)));
}
void rtc_minute(unsigned char min) {
	rtc_Write(minuteREG, (dec2bcd(min)));
}
void rtc_stunde(unsigned char std) {
	rtc_Write(hourREG, (dec2bcd(std)));
}
void rtc_wochentag(unsigned char wt) {
	rtc_Write(WTREG, (dec2bcd(wt)));
}
void rtc_tag(unsigned char tag) {
	rtc_Write(dateREG, (dec2bcd(tag)));
}
void rtc_monat(unsigned char mon) {
	rtc_Write(monthREG, (dec2bcd(mon)));
}
void rtc_jahr(unsigned char jahr) {
	rtc_Write(yearREG, (dec2bcd(jahr)));
}
//**************************************************************************************************
void rtc_set(tm* tt) {
	rtc_sekunde((unsigned char) tt->tm_sec);
	rtc_minute((unsigned char) tt->tm_min);
	rtc_stunde((unsigned char) tt->tm_hour);
	rtc_tag((unsigned char) tt->tm_mday);
	rtc_monat((unsigned char) tt->tm_mon + 1);
	rtc_jahr((unsigned char) tt->tm_year - 100);
	rtc_wochentag((unsigned char) tt->tm_wday);
}
//**************************************************************************************************
float rtc_temp() {
	float t = 0.0;
	unsigned char lowByte = 0;
	signed char highByte = 0;
	lowByte = rtc_Read(tempLSBREG);
	highByte = rtc_Read(tempMSBREG);
	lowByte >>= 6;
	lowByte &= 0x03;
	t = ((float) lowByte);
	t *= 0.25;
	t += highByte;
	return (t); // return temp value
}
//**************************************************************************************************
void rtc2mez() {

	unsigned short JaZiff;       //Jahresziffer
	unsigned short JhZiff = 6;   //Jahrhundertziffer für 20.Jahrhundert
	unsigned short TaZiff;       //Tagesziffer
	unsigned short WoTag;        //Wochentag
	unsigned short SJK = 0;        //Schaltjahreskorrektur
	unsigned short ZDiff;        //Zeitdifferenz UTC MEZ/MESZ
	unsigned short MoZiff[12] = { 0, 3, 3, 6, 1, 4, 6, 2, 5, 0, 3, 5 }; //Monatsziffer
	unsigned short Tage_Monat[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30,
			31 };
	unsigned short Jahr, Tag, Monat, Stunde, Minute, Sekunde;
	//RTC_setMonat(3);
	Jahr = rtc_jahr();
	if (Jahr > 99)
		Jahr = 0;
	Monat = rtc_monat();
	if (Monat > 12)
		Monat = 0;
	Tag = rtc_tag();
	if (Tag > 31)
		Tag = 0;
	Stunde = rtc_stunde();
	if (Stunde > 23)
		Stunde = 0;
	Minute = rtc_minute();
	if (Minute > 59)
		Minute = 0;
	Sekunde = rtc_sekunde();
	if (Sekunde > 59)
		Sekunde = 0;

	JaZiff = ((Jahr + Jahr / 4) % 7);
	TaZiff = (Tag % 7);
	if ((Jahr % 4) == 0)                          //Schaltjahr ?
	{
		Tage_Monat[1] = 29;                      //dann hat der Febr 29 Tage
		if (Monat < 3)
			SJK = 6;
		else
			SJK = 0;
	}
	WoTag = ((TaZiff + MoZiff[Monat - 1] + JhZiff + JaZiff + SJK) % 7);

	if (Monat < 3 || Monat > 10)
		ZDiff = 1; // keine Sommerzeit in Jan, Feb, Nov, Dez
	if (Monat > 3 && Monat < 10)
		ZDiff = 2; // Sommerz. in Apr, Mai, Jun, Jul, Aug, Sep
	if (Monat == 3) {
		ZDiff = 1;
		if (Tag > 24)     //Sommerzeit ab letzten Sonntag des Monats
		{
			if (Tag == 25) {
				if ((Tag + WoTag) < 26)
					ZDiff = 2;
			}
			if (Tag == 26) {
				if ((Tag + WoTag) < 28)
					ZDiff = 2;
			}
			if (Tag == 27) {
				if ((Tag + WoTag) < 30)
					ZDiff = 2;
			}
			if (Tag == 28) {
				if ((Tag + WoTag) < 32)
					ZDiff = 2;
			}
			if (Tag == 29) {
				if ((Tag + WoTag) < 34)
					ZDiff = 2;
			}
			if (Tag == 30) {
				if ((Tag + WoTag) < 36)
					ZDiff = 2;
			}
			if (Tag == 31) {
				if ((Tag + WoTag) < 38)
					ZDiff = 2;
			}
			if ((ZDiff == 2) && (Stunde + 1 < 2) && (WoTag == 0))
				ZDiff = 1; //erst ab 02 Uhr
		}
	}
	if (Monat == 10) {
		ZDiff = 2;
		if (Tag > 24)     //Sommerzeit ab letzten Sonntag des Monats
		{
			if (Tag == 25) {
				if ((Tag + WoTag) < 26)
					ZDiff = 1;
			}
			if (Tag == 26) {
				if ((Tag + WoTag) < 28)
					ZDiff = 1;
			}
			if (Tag == 27) {
				if ((Tag + WoTag) < 30)
					ZDiff = 1;
			}
			if (Tag == 28) {
				if ((Tag + WoTag) < 32)
					ZDiff = 1;
			}
			if (Tag == 29) {
				if ((Tag + WoTag) < 34)
					ZDiff = 1;
			}
			if (Tag == 30) {
				if ((Tag + WoTag) < 36)
					ZDiff = 1;
			}
			if (Tag == 25) {
				if ((Tag + WoTag) < 38)
					ZDiff = 1;
			}
			if ((ZDiff == 1) && (Stunde == 0) && (WoTag == 0))
				ZDiff = 2; //erst ab 02 Uhr
		}
	}
	Stunde = Stunde + ZDiff;
	if (Stunde > 23)       //Tageskorrektur
	{
		Stunde = Stunde - 24; //kann 0 oder 1 sein
		Tag = Tag + 1;
		WoTag = WoTag + 1;
		if (Tag > Tage_Monat[Monat - 1]) {
			Tag = 1;
			Monat = Monat + 1;
			if (Monat > 12) {
				Monat = 1;
				Jahr = Jahr + 1;
			}
		}
	}
	MEZ.WT = WoTag;          //So=0, Mo=1, Di=2 ...
	MEZ.sek1 = Sekunde % 10;
	MEZ.sek2 = Sekunde / 10;
	MEZ.sek12 = Sekunde;
	MEZ.min1 = Minute % 10;
	MEZ.min2 = Minute / 10;
	MEZ.min12 = Minute;
	MEZ.std1 = Stunde % 10;
	MEZ.std2 = Stunde / 10;
	MEZ.std12 = Stunde;
	MEZ.tag12 = Tag;
	MEZ.tag1 = Tag % 10;
	MEZ.tag2 = Tag / 10;
	MEZ.mon12 = Monat;
	MEZ.mon1 = Monat % 10;
	MEZ.mon2 = Monat / 10;
	MEZ.jahr12 = Jahr;
	MEZ.jahr1 = Jahr % 10;
	MEZ.jahr2 = Jahr / 10;
}

//*************************************************************************************************
const unsigned short InitArr[7][2] = { { 0x0C, 0x00 },    // display off
		{ 0x00, 0xFF },    // no LEDtest
		{ 0x09, 0x00 },    // BCD off
		{ 0x0F, 0x00 },    // normal operation
		{ 0x0B, 0x07 },    // start display
		{ 0x0A, 0x04 },    // brightness
		{ 0x0C, 0x01 }     // display on
};
//**************************************************************************************************
void max7219_init()  //all MAX7219 init
{
	unsigned short i, j;
	for (i = 0; i < 7; i++) {
		digitalWrite(CS, LOW);
		delayMicroseconds(1);
		for (j = 0; j < anzMAX; j++) {
			SPI.write(InitArr[i][0]);  //register
			SPI.write(InitArr[i][1]);  //value
		}
		digitalWrite(CS, HIGH);
	}
}
//**************************************************************************************************
void max7219_set_brightness(unsigned short br)  //brightness MAX7219
{
	unsigned short j;
	if (br < 16) {
		digitalWrite(CS, LOW);
		delayMicroseconds(1);
		for (j = 0; j < anzMAX; j++) {
			SPI.write(0x0A);  //register
			SPI.write(br);    //value
		}
		digitalWrite(CS, HIGH);
	}
}
//**************************************************************************************************
void helpArr_init(void)  //helperarray init
{
	unsigned short i, j, k;
	j = 0;
	k = 0;
	for (i = 0; i < anzMAX * 8; i++) {
		helpArrPos[i] = (1 << j);   //bitmask
		helpArrMAX[i] = k;
		j++;
		if (j > 7) {
			j = 0;
			k++;
		}
	}
}
//**************************************************************************************************
void clear_Display()   //clear all
{
	unsigned short i, j;
	for (i = 0; i < 8; i++)     //8 rows
	{
		digitalWrite(CS, LOW);
		delayMicroseconds(1);
		for (j = anzMAX; j > 0; j--) {
			LEDarr[j - 1][i] = 0;       //LEDarr clear
			SPI.write(i + 1);           //current row
			SPI.write(LEDarr[j - 1][i]);
		}
		digitalWrite(CS, HIGH);
	}
}
//**************************************************************************************************
void refresh_display() //take info into LEDarr
{
	unsigned short i, j;

	for (i = 0; i < 8; i++)     //8 rows
	{
		digitalWrite(CS, LOW);
		delayMicroseconds(1);
		for (j = anzMAX; j > 0; j--) {
			SPI.write(i + 1);  //current row
			SPI.write(LEDarr[j - 1][i]);
		}
		digitalWrite(CS, HIGH);
	}
}
//**************************************************************************************************
void char2Arr(unsigned short ch, int PosX, short PosY) { //characters into arr
	int i, j, k, l, m, o1, o2, o3, o4;  //in LEDarr
	PosX++;
	k = ch - 32;                        //ASCII position in font
	if ((k >= 0) && (k < 96))           //character found in font?
	{
		o4 = font1[k][0];                 //character width
		o3 = 1 << (o4 - 2);
		for (i = 0; i < o4; i++) {
			if (((PosX - i <= maxPosX) && (PosX - i >= 0))
					&& ((PosY > -8) && (PosY < 8))) //within matrix?
			{
				o1 = helpArrPos[PosX - i];
				o2 = helpArrMAX[PosX - i];
				for (j = 0; j < 8; j++) {
					if (((PosY >= 0) && (PosY <= j)) || ((PosY < 0) && (j < PosY + 8))) //scroll vertical
					{
						l = font1[k][j + 1];
						m = (l & (o3 >> i));  //e.g. o4=7  0zzzzz0, o4=4  0zz0
						if (m > 0)
							LEDarr[o2][j - PosY] = LEDarr[o2][j - PosY] | (o1);  //set point
						else
							LEDarr[o2][j - PosY] = LEDarr[o2][j - PosY] & (~o1); //clear point
					}
				}
			}
		}
	}
}

//**************************************************************************************************
void timer50ms() {
	static unsigned int cnt50ms = 0;
	static unsigned int cnt1s = 0;
	static unsigned int cnt1h = 0;
	f_tckr50ms = true;
	cnt50ms++;
	if (cnt50ms == 20) {
		f_tckr1s = true; // 1 sec
		cnt1s++;
		cnt50ms = 0;
	}
	if (cnt1s == 3600) { // 1h
		cnt1h++;
		cnt1s = 0;
	}
	if (cnt1h == 24) { // 1d
		f_tckr24h = true;
		cnt1h = 0;
	}
}
//**************************************************************************************************
//
//The setup function is called once at startup of the sketch
void setup() {
	// Add your initialization code here
	pinMode(CS, OUTPUT);
	digitalWrite(CS, HIGH);
	Serial.begin(115200);
	//rtc.init(SDA, SCL);
	SPI.begin();
	helpArr_init();
	max7219_init();
	rtc_init(SDA, SCL);
	clear_Display();
	tckr.attach(0.05, timer50ms);    // every 50 msec
	connect_to_WiFi();
	tm* tt;
	tt = connectNTP();
	if (tt != NULL)
		rtc_set(tt);
	else
		Serial.println("no timepacket received");
}
//**************************************************************************************************
// The loop function is called in an endless loop
void loop() {
	//Add your repeated code here
	unsigned int sek1 = 0, sek2 = 0, min1 = 0, min2 = 0, std1 = 0, std2 = 0;
	unsigned int sek11 = 0, sek12 = 0, sek21 = 0, sek22 = 0;
	unsigned int min11 = 0, min12 = 0, min21 = 0, min22 = 0;
	unsigned int std11 = 0, std12 = 0, std21 = 0, std22 = 0;
	signed int x = 0; //x1,x2;
	signed int y = 0, y1 = 0, y2 = 0;
	bool updown = false;
	unsigned int sc1 = 0, sc2 = 0, sc3 = 0, sc4 = 0, sc5 = 0, sc6 = 0;
	bool f_scrollend_y = false;
	unsigned int f_scroll_x = false;

	z_PosX = maxPosX;
	d_PosX = -8;
	//  x=0; x1=0; x2=0;

	refresh_display();
	updown = true;
	if (updown == false) {
		y2 = -9;
		y1 = 8;
	}
	if (updown == true) { //scroll  up to down
		y2 = 8;
		y1 = -8;
	}
	while (true) {
		yield();
		if (f_tckr24h == true) { //syncronisize RTC every day
			f_tckr24h = false;
			tm* tt;
			tt = connectNTP();
			if (tt != NULL)
				rtc_set(tt);
			else
				Serial.println("no timepacket received");
		}
		if (f_tckr1s == true)        // flag 1sek
		{
			rtc2mez();
			sek1 = MEZ.sek1;
			sek2 = MEZ.sek2;
			min1 = MEZ.min1;
			min2 = MEZ.min2;
			std1 = MEZ.std1;
			std2 = MEZ.std2;
			y = y2;                 //scroll updown
			sc1 = 1;
			sek1++;
			if (sek1 == 10) {
				sc2 = 1;
				sek2++;
				sek1 = 0;
			}
			if (sek2 == 6) {
				min1++;
				sek2 = 0;
				sc3 = 1;
			}
			if (min1 == 10) {
				min2++;
				min1 = 0;
				sc4 = 1;
			}
			if (min2 == 6) {
				std1++;
				min2 = 0;
				sc5 = 1;
			}
			if (std1 == 10) {
				std2++;
				std1 = 0;
				sc6 = 1;
			}
			if ((std2 == 2) && (std1 == 4)) {
				std1 = 0;
				std2 = 0;
				sc6 = 1;
			}

			sek11 = sek12;
			sek12 = sek1;
			sek21 = sek22;
			sek22 = sek2;
			min11 = min12;
			min12 = min1;
			min21 = min22;
			min22 = min2;
			std11 = std12;
			std12 = std1;
			std21 = std22;
			std22 = std2;
			f_tckr1s = false;
			if (MEZ.sek12 == 45)
				f_scroll_x = true;
		} // end 1s
		if (f_tckr50ms == true) {
			f_tckr50ms = false;
			if (f_scroll_x == true) {
				z_PosX++;
				d_PosX++;
				if (d_PosX == 95)
					z_PosX = 0;
				if (z_PosX == maxPosX) {
					f_scroll_x = false;
					d_PosX = -8;
				}
			}
			if (sc1 == 1) {
				if (updown == 1)
					y--;
				else
					y++;
				char2Arr(48 + sek12, z_PosX - 42, y);
				char2Arr(48 + sek11, z_PosX - 42, y + y1);
				if (y == 0) {
					sc1 = 0;
					f_scrollend_y = true;
				}
			}
			else
				char2Arr(48 + sek1, z_PosX - 42, 0);

			if (sc2 == 1) {
				char2Arr(48 + sek22, z_PosX - 36, y);
				char2Arr(48 + sek21, z_PosX - 36, y + y1);
				if (y == 0)
					sc2 = 0;
			}
			else
				char2Arr(48 + sek2, z_PosX - 36, 0);

			char2Arr(':', z_PosX - 32, 0);

			if (sc3 == 1) {
				char2Arr(48 + min12, z_PosX - 25, y);
				char2Arr(48 + min11, z_PosX - 25, y + y1);
				if (y == 0)
					sc3 = 0;
			}
			else
				char2Arr(48 + min1, z_PosX - 25, 0);

			if (sc4 == 1) {
				char2Arr(48 + min22, z_PosX - 19, y);
				char2Arr(48 + min21, z_PosX - 19, y + y1);
				if (y == 0)
					sc4 = 0;
			}
			else
				char2Arr(48 + min2, z_PosX - 19, 0);

			char2Arr(':', z_PosX - 15 + x, 0);

			if (sc5 == 1) {
				char2Arr(48 + std12, z_PosX - 8, y);
				char2Arr(48 + std11, z_PosX - 8, y + y1);
				if (y == 0)
					sc5 = 0;
			}
			else
				char2Arr(48 + std1, z_PosX - 8, 0);

			if (sc6 == 1) {
				char2Arr(48 + std22, z_PosX - 2, y);
				char2Arr(48 + std21, z_PosX - 2, y + y1);
				if (y == 0)
					sc6 = 0;
			}
			else
				char2Arr(48 + std2, z_PosX - 2, 0);

			char2Arr(WT_arr[MEZ.WT][0], d_PosX - 5, 0);        //day of the week
			char2Arr(WT_arr[MEZ.WT][1], d_PosX - 11, 0);
			char2Arr(WT_arr[MEZ.WT][2], d_PosX - 17, 0);
			char2Arr(48 + MEZ.tag2, d_PosX - 21, 0);           //day
			char2Arr(48 + MEZ.tag1, d_PosX - 27, 0);
			char2Arr(M_arr[MEZ.mon12 - 1][0], d_PosX - 33, 0); //month
			char2Arr(M_arr[MEZ.mon12 - 1][1], d_PosX - 37, 0);
			char2Arr(M_arr[MEZ.mon12 - 1][2], d_PosX - 43, 0);
			char2Arr(M_arr[MEZ.mon12 - 1][3], d_PosX - 49, 0);
			char2Arr(M_arr[MEZ.mon12][4], d_PosX - 55, 0);
			char2Arr('2', d_PosX - 62, 0);                     //year
			char2Arr('0', d_PosX - 68, 0);
			char2Arr(48 + MEZ.jahr2, d_PosX - 74, 0);
			char2Arr(48 + MEZ.jahr1, d_PosX - 80, 0);

			refresh_display(); //alle 50ms
			if (f_scrollend_y == true) {
				f_scrollend_y = false;
			}
		} //end 50ms
		if (y == 0) {
			// do something else
		}
	}  //end while(true)
	//this section can not be reached
}
