// keyboard_x11.cpp
/*
 * FRT - A Godot platform targeting single board computers
 * Copyright (c) 2017-2019  Emanuele Fornara
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifdef FRT_TEST
#define FRT_MOCK_GODOT_INPUT_MODIFIER_STATE
#else
#include "core/version.h"
#if VERSION_MAJOR == 3
#define FRT_MOCK_GODOT_INPUT_MODIFIER_STATE
#endif
#include "core/os/input_event.h"
#endif

#include "frt.h"
#include "bits/x11.h"

#include "import/gdkeys.h"

#ifndef FRT_MOCK_KEY_MAPPING_X11
#include "import/key_mapping_x11.h"
#endif

namespace frt {

static const long handled_mask = KeyPressMask | KeyReleaseMask;

static const int handled_types[] = {
	KeyPress,
	KeyRelease,
	0,
};

class KeyboardX11 : public Keyboard, public EventHandler {
private:
	X11User *x11;
	Display *display;
	Handler *h;
	InputModifierState st;

public:
	KeyboardX11()
		: x11(0), h(0) {
		st.shift = false;
		st.alt = false;
		st.control = false;
		st.meta = false;
	}
	// Module
	const char *get_id() const { return "keyboard_x11"; }
	bool probe() {
		if (!x11) {
			x11 = X11Context::acquire(handled_mask, handled_types, this);
			display = x11->get_display();
		}
		return true;
	}
	void cleanup() {
		if (x11) {
			x11->release();
			x11 = 0;
		}
	}
	// Keyboard
	void set_handler(Handler *handler) {
		h = handler;
	}
	void get_modifier_state(InputModifierState &state) const { state = st; }
	// EventHandler
	void handle_event() {
		// modelled after platform/x11/os_x11.cpp, see there for rationale
		XEvent ev;
		x11->get_event(ev);
		st.shift = ev.xkey.state & ShiftMask;
		st.alt = ev.xkey.state & Mod1Mask;
		st.control = ev.xkey.state & ControlMask;
		st.meta = ev.xkey.state & Mod4Mask;
		KeySym keysym_keycode = 0;
		char str[256 + 1];
		XLookupString(&ev.xkey, str, 256, &keysym_keycode, 0);
		uint32_t unicode = 0;
#ifndef FRT_MOCK_KEY_MAPPING_X11
		int keycode = KeyMappingX11::get_keycode(keysym_keycode);
		// just the simple case: on my system, using keysym_keycode is fine
		unicode = KeyMappingX11::get_unicode_from_keysym(keysym_keycode);
#else
		int keycode = str[0];
		if (!keycode)
			return;
#endif
		if (keycode >= 'a' && keycode <= 'z')
			keycode -= 'a' - 'A';
		bool pressed = ev.type == KeyPress;
		if (!pressed) { // echo?
			do {
				if (XPending(display) < 1)
					break;
				XEvent next_ev;
				XPeekEvent(display, &next_ev);
				if (next_ev.type != KeyPress)
					break;
				const int threshold = 5;
				int dt = (int)next_ev.xkey.time - (int)ev.xkey.time;
				if (dt < -threshold || dt > threshold)
					break;
				KeySym next_keysym;
				XLookupString(&next_ev.xkey, str, 256, &next_keysym, 0);
				if (next_keysym != keysym_keycode)
					break;
				XNextEvent(display, &next_ev);
				if (h)
					h->handle_keyboard_key(keycode, true, unicode, true);
				return;
			} while (false);
		}
		if (h)
			h->handle_keyboard_key(keycode, pressed, unicode, false);
	}
};

FRT_REGISTER(KeyboardX11)

} // namespace frt
