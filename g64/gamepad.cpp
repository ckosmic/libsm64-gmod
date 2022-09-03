#include "gamepad.h"
#include "utils.h"

SDL_GameController* gGameController = NULL;
GamepadInput* mainGamepad;

bool gamepad_init()
{
	mainGamepad = (GamepadInput*)calloc(1, sizeof(GamepadInput));

	if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0)
	{
		return false;
	}

	if (SDL_NumJoysticks() < 1)
	{
		// No joysticks connected
		return false;
	}
	else if(SDL_IsGameController(0))
	{
		gGameController = SDL_GameControllerOpen(0);
		if (gGameController == NULL)
		{
			// Failed to open game controller
			return false;
		}
		DBG_PRINT("[G64] Game controller connected: %s", SDL_JoystickNameForIndex(0));
	}
	else 
	{
		return false;
	}

	return true;
}

void gamepad_close()
{
	SDL_GameControllerClose(gGameController);
	gGameController = NULL;

	free(mainGamepad);

	SDL_QuitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
	SDL_Quit();
}

void poll_events()
{
	SDL_Event e;
	bool done = false;

	while (done == false && SDL_PollEvent(&e) != 0)
	{
		switch (e.type)
		{
			case SDL_QUIT:
				done = true;
				break;
			case SDL_JOYDEVICEADDED:
				if (SDL_IsGameController(e.jdevice.which))
				{
					gGameController = SDL_GameControllerOpen(e.jdevice.which);
					if (gGameController == NULL)
					{
						DBG_PRINT("[G64] Failed to open game controller: %d", e.jdevice.which);
					}
					DBG_PRINT("[G64] Game controller connected: %s", SDL_JoystickNameForIndex(0));
				}
				break;
			case SDL_JOYDEVICEREMOVED:
				SDL_GameControllerClose(gGameController);
				gGameController = NULL;
				break;
		}
	}
}

GamepadInput* poll_controller()
{
	mainGamepad->lAxisX = 0;
	mainGamepad->lAxisY = 0;
	mainGamepad->rAxisX = 0;
	mainGamepad->rAxisY = 0;
	mainGamepad->lTrigger = 0;
	mainGamepad->rTrigger = 0;

	float axisMaxValue = 32767.0f - JOYSTICK_DEAD_ZONE;

	int lAxisX = SDL_GameControllerGetAxis(gGameController, SDL_CONTROLLER_AXIS_LEFTX);
	if (lAxisX > JOYSTICK_DEAD_ZONE || lAxisX < -JOYSTICK_DEAD_ZONE)
	{
		if (lAxisX < 0) lAxisX += JOYSTICK_DEAD_ZONE;
		else lAxisX -= JOYSTICK_DEAD_ZONE;
		mainGamepad->lAxisX = (float)lAxisX / axisMaxValue;
	}
	int lAxisY = SDL_GameControllerGetAxis(gGameController, SDL_CONTROLLER_AXIS_LEFTY);
	if (lAxisY > JOYSTICK_DEAD_ZONE || lAxisY < -JOYSTICK_DEAD_ZONE)
	{
		if (lAxisY < 0) lAxisY += JOYSTICK_DEAD_ZONE;
		else lAxisY -= JOYSTICK_DEAD_ZONE;
		mainGamepad->lAxisY = (float)lAxisY / axisMaxValue;
	}
	int rAxisX = SDL_GameControllerGetAxis(gGameController, SDL_CONTROLLER_AXIS_RIGHTX);
	if (rAxisX > JOYSTICK_DEAD_ZONE || rAxisX < -JOYSTICK_DEAD_ZONE)
	{
		if (rAxisX < 0) rAxisX += JOYSTICK_DEAD_ZONE;
		else rAxisX -= JOYSTICK_DEAD_ZONE;
		mainGamepad->rAxisX = (float)rAxisX / axisMaxValue;
	}
	int rAxisY = SDL_GameControllerGetAxis(gGameController, SDL_CONTROLLER_AXIS_RIGHTY);
	if (rAxisY > JOYSTICK_DEAD_ZONE || rAxisY < -JOYSTICK_DEAD_ZONE)
	{
		if (rAxisY < 0) rAxisY += JOYSTICK_DEAD_ZONE;
		else rAxisY -= JOYSTICK_DEAD_ZONE;
		mainGamepad->rAxisY = (float)rAxisY / axisMaxValue;
	}

	float mag = sqrt((mainGamepad->lAxisX * mainGamepad->lAxisX) + (mainGamepad->lAxisY * mainGamepad->lAxisY));
	if (mag > 0)
	{
		if (mag < 1) mag = 1;
		mainGamepad->lAxisX /= mag;
		mainGamepad->lAxisY /= mag;
	}
	mag = sqrt((mainGamepad->rAxisX * mainGamepad->rAxisX) + (mainGamepad->rAxisY * mainGamepad->rAxisY));
	if (mag > 0)
	{
		if (mag < 1) mag = 1;
		mainGamepad->rAxisX /= mag;
		mainGamepad->rAxisY /= mag;
	}

	int lTrigger = SDL_GameControllerGetAxis(gGameController, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
	if (lTrigger > JOYSTICK_DEAD_ZONE)
	{
		if (lTrigger < 0) lTrigger += JOYSTICK_DEAD_ZONE;
		else lTrigger -= JOYSTICK_DEAD_ZONE;
		mainGamepad->lTrigger = (float)lTrigger / axisMaxValue;
	}
	int rTrigger = SDL_GameControllerGetAxis(gGameController, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
	if (rTrigger > JOYSTICK_DEAD_ZONE)
	{
		if (rTrigger < 0) rTrigger += JOYSTICK_DEAD_ZONE;
		else rTrigger -= JOYSTICK_DEAD_ZONE;
		mainGamepad->rTrigger = (float)rTrigger / axisMaxValue;
	}

	mainGamepad->aButton = SDL_GameControllerGetButton(gGameController, SDL_CONTROLLER_BUTTON_A);
	mainGamepad->bButton = SDL_GameControllerGetButton(gGameController, SDL_CONTROLLER_BUTTON_B);
	mainGamepad->xButton = SDL_GameControllerGetButton(gGameController, SDL_CONTROLLER_BUTTON_X);
	mainGamepad->yButton = SDL_GameControllerGetButton(gGameController, SDL_CONTROLLER_BUTTON_Y);

	mainGamepad->backButton = SDL_GameControllerGetButton(gGameController, SDL_CONTROLLER_BUTTON_BACK);
	mainGamepad->guideButton = SDL_GameControllerGetButton(gGameController, SDL_CONTROLLER_BUTTON_GUIDE);
	mainGamepad->startButton = SDL_GameControllerGetButton(gGameController, SDL_CONTROLLER_BUTTON_START);

	mainGamepad->lStickButton = SDL_GameControllerGetButton(gGameController, SDL_CONTROLLER_BUTTON_LEFTSTICK);
	mainGamepad->rStickButton = SDL_GameControllerGetButton(gGameController, SDL_CONTROLLER_BUTTON_RIGHTSTICK);
	mainGamepad->lShoulder = SDL_GameControllerGetButton(gGameController, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
	mainGamepad->rShoulder = SDL_GameControllerGetButton(gGameController, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);

	mainGamepad->dPadUp = SDL_GameControllerGetButton(gGameController, SDL_CONTROLLER_BUTTON_DPAD_UP);
	mainGamepad->dPadDown = SDL_GameControllerGetButton(gGameController, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
	mainGamepad->dPadLeft = SDL_GameControllerGetButton(gGameController, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
	mainGamepad->dPadRight = SDL_GameControllerGetButton(gGameController, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);

	return mainGamepad;
}

const char* get_gamepad_name()
{
	if (SDL_NumJoysticks() >= 1)
		return SDL_JoystickNameForIndex(0);
	else
		return "No gamepad connected.";
}