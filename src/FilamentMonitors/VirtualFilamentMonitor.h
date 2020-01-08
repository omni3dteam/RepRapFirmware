/*
 * VirtualFilamentSensor.h
 *
 *  Created on: 17 12 2019
 *      Author: mg
 */

#ifndef SRC_FILAMENTSENSORS_VIRTUALFILAMENTMONITOR_H_
#define SRC_FILAMENTSENSORS_VIRTUALFILAMENTMONITOR_H_

#include "FilamentMonitor.h"

class VirtualFilamentMonitor : public FilamentMonitor
{
public:
	VirtualFilamentMonitor(unsigned int extruder, int type);

	bool Configure(GCodeBuffer& gb, const StringRef& reply, bool& seen) override;
	FilamentSensorStatus Check(bool isPrinting, bool fromIsr, uint32_t isrMillis, float filamentConsumed) override;
	FilamentSensorStatus Clear() override;
	void Diagnostics(MessageType mtype, unsigned int extruder) override;
	bool Interrupt() override;

private:
	void Poll();

	bool filamentPresent;
	bool enabled;
};

#endif /* SRC_FILAMENTSENSORS_VIRTUALFILAMENTMONITOR_H_ */
