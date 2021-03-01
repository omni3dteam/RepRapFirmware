#ifndef SRC_FILAMENTSENSORS_SIMPLEFILTEREDFILAMENTMONITOR_H_
#define SRC_FILAMENTSENSORS_SIMPLEFILTEREDFILAMENTMONITOR_H_

#include "FilamentMonitor.h"

class SimpleFilteredFilamentMonitor : public FilamentMonitor
{
public:
	SimpleFilteredFilamentMonitor(unsigned int extruder, int type);

	bool Configure(GCodeBuffer& gb, const StringRef& reply, bool& seen) override;
	void GetConfiguration(const StringRef& reply) override;
	FilamentSensorStatus Check(bool isPrinting, bool fromIsr, uint32_t isrMillis, float filamentConsumed) override;
	FilamentSensorStatus Clear() override;
	void Diagnostics(MessageType mtype, unsigned int extruder) override;
	bool Interrupt() override;

private:
	void Poll();

	bool highWhenNoFilament;
	bool filamentPresent;
	bool enabled;
	bool firstNoFilament;

	double followFilamentChange;
	double filterDistance;
};

#endif /* SRC_FILAMENTSENSORS_SIMPLEFILTEREDFILAMENTMONITOR_H_ */
