//*********************************************************************************************************
//*    ESP8266 MatrixClock                                                                                *
//*********************************************************************************************************
//
// first release on 26.02.2017
// updated on    30.11.2020
// Version 2.0.2
//
//
// THE SOFTWARE IS PROVIDED "AS IS" FOR PRIVATE USE ONLY, IT IS NOT FOR COMMERCIAL USE IN WHOLE OR PART OR CONCEPT.
// FOR PERSONAL USE IT IS SUPPLIED WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
// WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHOR
// OR COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE
//
//

#include <Arduino.h>
#include <SPI.h>
#include <Ticker.h>
#include <ESP8266WiFi.h>
#include <time.h>

struct tm tm;         // http://www.cplusplus.com/reference/ctime/tm/

// Digital I/O used
#define MAX_CS        15 // Pin cs  (SPI)

// Credentials ----------------------------------------
const char* SSID = "******";      // "mySSID";
const char* PW   = "******";      // "myWiFiPassword";

// Timezone -------------------------------------------
#define TZName       "CET-1CEST,M3.5.0,M10.5.0/3"   // Berlin (examples see at the bottom)
//#define TZName     "GMT0BST,M3.5.0/1,M10.5.0"     // London
//#define TZName     "IST-5:30"                     // New Delhi

// User defined text ----------------------------------
//#define UDTXT        "    Добрый день!  ΕΠΙΧΡΥΣΟ  "

// other defines --------------------------------------
#define BRIGHTNESS   0     // values can be 0...15
#define anzMAX       6     // number of cascaded MAX7219
#define FORMAT24H          // if not defined time will be displayed in 12h fromat
#define SCROLLDOWN         // if not defined it scrolls up

// other displays -------------------------------------
//#define REVERSE_HORIZONTAL                        // Parola, Generic and IC-Station
//#define REVERSE_VERTICAL                          // IC-Station display
//#define ROTATE_90                                 // Generic display

/*
   p  A  B  C  D  E  F  G        7  6  5  4  3  2  1  0        G  F  E  D  C  B  A  p        G  F  E  D  C  B  A  p
  ------------------------      ------------------------      ------------------------      ------------------------
0 |o  o  o  o  o  o  o  o|    p |o  o  o  o  o  o  o  o|    0 |o  o  o  o  o  o  o  o|    7 |o  o  o  o  o  o  o  o|
1 |o  o  o  o  o  o  o  o|    A |o  o  o  o  o  o  o  o|    1 |o  o  o  o  o  o  o  o|    6 |o  o  o  o  o  o  o  o|
2 |o  o  o  o  o  o  o  o|    B |o  o  o  o  o  o  o  o|    2 |o  o  o  o  o  o  o  o|    5 |o  o  o  o  o  o  o  o|
3 |o  o              o  o|    C |o  o              o  o|    3 |o  o              o  o|    4 |o  o              o  o|
4 |o  o    FC-16     o  o|    D |o  o   Generic    o  o|    4 |o  o   Parola     o  o|    3 |o  o  IC-Station  o  o|
5 |o  o              o  o|    E |o  o              o  o|    5 |o  o              o  o|    2 |o  o              o  o|
6 |o  o  o  o  o  o  o  o|    F |o  o  o  o  o  o  o  o|    6 |o  o  o  o  o  o  o  o|    1 |o  o  o  o  o  o  o  o|
7 |o  o  o  o  o  o  o  o|    G |o  o  o  o  o  o  o  o|    7 |o  o  o  o  o  o  o  o|    0 |o  o  o  o  o  o  o  o|
  ------------------------      ------------------------      ------------------------      ------------------------
*/

unsigned short _maxPosX = anzMAX * 8 - 1;            // calculated maxpos
unsigned short _LEDarr[anzMAX][8];                   // character matrix to display (40*8)
unsigned short _helpArrMAX[anzMAX * 8];              // helperarray for chardecoding
unsigned short _helpArrPos[anzMAX * 8];              // helperarray pos of chardecoding
unsigned int   _zPosX = 0;                           // xPos for time
unsigned int   _dPosX = 0;                           // xPos for date
bool           _f_tckr50ms = false;                  // flag, set every 50msec
boolean        _f_updown = false;                    //scroll direction
uint16_t       _chbuf[256];

const char* NTP_SERVER[] = {"de.pool.ntp.org", "at.pool.ntp.org", "europe.pool.ntp.org"};

// The object for the Ticker
Ticker tckr;

String M_arr[12] = {"Jan.", "Feb.", "Mar.", "Apr.", "May", "June", "July", "Aug.", "Sep.", "Oct.", "Nov.", "Dec."};
String WD_arr[7] = {"Sun,", "Mon,", "Tue,", "Wed,", "Thu,", "Fri,", "Sat,"};

// Font 5x8 for 8x8 matrix, 0,0 is above right
const uint8_t font_t[96][9] = { // monospace font only for the time
        { 0x07, 0x1c, 0x22, 0x26, 0x2a, 0x32, 0x22, 0x1c, 0x00 },   // 0x30, 0
        { 0x07, 0x08, 0x18, 0x08, 0x08, 0x08, 0x08, 0x1c, 0x00 },   // 0x31, 1
        { 0x07, 0x1c, 0x22, 0x02, 0x04, 0x08, 0x10, 0x3e, 0x00 },   // 0x32, 2
        { 0x07, 0x1c, 0x22, 0x02, 0x0c, 0x02, 0x22, 0x1c, 0x00 },   // 0x33, 3
        { 0x07, 0x04, 0x0c, 0x14, 0x24, 0x3e, 0x04, 0x04, 0x00 },   // 0x34, 4
        { 0x07, 0x3e, 0x20, 0x3c, 0x02, 0x02, 0x22, 0x1c, 0x00 },   // 0x35, 5
        { 0x07, 0x0c, 0x10, 0x20, 0x3c, 0x22, 0x22, 0x1c, 0x00 },   // 0x36, 6
        { 0x07, 0x3e, 0x02, 0x04, 0x08, 0x10, 0x10, 0x10, 0x00 },   // 0x37, 7
        { 0x07, 0x1c, 0x22, 0x22, 0x1c, 0x22, 0x22, 0x1c, 0x00 },   // 0x38, 8
        { 0x07, 0x1c, 0x22, 0x22, 0x1e, 0x02, 0x04, 0x18, 0x00 },   // 0x39, 9
        { 0x04, 0x00, 0x06, 0x06, 0x00, 0x06, 0x06, 0x00, 0x00 },   // 0x3a, :
};

const uint8_t font_p[346][9] = { // proportional font
        /*POS.     LEN   1     2     3     4     5     6     7     8            UTF-8   CH   NAME  */
        /*0000*/ { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0x0020,      SPACE 1 PIXEL
        /*0001*/ { 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x01, 0x00 },   // 0x0021, !    EXCLAMATION MARK
        /*0002*/ { 0x05, 0x09, 0x09, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0x0022, "    QUOTATION MARK
        /*0003*/ { 0x05, 0x0a, 0x0a, 0x1f, 0x0a, 0x1f, 0x0a, 0x0a, 0x00 },   // 0x0023, #    NUMBER SIGN
        /*0004*/ { 0x05, 0x04, 0x0f, 0x14, 0x0e, 0x05, 0x1e, 0x04, 0x00 },   // 0x0024, $    DOLLAR SIGN
        /*0005*/ { 0x05, 0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13, 0x00 },   // 0x0025, %    PERCENT SIGN
        /*0006*/ { 0x06, 0x0c, 0x22, 0x14, 0x18, 0x25, 0x22, 0x1d, 0x00 },   // 0x0026, &    AMPERSAND
        /*0007*/ { 0x02, 0x01, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0x0027, '    APOSTROPHE
        /*0008*/ { 0x03, 0x01, 0x02, 0x04, 0x04, 0x04, 0x02, 0x01, 0x00 },   // 0x0028, (    LEFT PARENTHESIS
        /*0009*/ { 0x03, 0x04, 0x02, 0x01, 0x01, 0x01, 0x02, 0x04, 0x00 },   // 0x0029, )    RIGHT PARENTHESIS
        /*0010*/ { 0x05, 0x04, 0x15, 0x0e, 0x1f, 0x0e, 0x15, 0x04, 0x00 },   // 0x002a, *    ASTERISK
        /*0011*/ { 0x05, 0x00, 0x04, 0x04, 0x1f, 0x04, 0x04, 0x00, 0x00 },   // 0x002b, +    PLUS SIGN
        /*0012*/ { 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x02 },   // 0x022c, ,    COMMA
        /*0013*/ { 0x05, 0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00, 0x00 },   // 0x002d, -    HYPHEN-MINUS
        /*0014*/ { 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x00 },   // 0x002e, .    FULL STOP
        /*0015*/ { 0x05, 0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10, 0x00 },   // 0x002f, /    SOLIDUS
        /*0016*/ { 0x05, 0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e, 0x00 },   // 0x0030, 0    DIGIT ZERO
        /*0017*/ { 0x03, 0x02, 0x06, 0x02, 0x02, 0x02, 0x02, 0x07, 0x00 },   // 0x0031, 1    DIGIT ONE
        /*0018*/ { 0x05, 0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f, 0x00 },   // 0x0032, 2    DIGIT TWO
        /*0019*/ { 0x05, 0x0e, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0e, 0x00 },   // 0x0033, 3    DIGIT THREE
        /*0020*/ { 0x05, 0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02, 0x00 },   // 0x0034, 4    DIGIT FOUR
        /*0021*/ { 0x05, 0x1f, 0x10, 0x1e, 0x01, 0x01, 0x11, 0x0e, 0x00 },   // 0x0035, 5    DIGIT FIVE
        /*0022*/ { 0x05, 0x06, 0x08, 0x10, 0x1e, 0x11, 0x11, 0x0e, 0x00 },   // 0x0036, 6    DIGIT SIX
        /*0023*/ { 0x05, 0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08, 0x00 },   // 0x0037, 7    DIGIT SEVEN
        /*0024*/ { 0x05, 0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e, 0x00 },   // 0x0038, 8    DIGIT EIGHT
        /*0025*/ { 0x05, 0x0e, 0x11, 0x11, 0x0f, 0x01, 0x02, 0x0c, 0x00 },   // 0x0039, 9    DIGIT NINE
        /*0026*/ { 0x02, 0x00, 0x03, 0x03, 0x00, 0x03, 0x03, 0x00, 0x00 },   // 0x003a, :    COLON
        /*0027*/ { 0x02, 0x00, 0x00, 0x03, 0x03, 0x00, 0x03, 0x01, 0x02 },   // 0x023b, ;    SEMICOLON
        /*0028*/ { 0x04, 0x01, 0x02, 0x04, 0x08, 0x04, 0x02, 0x01, 0x00 },   // 0x003c, <    LESS-THAN SIGN
        /*0029*/ { 0x05, 0x00, 0x00, 0x1f, 0x00, 0x1f, 0x00, 0x00, 0x00 },   // 0x003d, =    EQUALS SIGN
        /*0030*/ { 0x04, 0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08, 0x00 },   // 0x003e, >    GREATER-THAN SIGN
        /*0031*/ { 0x05, 0x0e, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04, 0x00 },   // 0x003f, ?    QUESTION MARK
        /*0032*/ { 0x05, 0x0e, 0x11, 0x17, 0x15, 0x17, 0x10, 0x0f, 0x00 },   // 0x0040, @    COMMERCIAL AT
        /*0033*/ { 0x05, 0x04, 0x0a, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x00 },   // 0x0041, A    LATIN CAPITAL LETTER A
        /*0034*/ { 0x05, 0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e, 0x00 },   // 0x0042, B    LATIN CAPITAL LETTER B
        /*0035*/ { 0x05, 0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e, 0x00 },   // 0x0043, C    LATIN CAPITAL LETTER C
        /*0036*/ { 0x05, 0x1e, 0x09, 0x09, 0x09, 0x09, 0x09, 0x1e, 0x00 },   // 0x0044, D    LATIN CAPITAL LETTER D
        /*0037*/ { 0x05, 0x1f, 0x10, 0x10, 0x1c, 0x10, 0x10, 0x1f, 0x00 },   // 0x0045, E    LATIN CAPITAL LETTER E
        /*0038*/ { 0x05, 0x1f, 0x10, 0x10, 0x1f, 0x10, 0x10, 0x10, 0x00 },   // 0x0046, F    LATIN CAPITAL LETTER F
        /*0039*/ { 0x05, 0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0f, 0x00 },   // 0x0037, G    LATIN CAPITAL LETTER G
        /*0040*/ { 0x05, 0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11, 0x00 },   // 0x0048, H    LATIN CAPITAL LETTER H
        /*0041*/ { 0x05, 0x07, 0x02, 0x02, 0x02, 0x02, 0x02, 0x07, 0x00 },   // 0x0049, I    LATIN CAPITAL LETTER I
        /*0042*/ { 0x05, 0x1f, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0c, 0x00 },   // 0x004a, J    LATIN CAPITAL LETTER J
        /*0043*/ { 0x05, 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11, 0x00 },   // 0x004b, K    LATIN CAPITAL LETTER K
        /*0044*/ { 0x05, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f, 0x00 },   // 0x004c, L    LATIN CAPITAL LETTER L
        /*0045*/ { 0x05, 0x11, 0x1b, 0x15, 0x11, 0x11, 0x11, 0x11, 0x00 },   // 0x004d, M    LATIN CAPITAL LETTER M
        /*0046*/ { 0x05, 0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x00 },   // 0x004e, N    LATIN CAPITAL LETTER N
        /*0047*/ { 0x05, 0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e, 0x00 },   // 0x004f, O    LATIN CAPITAL LETTER O
        /*0048*/ { 0x05, 0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10, 0x00 },   // 0x0050, P    LATIN CAPITAL LETTER P
        /*0049*/ { 0x05, 0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d, 0x00 },   // 0x0051, Q    LATIN CAPITAL LETTER Q
        /*0050*/ { 0x05, 0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11, 0x00 },   // 0x0052, R    LATIN CAPITAL LETTER R
        /*0051*/ { 0x05, 0x0e, 0x11, 0x10, 0x0e, 0x01, 0x11, 0x0e, 0x00 },   // 0x0053, S    LATIN CAPITAL LETTER S
        /*0052*/ { 0x05, 0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00 },   // 0x0054, T    LATIN CAPITAL LETTER T
        /*0053*/ { 0x05, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e, 0x00 },   // 0x0055, U    LATIN CAPITAL LETTER U
        /*0054*/ { 0x05, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04, 0x00 },   // 0x0056, V    LATIN CAPITAL LETTER V
        /*0055*/ { 0x05, 0x11, 0x11, 0x11, 0x15, 0x15, 0x1b, 0x11, 0x00 },   // 0x0057, W    LATIN CAPITAL LETTER W
        /*0056*/ { 0x05, 0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11, 0x00 },   // 0x0058, X    LATIN CAPITAL LETTER X
        /*0057*/ { 0x05, 0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04, 0x00 },   // 0x0059, Y    LATIN CAPITAL LETTER Y
        /*0058*/ { 0x05, 0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f, 0x00 },   // 0x005a, Z    LATIN CAPITAL LETTER Z
        /*0059*/ { 0x03, 0x07, 0x04, 0x04, 0x04, 0x04, 0x04, 0x07, 0x00 },   // 0x005b, [    LEFT SQUARE BRACKET
        /*0060*/ { 0x05, 0x10, 0x10, 0x08, 0x04, 0x02, 0x01, 0x01, 0x00 },   // 0x005c, '\'  REVERSE SOLIDUS
        /*0061*/ { 0x03, 0x07, 0x01, 0x01, 0x01, 0x01, 0x01, 0x07, 0x00 },   // 0x005d, ]    RIGHT SQUARE BRACKET
        /*0062*/ { 0x05, 0x04, 0x0a, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0x005e, ^    CIRCUMFLEX ACCENT
        /*0063*/ { 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x00 },   // 0x005f, _    LOW LINE
        /*0064*/ { 0x02, 0x02, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0x0060, `    GRAVE ACCENT
        /*0065*/ { 0x05, 0x00, 0x0e, 0x01, 0x0d, 0x13, 0x13, 0x0d, 0x00 },   // 0x0061, a    LATIN SMALL LETTER A
        /*0066*/ { 0x05, 0x10, 0x10, 0x16, 0x19, 0x11, 0x19, 0x16, 0x00 },   // 0x0062, b    LATIN SMALL LETTER B
        /*0067*/ { 0x05, 0x00, 0x00, 0x07, 0x08, 0x08, 0x08, 0x07, 0x00 },   // 0x0063, c    LATIN SMALL LETTER C
        /*0068*/ { 0x05, 0x01, 0x01, 0x0d, 0x13, 0x11, 0x13, 0x0d, 0x00 },   // 0x0064, d    LATIN SMALL LETTER D
        /*0069*/ { 0x05, 0x00, 0x00, 0x0e, 0x11, 0x1f, 0x10, 0x0e, 0x00 },   // 0x0065, e    LATIN SMALL LETTER E
        /*0070*/ { 0x05, 0x06, 0x09, 0x08, 0x1c, 0x08, 0x08, 0x08, 0x00 },   // 0x0066, f    LATIN SMALL LETTER F
        /*0071*/ { 0x05, 0x00, 0x0e, 0x0f, 0x11, 0x11, 0x0f, 0x01, 0x0e },   // 0x0067, g    LATIN SMALL LETTER G
        /*0072*/ { 0x05, 0x10, 0x10, 0x16, 0x19, 0x11, 0x11, 0x11, 0x00 },   // 0x0068, h    LATIN SMALL LETTER H
        /*0073*/ { 0x03, 0x00, 0x02, 0x00, 0x06, 0x02, 0x02, 0x07, 0x00 },   // 0x0069, i    LATIN SMALL LETTER I
        /*0074*/ { 0x04, 0x00, 0x01, 0x00, 0x03, 0x01, 0x01, 0x09, 0x06 },   // 0x006a, j    LATIN SMALL LETTER J
        /*0075*/ { 0x04, 0x08, 0x08, 0x09, 0x0a, 0x0c, 0x0a, 0x09, 0x00 },   // 0x006b, k    LATIN SMALL LETTER K
        /*0076*/ { 0x03, 0x06, 0x02, 0x02, 0x02, 0x02, 0x02, 0x07, 0x00 },   // 0x006c, l    LATIN SMALL LETTER L
        /*0077*/ { 0x05, 0x00, 0x00, 0x1a, 0x15, 0x15, 0x11, 0x11, 0x00 },   // 0x006d, m    LATIN SMALL LETTER M
        /*0078*/ { 0x05, 0x00, 0x00, 0x16, 0x19, 0x11, 0x11, 0x11, 0x00 },   // 0x006e, n    LATIN SMALL LETTER N
        /*0079*/ { 0x05, 0x00, 0x00, 0x0e, 0x11, 0x11, 0x11, 0x0e, 0x00 },   // 0x006f, o    LATIN SMALL LETTER O
        /*0080*/ { 0x04, 0x00, 0x00, 0x0e, 0x09, 0x09, 0x0e, 0x08, 0x08 },   // 0x0070, p    LATIN SMALL LETTER P
        /*0081*/ { 0x04, 0x00, 0x00, 0x07, 0x09, 0x09, 0x07, 0x01, 0x01 },   // 0x0071, q    LATIN SMALL LETTER Q
        /*0082*/ { 0x05, 0x00, 0x00, 0x16, 0x19, 0x10, 0x10, 0x10, 0x00 },   // 0x0072, r    LATIN SMALL LETTER R
        /*0083*/ { 0x05, 0x00, 0x00, 0x0e, 0x10, 0x0e, 0x01, 0x1e, 0x00 },   // 0x0073, s    LATIN SMALL LETTER S
        /*0084*/ { 0x05, 0x08, 0x08, 0x1c, 0x08, 0x08, 0x09, 0x06, 0x00 },   // 0x0074, t    LATIN SMALL LETTER T
        /*0085*/ { 0x05, 0x00, 0x00, 0x11, 0x11, 0x11, 0x13, 0x0d, 0x00 },   // 0x0075, u    LATIN SMALL LETTER U
        /*0086*/ { 0x05, 0x00, 0x00, 0x11, 0x11, 0x11, 0x0a, 0x04, 0x00 },   // 0x0076, v    LATIN SMALL LETTER V
        /*0087*/ { 0x05, 0x00, 0x00, 0x11, 0x11, 0x15, 0x15, 0x0a, 0x00 },   // 0x0077, w    LATIN SMALL LETTER W
        /*0088*/ { 0x05, 0x00, 0x00, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x00 },   // 0x0078, x    LATIN SMALL LETTER X
        /*0089*/ { 0x05, 0x00, 0x00, 0x11, 0x11, 0x0f, 0x01, 0x11, 0x0e },   // 0x0079, y    LATIN SMALL LETTER Y
        /*0090*/ { 0x05, 0x00, 0x00, 0x1f, 0x02, 0x04, 0x08, 0x1f, 0x00 },   // 0x007a, z    LATIN SMALL LETTER Z
        /*0091*/ { 0x04, 0x03, 0x04, 0x04, 0x08, 0x04, 0x04, 0x03, 0x00 },   // 0x007b, {    LEFT CURLY BRACKET
        /*0092*/ { 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00 },   // 0x007c, |    VERTICAL LINE
        /*0093*/ { 0x04, 0x0c, 0x02, 0x02, 0x01, 0x02, 0x02, 0x0c, 0x00 },   // 0x007d, }    RIGHT CURLY BRACKET
        /*0094*/ { 0x05, 0x00, 0x00, 0x08, 0x15, 0x02, 0x00, 0x00, 0x00 },   // 0x007e, ~    TILDE
        /*0095*/ { 0x05, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x00 },   // 0x007f,      DEL

        /*0096*/ { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0xc2a0       NO-BREAK SPACE
        /*0097*/ { 0x01, 0x01, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00 },   // 0xc2a1  ¡    INVERTED EXCLAMATION MARK
        /*0098*/ { 0x04, 0x00, 0x02, 0x07, 0x0A, 0x0A, 0x07, 0x02, 0x00 },   // 0xc2a2  ¢    CENT SIGN
        /*0099*/ { 0x05, 0x06, 0x09, 0x08, 0x1E, 0x08, 0x08, 0x1F, 0x00 },   // 0xc2a3  £    POUND SIGN
        /*0100*/ { 0x06, 0x00, 0x21, 0x1e, 0x12, 0x12, 0x1e, 0x21, 0x00 },   // 0xc2a4  ¤    CURRENCY SIGN
        /*0101*/ { 0x05, 0x11, 0x0A, 0x04, 0x1F, 0x04, 0x1F, 0x04, 0x00 },   // 0xc2a5  ¥    YEN SIGN
        /*0102*/ { 0x01, 0x01, 0x01, 0x01, 0x00, 0x01, 0x01, 0x01, 0x00 },   // 0xc2a6  ¦    BROKEN BAR
        /*0103*/ { 0x05, 0x0F, 0x10, 0x1E, 0x11, 0x11, 0x0F, 0x01, 0x1E },   // 0xc2a7  §    PARAGRAPH SIGN
        /*0104*/ { 0x04, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0xc2a8  ¨    DIAERESIS
        /*0105*/ { 0x08, 0x3C, 0x42, 0x9D, 0xA1, 0xA1, 0x9D, 0x42, 0x3C },   // 0xc1a9  ©    COPYRIGHT SIGN
        /*0106*/ { 0x05, 0x0E, 0x01, 0x0D, 0x13, 0x0D, 0x00, 0x00, 0x00 },   // 0xc2aa  ª    FEMININE ORDINAL INDICATOR
        /*0107*/ { 0x06, 0x00, 0x00, 0x09, 0x12, 0x24, 0x12, 0x09, 0x00 },   // 0xc2ab  «    LEFT-POINTING DOUBLE ANGLE QUOTATION MARK
        /*0108*/ { 0x05, 0x00, 0x00, 0x00, 0x1F, 0x01, 0x00, 0x00, 0x00 },   // 0xc2ac  ¬    NOT SIGN
        /*0109*/ { 0x07, 0x00, 0x00, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x00 },   // 0xc2ad  ­    SOFT HYPHEN
        /*0110*/ { 0x08, 0x3C, 0x42, 0xB9, 0xA5, 0xB9, 0xA5, 0x42, 0x3C },   // 0xc2ae  ®    REGISTERED TRADE MARK SIGN
        /*0111*/ { 0x08, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0xc2af  ¯    MACRON
        /*0112*/ { 0x04, 0x06, 0x09, 0x09, 0x06, 0x00, 0x00, 0x00, 0x00 },   // 0xc2b0  °    DEGREE SIGN
        /*0113*/ { 0x05, 0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x1F, 0x00 },   // 0xc2b1  ±    PLUS-MINUS SIGN
        /*0114*/ { 0x03, 0x07, 0x01, 0x06, 0x04, 0x07, 0x00, 0x00, 0x00 },   // 0xc2b2  ²    SUPERSCRIPT TWO
        /*0115*/ { 0x03, 0x07, 0x01, 0x03, 0x01, 0x07, 0x00, 0x00, 0x00 },   // 0xc2b3  ³    SUPERSCRIPT THREE
        /*0116*/ { 0x02, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0xc2b4  ´    ACUTE ACCENT
        /*0117*/ { 0x04, 0x00, 0x00, 0x00, 0x09, 0x09, 0x09, 0x0E, 0x08 },   // 0xc2b5  µ    MICRO SIGN
        /*0118*/ { 0x05, 0x0F, 0x15, 0x15, 0x0D, 0x05, 0x05, 0x05, 0x00 },   // 0xc2b6  ¶    PILCROW SIGN
        /*0119*/ { 0x06, 0x00, 0x00, 0x00, 0x0c, 0x0c, 0x00, 0x00, 0x00 },   // 0xc2b7  ·    MIDDLE DOT
        /*0120*/ { 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x01, 0x02 },   // 0xc2b8  ¸    CEDILLA
        /*0121*/ { 0x02, 0x01, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00 },   // 0xc2b9  ¹    SUPERSCRIPT ONE
        /*0122*/ { 0x03, 0x02, 0x05, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0xc2ba  º    MASCULINE ORDINAL INDICATOR
        /*0123*/ { 0x06, 0x00, 0x00, 0x24, 0x12, 0x09, 0x12, 0x24, 0x00 },   // 0xc2bb  »    RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK
        /*0124*/ { 0x08, 0x41, 0xc2, 0x44, 0x49, 0x13, 0x25, 0x47, 0x81 },   // 0xc2bc  ¼    VULGAR FRACTION ONE QUARTER
        /*0125*/ { 0x08, 0x41, 0xc2, 0x44, 0x48, 0x13, 0x21, 0x42, 0x83 },   // 0xc2bd  ½    VULGAR FRACTION ONE HALF
        /*0126*/ { 0x08, 0xc1, 0x22, 0xc4, 0x29, 0xd3, 0x25, 0x47, 0x81 },   // 0xc2be  ¾    VULGAR FRACTION THREE QUARTERS
        /*0127*/ { 0x05, 0x04, 0x00, 0x04, 0x08, 0x10, 0x11, 0x0e, 0x00 },   // 0xc2bf  ¿    INVERTED QUESTION MARK

        /*0128*/ { 0x05, 0x08, 0x04, 0x00, 0x04, 0x0A, 0x11, 0x1F, 0x11 },   // 0xc380  À    LATIN CAPITAL LETTER A WITH GRAVE
        /*0129*/ { 0x05, 0x02, 0x04, 0x00, 0x04, 0x0A, 0x11, 0x1F, 0x11 },   // 0xc381  Á    LATIN CAPITAL LETTER A WITH ACUTE
        /*0130*/ { 0x05, 0x04, 0x0A, 0x00, 0x04, 0x0A, 0x11, 0x1F, 0x11 },   // 0xc382  Â    LATIN CAPITAL LETTER A WITH CIRCUMFLEX
        /*0131*/ { 0x05, 0x05, 0x0A, 0x00, 0x04, 0x0A, 0x11, 0x1F, 0x11 },   // 0xc383  Ã    LATIN CAPITAL LETTER A WITH TILDE
        /*0132*/ { 0x05, 0x11, 0x04, 0x0A, 0x11, 0x1F, 0x11, 0x11, 0x00 },   // 0xc384  Ä    LATIN CAPITAL LETTER A WITH DIAERESIS
        /*0133*/ { 0x05, 0x04, 0x00, 0x04, 0x0A, 0x11, 0x1F, 0x11, 0x00 },   // 0xc385  Å    LATIN CAPITAL LETTER A WITH RING ABOVE
        /*0134*/ { 0x08, 0x1F, 0x28, 0x48, 0xFF, 0x88, 0x88, 0x8F, 0x00 },   // 0xc386  Æ    LATIN CAPITAL LETTER AE
        /*0135*/ { 0x05, 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E, 0x04 },   // 0xc387  Ç    LATIN CAPITAL LETTER C WITH CEDILLA
        /*0136*/ { 0x04, 0x04, 0x02, 0x00, 0x0F, 0x08, 0x0F, 0x08, 0x0F },   // 0xc388  È    LATIN CAPITAL LETTER E WITH GRAVE
        /*0137*/ { 0x04, 0x01, 0x02, 0x00, 0x0F, 0x08, 0x0F, 0x08, 0x0F },   // 0xc389  É    LATIN CAPITAL LETTER E WITH ACUTE
        /*0138*/ { 0x04, 0x02, 0x05, 0x00, 0x0F, 0x08, 0x0F, 0x08, 0x0F },   // 0xc38a  Ê    LATIN CAPITAL LETTER E WITH CIRCUMFLEX
        /*0139*/ { 0x04, 0x0A, 0x00, 0x0F, 0x08, 0x0E, 0x08, 0x0F, 0x00 },   // 0xc38b  Ë    LATIN CAPITAL LETTER E WITH DIAERESIS
        /*0140*/ { 0x02, 0x02, 0x01, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00 },   // 0xc38c  Ì    LATIN CAPITAL LETTER I WITH GRAVE
        /*0141*/ { 0x02, 0x01, 0x02, 0x00, 0x02, 0x02, 0x02, 0x02, 0x00 },   // 0xc38d  Í    LATIN CAPITAL LETTER I WITH ACUTE
        /*0142*/ { 0x03, 0x02, 0x05, 0x00, 0x02, 0x02, 0x02, 0x02, 0x00 },   // 0xc38e  Î    LATIN CAPITAL LETTER I WITH CIRCUMFLEX
        /*0143*/ { 0x02, 0x05, 0x00, 0x02, 0x02, 0x02, 0x02, 0x02, 0x00 },   // 0xc38f  Ï    LATIN CAPITAL LETTER I WITH DIAERESIS
        /*0144*/ { 0x05, 0x1E, 0x09, 0x09, 0x1D, 0x09, 0x09, 0x1E, 0x00 },   // 0xc390  Ð    LATIN CAPITAL LETTER ETH
        /*0145*/ { 0x04, 0x05, 0x0A, 0x00, 0x09, 0x0D, 0x0B, 0x09, 0x09 },   // 0xc391  Ñ    LATIN CAPITAL LETTER N WITH TILDE
        /*0146*/ { 0x05, 0x08, 0x04, 0x00, 0x0E, 0x11, 0x11, 0x11, 0x0E },   // 0xc392  Ò    LATIN CAPITAL LETTER O WITH GRAVE
        /*0147*/ { 0x05, 0x02, 0x04, 0x00, 0x0E, 0x11, 0x11, 0x11, 0x0E },   // 0xc393  Ó    LATIN CAPITAL LETTER O WITH ACUTE
        /*0148*/ { 0x05, 0x04, 0x0A, 0x00, 0x0E, 0x11, 0x11, 0x11, 0x0E },   // 0xc394  Ô    LATIN CAPITAL LETTER O WITH CIRCUMFLEX
        /*0149*/ { 0x05, 0x05, 0x0A, 0x00, 0x0E, 0x11, 0x11, 0x11, 0x0E },   // 0xc395  Õ    LATIN CAPITAL LETTER O WITH TILDE
        /*0150*/ { 0x05, 0x0A, 0x00, 0x0E, 0x11, 0x11, 0x11, 0x0E, 0x00 },   // 0xc396  Ö    LATIN CAPITAL LETTER O WITH DIAERESIS
        /*0151*/ { 0x03, 0x00, 0x00, 0x00, 0x05, 0x02, 0x05, 0x00, 0x00 },   // 0xc397  ×    MULTIPLICATION SIGN
        /*0152*/ { 0x06, 0x01, 0x0E, 0x13, 0x15, 0x19, 0x11, 0x2E, 0x00 },   // 0xc398  Ø    LATIN CAPITAL LETTER O WITH STROKE
        /*0153*/ { 0x05, 0x08, 0x04, 0x11, 0x11, 0x11, 0x11, 0x0E, 0x00 },   // 0xc399  Ù    LATIN CAPITAL LETTER U WITH GRAVE
        /*0154*/ { 0x05, 0x02, 0x04, 0x11, 0x11, 0x11, 0x11, 0x0E, 0x00 },   // 0xc39a  Ú    LATIN CAPITAL LETTER U WITH ACUTE
        /*0155*/ { 0x05, 0x04, 0x0A, 0x00, 0x11, 0x11, 0x11, 0x11, 0x0E },   // 0xc39b  Û    LATIN CAPITAL LETTER U WITH CIRCUMFLEX
        /*0156*/ { 0x05, 0x0A, 0x00, 0x11, 0x11, 0x11, 0x11, 0x0E, 0x00 },   // 0xc39c  Ü    LATIN CAPITAL LETTER U WITH DIAERESIS
        /*0157*/ { 0x05, 0x02, 0x04, 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04 },   // 0xc39d  Ý    LATIN CAPITAL LETTER Y WITH ACUTE
        /*0158*/ { 0x05, 0x1C, 0x08, 0x0E, 0x09, 0x0E, 0x08, 0x1C, 0x00 },   // 0xc39e  Þ    LATIN CAPITAL LETTER THORN
        /*0159*/ { 0x04, 0x0E, 0x09, 0x09, 0x0E, 0x09, 0x09, 0x0A, 0x08 },   // 0xc39f  ß    LATIN SMALL LETTER SHARP S
        /*0160*/ { 0x04, 0x04, 0x02, 0x00, 0x06, 0x01, 0x05, 0x0B, 0x05 },   // 0xc3a0  à    LATIN SMALL LETTER A WITH GRAVE
        /*0161*/ { 0x04, 0x01, 0x02, 0x00, 0x06, 0x01, 0x05, 0x0B, 0x05 },   // 0xc3a1  á    LATIN SMALL LETTER A WITH ACUTE
        /*0162*/ { 0x04, 0x04, 0x0A, 0x00, 0x06, 0x01, 0x05, 0x0B, 0x05 },   // 0xc3a2  â    LATIN SMALL LETTER A WITH CIRCUMFLEX
        /*0163*/ { 0x04, 0x05, 0x0A, 0x00, 0x06, 0x01, 0x05, 0x0B, 0x05 },   // 0xc3a3  ã    LATIN SMALL LETTER A WITH TILDE
        /*0164*/ { 0x04, 0x05, 0x00, 0x06, 0x01, 0x05, 0x0B, 0x0D, 0x00 },   // 0xc3a4  ä    LATIN SMALL LETTER A WITH DIAERESIS
        /*0165*/ { 0x04, 0x02, 0x00, 0x06, 0x01, 0x05, 0x0B, 0x0D, 0x00 },   // 0xc3a5  å    LATIN SMALL LETTER A WITH RING ABOVE
        /*0166*/ { 0x07, 0x00, 0x00, 0x16, 0x09, 0x3E, 0x48, 0x37, 0x00 },   // 0xc3a6  æ    LATIN SMALL LETTER AE
        /*0167*/ { 0x04, 0x00, 0x00, 0x07, 0x08, 0x08, 0x08, 0x07, 0x02 },   // 0xc3a7  ç    LATIN SMALL LETTER C WITH CEDILLA
        /*0168*/ { 0x05, 0x08, 0x04, 0x00, 0x0E, 0x11, 0x1F, 0x10, 0x0E },   // 0xc3a8  è    LATIN SMALL LETTER E WITH GRAVE
        /*0169*/ { 0x05, 0x02, 0x04, 0x00, 0x0E, 0x11, 0x1F, 0x10, 0x0E },   // 0xc3a9  é    LATIN SMALL LETTER E WITH ACUTE
        /*0170*/ { 0x05, 0x04, 0x0A, 0x00, 0x0E, 0x11, 0x1F, 0x10, 0x0E },   // 0xc3aa  ê    LATIN SMALL LETTER E WITH CIRCUMFLEX
        /*0171*/ { 0x05, 0x0A, 0x00, 0x0E, 0x11, 0x1F, 0x10, 0x0E, 0x00 },   // 0xc3ab  ë    LATIN SMALL LETTER E WITH DIAERESIS
        /*0172*/ { 0x03, 0x04, 0x02, 0x00, 0x02, 0x02, 0x02, 0x07, 0x00 },   // 0xc3ac  ì    LATIN SMALL LETTER I WITH GRAVE
        /*0173*/ { 0x03, 0x01, 0x02, 0x00, 0x02, 0x02, 0x02, 0x07, 0x00 },   // 0xc3ad  í    LATIN SMALL LETTER I WITH ACUTE
        /*0174*/ { 0x03, 0x02, 0x05, 0x00, 0x02, 0x02, 0x02, 0x07, 0x00 },   // 0xc3ae  î    LATIN SMALL LETTER I WITH CIRCUMFLEX
        /*0175*/ { 0x03, 0x00, 0x05, 0x00, 0x02, 0x02, 0x02, 0x07, 0x00 },   // 0xc3af  ï    LATIN SMALL LETTER I WITH DIAERESIS
        /*0176*/ { 0x05, 0x14, 0x08, 0x14, 0x02, 0x07, 0x09, 0x09, 0x06 },   // 0xc3b0  ð    LATIN SMALL LETTER ETH
        /*0177*/ { 0x05, 0x05, 0x0A, 0x00, 0x16, 0x19, 0x11, 0x11, 0x00 },   // 0xc3b1  ñ    LATIN SMALL LETTER N WITH TILDE
        /*0178*/ { 0x04, 0x04, 0x02, 0x00, 0x06, 0x09, 0x09, 0x06, 0x00 },   // 0xc3b2  ò    LATIN SMALL LETTER O WITH GRAVE
        /*0179*/ { 0x04, 0x02, 0x04, 0x00, 0x06, 0x09, 0x09, 0x06, 0x00 },   // 0xc3b3  ó    LATIN SMALL LETTER O WITH ACUTE
        /*0180*/ { 0x04, 0x02, 0x05, 0x00, 0x06, 0x09, 0x09, 0x06, 0x00 },   // 0xc3b4  ô    LATIN SMALL LETTER O WITH CIRCUMFLEX
        /*0181*/ { 0x04, 0x05, 0x0A, 0x00, 0x06, 0x09, 0x09, 0x06, 0x00 },   // 0xc3b5  õ    LATIN SMALL LETTER O WITH TILDE
        /*0182*/ { 0x05, 0x0A, 0x00, 0x0E, 0x11, 0x11, 0x11, 0x0E, 0x00 },   // 0xc3b6  ö    LATIN SMALL LETTER O WITH DIAERESIS
        /*0183*/ { 0x05, 0x00, 0x04, 0x00, 0x1F, 0x00, 0x04, 0x00, 0x00 },   // 0xc3b7  ÷    DIVISION SIGN
        /*0184*/ { 0x06, 0x00, 0x01, 0x0E, 0x13, 0x15, 0x19, 0x2E, 0x00 },   // 0xc3b8  ø    LATIN SMALL LETTER O WITH STROKE
        /*0185*/ { 0x05, 0x08, 0x04, 0x00, 0x11, 0x11, 0x11, 0x0E, 0x00 },   // 0xc3b9  ù    LATIN SMALL LETTER U WITH GRAVE
        /*0186*/ { 0x05, 0x02, 0x04, 0x00, 0x11, 0x11, 0x11, 0x0E, 0x00 },   // 0xc3ba  ú    LATIN SMALL LETTER U WITH ACUTE
        /*0187*/ { 0x05, 0x04, 0x0A, 0x00, 0x11, 0x11, 0x11, 0x0E, 0x00 },   // 0xc3bb  û    LATIN SMALL LETTER U WITH CIRCUMFLEX
        /*0188*/ { 0x05, 0x0A, 0x00, 0x11, 0x11, 0x11, 0x13, 0x0D, 0x00 },   // 0xc3bc  ü    LATIN SMALL LETTER U WITH DIAERESIS
        /*0189*/ { 0x05, 0x02, 0x04, 0x11, 0x11, 0x0F, 0x01, 0x11, 0x0E },   // 0xc3bd  ý    LATIN SMALL LETTER Y WITH ACUTE
        /*0190*/ { 0x05, 0x00, 0x18, 0x08, 0x0E, 0x09, 0x0E, 0x08, 0x1C },   // 0xc3be  þ    LATIN SMALL LETTER THORN
        /*0191*/ { 0x05, 0x0A, 0x00, 0x11, 0x11, 0x0F, 0x01, 0x11, 0x0E },   // 0xc3bf  ÿ    LATIN SMALL LETTER Y WITH DIAERESIS

        /*0192*/ { 0x05, 0x04, 0x0A, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x00 },   // 0xce91  Α    GREEK CAPITAL LETTER ALPHA
        /*0193*/ { 0x05, 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E, 0x00 },   // 0xce92  Β    GREEK CAPITAL LETTER BETA
        /*0194*/ { 0x06, 0x3F, 0x11, 0x10, 0x10, 0x10, 0x10, 0x38, 0x00 },   // 0xce93  Γ    GREEK CAPITAL LETTER GAMMA
        /*0195*/ { 0x07, 0x08, 0x14, 0x14, 0x22, 0x22, 0x41, 0x7F, 0x00 },   // 0xce94  Δ    GREEK CAPITAL LETTER DELTA
        /*0196*/ { 0x05, 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F, 0x00 },   // 0xce95  Ε    GREEK CAPITAL LETTER EPSILON
        /*0197*/ { 0x05, 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F, 0x00 },   // 0xce96  Ζ    GREEK CAPITAL LETTER ZETA
        /*0198*/ { 0x05, 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11, 0x00 },   // 0xce97  Η    GREEK CAPITAL LETTER ETA
        /*0199*/ { 0x06, 0x1E, 0x21, 0x21, 0x2D, 0x21, 0x21, 0x1E, 0x00 },   // 0xce98  Θ    GREEK CAPITAL LETTER THETA
        /*0200*/ { 0x03, 0x07, 0x02, 0x02, 0x02, 0x02, 0x02, 0x07, 0x00 },   // 0xce99  Ι    GREEK CAPITAL LETTER IOTA
        /*0201*/ { 0x05, 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11, 0x00 },   // 0xce9a  Κ    GREEK CAPITAL LETTER KAPPA
        /*0202*/ { 0x07, 0x08, 0x14, 0x14, 0x22, 0x22, 0x41, 0x41, 0x00 },   // 0xce9b  Λ    GREEK CAPITAL LETTER LAMDA
        /*0203*/ { 0x07, 0x41, 0x63, 0x55, 0x49, 0x41, 0x41, 0x41, 0x00 },   // 0xce9c  Μ    GREEK CAPITAL LETTER MU
        /*0204*/ { 0x07, 0x41, 0x61, 0x51, 0x49, 0x45, 0x43, 0x41, 0x00 },   // 0xce9d  Ν    GREEK CAPITAL LETTER NU
        /*0205*/ { 0x07, 0x7F, 0x41, 0x00, 0x3E, 0x00, 0x41, 0x7F, 0x00 },   // 0xce9e  Ξ    GREEK CAPITAL LETTER XI
        /*0206*/ { 0x06, 0x1E, 0x21, 0x21, 0x21, 0x21, 0x21, 0x1E, 0x00 },   // 0xce9f  Ο    GREEK CAPITAL LETTER OMICRON
        /*0207*/ { 0x07, 0x7F, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x00 },   // 0xcea0  Π    GREEK CAPITAL LETTER PI
        /*0208*/ { 0x05, 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10, 0x00 },   // 0xcea1  Ρ    GREEK CAPITAL LETTER RHO
        /*0209*/ { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0xcea2  ΢
        /*0210*/ { 0x06, 0x3F, 0x21, 0x10, 0x08, 0x10, 0x21, 0x3F, 0x00 },   // 0xcea3  Σ    GREEK CAPITAL LETTER SIGMA
        /*0211*/ { 0x07, 0x7F, 0x49, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00 },   // 0xcea4  Τ    GREEK CAPITAL LETTER TAU
        /*0212*/ { 0x07, 0x41, 0x22, 0x14, 0x08, 0x08, 0x08, 0x1C, 0x00 },   // 0xcea5  Υ    GREEK CAPITAL LETTER UPSILON
        /*0213*/ { 0x07, 0x08, 0x3E, 0x49, 0x49, 0x49, 0x3E, 0x08, 0x00 },   // 0xcea6  Φ    GREEK CAPITAL LETTER PHI
        /*0214*/ { 0x07, 0x63, 0x22, 0x14, 0x08, 0x14, 0x22, 0x63, 0x00 },   // 0xcea7  Χ    GREEK CAPITAL LETTER CHI
        /*0215*/ { 0x07, 0x49, 0x2A, 0x2A, 0x1C, 0x08, 0x08, 0x1C, 0x00 },   // 0xcea8  Ψ    GREEK CAPITAL LETTER PSI
        /*0216*/ { 0x07, 0x1C, 0x22, 0x41, 0x41, 0x22, 0x14, 0x77, 0x00 },   // 0xcea9  Ω    GREEK CAPITAL LETTER OMEGA
        /*0217*/ { 0x03, 0x05, 0x00, 0x07, 0x02, 0x02, 0x02, 0x07, 0x00 },   // 0xceaa  Ϊ    GREEK CAPITAL LETTER IOTA WITH DIALYTIKA
        /*0218*/ { 0x07, 0x14, 0x41, 0x22, 0x14, 0x08, 0x08, 0x1c, 0x00 },   // 0xceab  Ϋ    GREEK CAPITAL LETTER UPSILON WITH DIALYTIKA
        /*0219*/ { 0x05, 0x04, 0x01, 0x0d, 0x12, 0x12, 0x15, 0x0d, 0x00 },   // 0xceac  ά    GREEK SMALL LETTER ALPHA WITH TONOS
        /*0220*/ { 0x04, 0x03, 0x00, 0x07, 0x08, 0x06, 0x08, 0x07, 0x00 },   // 0xcead  έ    GREEK SMALL LETTER EPSILON WITH TONOS
        /*0221*/ { 0x04, 0x01, 0x02, 0x00, 0x0a, 0x0d, 0x09, 0x09, 0x01 },   // 0xceae  ή    GREEK SMALL LETTER ETA WITH TONOS
        /*0222*/ { 0x02, 0x01, 0x02, 0x00, 0x02, 0x02, 0x02, 0x01, 0x00 },   // 0xceaf  ί    GREEK SMALL LETTER IOTA WITH TONOS
        /*0223*/ { 0x05, 0x02, 0x0d, 0x00, 0x11, 0x09, 0x09, 0x06, 0x00 },   // 0xceb0  ΰ    GREEK SMALL LETTER UPSILON WITH DIALYTIKA AND TONOS
        /*0224*/ { 0x05, 0x00, 0x01, 0x0d, 0x12, 0x12, 0x15, 0x0d, 0x00 },   // 0xceb1  α    GREEK SMALL LETTER ALPHA
        /*0225*/ { 0x04, 0x0E, 0x09, 0x09, 0x0E, 0x09, 0x09, 0x0A, 0x08 },   // 0xceb2  β    GREEK SMALL LETTER BETA
        /*0226*/ { 0x06, 0x00, 0x31, 0x0A, 0x04, 0x0A, 0x11, 0x0A, 0x04 },   // 0xceb3  γ    GREEK SMALL LETTER GAMMA
        /*0227*/ { 0x05, 0x0C, 0x10, 0x1E, 0x11, 0x11, 0x11, 0x0E, 0x00 },   // 0xceb4  δ    GREEK SMALL LETTER DELTA
        /*0228*/ { 0x04, 0x00, 0x07, 0x08, 0x06, 0x08, 0x09, 0x07, 0x00 },   // 0xceb5  ε    GREEK SMALL LETTER EPSILON
        /*0229*/ { 0x04, 0x01, 0x06, 0x08, 0x08, 0x08, 0x06, 0x01, 0x03 },   // 0xceb6  ζ    GREEK SMALL LETTER ZETA
        /*0230*/ { 0x05, 0x00, 0x00, 0x16, 0x19, 0x11, 0x11, 0x11, 0x01 },   // 0xceb7  η    GREEK SMALL LETTER ETA
        /*0231*/ { 0x05, 0x00, 0x00, 0x0E, 0x11, 0x1F, 0x11, 0x0E, 0x00 },   // 0xceb8  θ    GREEK SMALL LETTER THETA
        /*0232*/ { 0x03, 0x00, 0x00, 0x06, 0x02, 0x02, 0x02, 0x01, 0x00 },   // 0xceb9  ι    GREEK SMALL LETTER IOTA
        /*0233*/ { 0x04, 0x00, 0x09, 0x0A, 0x0C, 0x0A, 0x09, 0x09, 0x00 },   // 0xceba  κ    GREEK SMALL LETTER KAPPA
        /*0234*/ { 0x06, 0x00, 0x08, 0x04, 0x0C, 0x0A, 0x12, 0x21, 0x00 },   // 0xcebb  λ    GREEK SMALL LETTER LAMDA
        /*0235*/ { 0x05, 0x00, 0x00, 0x11, 0x11, 0x11, 0x19, 0x16, 0x10 },   // 0xcebc  μ    GREEK SMALL LETTER MU
        /*0236*/ { 0x07, 0x00, 0x00, 0x41, 0x22, 0x22, 0x14, 0x08, 0x00 },   // 0xcebd  ν    GREEK SMALL LETTER NU
        /*0237*/ { 0x05, 0x03, 0x04, 0x02, 0x0C, 0x10, 0x10, 0x0E, 0x01 },   // 0xcebe  ξ    GREEK SMALL LETTER XI
        /*0238*/ { 0x05, 0x00, 0x00, 0x0E, 0x11, 0x11, 0x11, 0x0E, 0x00 },   // 0xcebf  ο    GREEK SMALL LETTER OMICRON
        /*0239*/ { 0x06, 0x00, 0x00, 0x3F, 0x12, 0x12, 0x12, 0x12, 0x00 },   // 0xcf80  π    GREEK SMALL LETTER PI
        /*0240*/ { 0x05, 0x00, 0x00, 0x0E, 0x11, 0x11, 0x19, 0x16, 0x10 },   // 0xcf81  ρ    GREEK SMALL LETTER RHO
        /*0241*/ { 0x04, 0x00, 0x06, 0x08, 0x08, 0x06, 0x01, 0x06, 0x08 },   // 0xcf82  ς    GREEK SMALL LETTER FINAL SIGMA
        /*0242*/ { 0x05, 0x00, 0x01, 0x0E, 0x11, 0x11, 0x11, 0x0E, 0x00 },   // 0xcf83  σ    GREEK SMALL LETTER SIGMA
        /*0243*/ { 0x05, 0x00, 0x01, 0x0E, 0x14, 0x04, 0x04, 0x02, 0x00 },   // 0xcf84  τ    GREEK SMALL LETTER TAU
        /*0244*/ { 0x06, 0x00, 0x00, 0x32, 0x11, 0x11, 0x11, 0x0E, 0x00 },   // 0xcf85  υ    GREEK SMALL LETTER UPSILON
        /*0245*/ { 0x07, 0x00, 0x26, 0x49, 0x49, 0x49, 0x3E, 0x08, 0x08 },   // 0xcf86  φ    GREEK SMALL LETTER PHI
        /*0246*/ { 0x07, 0x00, 0x62, 0x12, 0x14, 0x08, 0x14, 0x23, 0x00 },   // 0xcf87  χ    GREEK SMALL LETTER CHI
        /*0247*/ { 0x07, 0x00, 0x49, 0x49, 0x2A, 0x1C, 0x08, 0x08, 0x08 },   // 0xcf88  ψ    GREEK SMALL LETTER PSI
        /*0248*/ { 0x07, 0x00, 0x00, 0x22, 0x49, 0x49, 0x49, 0x36, 0x00 },   // 0xcf89  ω    GREEK SMALL LETTER OMEGA

        /*0249*/ { 0x04, 0x04, 0x02, 0x00, 0x0f, 0x08, 0x0f, 0x08, 0x0f },   // 0xd080  Ѐ    CYRILLIC CAPITAL LETTER IE WITH GRAVE
        /*0250*/ { 0x05, 0x0A, 0x00, 0x1F, 0x10, 0x1E, 0x10, 0x1F, 0x00 },   // 0xd081  Ё    CYRILLIC CAPITAL LETTER IO
        /*0251*/ { 0x07, 0x7C, 0x10, 0x16, 0x19, 0x11, 0x11, 0x16, 0x00 },   // 0xd082  Ђ    CYRILLIC CAPITAL LETTER DJE
        /*0252*/ { 0x04, 0x02, 0x00, 0x0F, 0x08, 0x08, 0x08, 0x08, 0x00 },   // 0xd083  Ѓ    CYRILLIC CAPITAL LETTER GJE
        /*0253*/ { 0x05, 0x0E, 0x11, 0x10, 0x1C, 0x10, 0x11, 0x0E, 0x00 },   // 0xd084  Є    CYRILLIC CAPITAL LETTER UKRAINIAN IE
        /*0254*/ { 0x05, 0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E, 0x00 },   // 0xd085  Ѕ    CYRILLIC CAPITAL LETTER DZE
        /*0255*/ { 0x03, 0x07, 0x02, 0x02, 0x02, 0x02, 0x02, 0x07, 0x00 },   // 0xd086  І    CYRILLIC CAPITAL LETTER BYELORUSSION-UKRAINIAN I
        /*0256*/ { 0x03, 0x05, 0x00, 0x02, 0x02, 0x02, 0x02, 0x02, 0x00 },   // 0xd087  Ї    CYRILLIC CAPITAL LETTER YI
        /*0257*/ { 0x04, 0x0F, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C, 0x00 },   // 0xd088  Ј    CYRILLIC CAPITAL LETTER JE
        /*0258*/ { 0x07, 0x38, 0x28, 0x28, 0x2E, 0x29, 0x29, 0x4E, 0x00 },   // 0xd089  Љ    CYRILLIC CAPITAL LETTER LJE
        /*0259*/ { 0x06, 0x28, 0x28, 0x28, 0x3E, 0x29, 0x29, 0x2E, 0x00 },   // 0xd08a  Њ    CYRILLIC CAPITAL LETTER NJE
        /*0260*/ { 0x06, 0x38, 0x10, 0x16, 0x19, 0x11, 0x11, 0x11, 0x00 },   // 0xd08b  Ћ    CYRILLIC CAPITAL LETTER TSHE
        /*0261*/ { 0x06, 0x29, 0x22, 0x24, 0x38, 0x24, 0x22, 0x21, 0x00 },   // 0xd08c  Ќ    CYRILLIC CAPITAL LETTER KJE
        /*0262*/ { 0x05, 0x08, 0x04, 0x11, 0x13, 0x15, 0x19, 0x11, 0x00 },   // 0xd08d  Ѝ    CYRILLIC CAPITAL LETTER I WITH GRAVE
        /*0263*/ { 0x05, 0x15, 0x15, 0x11, 0x09, 0x07, 0x01, 0x02, 0x1C },   // 0xd08e  Ў    CYRILLIC CAPITAL LETTER SHORT U
        /*0264*/ { 0x05, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1F, 0x04 },   // 0xd08f  Џ    CYRILLIC CAPITAL LETTER DZHE
        /*0265*/ { 0x05, 0x04, 0x0a, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x00 },   // 0xd090  А    CYRILLIC CAPITAL LETTER A
        /*0266*/ { 0x05, 0x1F, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x1E, 0x00 },   // 0xd091  Б    CYRILLIC CAPITAL LETTER BE
        /*0267*/ { 0x05, 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E, 0x00 },   // 0xd092  В    CYRILLIC CAPITAL LETTER VE
        /*0268*/ { 0x05, 0x1F, 0x11, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00 },   // 0xd093  Г    CYRILLIC CAPITAL LETTER GHE
        /*0269*/ { 0x06, 0x0C, 0x12, 0x12, 0x12, 0x12, 0x12, 0x3F, 0x21 },   // 0xd094  Д    CYRILLIC CAPITAL LETTER DE
        /*0270*/ { 0x05, 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F, 0x00 },   // 0xd095  Е    CYRILLIC CAPITAL LETTER IE
        /*0271*/ { 0x05, 0x15, 0x15, 0x15, 0x0E, 0x15, 0x15, 0x15, 0x00 },   // 0xd096  Ж    CYRILLIC CAPITAL LETTER ZHE
        /*0272*/ { 0x05, 0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E, 0x00 },   // 0xd097  З    CYRILLIC CAPITAL LETTER ZE
        /*0273*/ { 0x05, 0x11, 0x11, 0x13, 0x15, 0x19, 0x11, 0x11, 0x00 },   // 0xd098  И    CYRILLIC CAPITAL LETTER I
        /*0274*/ { 0x05, 0x15, 0x11, 0x13, 0x15, 0x19, 0x11, 0x11, 0x00 },   // 0xd099  Й    CYRILLIC CAPITAL LETTER SHORT I
        /*0275*/ { 0x05, 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11, 0x00 },   // 0xd09a  К    CYRILLIC CAPITAL LETTER KA
        /*0276*/ { 0x05, 0x07, 0x09, 0x09, 0x09, 0x09, 0x09, 0x11, 0x00 },   // 0xd09b  Л    CYRILLIC CAPITAL LETTER EL
        /*0277*/ { 0x05, 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11, 0x00 },   // 0xd09c  М    CYRILLIC CAPITAL LETTER EM
        /*0278*/ { 0x05, 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11, 0x00 },   // 0xd09d  Н    CYRILLIC CAPITAL LETTER EN
        /*0279*/ { 0x05, 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E, 0x00 },   // 0xd09e  О    CYRILLIC CAPITAL LETTER O
        /*0280*/ { 0x05, 0x1F, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x00 },   // 0xd09f  П    CYRILLIC CAPITAL LETTER PE
        /*0281*/ { 0x05, 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10, 0x00 },   // 0xd0a0  Р    CYRILLIC CAPITAL LETTER ER
        /*0282*/ { 0x05, 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E, 0x00 },   // 0xd0a1  С    CYRILLIC CAPITAL LETTER ES
        /*0283*/ { 0x05, 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00 },   // 0xd0a2  Т    CYRILLIC CAPITAL LETTER TE
        /*0284*/ { 0x05, 0x11, 0x11, 0x11, 0x0F, 0x01, 0x11, 0x0E, 0x00 },   // 0xd0a3  У    CYRILLIC CAPITAL LETTER U
        /*0285*/ { 0x07, 0x08, 0x3E, 0x49, 0x49, 0x49, 0x3E, 0x08, 0x00 },   // 0xd0a4  Ф    CYRILLIC CAPITAL LETTER EF
        /*0286*/ { 0x05, 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11, 0x00 },   // 0xd0a5  Х    CYRILLIC CAPITAL LETTER HA
        /*0287*/ { 0x06, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x3F, 0x01 },   // 0xd0a6  Ц    CYRILLIC CAPITAL LETTER TSE
        /*0288*/ { 0x05, 0x11, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x01, 0x00 },   // 0xd0a7  Ч    CYRILLIC CAPITAL LETTER CHE
        /*0289*/ { 0x05, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x1F, 0x00 },   // 0xd0a8  Ш    CYRILLIC CAPITAL LETTER SHA
        /*0290*/ { 0x06, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x3F, 0x01 },   // 0xd0a9  Щ    CYRILLIC CAPITAL LETTER SHCHA
        /*0291*/ { 0x07, 0x70, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x1E, 0x00 },   // 0xd0aa  Ъ    CYRILLIC CAPITAL LETTER HARD SIGN
        /*0292*/ { 0x07, 0x41, 0x41, 0x41, 0x79, 0x45, 0x45, 0x79, 0x00 },   // 0xd0ab  Ы    CYRILLIC CAPITAL LETTER YERU
        /*0293*/ { 0x05, 0x10, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x1E, 0x00 },   // 0xd0ac  Ь    CYRILLIC CAPITAL LETTER SOFT SIGN
        /*0294*/ { 0x04, 0x0E, 0x01, 0x01, 0x07, 0x01, 0x01, 0x0E, 0x00 },   // 0xd0ad  Э    CYRILLIC CAPITAL LETTER E
        /*0295*/ { 0x06, 0x26, 0x29, 0x29, 0x39, 0x29, 0x29, 0x26, 0x00 },   // 0xd0ae  Ю    CYRILLIC CAPITAL LETTER YU
        /*0296*/ { 0x05, 0x0F, 0x11, 0x11, 0x0F, 0x05, 0x09, 0x11, 0x00 },   // 0xd0af  Я    CYRILLIC CAPITAL LETTER YA
        /*0297*/ { 0x05, 0x00, 0x00, 0x0E, 0x01, 0x0F, 0x11, 0x0F, 0x00 },   // 0xd0b0  а    CYRILLIC SMALL LETTER A
        /*0298*/ { 0x05, 0x03, 0x0C, 0x10, 0x1E, 0x11, 0x11, 0x0E, 0x00 },   // 0xd0b1  б    CYRILLIC SMALL LETTER BE
        /*0299*/ { 0x05, 0x00, 0x00, 0x1E, 0x11, 0x1E, 0x11, 0x1E, 0x00 },   // 0xd0b2  в    CYRILLIC SMALL LETTER VE
        /*0300*/ { 0x04, 0x00, 0x00, 0x0F, 0x09, 0x08, 0x08, 0x08, 0x00 },   // 0xd0b3  г    CYRILLIC SMALL LETTER GHE
        /*0301*/ { 0x06, 0x00, 0x00, 0x0C, 0x12, 0x12, 0x12, 0x3F, 0x21 },   // 0xd0b4  д    CYRILLIC SMALL LETTER DE
        /*0302*/ { 0x05, 0x00, 0x00, 0x0E, 0x11, 0x1F, 0x10, 0x0E, 0x00 },   // 0xd0b5  е    CYRILLIC SMALL LETTER IE
        /*0303*/ { 0x05, 0x00, 0x00, 0x15, 0x15, 0x0E, 0x15, 0x15, 0x00 },   // 0xd0b6  ж    CYRILLIC SMALL LETTER ZHE
        /*0304*/ { 0x05, 0x00, 0x00, 0x1E, 0x01, 0x0E, 0x01, 0x1E, 0x00 },   // 0xd0b7  з    CYRILLIC SMALL LETTER ZE
        /*0305*/ { 0x05, 0x00, 0x00, 0x11, 0x13, 0x15, 0x19, 0x11, 0x00 },   // 0xd0b8  и    CYRILLIC SMALL LETTER I
        /*0306*/ { 0x05, 0x0e, 0x00, 0x11, 0x13, 0x15, 0x19, 0x11, 0x00 },   // 0xd0b9  й    CYRILLIC SMALL LETTER SHORT I
        /*0307*/ { 0x05, 0x00, 0x00, 0x11, 0x12, 0x1C, 0x12, 0x11, 0x00 },   // 0xd0ba  к    CYRILLIC SMALL LETTER KA
        /*0308*/ { 0x05, 0x00, 0x00, 0x07, 0x09, 0x09, 0x09, 0x11, 0x00 },   // 0xd0bb  л    CYRILLIC SMALL LETTER EL
        /*0309*/ { 0x05, 0x00, 0x00, 0x11, 0x1B, 0x15, 0x11, 0x11, 0x00 },   // 0xd0bc  м    CYRILLIC SMALL LETTER EM
        /*0310*/ { 0x05, 0x00, 0x00, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x00 },   // 0xd0bd  н    CYRILLIC SMALL LETTER EN
        /*0311*/ { 0x05, 0x00, 0x00, 0x0E, 0x11, 0x11, 0x11, 0x0E, 0x00 },   // 0xd0be  о    CYRILLIC SMALL LETTER O
        /*0312*/ { 0x05, 0x00, 0x00, 0x1F, 0x11, 0x11, 0x11, 0x11, 0x00 },   // 0xd0bf  п    CYRILLIC SMALL LETTER PE
        /*0313*/ { 0x05, 0x00, 0x00, 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10 },   // 0xd180  р    CYRILLIC SMALL LETTER ER
        /*0314*/ { 0x04, 0x00, 0x00, 0x07, 0x08, 0x08, 0x08, 0x07, 0x00 },   // 0xd181  с    CYRILLIC SMALL LETTER ES
        /*0315*/ { 0x05, 0x00, 0x00, 0x1F, 0x04, 0x04, 0x04, 0x04, 0x00 },   // 0xd182  т    CYRILLIC SMALL LETTER TE
        /*0316*/ { 0x05, 0x00, 0x00, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E },   // 0xd183  у    CYRILLIC SMALL LETTER U
        /*0317*/ { 0x07, 0x00, 0x00, 0x08, 0x3E, 0x49, 0x3E, 0x08, 0x08 },   // 0xd184  ф    CYRILLIC SMALL LETTER EF
        /*0318*/ { 0x05, 0x00, 0x00, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x00 },   // 0xd185  х    CYRILLIC SMALL LETTER HA
        /*0319*/ { 0x05, 0x00, 0x00, 0x12, 0x12, 0x12, 0x12, 0x1F, 0x01 },   // 0xd186  ц    CYRILLIC SMALL LETTER TSE
        /*0320*/ { 0x05, 0x00, 0x00, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x00 },   // 0xd187  ч    CYRILLIC SMALL LETTER CHE
        /*0321*/ { 0x05, 0x00, 0x00, 0x15, 0x15, 0x15, 0x15, 0x1F, 0x00 },   // 0xd188  ш    CYRILLIC SMALL LETTER CHA
        /*0322*/ { 0x06, 0x00, 0x00, 0x2A, 0x2A, 0x2A, 0x2A, 0x3F, 0x01 },   // 0xd189  щ    CYRILLIC SMALL LETTER SHCHA
        /*0323*/ { 0x05, 0x00, 0x00, 0x18, 0x08, 0x0E, 0x09, 0x0E, 0x00 },   // 0xd18a  ъ    CYRILLIC SMALL LETTER HARD SIGN
        /*0324*/ { 0x06, 0x00, 0x00, 0x21, 0x21, 0x39, 0x25, 0x39, 0x00 },   // 0xd18b  ы    CYRILLIC SMALL LETTER YERU
        /*0325*/ { 0x04, 0x00, 0x00, 0x08, 0x08, 0x0E, 0x09, 0x0E, 0x00 },   // 0xd18c  ь    CYRILLIC SMALL LETTER SOFT SIGN
        /*0326*/ { 0x04, 0x00, 0x00, 0x0E, 0x01, 0x07, 0x01, 0x0E, 0x00 },   // 0xd18d  э    CYRILLIC SMALL LETTER E
        /*0327*/ { 0x06, 0x00, 0x00, 0x26, 0x29, 0x39, 0x29, 0x26, 0x00 },   // 0xd18e  ю    CYRILLIC SMALL LETTER YU
        /*0328*/ { 0x04, 0x00, 0x00, 0x07, 0x09, 0x07, 0x05, 0x09, 0x00 },   // 0xd18f  я    CYRILLIC SMALL LETTER YA
        /*0329*/ { 0x05, 0x0A, 0x00, 0x0E, 0x11, 0x1F, 0x10, 0x0E, 0x00 },   // 0xd191  ё    CYRILLIC SMALL LETTER IO
        /*0330*/ { 0x06, 0x10, 0x38, 0x10, 0x16, 0x19, 0x11, 0x11, 0x02 },   // 0xd192  ђ    CYRILLIC SMALL LETTER DJE
        /*0331*/ { 0x03, 0x00, 0x02, 0x00, 0x07, 0x04, 0x04, 0x04, 0x00 },   // 0xd193  ѓ    CYRILLIC SMALL LETTER GJE
        /*0332*/ { 0x04, 0x00, 0x00, 0x07, 0x08, 0x0E, 0x08, 0x07, 0x00 },   // 0xd194  є    CYRILLIC SMALL LETTER UKRAINIAN IE
        /*0333*/ { 0x04, 0x00, 0x00, 0x07, 0x08, 0x06, 0x01, 0x0E, 0x00 },   // 0xd195  ѕ    CYRILLIC SMALL LETTER DZE
        /*0334*/ { 0x03, 0x00, 0x02, 0x00, 0x06, 0x02, 0x02, 0x07, 0x00 },   // 0xd196  і    CYRILLIC SMALL LETTER BYELORUSSION-UKRAINIAN I
        /*0335*/ { 0x03, 0x00, 0x05, 0x00, 0x02, 0x02, 0x02, 0x02, 0x00 },   // 0xd197  ї    CYRILLIC SMALL LETTER YI
        /*0336*/ { 0x04, 0x00, 0x01, 0x00, 0x03, 0x01, 0x01, 0x09, 0x06 },   // 0xd198  ј    CYRILLIC SMALL LETTER JE
        /*0337*/ { 0x07, 0x00, 0x00, 0x38, 0x28, 0x2E, 0x29, 0x4E, 0x00 },   // 0xd199  љ    CYRILLIC SMALL LETTER LJE
        /*0338*/ { 0x06, 0x00, 0x28, 0x28, 0x28, 0x3E, 0x29, 0x2E, 0x00 },   // 0xd19a  њ    CYRILLIC SMALL LETTER NJE
        /*0339*/ { 0x06, 0x10, 0x38, 0x10, 0x16, 0x19, 0x11, 0x11, 0x00 },   // 0xd19b  ћ    CYRILLIC SMALL LETTER TSHE
        /*0340*/ { 0x04, 0x0A, 0x08, 0x09, 0x0A, 0x0C, 0x0A, 0x09, 0x00 },   // 0xd19c  ќ    CYRILLIC SMALL LETTER KJE
        /*0341*/ { 0x04, 0x04, 0x02, 0x00, 0x09, 0x0b, 0x0d, 0x09, 0x00 },   // 0xd19d  ѝ    CYRILLIC SMALL LETTER I WITH GRAVE
        /*0342*/ { 0x05, 0x04, 0x04, 0x11, 0x11, 0x0F, 0x01, 0x11, 0x0E },   // 0xd19e  ў    CYRILLIC SMALL LETTER SHORT U
        /*0343*/ { 0x05, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11, 0x1F, 0x04 },   // 0xd19f  џ    CYRILLIC SMALL LETTER DZHE
        /*0344*/ { 0x04, 0x01, 0x0F, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00 },   // 0xd290  Ґ    CYRILLIC CAPITAL LETTER GHE WITH UPTURN
        /*0345*/ { 0x04, 0x00, 0x01, 0x0F, 0x08, 0x08, 0x08, 0x08, 0x00 },   // 0xd291  ґ    CYRILLIC SMALL LETTER GHE WITH UPTURN
};

//----------------------------------------------------------------------------------------------------------------------

extern "C" uint8_t sntp_getreachability(uint8_t);

bool getNtpServer() { // connect WiFi -> fetch ntp packet -> disconnect Wifi
    uint8_t cnt = 0;
    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, PW);

    while(WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        cnt++;
        if(cnt > 20)
            break;
    }

    if(WiFi.status() != WL_CONNECTED) return false;

    Serial.println("\nconnected with: " + WiFi.SSID());
    Serial.println("IP Address: " + WiFi.localIP().toString());
    bool timeSync;
    uint32_t timeout { millis() };
    configTime(TZName, NTP_SERVER[0], NTP_SERVER[1], NTP_SERVER[2]);
    do {
        delay(25);
        if(millis() - timeout >= 1e3) {
            Serial.printf("waiting for NTP %02ld sec\n", (millis() - timeout) / 1000);
            delay(975);
        }
        sntp_getreachability(0) ? timeSync = true : sntp_getreachability(1) ? timeSync = true :
        sntp_getreachability(2) ? timeSync = true : false;
    } while(millis() - timeout <= 16e3 && !timeSync);

    Serial.printf("NTP Synchronization %s!\n", timeSync ? "successfully" : "failed");
    WiFi.disconnect();
    return timeSync;
}
//----------------------------------------------------------------------------------------------------------------------

const uint8_t InitArr[7][2] = {
        { 0x0C, 0x00 },    // display off
        { 0x00, 0xFF },    // no LEDtest
        { 0x09, 0x00 },    // BCD off
        { 0x0F, 0x00 },    // normal operation
        { 0x0B, 0x07 },    // start display
        { 0x0A, 0x04 },    // brightness
        { 0x0C, 0x01 }     // display on
};
//----------------------------------------------------------------------------------------------------------------------

void helpArr_init(void) {  //helperarray init
    uint8_t i, j, k;
    j = 0;
    k = 0;
    for (i = 0; i < anzMAX * 8; i++) {
        _helpArrPos[i] = (1 << j);   //bitmask
        _helpArrMAX[i] = k;
        j++;
        if (j > 7) {
            j = 0;
            k++;
        }
    }
}
//----------------------------------------------------------------------------------------------------------------------

void max7219_init() {  //all MAX7219 init
    uint8_t i, j;
    for (i = 0; i < 7; i++) {
        digitalWrite(MAX_CS, LOW);
        delayMicroseconds(1);
        for (j = 0; j < anzMAX; j++) {
            SPI.write(InitArr[i][0]);  //register
            SPI.write(InitArr[i][1]);  //value
        }
        digitalWrite(MAX_CS, HIGH);
    }
}
//----------------------------------------------------------------------------------------------------------------------

void max7219_set_brightness(unsigned short br) { //brightness MAX7219
    uint8_t j;
    if (br < 16) {
        digitalWrite(MAX_CS, LOW);
        delayMicroseconds(1);
        for (j = 0; j < anzMAX; j++) {
            SPI.write(0x0A);  //register
            SPI.write(br);    //value
        }
        digitalWrite(MAX_CS, HIGH);
    }
}
//----------------------------------------------------------------------------------------------------------------------

void clear_Display() {  //clear all
    uint8_t i, j;
    for(i = 0; i < 8; i++) {    //8 rows
        digitalWrite(MAX_CS, LOW);
        delayMicroseconds(1);
        for(j = anzMAX; j > 0; j--) {
            _LEDarr[j - 1][i] = 0;       //LEDarr clear
            SPI.write(i + 1);           //current row
            SPI.write(_LEDarr[j - 1][i]);
        }
        digitalWrite(MAX_CS, HIGH);
    }
}
//----------------------------------------------------------------------------------------------------------------------

void rotate_90() { // for Generic displays
    for(uint8_t k = anzMAX; k > 0; k--) {
        uint8_t i, j, m, imask, jmask;
        uint8_t tmp[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
        for(i = 0, imask = 0x01; i < 8; i++, imask <<= 1) {
            for(j = 0, jmask = 0x01; j < 8; j++, jmask <<= 1) {
                if(_LEDarr[k - 1][i] & jmask) {
                    tmp[j] |= imask;
                }
            }
        }
        for(m = 0; m < 8; m++) {
            _LEDarr[k - 1][m] = tmp[m];
        }
    }
}
//----------------------------------------------------------------------------------------------------------------------

void refresh_display() { //take info into LEDarr
    uint8_t i, j;
#ifdef ROTATE_90
    rotate_90();
#endif
    for(i = 0; i < 8; i++) {    //8 rows
        digitalWrite(MAX_CS, LOW);
        delayMicroseconds(1);
        for(j = anzMAX; j > 0; j--) {
            SPI.write(i + 1);  //current row
#ifdef REVERSE_HORIZONTAL
            SPI.setBitOrder(LSBFIRST);      // bitorder for reverse columns
#endif

#ifdef REVERSE_VERTICAL
            SPI.write(_LEDarr[j - 1][7-i]);
#else
            SPI.write(_LEDarr[j - 1][i]);
#endif

#ifdef REVERSE_HORIZONTAL
            SPI.setBitOrder(MSBFIRST);      // reset bitorder
#endif
        }
        digitalWrite(MAX_CS, HIGH);
    }
}
//----------------------------------------------------------------------------------------------------------------------

uint8_t char2Arr_t(unsigned short ch, int PosX, short PosY) { //characters into arr, shows only the time
    int i, j, k, l, m, o1, o2, o3, o4 = 0;
    PosX++;
    k = ch - 0x30;                       //ASCII position in font
    if((k >= 0) && (k < 11)) {           //character found in font?
        o4 = font_t[k][0];               //character width
        o3 = 1 << (o4 - 1);
        for(i = 0; i < o4; i++) {
            if(((PosX - i <= _maxPosX) && (PosX - i >= 0)) && ((PosY > -8) && (PosY < 8))) { //within matrix?
                o1 = _helpArrPos[PosX - i];
                o2 = _helpArrMAX[PosX - i];
                for(j = 0; j < 8; j++) {
                    if(((PosY >= 0) && (PosY <= j)) || ((PosY < 0) && (j < PosY + 8))) { //scroll vertical
                        l = font_t[k][j + 1];
                        m = (l & (o3 >> i));  //e.g. o4=7  0zzzzz0, o4=4  0zz0
                        if(m > 0)
                            _LEDarr[o2][j - PosY] = _LEDarr[o2][j - PosY] | (o1);  //set point
                        else
                            _LEDarr[o2][j - PosY] = _LEDarr[o2][j - PosY] & (~o1); //clear point
                    }
                }
            }
        }
    }
    return o4;
}
//----------------------------------------------------------------------------------------------------------------------

uint8_t char2Arr_p(uint16_t ch, int PosX) { //characters into arr, proportional font
    int i, j, l, m, o1, o2, o3, o4 = 0;
    if(ch <= 345) {                   //character found in font?
        o4 = font_p[ch][0];              //character width
        o3 = 1 << (o4 - 1);
        for(i = 0; i < o4; i++) {
            if((PosX - i <= _maxPosX) && (PosX - i >= 0)) { //within matrix?
                o1 = _helpArrPos[PosX - i];
                o2 = _helpArrMAX[PosX - i];
                for(j = 0; j < 8; j++) {
                    l = font_p[ch][j + 1];
                    m = (l & (o3 >> i));  //e.g. o4=7  0zzzzz0, o4=4  0zz0
                    if(m > 0)
                        _LEDarr[o2][j] = _LEDarr[o2][j] | (o1);  //set point
                    else
                        _LEDarr[o2][j] = _LEDarr[o2][j] & (~o1); //clear point
                }
            }
        }
    }
    return o4;
}
//----------------------------------------------------------------------------------------------------------------------

uint16_t scrolltext(int16_t posX, String txt) {
    uint16_t i=0, j=0;
    boolean k=false;
    while((txt[i]!=0)&&(j<256)){
        if((txt[i]>=0x20)&&(txt[i]<=0x7f)){     // ASCII section
            _chbuf[j]=txt[i]-0x20; k=true; i++; j++;
        }
        if(txt[i]==0xC2){   // basic latin section (0x80...0x9f are controls, not used)
            if((txt[i+1]>=0xA0)&&(txt[i+1]<=0xBF)){_chbuf[j]=txt[i+1]-0x40; k=true; i+=2; j++;}
        }
        if(txt[i]==0xC3){   // latin1 supplement section
            if((txt[i+1]>=0x80)&&(txt[i+1]<=0xBF)){_chbuf[j]=txt[i+1]+0x00; k=true; i+=2; j++;}
        }
        if(txt[i]==0xCE){   // greek section
            if((txt[i+1]>=0x91)&&(txt[i+1]<=0xBF)){_chbuf[j]=txt[i+1]+0x2F; k=true; i+=2; j++;}
        }
        if(txt[i]==0xCF){   // greek section
            if((txt[i+1]>=0x80)&&(txt[i+1]<=0x89)){_chbuf[j]=txt[i+1]+0x6F; k=true; i+=2; j++;}
        }
        if(txt[i]==0xD0){   // cyrillic section
            if((txt[i+1]>=0x80)&&(txt[i+1]<=0xBF)){_chbuf[j]=txt[i+1]+0x79; k=true; i+=2; j++;}
        }
        if(txt[i]==0xD1){   // cyrillic section
            if((txt[i+1]>=0x80)&&(txt[i+1]<=0x9F)){_chbuf[j]=txt[i+1]+0xB9; k=true; i+=2; j++;}
        }
        if(k==false){
            _chbuf[j]=0x00; // space 1px
            i++; j++;
        }
        k=false;
    }
//  _chbuf stores the position of th char in font and in j is the length of the real string

    int16_t p=0;
    for(int k=0; k<j; k++){
        p+=char2Arr_p(_chbuf[k], posX - p);
        p+=char2Arr_p(0, posX - p); // 1px space
        if(_chbuf[k]==0) p+=2;      // +2px space
    }
    return p;
}
//----------------------------------------------------------------------------------------------------------------------

void timer50ms() {

    static unsigned int cnt50ms = 0;
    static unsigned int cnt1s = 0;
    static unsigned int cnt1h = 0;
    _f_tckr50ms = true;
    cnt50ms++;
    if (cnt50ms == 20) {
        cnt1s++;
        cnt50ms = 0;
    }
    if (cnt1s == 3600) { // 1h
        cnt1h++;
        cnt1s = 0;
    }
    if (cnt1h == 24) { // 1d
        if (getNtpServer()) Serial.println("internal clock synchronized with ntp");
        else Serial.println("no daily timepacket received");
        cnt1h = 0;
    }
}
//----------------------------------------------------------------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    delay(100);
    pinMode(MAX_CS, OUTPUT);
    digitalWrite(MAX_CS, HIGH);

    if (!getNtpServer()){
        Serial.println("can't get time from NTP");
        while(1){;}  // endless loop, stop
    }

    SPI.begin();
    helpArr_init();
    max7219_init();
    clear_Display();
    max7219_set_brightness(BRIGHTNESS);
    tckr.attach(0.05, timer50ms);    // every 50 msec

#ifdef SCROLLDOWN
    _f_updown = true;
#else
    _f_updown = false;
#endif //SCROLLDOWN

    _zPosX = _maxPosX;
    _dPosX = -8;

    refresh_display();
}
//----------------------------------------------------------------------------------------------------------------------

void loop() {
    static uint8_t sec1 = 0, sec2 = 0, min1 = 0, min2 = 0, hour1 = 0, hour2 = 0;
    static uint8_t sec11 = 0, sec12 = 0, sec21 = 0, sec22 = 0;
    static uint8_t min11 = 0, min12 = 0, min21 = 0, min22 = 0;
    static uint8_t hour11 = 0, hour12 = 0, hour21 = 0, hour22 = 0;
    static time_t lastsec {0};
    static signed int x = 0; //x1,x2;
    static signed int y = 0, y1 = 0, y2 = 0;
    static unsigned int sc1 = 0, sc2 = 0, sc3 = 0, sc4 = 0, sc5 = 0, sc6 = 0;
    static uint16_t sctxtlen=0;
    static boolean f_scrollend_y = false;
    static boolean f_scroll_x1 = false;
    static boolean f_scroll_x2 = false;

    time_t now = time(&now);
    localtime_r(&now, &tm);

    if (_f_updown == false) {
        y2 = -9;
        y1 = 8;
    }

    if (_f_updown == true) { //scroll  up to down
        y2 = 8;
        y1 = -8;
    }

    if (tm.tm_sec != lastsec) {
        lastsec = tm.tm_sec;

        sec1 = (tm.tm_sec % 10);
        sec2 = (tm.tm_sec / 10);
        min1 = (tm.tm_min % 10);
        min2 = (tm.tm_min / 10);
#ifdef FORMAT24H
        hour1 = (tm.tm_hour % 10);  // 24 hour format
        hour2 = (tm.tm_hour / 10);
#else
        uint8_t h=tm.tm_hour;    // convert to 12 hour format
        if(h>12) h-=12;
        if(h==0) h=12;
        hour1 = (h % 10);
        hour2 = (h / 10);
#endif //FORMAT24H

        y = y2;                 //scroll updown
        sc1 = 1;
        sec1++;
        if (sec1 == 10) {
            sc2 = 1;
            sec2++;
            sec1 = 0;
        }
        if (sec2 == 6) {
            min1++;
            sec2 = 0;
            sc3 = 1;
        }
        if (min1 == 10) {
            min2++;
            min1 = 0;
            sc4 = 1;
        }
        if (min2 == 6) {
            hour1++;
            min2 = 0;
            sc5 = 1;
        }
        if (hour1 == 10) {
            hour2++;
            hour1 = 0;
            sc6 = 1;
        }
#ifdef FORMAT24H
        if ((hour2 == 2) && (hour1 == 4)) {
            hour1 = 0;
            hour2 = 0;
            sc6 = 1;
        }
#else
        if ((hour2 == 1) && (hour1 == 3)) { // 12 hour format
            hour1 = 1;
            hour2 = 0;
            sc6 = 1;
        }
#endif //FORMAT24H
        sec11 = sec12;
        sec12 = sec1;
        sec21 = sec22;
        sec22 = sec2;
        min11 = min12;
        min12 = min1;
        min21 = min22;
        min22 = min2;
        hour11 = hour12;
        hour12 = hour1;
        hour21 = hour22;
        hour22 = hour2;
        if (tm.tm_sec == 45) f_scroll_x1 = true; // scroll ddmmyy
#ifdef UDTXT
        if (tm.tm_sec == 25) f_scroll_x2 = true; // scroll userdefined text
#endif //UDTXT
    } // end lastsec
// ----------------------------------------------
    if (_f_tckr50ms == true) {
        _f_tckr50ms = false;
// -------------------------------------
        if (f_scroll_x1 == true) {
            _zPosX++;
            _dPosX++;
            if (_dPosX == sctxtlen)  _zPosX = 0;
            if (_zPosX == _maxPosX){f_scroll_x1 = false; _dPosX = -8;}
        }
// -------------------------------------
        if (f_scroll_x2 == true) {
            _zPosX++;
            _dPosX++;
            if (_dPosX == sctxtlen)  _zPosX = 0;
            if (_zPosX == _maxPosX){f_scroll_x2 = false; _dPosX = -8;}
        }
// -------------------------------------
        if (sc1 == 1) {
            if (_f_updown == 1) y--;
            else                y++;
            char2Arr_t(48 + sec12, _zPosX - 42, y);
            char2Arr_t(48 + sec11, _zPosX - 42, y + y1);
            if (y == 0) {sc1 = 0; f_scrollend_y = true;}
        }
        else char2Arr_t(48 + sec1, _zPosX - 42, 0);
//  -------------------------------------
        if (sc2 == 1) {
            char2Arr_t(48 + sec22, _zPosX - 36, y);
            char2Arr_t(48 + sec21, _zPosX - 36, y + y1);
            if (y == 0) sc2 = 0;
        }
        else char2Arr_t(48 + sec2, _zPosX - 36, 0);
        char2Arr_t(':', _zPosX - 32, 0);
//  -------------------------------------
        if (sc3 == 1) {
            char2Arr_t(48 + min12, _zPosX - 25, y);
            char2Arr_t(48 + min11, _zPosX - 25, y + y1);
            if (y == 0) sc3 = 0;
        }
        else char2Arr_t(48 + min1, _zPosX - 25, 0);
// -------------------------------------
        if (sc4 == 1) {
            char2Arr_t(48 + min22, _zPosX - 19, y);
            char2Arr_t(48 + min21, _zPosX - 19, y + y1);
            if (y == 0) sc4 = 0;
        }
        else char2Arr_t(48 + min2, _zPosX - 19, 0);
        char2Arr_t(':', _zPosX - 15 + x, 0);
// -------------------------------------
        if (sc5 == 1) {
            char2Arr_t(48 + hour12, _zPosX - 8, y);
            char2Arr_t(48 + hour11, _zPosX - 8, y + y1);
            if (y == 0) sc5 = 0;
        }
        else char2Arr_t(48 + hour1, _zPosX - 8, 0);
// -------------------------------------
        if (sc6 == 1) {
            char2Arr_t(48 + hour22, _zPosX - 2, y);
            char2Arr_t(48 + hour21, _zPosX - 2, y + y1);
            if (y == 0) sc6 = 0;
        }
        else char2Arr_t(48 + hour2, _zPosX - 2, 0);
//      -------------------------------------
        if(f_scroll_x1){ // day month year
            String txt= "   ";
            txt += WD_arr[tm.tm_wday] + " ";
            txt += String(tm.tm_mday) + ". ";
            txt += M_arr[tm.tm_mon] + " ";
            txt += String(tm.tm_year + 1900) + "   ";
            sctxtlen=scrolltext(_dPosX, txt);
        }
//      -------------------------------------
        if(f_scroll_x2){ // user defined text
#ifdef UDTXT
            sctxtlen=scrolltext(_dPosX, UDTXT );
#endif //UDTXT
        }
//      -------------------------------------
        refresh_display(); //all 50msec
        if (f_scrollend_y == true) f_scrollend_y = false;
    } //end 50ms
// -----------------------------------------------
    if (y == 0) {
        // do something else
    }
}



//*********************************************************************************************************
//    Examples for time zones                                                                             *
//*********************************************************************************************************
//    UTC                       GMT0
//    Africa/Abidjan            GMT0
//    Africa/Accra              GMT0
//    Africa/Addis_Ababa        EAT-3
//    Africa/Algiers            CET-1
//    Africa/Blantyre, Harare   CAT-2
//    Africa/Cairo              EEST
//    Africa/Casablanca         WET0
//    Africa/Freetown           GMT0
//    Africa/Johannesburg       SAST-2
//    Africa/Kinshasa           WAT-1
//    Africa/Lome               GMT0
//    Africa/Maseru             SAST-2
//    Africa/Mbabane            SAST-2
//    Africa/Nairobi            EAT-3
//    Africa/Tripoli            EET-2
//    Africa/Tunis              CET-1CEST,M3.5.0,M10.5.0/3
//    Africa/Windhoek           WAT-1WAST,M9.1.0,M4.1.0
//    America/Adak              HAST10HADT,M3.2.0,M11.1.0
//    America/Alaska            AKST9AKDT,M3.2.0,M11.1.0
//    America/Anguilla,Dominica AST4
//    America/Araguaina         BRT3
//    Argentina/San_Luis        ART3
//    America/Asuncion          PYT4PYST,M10.3.0/0,M3.2.0/0
//    America/Atka              HAST10HADT,M3.2.0,M11.1.0
//    America/Boa_Vista         AMT4
//    America/Bogota            COT5
//    America/Campo_Grande      AMT4AMST,M10.2.0/0,M2.3.0/0
//    America/Caracas           VET4:30
//    America/Catamarca         ART3ARST,M10.1.0/0,M3.3.0/0
//    America/Cayenne           GFT3
//    America/Chicago           CST6CDT,M3.2.0,M11.1.0
//    America/Costa_Rica        CST6
//    America/Los_Angeles       PST8PDT,M3.2.0,M11.1.0
//    America/Dawson_Creek      MST7
//    America/Denver            MST7MDT,M3.2.0,M11.1.0
//    America/Detroit           EST5EDT,M3.2.0,M11.1.0
//    America/Eirunepe          ACT5
//    America/Godthab           WGST
//    America/Guayaquil         ECT5
//    America/Guyana            GYT4
//    America/Havana            CST5CDT,M3.3.0/0,M10.5.0/1
//    America/Hermosillo        MST7
//    America/Jamaica           EST5
//    America/La_Paz            BOT4
//    America/Lima              PET5
//    America/Miquelon          PMST3PMDT,M3.2.0,M11.1.0
//    America/Montevideo        UYT3UYST,M10.1.0,M3.2.0
//    America/Noronha           FNT2
//    America/Paramaribo        SRT3
//    America/Phoenix           MST7
//    America/Santiago          CLST
//    America/Sao_Paulo         BRT3BRST,M10.2.0/0,M2.3.0/0
//    America/Scoresbysund      EGT1EGST,M3.5.0/0,M10.5.0/1
//    America/St_Johns          NST3:30NDT,M3.2.0/0:01,M11.1.0/0:01
//    America/Toronto           EST5EDT,M3.2.0,M11.1.0
//    Antarctica/Casey          WST-8
//    Antarctica/Davis          DAVT-7
//    Antarctica/DumontDUrville DDUT-10
//    Antarctica/Mawson         MAWT-6
//    Antarctica/McMurdo        NZST-12NZDT,M9.5.0,M4.1.0/3
//    Antarctica/Palmer         CLST
//    Antarctica/Rothera        ROTT3
//    Antarctica/South_Pole     NZST-12NZDT,M9.5.0,M4.1.0/3
//    Antarctica/Syowa          SYOT-3
//    Antarctica/Vostok         VOST-6
//    Arctic/Longyearbyen       CET-1CEST,M3.5.0,M10.5.0/3
//    Argentina/Buenos_Aires    ART3ARST,M10.1.0/0,M3.3.0/0
//    Asia/Almaty               ALMT-6
//    Asia/Amman                EET-2EEST,M3.5.4/0,M10.5.5/1
//    Asia/Anadyr               ANAT-12ANAST,M3.5.0,M10.5.0/3
//    Asia/Aqtau, Aqtobe        AQTT-5
//    Asia/Ashgabat             TMT-5
//    Asia/Ashkhabad            TMT-5
//    Asia/Baku                 AZT-4AZST,M3.5.0/4,M10.5.0/5
//    Asia/Bangkok              ICT-7
//    Asia/Bishkek              KGT-6
//    Asia/Brunei               BNT-8
//    Asia/Calcutta             IST-5:30
//    Asia/Choibalsan           CHOT-9
//    Asia/Chongqing            CST-8
//    Asia/Colombo              IST-5:30
//    Asia/Dacca                BDT-6
//    Asia/Damascus             EET-2EEST,M4.1.5/0,J274/0
//    Asia/Dili                 TLT-9
//    Asia/Dubai                GST-4
//    Asia/Dushanbe             TJT-5
//    Asia/Gaza                 EET-2EEST,J91/0,M9.2.4
//    Asia/Ho_Chi_Minh          ICT-7
//    Asia/Hong_Kong            HKT-8
//    Asia/Hovd                 HOVT-7
//    Asia/Irkutsk              IRKT-8IRKST,M3.5.0,M10.5.0/3
//    Asia/Jakarta, Pontianak   WIT-7
//    Asia/Jayapura             EIT-9
//    Asia/Jerusalem            IDDT
//    Asia/Kabul                AFT-4:30
//    Asia/Kamchatka            PETT-12PETST,M3.5.0,M10.5.0/3
//    Asia/Karachi              PKT-5
//    Asia/Katmandu             NPT-5:45
//    Asia/Kolkata              IST-5:30
//    Asia/Krasnoyarsk          KRAT-7KRAST,M3.5.0,M10.5.0/3
//    Asia/Kuala_Lumpur         MYT-8
//    Asia/Kuching              MYT-8
//    Asia/Kuwait, Bahrain      AST-3
//    Asia/Magadan              MAGT-11MAGST,M3.5.0,M10.5.0/3
//    Asia/Makassar             CIT-8
//    Asia/Manila               PHT-8
//    Asia/Mideast/Riyadh87     zzz-3:07:04
//    Asia/Muscat               GST-4
//    Asia/Novosibirsk          NOVT-6NOVST,M3.5.0,M10.5.0/3
//    Asia/Omsk                 OMST-6OMSST,M3.5.0,M10.5.0/3
//    Asia/Oral                 ORAT-5
//    Asia/Phnom_Penh           ICT-7
//    Asia/Pyongyang            KST-9
//    Asia/Qyzylorda            QYZT-6
//    Asia/Rangoon              MMT-6:30
//    Asia/Saigon               ICT-7
//    Asia/Sakhalin             SAKT-10SAKST,M3.5.0,M10.5.0/3
//    Asia/Samarkand            UZT-5
//    Asia/Seoul                KST-9
//    Asia/Singapore            SGT-8
//    Asia/Taipei               CST-8
//    Asia/Tashkent             UZT-5
//    Asia/Tbilisi              GET-4
//    Asia/Tehran               IRDT
//    Asia/Tel_Aviv             IDDT
//    Asia/Thimbu               BTT-6
//    Asia/Thimphu              BTT-6
//    Asia/Tokyo                JST-9
//    Asia/Ujung_Pandang        CIT-8
//    Asia/Ulaanbaatar          ULAT-8
//    Asia/Ulan_Bator           ULAT-8
//    Asia/Urumqi               CST-8
//    Asia/Vientiane            ICT-7
//    Asia/Vladivostok          VLAT-10VLAST,M3.5.0,M10.5.0/3
//    Asia/Yekaterinburg        YAKT-9YAKST,M3.5.0,M10.5.0/3
//    Asia/Yerevan              AMT-4AMST,M3.5.0,M10.5.0/3
//    Atlantic/Azores           AZOT1AZOST,M3.5.0/0,M10.5.0/1
//    Atlantic/Canary           WET0WEST,M3.5.0/1,M10.5.0
//    Atlantic/Cape_Verde       CVT1
//    Atlantic/Jan_Mayen        CET-1CEST,M3.5.0,M10.5.0/3
//    Atlantic/South_Georgia    GST2
//    Atlantic/St_Helena        GMT0
//    Atlantic/Stanley          FKT4FKST,M9.1.0,M4.3.0
//    Australia/Adelaide        CST-9:30CST,M10.1.0,M4.1.0/3
//    Australia/Brisbane        EST-10
//    Australia/Darwin          CST-9:30
//    Australia/Eucla           CWST-8:45
//    Australia/LHI             LHST-10:30LHST-11,M10.1.0,M4.1.0
//    Australia/Lindeman        EST-10
//    Australia/Lord_Howe       LHST-10:30LHST-11,M10.1.0,M4.1.0
//    Australia/Melbourne       EST-10EST,M10.1.0,M4.1.0/3
//    Australia/North           CST-9:30
//    Australia/Perth, West     WST-8
//    Australia/Queensland      EST-10
//    Brazil/Acre               ACT5
//    Brazil/DeNoronha          FNT2
//    Brazil/East               BRT3BRST,M10.2.0/0,M2.3.0/0
//    Brazil/West               AMT4
//    Canada/Central            CST6CDT,M3.2.0,M11.1.0
//    Canada/Eastern            EST5EDT,M3.2.0,M11.1.0
//    Canada/Newfoundland       NST3:30NDT,M3.2.0/0:01,M11.1.0/0:01
//    Canada/Pacific            PST8PDT,M3.2.0,M11.1.0
//    Chile/Continental         CLST
//    Chile/EasterIsland        EASST
//    Europe/Berlin             CET-1CEST,M3.5.0,M10.5.0/3
//    Europe/Athens             EET-2EEST,M3.5.0/3,M10.5.0/4
//    Europe/Belfast            GMT0BST,M3.5.0/1,M10.5.0
//    Europe/Kaliningrad        EET-2EEST,M3.5.0,M10.5.0/3
//    Europe/Lisbon             WET0WEST,M3.5.0/1,M10.5.0
//    Europe/London             GMT0BST,M3.5.0/1,M10.5.0
//    Europe/Minsk              EET-2EEST,M3.5.0,M10.5.0/3
//    Europe/Moscow             MSK-3MSD,M3.5.0,M10.5.0/3
//    Europe/Samara             SAMT-4SAMST,M3.5.0,M10.5.0/3
//    Europe/Volgograd          VOLT-3VOLST,M3.5.0,M10.5.0/3
//    Indian/Chagos             IOT-6
//    Indian/Christmas          CXT-7
//    Indian/Cocos              CCT-6:30
//    Indian/Kerguelen          TFT-5
//    Indian/Mahe               SCT-4
//    Indian/Maldives           MVT-5
//    Indian/Mauritius          MUT-4
//    Indian/Reunion            RET-4
//    Mexico/General            CST6CDT,M4.1.0,M10.5.0
//    Pacific/Apia              WST11
//    Pacific/Auckland          NZST-12NZDT,M9.5.0,M4.1.0/3
//    Pacific/Chatham           CHAST-12:45CHADT,M9.5.0/2:45,M4.1.0/3:45
//    Pacific/Easter            EASST
//    Pacific/Efate             VUT-11
//    Pacific/Enderbury         PHOT-13
//    Pacific/Fakaofo           TKT10
//    Pacific/Fiji              FJT-12
//    Pacific/Funafuti          TVT-12
//    Pacific/Galapagos         GALT6
//    Pacific/Gambier           GAMT9
//    Pacific/Guadalcanal       SBT-11
//    Pacific/Guam              ChST-10
//    Pacific/Honolulu          HST10
//    Pacific/Johnston          HST10
//    Pacific/Kiritimati        LINT-14
//    Pacific/Kosrae            KOST-11
//    Pacific/Kwajalein         MHT-12
//    Pacific/Majuro            MHT-12
//    Pacific/Marquesas         MART9:30
//    Pacific/Midway            SST11
//    Pacific/Nauru             NRT-12
//    Pacific/Niue              NUT11
//    Pacific/Norfolk           NFT-11:30
//    Pacific/Noumea            NCT-11
//    Pacific/Pago_Pago         SST11
//    Pacific/Palau             PWT-9
//    Pacific/Pitcairn          PST8
//    Pacific/Ponape            PONT-11
//    Pacific/Port_Moresby      PGT-10
//    Pacific/Rarotonga         CKT10
//    Pacific/Saipan            ChST-10
//    Pacific/Samoa             SST11
//    Pacific/Tahiti            TAHT10
//    Pacific/Tarawa            GILT-12
//    Pacific/Tongatapu         TOT-13
//    Pacific/Truk              TRUT-10
//    Pacific/Wake              WAKT-12
//    Pacific/Wallis            WFT-12
//    Pacific/Yap               TRUT-10
//    SystemV/HST10             HST10
//    SystemV/MST7              MST7
//    SystemV/PST8              PST8
//    SystemV/YST9              GAMT9
//    US/Aleutian               HAST10HADT,M3.2.0,M11.1.0
//    US/Arizona                MST7
//    US/Eastern                EST5EDT,M3.2.0,M11.1.0
//    US/East-Indiana           EST5EDT,M3.2.0,M11.1.0
//    US/Hawaii                 HST10
//    US/Michigan               EST5EDT,M3.2.0,M11.1.0
//    US/Samoa                  SST11






