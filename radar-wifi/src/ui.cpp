#include "ui.h"

#include <U8g2lib.h>

#include <cstdio>
#include <cstring>

#include "logo.h"

using namespace radar;

namespace {

// Heltec WiFi Kit 32: SSD1306 on SCL 15, SDA 4, reset 16.
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, /*reset=*/16, /*clock=*/15,
                                         /*data=*/4);

const uint8_t* const kSmall = u8g2_font_6x10_tf;
const uint8_t* const kTiny = u8g2_font_5x7_tf;
const uint8_t* const kBig = u8g2_font_helvB14_tr;

constexpr int kHoldY = 62;  // 2 px press-progress bar on the bottom rows
constexpr uint32_t kPreviewAfterMs = 150;  // quick taps don't flash the panel

constexpr int kRssiMin = -95;  // empty bar
constexpr int kRssiMax = -30;  // full bar

// Copies at most `maxChars` UTF-8 characters, never splitting one.
void fitName(const char* ssid, char* out, size_t outSize, int maxChars) {
  if (ssid[0] == '\0') {
    std::snprintf(out, outSize, "(oculta)");
    return;
  }
  size_t i = 0;
  int chars = 0;
  while (ssid[i] && i + 1 < outSize) {
    if ((ssid[i] & 0xC0) != 0x80) {  // start of a character
      if (chars == maxChars) break;
      chars++;
    }
    out[i] = ssid[i];
    i++;
  }
  while (i > 0 && (out[i - 1] & 0xC0) == 0xC0) i--;  // drop a cut lead byte
  out[i] = '\0';
}

int level(float rssi, int steps) {
  const float t = (rssi - kRssiMin) / float(kRssiMax - kRssiMin);
  const int l = int(t * steps + 0.5f);
  return l < 0 ? 0 : (l > steps ? steps : l);
}

void signalBars(int x, int y, int rssi) {
  const int l = level(rssi, 4);
  for (int b = 0; b < 4; b++) {
    const int h = 2 + b * 2;
    if (b < l) oled.drawBox(x + b * 3, y - h, 2, h);
    else oled.drawPixel(x + b * 3, y - 1);
  }
}

void formatMeters(float m, char* out, size_t size) {
  if (m < 10.0f) std::snprintf(out, size, "%.1f", m);
  else std::snprintf(out, size, "%.0f", m);
}

void drawList(const Nav& nav, const NetList& nets, bool scanning, uint32_t now) {
  char line[24];
  oled.setFont(kSmall);
  std::snprintf(line, sizeof(line), "Hackwise Scanner (%d)", nets.size());
  oled.drawStr(0, 8, line);
  oled.drawHLine(0, 10, 128);
  if (scanning) {  // a gap running along the underline: the title fills the row
    oled.setDrawColor(0);
    oled.drawHLine(int(now / 8 % 140) - 12, 10, 12);
    oled.setDrawColor(1);
  }

  if (nets.size() == 0) {
    oled.drawUTF8(0, 32, scanning ? "Escaneando..." : "Sin redes");
    return;
  }
  for (int row = 0; row < Nav::kRows; row++) {
    const int i = nav.top + row;
    if (i >= nets.size()) break;
    const Network& n = nets.at(i);
    const int y = 20 + row * 10;
    const bool selected = i == nav.cursor;
    if (selected) {
      oled.drawBox(0, y - 8, 128, 10);
      oled.setDrawColor(0);
    }
    char name[40];
    fitName(n.ssid, name, sizeof(name), 12);
    oled.drawUTF8(1, y, name);
    signalBars(76, y, n.rssi);
    std::snprintf(line, sizeof(line), "%4d", n.rssi);
    oled.drawStr(89, y, line);
    std::snprintf(line, sizeof(line), "%2u", n.channel);
    oled.drawStr(115, y, line);
    oled.setDrawColor(1);
  }
}

void drawTrend(int y, Trend t) {
  const int cx = 5;
  switch (t) {
    case Trend::Approaching:
      oled.drawTriangle(cx - 5, y, cx + 5, y, cx, y - 8);
      oled.drawStr(14, y, "ACERCANDO");
      break;
    case Trend::Receding:
      oled.drawTriangle(cx - 5, y - 8, cx + 5, y - 8, cx, y);
      oled.drawStr(14, y, "ALEJANDO");
      break;
    case Trend::Stable:
      oled.drawBox(cx - 5, y - 5, 11, 3);
      oled.drawStr(14, y, "ESTABLE");
      break;
  }
}

void drawTrack(const TrackView& v) {
  char line[40];
  char name[40];
  oled.setFont(kSmall);
  fitName(v.target->ssid, name, sizeof(name), 15);
  oled.drawUTF8(0, 8, name);
  std::snprintf(line, sizeof(line), "c%u", v.target->channel);
  oled.drawStr(128 - oled.getStrWidth(line), 8, line);
  oled.drawHLine(0, 10, 128);

  if (v.signal != Signal::Ok) {
    oled.drawUTF8(0, 30, v.signal == Signal::Lost ? "Señal perdida" : "Buscando señal...");
    if (v.signal == Signal::Lost) oled.drawUTF8(0, 42, "Buscando en otros canales");
    return;
  }

  char est[12], lo[12], hi[12];
  formatMeters(v.range.est, est, sizeof(est));
  formatMeters(v.range.min, lo, sizeof(lo));
  formatMeters(v.range.max, hi, sizeof(hi));

  oled.setFont(kBig);
  std::snprintf(line, sizeof(line), "~%s m", est);
  oled.drawStr(0, 27, line);

  oled.setFont(kSmall);
  std::snprintf(line, sizeof(line), "%.0fdBm", v.rssi);
  oled.drawStr(128 - oled.getStrWidth(line), 27, line);
  std::snprintf(line, sizeof(line), "rango %s-%s m", lo, hi);
  oled.drawStr(0, 38, line);

  drawTrend(48, v.trend);
  if (v.clients >= 0) {
    std::snprintf(line, sizeof(line), "%d disp", v.clients);
    oled.drawStr(128 - oled.getStrWidth(line), 48, line);
  }

  // Cold-to-hot bar.
  oled.drawFrame(0, 53, 128, 6);
  oled.drawBox(1, 54, level(v.rssi, 126), 4);
}

void drawMenu(const Nav& nav, bool outdoor, float rssiAt1m) {
  oled.setFont(kSmall);
  oled.drawUTF8(0, 8, "Menú");
  oled.drawHLine(0, 10, 128);

  char items[4][24];
  std::snprintf(items[0], sizeof(items[0]), "Calibrar 1 m (%.0f)", rssiAt1m);
  std::snprintf(items[1], sizeof(items[1]), "Entorno: %s", outdoor ? "Exterior" : "Interior");
  std::snprintf(items[2], sizeof(items[2]), "Deauth (lab)");
  std::snprintf(items[3], sizeof(items[3]), "Volver");
  for (int i = 0; i < 4; i++) {
    const int y = 22 + i * 12;
    if (i == nav.menu) {
      oled.drawBox(0, y - 9, 128, 12);
      oled.setDrawColor(0);
    }
    oled.drawStr(2, y, items[i]);
    oled.setDrawColor(1);
  }
}

void drawDeauth(const DeauthView& v, uint32_t now) {
  char line[40];
  char name[40];
  oled.setFont(kSmall);
  fitName(v.target->ssid, name, sizeof(name), 12);
  oled.drawUTF8(0, 8, name);
  std::snprintf(line, sizeof(line), "c%u", v.target->channel);
  oled.drawStr(128 - oled.getStrWidth(line), 8, line);
  oled.drawHLine(0, 10, 128);

  // The status line blinks while frames are going out.
  const bool blinkOff = v.running && now / 500 % 2 == 0;
  if (!blinkOff) {
    if (v.err) {
      std::snprintf(line, sizeof(line), "%s e=0x%X",
                    v.running ? "ENVIANDO" : "Detenido", v.err);
      oled.drawUTF8(0, 26, line);
    } else {
      oled.drawUTF8(0, 26, v.running ? "ENVIANDO DEAUTH" : "Detenido");
    }
  }

  oled.setFont(kTiny);
  std::snprintf(line, sizeof(line), "%u tramas  %us",
                static_cast<unsigned>(v.sent),
                static_cast<unsigned>(v.elapsedMs / 1000));
  oled.drawUTF8(0, 40, line);
  if (v.clients >= 0) {
    std::snprintf(line, sizeof(line), "disp: %d", v.clients);
    oled.drawUTF8(0, 52, line);
  }
}

// Bar on the bottom rows while the button is held: it fills up until the
// action on the panel changes, then starts again.
void drawHold(uint32_t holdMs) {
  if (holdMs == 0) return;
  const uint32_t step = holdMs < Button::kLongMs ? Button::kLongMs : Button::kStepMs;
  const uint32_t done = step - Button::msToNext(holdMs);
  oled.setDrawColor(0);
  oled.drawBox(0, kHoldY, 128, 2);
  oled.setDrawColor(1);
  oled.drawBox(0, kHoldY, int(128 * done / step), 2);
}

// While the button is held: what releasing now does, and what comes next.
void drawPreview(const Nav& nav, int listSize, Gesture pending, uint32_t holdMs,
                 bool deauthRunning) {
  if (pending == Gesture::None || holdMs < kPreviewAfterMs) return;
  const uint32_t toNext = Button::msToNext(holdMs);
  const Gesture next = Button::gestureAt(holdMs + toNext);

  oled.setDrawColor(0);
  oled.drawBox(0, 19, 128, 35);
  oled.setDrawColor(1);
  oled.drawFrame(0, 19, 128, 35);

  oled.setFont(kTiny);
  oled.drawUTF8(3, 27, "Si sueltas ahora:");
  oled.setFont(kSmall);
  oled.drawUTF8(3, 39, gestureLabel(nav, pending, listSize, deauthRunning));

  char line[48];
  std::snprintf(line, sizeof(line), "+%.1fs: %s", toNext / 1000.0f,
                gestureLabel(nav, next, listSize, deauthRunning));
  oled.setFont(kTiny);
  oled.drawUTF8(3, 50, line);
}

void drawToast(const char* msg) {
  oled.setFont(kSmall);
  const int w = oled.getUTF8Width(msg) + 8;
  const int x = (128 - w) / 2;
  oled.setDrawColor(0);
  oled.drawBox(x, 18, w, 16);
  oled.setDrawColor(1);
  oled.drawFrame(x, 18, w, 16);
  oled.drawUTF8(x + 4, 29, msg);
}

}  // namespace

void uiBegin() {
  oled.setBusClock(400000);
  oled.begin();
}

void uiSplash() {
  oled.clearBuffer();
  oled.drawXBMP((128 - kLogoW) / 2, 8, kLogoW, kLogoH, kLogo);
  oled.setFont(kSmall);
  const char* title = "Hackwise Scanner";
  oled.drawStr((128 - oled.getStrWidth(title)) / 2, 54, title);
  oled.sendBuffer();
}

void uiDraw(const Nav& nav, const NetList& nets, bool scanning,
            const TrackView& track, const DeauthView& deauth, bool outdoor,
            float rssiAt1m, uint32_t holdMs, Gesture pending, const char* toast,
            uint32_t now) {
  oled.clearBuffer();
  switch (nav.screen) {
    case Screen::List: drawList(nav, nets, scanning, now); break;
    case Screen::Track: drawTrack(track); break;
    case Screen::Menu: drawMenu(nav, outdoor, rssiAt1m); break;
    case Screen::Deauth: drawDeauth(deauth, now); break;
  }
  drawPreview(nav, nets.size(), pending, holdMs, deauth.running);
  drawHold(holdMs);
  if (toast) drawToast(toast);
  oled.sendBuffer();
}
