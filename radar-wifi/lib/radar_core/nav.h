#pragma once

#include "button.h"

namespace radar {

enum class Screen { List, Track, Menu };
enum class MenuItem { Calibrate, Environment, Back, Count };
enum class Action { None, Rescan, StartTrack, StopTrack, Calibrate, ToggleEnv };

// Screen navigation driven by button gestures. Returns the side effect the
// caller must perform; it never touches hardware itself.
struct Nav {
  static constexpr int kRows = 5;  // list rows that fit on the OLED

  Screen screen = Screen::List;
  int cursor = 0;  // selected row in the list
  int top = 0;     // first visible row in the list
  int menu = 0;

  MenuItem menuItem() const { return static_cast<MenuItem>(menu); }

  void setCursor(int c, int listSize) {
    cursor = listSize <= 0 ? 0 : (c < 0 ? 0 : (c >= listSize ? listSize - 1 : c));
    if (cursor < top) top = cursor;
    if (cursor >= top + kRows) top = cursor - kRows + 1;
    const int maxTop = listSize > kRows ? listSize - kRows : 0;
    if (top > maxTop) top = maxTop;
    if (top < 0) top = 0;
  }

  Action onGesture(Gesture g, int listSize) {
    switch (screen) {
      case Screen::List:
        if (g == Gesture::Short && listSize > 0) {
          setCursor(cursor + 1 >= listSize ? 0 : cursor + 1, listSize);
        } else if (g == Gesture::Long && listSize > 0) {
          screen = Screen::Track;
          return Action::StartTrack;
        } else if (g == Gesture::VeryLong) {
          return Action::Rescan;
        }
        break;

      case Screen::Track:
        if (g == Gesture::Long) {
          screen = Screen::Menu;
          menu = 0;
        } else if (g == Gesture::VeryLong) {
          screen = Screen::List;
          return Action::StopTrack;
        }
        break;

      case Screen::Menu:
        if (g == Gesture::Short) {
          menu = (menu + 1) % static_cast<int>(MenuItem::Count);
        } else if (g == Gesture::VeryLong) {
          screen = Screen::Track;
        } else if (g == Gesture::Long) {
          switch (menuItem()) {
            case MenuItem::Calibrate:
              screen = Screen::Track;
              return Action::Calibrate;
            case MenuItem::Environment:
              return Action::ToggleEnv;
            default:
              screen = Screen::Track;
              break;
          }
        }
        break;
    }
    return Action::None;
  }
};

// Short description of what gesture `g` would do on the current screen.
// Shown while the button is held, so the user knows when to let go.
inline const char* gestureLabel(const Nav& nav, Gesture g, int listSize) {
  if (g == Gesture::None) return "";
  switch (nav.screen) {
    case Screen::List:
      if (g == Gesture::VeryLong) return "Re-escanear";
      if (listSize <= 0) return "Nada";
      return g == Gesture::Short ? "Bajar" : "Rastrear red";
    case Screen::Track:
      if (g == Gesture::Short) return "Nada";
      return g == Gesture::Long ? "Abrir menú" : "Volver a la lista";
    case Screen::Menu:
      if (g == Gesture::Short) return "Siguiente opción";
      if (g == Gesture::VeryLong) return "Volver";
      switch (nav.menuItem()) {
        case MenuItem::Calibrate: return "Calibrar a 1 m";
        case MenuItem::Environment: return "Cambiar entorno";
        default: return "Volver";
      }
  }
  return "";
}

}  // namespace radar
