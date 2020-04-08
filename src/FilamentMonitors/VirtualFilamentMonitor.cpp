/*
 * VirtualFilamentSensor.cpp
 *
 *  Created on: 17 12 2019
 *      Author: mg
 */

#include "VirtualFilamentMonitor.h"
#include "GCodes/GCodeBuffer.h"
#include "Platform.h"
#include "RepRap.h"
#include "Movement/Move.h"

VirtualFilamentMonitor::VirtualFilamentMonitor(unsigned int extruder, int type)
	: FilamentMonitor(extruder, type), filamentPresent(true), enabled(false)
{
}

// Configure this sensor, returning true if error and setting 'seen' if we processed any configuration parameters
bool VirtualFilamentMonitor::Configure(GCodeBuffer& gb, const StringRef& reply, bool& seen)
{
	if (gb.Seen('S'))
	{
		seen = true;
		enabled = (gb.GetIValue() > 0);
	}
	return false;
}

void VirtualFilamentMonitor::GetConfiguration(const StringRef& reply)
{
	// not implemented yet
}

// ISR for when the pin state changes. It should return true if the ISR wants the commanded extrusion to be fetched.
bool VirtualFilamentMonitor::Interrupt()
{
	// Nothing needed here
	detachInterrupt(GetPin());
	return false;
}

// Call the following regularly to keep the status up to date
void VirtualFilamentMonitor::Poll()
{
	filamentPresent = true;
}

FilamentSensorStatus VirtualFilamentMonitor::Check(bool isPrinting, bool fromIsr, uint32_t isrMillis, float filamentConsumed)
{
	Poll();
	// adding the current extrusion measured value to the total value
	AddExtrusionMeasured(filamentConsumed);
	return (!enabled || filamentPresent) ? FilamentSensorStatus::ok : FilamentSensorStatus::noFilament;
}

// Clear the measurement state - called when we are not printing a file. Return the present/not present status if available.
FilamentSensorStatus VirtualFilamentMonitor::Clear()
{
	Poll();
	return (!enabled || filamentPresent) ? FilamentSensorStatus::ok : FilamentSensorStatus::noFilament;
}

// Print diagnostic info for this sensor
void VirtualFilamentMonitor::Diagnostics(MessageType mtype, unsigned int extruder)
{
	Poll();
	const char* const statusText = "ok";
	reprap.GetPlatform().MessageF(mtype, "Extruder %u sensor: %s\n", extruder, statusText);
}

// End
