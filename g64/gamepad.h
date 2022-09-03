#pragma once
#include <SDL.h>

#define JOYSTICK_DEAD_ZONE 8000

struct GamepadInput
{
	float lAxisX;
	float lAxisY;
	float rAxisX;
	float rAxisY;
	float lTrigger;
	float rTrigger;
	bool aButton;
	bool bButton;
	bool xButton;
	bool yButton;
	bool backButton;
	bool guideButton;
	bool startButton;
	bool lStickButton;
	bool rStickButton;
	bool lShoulder;
	bool rShoulder;
	bool dPadUp;
	bool dPadDown;
	bool dPadLeft;
	bool dPadRight;
};

extern GamepadInput* mainGamepad;
extern SDL_GameController* gGameController;

void poll_events();
GamepadInput* poll_controller();
bool gamepad_init();
void gamepad_close();
const char* get_gamepad_name();