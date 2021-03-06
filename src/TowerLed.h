#ifndef SRC_TOWER_LED_H_
#define SRC_TOWER_LED_H_

#include "RepRap.h"

// Led pins due to documentation
enum class PinTowerLed : uint8_t
{
	green = 24,
	yellow,
	red
};

enum class ActionTowerLed : uint8_t
{
	light,
	blink
};

class TowerLed
{
private:
	char previousState;
	bool previousHeatStatus;
	bool previousPrintStatus;
	PinTowerLed activeLed;
	ActionTowerLed actionState;
	uint32_t lastActionTime;
	uint32_t towerLedSpinTime;

	void SetGreedLed(ActionTowerLed action);
	void SetRedLed(ActionTowerLed action);
	void SetYellowLed(ActionTowerLed action);

	void SetLedState(PinTowerLed led, bool state);
	void Show();
public:
	void Spin();
	void Init();

};

#endif
// SRC_TOWER_LED_H_
