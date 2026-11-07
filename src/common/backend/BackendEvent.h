#pragma once

#include <stdint.h>
#include <string>

enum eventtype {
	eventquit,
	eventtextinput,
	eventrawkeyup,
	eventrawkeydown,
	eventmousebuttondown,
	eventmousebuttonup,
	eventmousemove,
	eventresizewindow,
};
enum eventbuttons { buttonleft = 0x1,
	buttonright = 0x2,
	buttonmiddle = 0x4,
	buttonwheeldown = 0x8,
	buttonwheelup = 0x10 };

struct BackendEvent {
	uint32_t window_id;
	eventtype type;
	float x, y;
	int xrel, yrel;
	int key;
	unsigned int button;
	float wheel_x = 0, wheel_y = 0; // precise scroll amounts (touchpad two-finger scrolling)
	std::string text;
};