#ifndef INPUT_CODES_H
#define INPUT_CODES_H

#include <InputEvents.h>   // for Key, Scancode, Keymod typedefs

namespace SE {

// Key codes (SDL_Keycode-compatible: printable = Unicode, specials = (1<<30)|scancode)
namespace Keys {
    // Printable letters — values are their ASCII/Unicode codepoints
    constexpr Key A = 'a', B = 'b', C = 'c', D = 'd', E = 'e', F = 'f',
                  G = 'g', H = 'h', I = 'i', J = 'j', K = 'k', L = 'l',
                  M = 'm', N = 'n', O = 'o', P = 'p', Q = 'q', R = 'r',
                  S = 's', T = 't', U = 'u', V = 'v', W = 'w', X = 'x',
                  Y = 'y', Z = 'z';

    // Keys with traditional ASCII/control-character values in SDL2
    constexpr Key RETURN    = '\r';    // 13
    constexpr Key ESCAPE    = '\033';  // 27
    constexpr Key BACKSPACE = '\b';    // 8
    constexpr Key TAB       = '\t';    // 9
    constexpr Key SPACE     = ' ';     // 32

    // Keys with no traditional value — SDL2 uses (1<<30) | SDL_Scancode
    constexpr Key DELETE_   = (1<<30) | 76;
    constexpr Key RIGHT     = (1<<30) | 79;
    constexpr Key LEFT      = (1<<30) | 80;
    constexpr Key DOWN      = (1<<30) | 81;
    constexpr Key UP        = (1<<30) | 82;
    constexpr Key INSERT    = (1<<30) | 73;
    constexpr Key HOME      = (1<<30) | 74;
    constexpr Key END       = (1<<30) | 77;
    constexpr Key PAGE_UP   = (1<<30) | 75;
    constexpr Key PAGE_DOWN = (1<<30) | 78;

    // Function keys
    constexpr Key F1  = (1<<30)|58;  constexpr Key F2  = (1<<30)|59;
    constexpr Key F3  = (1<<30)|60;  constexpr Key F4  = (1<<30)|61;
    constexpr Key F5  = (1<<30)|62;  constexpr Key F6  = (1<<30)|63;
    constexpr Key F7  = (1<<30)|64;  constexpr Key F8  = (1<<30)|65;
    constexpr Key F9  = (1<<30)|66;  constexpr Key F10 = (1<<30)|67;
    constexpr Key F11 = (1<<30)|68;  constexpr Key F12 = (1<<30)|69;
} // namespace Keys

// Scan codes (USB HID table, SDL_Scancode-compatible)
namespace Scancodes {
    constexpr Scancode A          = 4;
    constexpr Scancode B          = 5;
    constexpr Scancode C          = 6;
    constexpr Scancode D          = 7;
    constexpr Scancode E          = 8;
    constexpr Scancode F          = 9;
    constexpr Scancode G          = 10;
    constexpr Scancode H          = 11;
    constexpr Scancode I          = 12;
    constexpr Scancode J          = 13;
    constexpr Scancode K          = 14;
    constexpr Scancode L          = 15;
    constexpr Scancode M          = 16;
    constexpr Scancode N          = 17;
    constexpr Scancode O          = 18;
    constexpr Scancode P          = 19;
    constexpr Scancode Q          = 20;
    constexpr Scancode R          = 21;
    constexpr Scancode S          = 22;
    constexpr Scancode T          = 23;
    constexpr Scancode U          = 24;
    constexpr Scancode V          = 25;
    constexpr Scancode W          = 26;
    constexpr Scancode X          = 27;
    constexpr Scancode Y          = 28;
    constexpr Scancode Z          = 29;
    constexpr Scancode RETURN     = 40;
    constexpr Scancode ESCAPE     = 41;
    constexpr Scancode BACKSPACE  = 42;
    constexpr Scancode TAB        = 43;
    constexpr Scancode SPACE      = 44;
    constexpr Scancode INSERT     = 73;
    constexpr Scancode HOME       = 74;
    constexpr Scancode PAGE_UP    = 75;
    constexpr Scancode DELETE_    = 76;
    constexpr Scancode END        = 77;
    constexpr Scancode PAGE_DOWN  = 78;
    constexpr Scancode RIGHT      = 79;
    constexpr Scancode LEFT       = 80;
    constexpr Scancode DOWN       = 81;
    constexpr Scancode UP         = 82;

    // Function keys (USB HID / SDL_Scancode values)
    constexpr Scancode F1  = 58;  constexpr Scancode F2  = 59;
    constexpr Scancode F3  = 60;  constexpr Scancode F4  = 61;
    constexpr Scancode F5  = 62;  constexpr Scancode F6  = 63;
    constexpr Scancode F7  = 64;  constexpr Scancode F8  = 65;
    constexpr Scancode F9  = 66;  constexpr Scancode F10 = 67;
    constexpr Scancode F11 = 68;  constexpr Scancode F12 = 69;
} // namespace Scancodes

// Key modifier bitmasks (SDL_Keymod-compatible)
namespace Keymods {
    constexpr Keymod LSHIFT = 0x0001;
    constexpr Keymod RSHIFT = 0x0002;
    constexpr Keymod SHIFT  = 0x0003;
    constexpr Keymod LCTRL  = 0x0040;
    constexpr Keymod RCTRL  = 0x0080;
    constexpr Keymod CTRL   = 0x00C0;
    constexpr Keymod LALT   = 0x0100;
    constexpr Keymod RALT   = 0x0200;
    constexpr Keymod ALT    = 0x0300;
} // namespace Keymods

} // namespace SE

#endif
