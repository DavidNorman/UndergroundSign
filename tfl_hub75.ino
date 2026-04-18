/*
  Code designed for a LOLIN S2 Mini, aka WEMOS ESP32S2 board.
*/

#include <cJSON.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>

#include <esp_pm.h>

#include <vector>
#include <utility>

/* GPIO pins in ESP32 format */
#define PIN_BUTTON GPIO_NUM_0
#define PIN_R1     GPIO_NUM_1
#define PIN_G1     GPIO_NUM_3
#define PIN_B1     GPIO_NUM_2
#define PIN_R2     GPIO_NUM_4
#define PIN_G2     GPIO_NUM_5
#define PIN_B2     GPIO_NUM_6
#define PIN_A      GPIO_NUM_8
#define PIN_B      GPIO_NUM_7
#define PIN_C      GPIO_NUM_10
#define PIN_D      GPIO_NUM_9
#define PIN_CLK    GPIO_NUM_13
#define PIN_LAT    GPIO_NUM_11
#define PIN_OE     GPIO_NUM_14

/* Maximum number of lines on the screen.  64 lines high at 8 pixels per character */
#define MAX_LINE_COUNT 4

/* Maximum number of pages to display - the index is a single digit, so there can
   be no more than 3 pages of 3 trains each */
#define MAX_PAGES 3

/* Trains per page - there are three, the fourth line being the time */
#define TRAINS_PER_PAGE 3

/* Screen size.  Two panels of 64x32 */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32

#define ONE_NOP {__asm__ __volatile__("nop");}

/* Arduino classes for various functions */
Preferences prefs;
HTTPClient client;
WiFiClientSecure net;

/* Handles for the ESP32 clock task */
esp_timer_handle_t screen_refresh_timer;
esp_timer_handle_t clock_refresh_timer;

/* A screen buffer.  Pixels are either white or black so one bit per pixel */
static unsigned char screen[SCREEN_HEIGHT/8][SCREEN_WIDTH];

/* To hold the sorted list of trains and their destinations and arrival times */
struct Train {
  String towards;
  uint32_t minsToArrival;
  bool westward;
};
std::vector<Train> trains;

static unsigned int epoc_secs = 0;
static unsigned long ref_secs = 0;

/* The website certificate for the root of the TfL website chain,
   and the Time website chain */
static const char root_ca[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIICCTCCAY6gAwIBAgINAgPlwGjvYxqccpBQUjAKBggqhkjOPQQDAzBHMQswCQYD
VQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEUMBIG
A1UEAxMLR1RTIFJvb3QgUjQwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAwMDAw
WjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2Vz
IExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjQwdjAQBgcqhkjOPQIBBgUrgQQAIgNi
AATzdHOnaItgrkO4NcWBMHtLSZ37wWHO5t5GvWvVYRg1rkDdc/eJkTBa6zzuhXyi
QHY7qca4R9gq55KRanPpsXI5nymfopjTX15YhmUPoYRlBtHci8nHc8iMai/lxKvR
HYqjQjBAMA4GA1UdDwEB/wQEAwIBhjAPBgNVHRMBAf8EBTADAQH/MB0GA1UdDgQW
BBSATNbrdP9JNqPV2Py1PsVq8JQdjDAKBggqhkjOPQQDAwNpADBmAjEA6ED/g94D
9J+uHXqnLrmvT/aDHQ4thQEd0dlq7A/Cr8deVl5c1RxYIigL9zC2L7F8AjEA8GE8
p/ 
SgguMh1YQdc4acLa/KNJvxn7kjNuK8YAOdgLOaVsjh4rsUecrNIdSUtUlD
-----END CERTIFICATE-----
)EOF";

/* Font table
   - derived from https://github.com/petabyt/font
   - modified to non-monospaced, and made a little more compact
   - each glyph is 5 bytes, 0x80 is 'none' for narrower characters.
*/
static unsigned char font[] PROGMEM =  {
/*   */
0x0, 0x0, 0x0, 0x0, 0x80, 
/* A */
0x7e, 0x11, 0x11, 0x11, 0x7e, 
/* B */
0x7f, 0x49, 0x49, 0x49, 0x36, 
/* C */
0x3e, 0x41, 0x41, 0x41, 0x41, 
/* D */
0x7f, 0x41, 0x41, 0x41, 0x3e, 
/* E */
0x7f, 0x49, 0x49, 0x49, 0x49, 
/* F */
0x7f, 0x9, 0x9, 0x9, 0x9, 
/* G */
0x3e, 0x41, 0x41, 0x49, 0x79, 
/* H */
0x7f, 0x8, 0x8, 0x8, 0x7f, 
/* I */
0x7f, 0x80, 0x80, 0x80, 0x80, 
/* J */
0x40, 0x40, 0x40, 0x40, 0x3f, 
/* K */
0x7f, 0x8, 0x14, 0x22, 0x41, 
/* L */
0x7f, 0x40, 0x40, 0x40, 0x40, 
/* M */
0x7f, 0x2, 0x4, 0x2, 0x7f, 
/* N */
0x7f, 0x2, 0x4, 0x8, 0x7f, 
/* O */
0x3e, 0x41, 0x41, 0x41, 0x3e, 
/* P */
0x7f, 0x9, 0x9, 0x9, 0x6, 
/* Q */
0x3e, 0x41, 0x41, 0x61, 0x7e, 
/* R */
0x7f, 0x11, 0x11, 0x11, 0x6e, 
/* S */
0x46, 0x49, 0x49, 0x49, 0x31, 
/* T */
0x1, 0x1, 0x7f, 0x1, 0x1, 
/* U */
0x3f, 0x40, 0x40, 0x40, 0x3f, 
/* V */
0x1f, 0x20, 0x40, 0x20, 0x1f, 
/* W */
0x7f, 0x20, 0x10, 0x20, 0x7f, 
/* X */
0x63, 0x14, 0x8, 0x14, 0x63, 
/* Y */
0x7, 0x8, 0x78, 0x8, 0x7, 
/* Z */
0x61, 0x51, 0x49, 0x45, 0x43, 
/* a */
0x20, 0x54, 0x54, 0x78, 0x80,
/* b */
0x7f, 0x44, 0x44, 0x38, 0x80,
/* c */
0x38, 0x44, 0x44, 0x44, 0x28, 
/* d */
0x38, 0x44, 0x44, 0x7f, 0x80,
/* e */
0x38, 0x54, 0x54, 0x58, 0x80,
/* f */
0x8, 0x7e, 0x9, 0x1, 0x2, 
/* g */
0x4c, 0x52, 0x52, 0x3e, 0x80,
/* h */
0x7f, 0x4, 0x4, 0x78, 0x80,
/* i */
0x7d, 0x80, 0x80, 0x80, 0x80, 
/* j */
0x20, 0x40, 0x40, 0x44, 0x3d, 
/* k */
0x7f, 0x10, 0x28, 0x44, 0x80,
/* l */
0x7f, 0x40, 0x80, 0x80, 0x80, 
/* m */
0x7c, 0x4, 0x7c, 0x4, 0x78, 
/* n */
0x7c, 0x4, 0x4, 0x78, 0x80,
/* o */
0x38, 0x44, 0x44, 0x38, 0x80,
/* p */
0x7e, 0x12, 0x12, 0xc, 0x80,
/* q */
0x8, 0x14, 0x14, 0x7c, 0x80,
/* r */
0x7c, 0x8, 0x4, 0x4, 0x80, 
/* s */
0x48, 0x54, 0x54, 0x24, 0x80,
/* t */
0x4, 0x3f, 0x44, 0x80, 0x80, 
/* u */
0x3c, 0x40, 0x40, 0x40, 0x3c, 
/* v */
0x1c, 0x20, 0x40, 0x20, 0x1c, 
/* w */
0x3c, 0x40, 0x30, 0x40, 0x3c, 
/* x */
0x44, 0x28, 0x10, 0x28, 0x44, 
/* y */
0x4c, 0x50, 0x50, 0x50, 0x3c, 
/* z */
0x44, 0x64, 0x54, 0x4c, 0x44, 
/* 0 */
0x3e, 0x51, 0x49, 0x45, 0x3e, 
/* 1 */
0x0, 0x41, 0x7f, 0x40, 0x80,
/* 2 */
0x62, 0x51, 0x49, 0x49, 0x46, 
/* 3 */
0x41, 0x49, 0x49, 0x49, 0x36, 
/* 4 */
0xf, 0x8, 0x8, 0x8, 0x7f, 
/* 5 */
0x4f, 0x49, 0x49, 0x49, 0x31, 
/* 6 */
0x36, 0x49, 0x49, 0x49, 0x31, 
/* 7 */
0x1, 0x71, 0x9, 0x9, 0x7, 
/* 8 */
0x36, 0x49, 0x49, 0x49, 0x36, 
/* 9 */
0x6, 0x49, 0x49, 0x49, 0x3e, 
};

/* Lookup table from ASCII to font - this doesn't do UTF8 or extended 
   character codes.  Have to hope that TfL don't start putting those codes
   into their JSON
*/
static char ascii[] PROGMEM = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0-15 */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 16-31 */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 32-47 */
  53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 0, 0, 0, 0, 0, 0, /* 48-63 */
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, /* 64-79 */
  16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 0, 0, 0, 0, 0, /* 80-95 */
  0, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, /* 96-111 */
  42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 0, 0, 0, 0, 0, /* 112-127 */
};

/* Train sprites - sprites are arbitrary length terminated by 0x80 */
static char west_train_sprite[] PROGMEM =  {
  0x1e, 0x35, 0x55, 0x35, 0x13, 0x11, 0x31, 0x51, 0x31, 0x11, 0x80
};
static char east_train_sprite[] PROGMEM =  {
  0x11, 0x31, 0x51, 0x31, 0x11, 0x13, 0x35, 0x55, 0x35, 0x1e, 0x80
};

/* FUNCTIONS */

/* Render the screen out to the HUB75 interface */
void IRAM_ATTR draw_screen(void*) {
  static int a=0;

  noInterrupts();

  gpio_set_level(PIN_OE, 0);
  ONE_NOP;

  gpio_set_level(PIN_A, (a >> 0) & 0x1);
  gpio_set_level(PIN_B, (a >> 1) & 0x1);
  gpio_set_level(PIN_C, (a >> 2) & 0x1);
  gpio_set_level(PIN_D, (a >> 3) & 0x1);

  unsigned char mask = (0x01 << (a & 0x7));
  unsigned char *row_1 = &screen[(a / 8) + 0][0];
  unsigned char *row_2 = &screen[(a / 8) + 2][0];

  for (uint32_t i=0; i<SCREEN_WIDTH; i++) {
    uint32_t pix_1 = (*row_1++ & mask) ? 1 : 0;
    uint32_t pix_2 = (*row_2++ & mask) ? 1 : 0;

    gpio_set_level(PIN_R1, pix_1);
    gpio_set_level(PIN_B1, pix_1);
    gpio_set_level(PIN_G1, pix_1);
    gpio_set_level(PIN_R2, pix_2);
    gpio_set_level(PIN_B2, pix_2);
    gpio_set_level(PIN_G2, pix_2);
    gpio_set_level(PIN_CLK, HIGH);
    gpio_set_level(PIN_CLK, LOW);
    gpio_set_level(PIN_R1, LOW);
  }

  gpio_set_level(PIN_LAT, HIGH);
  gpio_set_level(PIN_LAT, LOW);
  ONE_NOP;
  gpio_set_level(PIN_OE, 1);

  if (++a >= SCREEN_HEIGHT/2) a = 0;

  interrupts();
}

/* Clear a range of one line */
void IRAM_ATTR clear_screen_line(int l, int s, int w) {
  void* line = &screen[l][s];
  memset(line, 0, w);
}

/* Zero out the screen pixels */
void IRAM_ATTR clear_screen() {
  memset(&screen[0][0], 0, SCREEN_WIDTH * SCREEN_HEIGHT / 8);
}

/* Draw some text onto the screen buffer */
void IRAM_ATTR print_string(uint32_t row, uint32_t col, const char* str, size_t len=-1) {
  while (*str != 0 && len) {
    if (static_cast<unsigned char>(*str) < 128) {
      unsigned char *c = &font[ascii[*str] * 5];

      for (uint32_t i=0; i<5; i++) {
        if (col >= SCREEN_WIDTH) return;
        if (*c == 0x80) break;
        screen[row][col] = *c++;
        col++;
      }

      col++;
      str++;
      len--;
    }
  }
}

/* Draw a sprite onto the screen buffer */
void print_sprite(uint32_t row, uint32_t col, const char* sprite) {
  while (1) {
    if (*sprite == 0x80) break;
    screen[row][col] = *sprite++;
    if (++col >= SCREEN_WIDTH) break;
  }
}

/* Print only the time part of the screen */
void IRAM_ATTR print_clock(void*) {
  if (ref_secs > 0) {
    unsigned long time_since_ref = millis() / 1000 - ref_secs;
    unsigned long actual_secs = (epoc_secs + time_since_ref) % (60*60*24);

    unsigned long secs = actual_secs % 60;
    unsigned long mins = (actual_secs / 60) % 60;
    unsigned long hours = (actual_secs / 3600) % 24;

    clear_screen_line(3, 44, 42);

    char buf[3];
    buf[0] = (hours / 10) + '0';
    buf[1] = (hours % 10) + '0';
    print_string(3, 44, buf, 2);

    buf[0] = (mins / 10) + '0';
    buf[1] = (mins % 10) + '0';
    print_string(3, 58, buf, 2);

    buf[0] = (secs / 10) + '0';
    buf[1] = (secs % 10) + '0';
    print_string(3, 72, buf, 2);
  }
}

/* Print the train list onto the screen buffer */
void print_train_info(uint32_t page) {
  clear_screen();

  print_clock(NULL);

  if (trains.size() == 0) {
    print_string(1, 43, "No trains");
    return;
  }

  uint32_t page_count = (trains.size() + (MAX_PAGES - 1)) / MAX_PAGES;
  if (page_count > MAX_PAGES) page_count = MAX_PAGES;
  page = page % page_count;
  uint32_t offset = page * TRAINS_PER_PAGE;

  char arrival[5];
  char pos[3];
  for (size_t i=0; i<TRAINS_PER_PAGE; i++) {
    size_t t = i + offset;
    if (t < trains.size()) {
      itoa(t+1, pos, 10);
      sprintf(arrival, "%2dm", trains[t].minsToArrival);

      print_string(i, 0, pos, 1);
      print_sprite(i, 7, trains[t].westward ? west_train_sprite : east_train_sprite);
      print_string(i, 20, trains[t].towards.c_str(), 15);
      print_string(i, 110, arrival, 3);
    }
  }
}

/* Convert a 2 digit string to integer - without needing null terminator */
unsigned char two_digit_convert(const char* digits) {
  return (digits[0]-'0') * 10 + (digits[1]-'0');
}


/* Fetch the current time */
void get_time_info() {
  if (WiFi.status() == WL_CONNECTED) {
    client.begin(net, "https://time.now/developer/api/timezone/Europe/London");

    uint32_t httpCode = client.GET();
    if (httpCode == 200) {
      const char* json_string = client.getString().c_str();
      cJSON *json = cJSON_Parse(json_string);
      if (json) {
        cJSON *datetime = cJSON_GetObjectItemCaseSensitive(json, "datetime");
        if (datetime && cJSON_IsString(datetime)) {
          const char* str = datetime->valuestring;
          ref_secs = millis() / 1000;
          unsigned char hour = two_digit_convert(&str[11]);
          unsigned char min = two_digit_convert(&str[14]);
          unsigned char sec = two_digit_convert(&str[17]);
          epoc_secs = ((hour * 60) + min) * 60 + sec;
        }

        cJSON_Delete(json);
      }
    }
    client.end();
  }
}

/* Fetch the Plaistow train info - hardcoded the naptan which is 940GZZLUPLW */
void get_train_arrival_info() {
  if (WiFi.status() == WL_CONNECTED) {

    client.begin(net, "https://api.tfl.gov.uk/StopPoint/940GZZLUPLW/Arrivals");

    uint32_t httpCode = client.GET();
    if (httpCode == 200) {
      trains.clear();
      
      cJSON *json = cJSON_Parse(client.getString().c_str());

      if (json) {
        bool parse_error = false;
        uint32_t trainCount = cJSON_GetArraySize(json);
        trains.clear();

        for(uint32_t i=0; i<trainCount; i++) {
          cJSON *train = cJSON_GetArrayItem(json, i);

          cJSON *towards = cJSON_GetObjectItemCaseSensitive(train, "towards");
          cJSON *eta = cJSON_GetObjectItemCaseSensitive(train, "timeToStation");
          cJSON *platformName = cJSON_GetObjectItemCaseSensitive(train, "platformName");

          if (towards && cJSON_IsString(towards) &&
              eta && cJSON_IsNumber(eta) &&
              platformName && cJSON_IsString(platformName)) {
            uint32_t minsToArrival = eta->valueint /60;
            if (minsToArrival < 20 && strncmp(towards->valuestring, "Check", 5) != 0) {
              bool westward = (platformName->valuestring[0] == 'W');
              trains.push_back({towards->valuestring, minsToArrival, westward});
            }
          } else {
            parse_error = true;
          }
        }
        cJSON_Delete(json);

        if (parse_error) {
          trains.clear();
        }
      }

      client.end();
    }

    std::sort(trains.begin(), trains.end(),
      [](const Train&a, const Train& b) {
        return a.minsToArrival < b.minsToArrival;
      });

  }
}

/* Store off the WiFi details to flash and restart the board */
void write_wifi_details(const String& ssid, const String& pass) {
      prefs.begin("wifi", false);
      prefs.putString("ssid", ssid);
      prefs.putString("pass", pass);
      prefs.end();

      delay(250);
      ESP.restart();
}

/* If we are in 'getting the WiFi details mode, read the details from serial port and save to flash */
void handle_wifi_connection_details() {
  static String line;

  if (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      line.trim();
      if (line.length()) {

        // Parse into two tokens: SSID + PASSWORD
        uint32_t space = line.indexOf(' ');
        if (space < 1) {
          line = "";
          return;
        }

        write_wifi_details(line.substring(0, space), line.substring(space + 1));
      }
    } else {
      line += c;
    }
  }
}

/* Setup board, GPIOS, WiFi, etc*/
void setup() {
  delay(500);

  /* prevent frequency changes from interfering with screen drawing */
  esp_pm_config_esp32_t pm_config = {
      .max_freq_mhz = 240,
      .min_freq_mhz = 240,
      .light_sleep_enable = false
  };
  esp_pm_configure(&pm_config);

  /* Configure GPIO */
  gpio_config_t io_conf_op = {
    .pin_bit_mask = 1ULL << PIN_R1 | 1ULL << PIN_G1 | 1ULL << PIN_B1 | 1ULL << PIN_R2 | 1ULL << PIN_G2 |
                    1ULL << PIN_B2 | 1ULL << PIN_A | 1ULL << PIN_B | 1ULL << PIN_C | 1ULL << PIN_D |
                    1ULL << PIN_CLK | 1ULL << PIN_LAT | 1ULL << PIN_OE,
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE
  };
  gpio_config(&io_conf_op);

  gpio_config_t io_conf_ip = {
    .pin_bit_mask = 1ULL << PIN_BUTTON,
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE
  };
  gpio_config(&io_conf_ip);

  /* Serial is used for getting wifi credentials */
  Serial.begin(115200);

  /* Setup screen drawing timer */
  const esp_timer_create_args_t screen_timer_args = {
    .callback = draw_screen,
    .arg = NULL,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "draw"
  };
  esp_timer_create(&screen_timer_args, &screen_refresh_timer);
  esp_timer_start_periodic(screen_refresh_timer, 600);

  /* Setup clock update timer */
  const esp_timer_create_args_t clock_timer_args = {
    .callback = print_clock,
    .arg = NULL,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "clock"
  };
  esp_timer_create(&clock_timer_args, &clock_refresh_timer);
  esp_timer_start_periodic(clock_refresh_timer, 1000000);

  /* WiFi init, using a certificate from the TfL site */
  WiFi.mode(WIFI_MODE_STA);
  net.setCACert(root_ca);

  /* Use HTTP 1.0 because we cannot handle chunked transfer encoding */
  client.useHTTP10(true);

  /* Fetch wifi creds and try to connect */
  prefs.begin("wifi", true);
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  prefs.end();

  if (ssid.length() > 0) {
    WiFi.begin(ssid.c_str(), pass.c_str());

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < 15000) {
      delay(50);
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    clear_screen();
    print_string(1, 17, "Enter wifi details");
  }
}

void loop() {
  static unsigned long start_of_period = millis();
  static unsigned long current_period = -1;
  static bool need_train_update = true;

  static unsigned long next_clock_update = 0;

  /* Can force a drop to getting WiFi credentials by pressing button 0 */
  if (gpio_get_level(PIN_BUTTON) == 0) {
    write_wifi_details("", "");
  }

  if (WiFi.status() == WL_CONNECTED) {
    unsigned long now = millis();
    unsigned long period = (now - start_of_period) / 10000;
    bool update_screen = false;

    if (next_clock_update < now) {
      get_time_info();
      next_clock_update = now + 3600000;
    }

    if (current_period != period) {
      if (period >= 6) need_train_update = true;

      if (need_train_update) {
        get_train_arrival_info();

        start_of_period = now;
        period = 0;
        need_train_update = false;
      }
      current_period = period;
      print_train_info(current_period);
    }

  } else {
    handle_wifi_connection_details();
  }

  /* Yeild just in case it makes a difference */
  delay(0);
}

