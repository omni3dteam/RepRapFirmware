/*
 * SimpleFilamentSensor.cpp
 *
 *  Created on: 20 Jul 2017
 *      Author: David
 */

#include "SimpleFilteredFilamentMonitor.h"
#include "RepRap.h"
#include "Platform.h"
#include "GCodes/GCodeBuffer.h"

SimpleFilteredFilamentMonitor::SimpleFilteredFilamentMonitor(unsigned int extruder, int type)
	: FilamentMonitor(extruder, type), highWhenNoFilament(type == 12), filamentPresent(false), enabled(false)
{
	filterDistance = 2.0;
}

// Configure this sensor, returning true if error and setting 'seen' if we processed any configuration parameters
bool SimpleFilteredFilamentMonitor::Configure(GCodeBuffer& gb, const StringRef& reply, bool& seen)
{
	if (ConfigurePin(gb, reply, INTERRUPT_MODE_NONE, seen))
	{
		return true;
	}

	seen = false;
	float tempfilterDistance;

	gb.TryGetFValue('L', tempfilterDistance, seen);
	if (seen)
	{
		filterDistance = tempfilterDistance > 0 ? tempfilterDistance : 2.0;
	}

	if (gb.Seen('S'))
	{
		seen = true;
		enabled = (gb.GetIValue() > 0);
	}

	if (seen)
	{
		Check(false, false, 0, 0.0);
	}
	else
	{
		reply.printf("Simple filtered filament sensor on endstop %d, %s, output %s when no filament, filament present: %s, filter distance: %.2f",
						GetEndstopNumber(),
						(enabled) ? "enabled" : "disabled",
						(highWhenNoFilament) ? "high" : "low",
						(filamentPresent) ? "yes" : "no",
						(double)filterDistance);
	}

	return false;
}

void SimpleFilteredFilamentMonitor::GetConfiguration(const StringRef& reply)
{
	reply.printf("P%s C%d S%d L%.2f", highWhenNoFilament ? "12" : "11", GetEndstopNumber(), enabled, (double)filterDistance);
}

// ISR for when the pin state changes
bool SimpleFilteredFilamentMonitor::Interrupt()
{
	// Nothing needed here
	detachInterrupt(GetPin());
	return false;
}

// Call the following regularly to keep the status up to date
void SimpleFilteredFilamentMonitor::Poll()
{
	const bool b = IoPort::ReadPin(GetPin());
	filamentPresent = (highWhenNoFilament) ? !b : b;
}

// Call the following at intervals to check the status. This is only called when extrusion is in progress or imminent.
// 'filamentConsumed' is the net amount of extrusion since the last call to this function.
FilamentSensorStatus SimpleFilteredFilamentMonitor::Check(bool isPrinting, bool fromIsr, uint32_t isrMillis, float filamentConsumed)
{
	Poll();
	// adding the current extrusion measured value to the total value
	AddExtrusionMeasured(filamentConsumed);

	FilamentSensorStatus fstat = (!enabled || filamentPresent) ? FilamentSensorStatus::ok : FilamentSensorStatus::noFilament;

	if (fstat == FilamentSensorStatus::noFilament)
	{
		fstat = FilamentSensorStatus::ok;

		if (!firstNoFilament)
		{
			firstNoFilament = true;
			followFilamentChange = GetExtrusionMeasured();
		}
		else
		{
			const float totalExtrusion = GetExtrusionMeasured();

			if (totalExtrusion - followFilamentChange > filterDistance)
			{
				firstNoFilament = false;
				fstat = FilamentSensorStatus::noFilament;
			}
		}
	}
	else
	{
		firstNoFilament = false;
	}

	return fstat;
}

// Clear the measurement state - called when we are not printing a file. Return the present/not present status if available.
FilamentSensorStatus SimpleFilteredFilamentMonitor::Clear()
{
	Poll();
	return (!enabled || filamentPresent) ? FilamentSensorStatus::ok : FilamentSensorStatus::noFilament;
}

// Print diagnostic info for this sensor
void SimpleFilteredFilamentMonitor::Diagnostics(MessageType mtype, unsigned int extruder)
{
	reprap.GetPlatform().MessageF(mtype, "Extruder %u sensor: %s\n", extruder, (filamentPresent) ? "ok" : "no filament");
}

// End
