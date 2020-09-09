#include "TowerLed.h"
#include "Platform.h"
#include "GCodes/Gcodes.h"

void TowerLed::Init()
{
	previousState = 0;
	previousHeatStatus = false;
}

void TowerLed::Spin()
{

	char state = reprap.GetStatusCharacter();

	// while printing we can heat up so we need to check it
	if ((state != previousState) || (state == 'P'))
	{
		previousState = state;

		switch(state)
		{
		case 'I':			// Idle
			SetGreedLed(ActionTowerLed::light);
			break;
		case 'P':			// Printing
			{
				bool heatStatus = reprap.GetGCodes().IsHeatingUp();

				if (heatStatus != previousHeatStatus)
				{
					previousHeatStatus = heatStatus;

					if (heatStatus)
					{
						SetGreedLed(ActionTowerLed::blink);
					}
					else
					{
						SetGreedLed(ActionTowerLed::light);
					}
				}
			}
			break;
		case 'F':			// Flashing a new firmware binary
		case 'C':			// Reading the configuration file
		case 'B':			// Busy
		case 'T':			// Changing Tool
			SetGreedLed(ActionTowerLed::blink);
			break;
		case 'O':			// Off i.e. powered down
		case 'M':			// Simulating
			SetRedLed(ActionTowerLed::blink);
			break;
		case 'R':			// Resuming
		case 'D':			// Pausing / Decelerating
			SetYellowLed(ActionTowerLed::blink);
			break;
		case 'S':			// Paused / Stopped
		case 'H':			// Halted
			SetYellowLed(ActionTowerLed::light);
			break;
		default:
			SetRedLed(ActionTowerLed::light);
		}
	}

	Show();
}

void TowerLed::Show()
{
	if (actionState == ActionTowerLed::blink)
	{
		static bool state = false;
		uint32_t getTime = millis();

		if (getTime - lastActionTime > 500)
		{
			state = !state;
			SetLedState(activeLed, state);

			lastActionTime = getTime;
		}
	}
}

void TowerLed::SetGreedLed(ActionTowerLed action)
{
	SetLedState(PinTowerLed::green, true);
	SetLedState(PinTowerLed::yellow, false);
	SetLedState(PinTowerLed::red, false);

	actionState = action;
}

void TowerLed::SetYellowLed(ActionTowerLed action)
{
	SetLedState(PinTowerLed::green, false);
	SetLedState(PinTowerLed::yellow, true);
	SetLedState(PinTowerLed::red, false);

	actionState = action;
}

void TowerLed::SetRedLed(ActionTowerLed action)
{
	SetLedState(PinTowerLed::green, false);
	SetLedState(PinTowerLed::yellow, false);
	SetLedState(PinTowerLed::red, true);

	actionState = action;
}


void TowerLed::SetLedState(PinTowerLed led, bool state)
{
	Pin pin;
	bool invert;

	if (reprap.GetPlatform().GetFirmwarePin((Pin)led, PinAccess::write, pin, invert))
	{
		IoPort::WriteDigital(pin, state);

		if (state)
		{
			activeLed = led;
		}
	}
}
