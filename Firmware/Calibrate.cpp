#include <main.h>

extern const int maxPoles;

const int calibPower = sin_range/2;
const int quadrantDiv = SENSOR_MAX / numQuadrants;
	
ConfigData* config = (ConfigData*)flashPageAddress;

int currentPole = 0;

int getElectricDegrees() {
	int angle = spiCurrentAngle % sin_period;
	if (angle < 0) angle += sin_period;
	
	int a;
	int q = angle / quadrantDiv;
	int range = config->quadrants[q].range;
	int qstart;
	
	if (config->up)
	{
		int min = config->quadrants[q].minAngle;
		qstart = q * quadrantDiv;
		a = min + (angle - qstart) * range / quadrantDiv;
	}
	else
	{
		int max = config->quadrants[q].maxAngle;
		qstart = q * quadrantDiv;
		a = max - (angle - qstart) * range / quadrantDiv;
	}
	
	return a;
}

void calibrate() {
	const int step = 5;
	int a = 0;
	int i = 0;
	int sensor;
	int prevSensor;
	
	bool up;
	int q = 0;
	int prevQ = -1;
	int firstQ = -1;
	int quadrantsLeft = numQuadrants;
	
	QuadrantData qUp[numQuadrants] = { 0 };
	QuadrantData qDn[numQuadrants] = { 0 };
	
	for (i = 0; i < numQuadrants; i++)
	{
		qUp[i].minAngle = 0xFFFFFF;
		qDn[i].minAngle = 0xFFFFFF;
	}

	// gently set 0 angle
	
	for (int p = 0; p < calibPower / 10; p++)
	{
		delay(1);
		setPwm(0, p * 10);
	}
	
	// find the edge of the quadrant, detect direction
	
	int sensorFirst = spiReadAngle();
	int q1 = -1;
	int q2 = -1;
	int q3 = -1;
	while (true)
	{
		sensor = spiReadAngle();
		prevSensor = sensor;
		q = sensor / quadrantDiv;
			
		if (q1 == -1) q1 = q;
		else if (q2 == -1 && q1 != q) q2 = q;
		else if (q3 == -1 && q1 != q && q2 != q) q3 = q;
		else if (q1 != q && q2 != q && q3 != q) break;
		
		a += step;
		delay(1);
		setPwm(a, calibPower);		
	}
	
	if (sensor > sensorFirst)
	{
		if (sensor - sensorFirst < sensorFirst + (SENSOR_MAX - sensor))
			up = true;
		else
			up = false;
	}
	else
	{
		if (sensorFirst - sensor < sensor + (SENSOR_MAX - sensorFirst))
			up = false;
		else
			up = true;
	}

	// full turn forward

	bool awayFromFirst = false;
	bool backToFirst = false;
	prevQ = q;
	while (true)
	{
		sensor = spiReadAngle();
		q = sensor / quadrantDiv;
		if (q != prevQ)
		{
			q1 = q;
		}

		// noice on the quadrant edges
		if (up)
		{
			if ((q != 0 || prevQ != numQuadrants - 1) && q < prevQ) q = prevQ;
			else if (q == numQuadrants - 1 && prevQ == 0) q = prevQ;
		}
		else
		{
			if ((q != numQuadrants - 1 || prevQ != 0) && q > prevQ) q = prevQ;
			else if (q == 0 && prevQ == numQuadrants - 1) q = prevQ;
		}

		if (q != prevQ)
		{
			prevQ = q;			
		}
		if (firstQ == -1) firstQ = q;
		prevQ = q;		
		
		awayFromFirst |= (q - firstQ  == numQuadrants / 2) || (firstQ - q == numQuadrants / 2);
		backToFirst |= awayFromFirst && (q == firstQ);
		if (backToFirst) break;
		
		if (qUp[q].minAngle > a) qUp[q].minAngle = a;
		if (qUp[q].maxAngle < a) qUp[q].maxAngle = a;
		
		a += step * 2;
		delay(1);
		setPwm(a, calibPower);
	}
		
	for (int i = 0; i < numQuadrants; i++)
	{
		qUp[i].range = qUp[i].maxAngle - qUp[i].minAngle;
	}
	
	//  up and down
	
	int qForth, qBack;
	if (up)
	{
		qForth = q + 1;
		if (qForth == numQuadrants) qForth = 0;
		
		qBack = q - 1;
		if (qBack == -1) qBack = numQuadrants - 1;
	}
	else
	{
		qForth = q - 1;
		if (qForth == -1) qForth = numQuadrants - 1;
		
		qBack = q + 1;
		if (qBack == numQuadrants) qBack = 0;
	}
	
	while (true)
	{
		sensor = spiReadAngle();
		q = sensor / quadrantDiv;
		
		if (q == qForth) break;
		
		a += step * 2;
		delay(1);
		setPwm(a, calibPower);
	}
	
	while (true)
	{
		sensor = spiReadAngle();
		q = sensor / quadrantDiv;
		
		if (q == qBack) break;
		
		a -= step * 2;
		delay(1);
		setPwm(a, calibPower);
	}
	
	// full turn back
	
	awayFromFirst = false;
	backToFirst = false;
	firstQ = -1;
	prevQ = q;
	while (true)
	{
		sensor = spiReadAngle();
		q = sensor / quadrantDiv;
		if (q != prevQ)
		{
			q1 = q;
		}

		// noice on the quadrant edges
		if (up)
		{
			if ((q != numQuadrants - 1 || prevQ != 0) && q > prevQ) q = prevQ;
			else if (q == 0 && prevQ == numQuadrants - 1) q = prevQ;
		}
		else
		{
			if ((q != 0 || prevQ != numQuadrants - 1) && q < prevQ) q = prevQ;
			else if (q == numQuadrants - 1 && prevQ == 0) q = prevQ;
		}

		if (q != prevQ)
		{
			prevQ = q;
		}
		if (firstQ == -1) firstQ = q;
		prevQ = q;
		
		awayFromFirst |= (q - firstQ  == numQuadrants / 2) || (firstQ - q == numQuadrants / 2);
		backToFirst |= awayFromFirst && (q == firstQ);
		if (backToFirst) break;
		
		if (qDn[q].minAngle > a) qDn[q].minAngle = a;
		if (qDn[q].maxAngle < a) qDn[q].maxAngle = a;
		
		a -= step * 2;
		delay(1);
		setPwm(a, calibPower);
	}
	
	// gently release
	
	for (int p = calibPower / 10; p > 0; p--)
	{
		delay(1);
		setPwm(a, p * 10);
	}

	setPwm(0, 0);
	
	ConfigData lc;
	lc.controllerId = config->controllerId;	
	
	for (int i = 0; i < numQuadrants; i++)
	{
		qDn[i].range = qDn[i].maxAngle - qDn[i].minAngle;
	}	
	
	// calc average quadrants
	
	int minRange;
	int maxRange;
	for (int i = 0; i < numQuadrants; i++)
	{
		lc.quadrants[i].minAngle = (qUp[i].minAngle + qDn[i].minAngle) / 2;
		lc.quadrants[i].maxAngle = (qUp[i].maxAngle + qDn[i].maxAngle) / 2;
		int range = lc.quadrants[i].maxAngle - lc.quadrants[i].minAngle;
		lc.quadrants[i].range = range;
		
		if (i == 0 || minRange > range) minRange = range;
		if (i == 0 || maxRange < range) maxRange = range;
	}
	
	// store in flash
	lc.calibrated = true;
	lc.up = up;
	writeFlash((uint16_t*)&lc, sizeof(ConfigData) / sizeof(uint16_t));
}