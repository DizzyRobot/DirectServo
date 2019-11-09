#include "main.h"

const int32_t calibPower = sin_range / 2;
const int32_t quadrantDiv = SENSOR_MAX / numQuadrants;

ConfigData *config = (ConfigData*) flashPageAddress;
int32_t currentPole = 0;

int32_t getElectricDegrees()
{
    int32_t angle = spiCurrentAngle % sin_period;
    if (angle < 0)
        angle += sin_period;

    int32_t a;
    int32_t q = angle / quadrantDiv;
    int32_t range = config->quadrants[q].range;
    int32_t qstart;

    if (config->up)
    {
        int32_t min = config->quadrants[q].minAngle;
        qstart = q * quadrantDiv;
        a = min + (angle - qstart) * range / quadrantDiv;
    }
    else
    {
        int32_t max = config->quadrants[q].maxAngle;
        qstart = q * quadrantDiv;
        a = max - (angle - qstart) * range / quadrantDiv;
    }

    return a;
}

// Measures how much angular change has occurred and averages it over SamplesToTake samples
int32_t AvgAngularChange(const int SamplesToTake = 100)
{
    int32_t AvgDiff = 0;
    int32_t OldAngle = ReportedAngle;
    for (int i = 0; i < SamplesToTake; i++)
    {
        spiReadAngleFiltered();
        setPwmTorque();
        delay_ticks(1);
        AvgDiff += (ReportedAngle - OldAngle);
        OldAngle = ReportedAngle;
    }
    AvgDiff /= SamplesToTake;
    return AvgDiff;
}

bool LimitCalibrate(bool UseFlash, int32_t NewTorqueLimit,
        int32_t NewTorqueSearchMagnitude, int32_t NewAngleOffsetBuffer)
{

    if (UseFlash)
    {
        NewTorqueLimit = config->Failsafe.TorqueLimit;
        NewTorqueSearchMagnitude = config->Failsafe.TorqueSearchMagnitude;
        NewAngleOffsetBuffer = config->Failsafe.AngleOffsetBuffer;
    }
    FailsafeConfig DefaultFSconfig;
    ConfigData lc;
    memcpy(&lc, config, sizeof(ConfigData));

    // Remove any existing torque limits so that the calibration can occur (by making sure it's saved in flash)
    lc.Failsafe = DefaultFSconfig;
    lc.Failsafe.TorqueSearchMagnitude = NewTorqueSearchMagnitude;
    lc.Failsafe.AngleOffsetBuffer = NewAngleOffsetBuffer;
    writeFlash((uint16_t*) &lc, sizeof(ConfigData) / sizeof(uint16_t));

    // If the user selected a NewTorqueLimit == TorqueLimitMax, then don't bother searching for torque limits
    // since they effectively requested to remove the limiter, and we may also then mark it as calibrated
    FailsafeCalibrated = NewTorqueLimit == TorqueLimitMax;
    if (FailsafeCalibrated)
        return FailsafeCalibrated;

    // if not calibrated, don't move forward with the operation
    if (!config->calibrated)
        return false;

    // let freely spin for a second
    usartTorqueCommandValue = 0;
    AvgAngularChange(1000);

    // search for positive limit
    usartTorqueCommandValue = NewTorqueSearchMagnitude;
    AvgAngularChange(1000); // get it moving
    int32_t avgChanges = 20;
    while (avgChanges > 0)
    {
        avgChanges = AvgAngularChange();
    }
    lc.Failsafe.MaxAngle = ReportedAngle - NewAngleOffsetBuffer;

    // let freely spin for a second
    usartTorqueCommandValue = 0;
    AvgAngularChange(1000);

    // search for negative limit
    usartTorqueCommandValue = -NewTorqueSearchMagnitude;
    AvgAngularChange(1000); // get it moving
    avgChanges = -20;
    while (avgChanges < 0)
    {
        avgChanges = AvgAngularChange();
    }
    lc.Failsafe.MinAngle = ReportedAngle + NewAngleOffsetBuffer;

    // move away from the offset, so it doesn't re-trigger immediately
    usartTorqueCommandValue = NewTorqueSearchMagnitude;
    while (ReportedAngle < lc.Failsafe.MinAngle)
    {
        spiReadAngleFiltered();
        setPwmTorque();
        delay_ticks(1);
    }

    // turn off the motors
    usartTorqueCommandValue = 0;
    AvgAngularChange();

    // save the new TorqueLimit
    lc.Failsafe.TorqueLimit = NewTorqueLimit;

    // if the failsafe angles are too close together (< 10 degrees), an error has likely occurred
    FailsafeCalibrated = (((lc.Failsafe.MaxAngle + NewAngleOffsetBuffer)
            - (lc.Failsafe.MinAngle - NewAngleOffsetBuffer))
            >= (0xFFFF / 360) * 10);

    // make sure a failed calibration doesn't get set
    if (!FailsafeCalibrated)
        lc.Failsafe = DefaultFSconfig;

    // save new failsafe data to flash
    writeFlash((uint16_t*) &lc, sizeof(ConfigData) / sizeof(uint16_t));

    return FailsafeCalibrated;
}

bool calibrate()
{
    const int32_t step = 5;
    uint32_t a = 0;
    int32_t i = 0;
    int32_t sensor;

    bool up;
    int32_t q = 0;
    int32_t prevQ = -1;
    int32_t firstQ = -1;

    QuadrantData qUp[numQuadrants];
    QuadrantData qDn[numQuadrants];
    for (i = 0; i < numQuadrants; i++)
    {
        qUp[i].minAngle = 0xFFFFFF;
        qDn[i].minAngle = 0xFFFFFF;
    }

    // gently set 0 angle

    for (int32_t p = 0; p < calibPower / 10; p++)
    {
        delay_ticks(1);
        setPwm(0, p * 10);
    }

    // find the edge of the quadrant, detect direction

    int32_t sensorFirst = spiReadAngle();
    int32_t q1 = -1;
    int32_t q2 = -1;
    int32_t q3 = -1;
    while (true)
    {
        sensor = spiReadAngle();
        q = sensor / quadrantDiv;

        if (q1 == -1)
            q1 = q;
        else if (q2 == -1 && q1 != q)
            q2 = q;
        else if (q3 == -1 && q1 != q && q2 != q)
            q3 = q;
        else if (q1 != q && q2 != q && q3 != q)
            break;

        a += step;
        delay_ticks(1);
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
            if ((q != 0 || prevQ != numQuadrants - 1) && q < prevQ)
                q = prevQ;
            else if (q == numQuadrants - 1 && prevQ == 0)
                q = prevQ;
        }
        else
        {
            if ((q != numQuadrants - 1 || prevQ != 0) && q > prevQ)
                q = prevQ;
            else if (q == 0 && prevQ == numQuadrants - 1)
                q = prevQ;
        }

        if (q != prevQ)
        {
            prevQ = q;
        }
        if (firstQ == -1)
            firstQ = q;
        prevQ = q;

        awayFromFirst |= (q - firstQ == numQuadrants / 2)
                || (firstQ - q == numQuadrants / 2);
        backToFirst |= awayFromFirst && (q == firstQ);
        if (backToFirst)
            break;

        if (qUp[q].minAngle > a)
            qUp[q].minAngle = a;
        if (qUp[q].maxAngle < a)
            qUp[q].maxAngle = a;

        a += step * 2;
        delay_ticks(1);
        setPwm(a, calibPower);
    }

    for (int32_t i = 0; i < numQuadrants; i++)
    {
        qUp[i].range = qUp[i].maxAngle - qUp[i].minAngle;
    }

    //  up and down

    int32_t qForth, qBack;
    if (up)
    {
        qForth = q + 1;
        if (qForth == numQuadrants)
            qForth = 0;

        qBack = q - 1;
        if (qBack == -1)
            qBack = numQuadrants - 1;
    }
    else
    {
        qForth = q - 1;
        if (qForth == -1)
            qForth = numQuadrants - 1;

        qBack = q + 1;
        if (qBack == numQuadrants)
            qBack = 0;
    }

    while (true)
    {
        sensor = spiReadAngle();
        q = sensor / quadrantDiv;

        if (q == qForth)
            break;

        a += step * 2;
        delay_ticks(1);
        setPwm(a, calibPower);
    }

    while (true)
    {
        sensor = spiReadAngle();
        q = sensor / quadrantDiv;

        if (q == qBack)
            break;

        a -= step * 2;
        delay_ticks(1);
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
            if ((q != numQuadrants - 1 || prevQ != 0) && q > prevQ)
                q = prevQ;
            else if (q == 0 && prevQ == numQuadrants - 1)
                q = prevQ;
        }
        else
        {
            if ((q != 0 || prevQ != numQuadrants - 1) && q < prevQ)
                q = prevQ;
            else if (q == numQuadrants - 1 && prevQ == 0)
                q = prevQ;
        }

        if (q != prevQ)
        {
            prevQ = q;
        }
        if (firstQ == -1)
            firstQ = q;
        prevQ = q;

        awayFromFirst |= (q - firstQ == numQuadrants / 2)
                || (firstQ - q == numQuadrants / 2);
        backToFirst |= awayFromFirst && (q == firstQ);
        if (backToFirst)
            break;

        if (qDn[q].minAngle > a)
            qDn[q].minAngle = a;
        if (qDn[q].maxAngle < a)
            qDn[q].maxAngle = a;

        a -= step * 2;
        delay_ticks(1);
        setPwm(a, calibPower);
    }

    // gently release

    for (int32_t p = calibPower / 10; p > 0; p--)
    {
        delay_ticks(1);
        setPwm(a, p * 10);
    }

    setPwm(0, 0);

    ConfigData lc;
    memcpy(&lc, config, sizeof(ConfigData)); // making sure to carry over any other settings

    lc.controllerId = config->controllerId;

    for (int32_t i = 0; i < numQuadrants; i++)
    {
        qDn[i].range = qDn[i].maxAngle - qDn[i].minAngle;
    }

    // calc average quadrants

    int32_t minRange = 0;
    int32_t maxRange = 0;
    for (int32_t i = 0; i < numQuadrants; i++)
    {
        lc.quadrants[i].minAngle = (qUp[i].minAngle + qDn[i].minAngle) / 2;
        lc.quadrants[i].maxAngle = (qUp[i].maxAngle + qDn[i].maxAngle) / 2;
        int32_t range = lc.quadrants[i].maxAngle - lc.quadrants[i].minAngle;
        lc.quadrants[i].range = range;

        if (i == 0 || minRange > range)
            minRange = range;
        if (i == 0 || maxRange < range)
            maxRange = range;
    }

    // store in flash
    lc.calibrated = true;
    lc.up = up;
    writeFlash((uint16_t*) &lc, sizeof(ConfigData) / sizeof(uint16_t));

    // If the failsafe is active, but not set then calibrate it also
    if (!FailsafeCalibrated)
        FailsafeCalibrated = LimitCalibrate(true);

    // if both are okay de-blink
    if (config->calibrated && FailsafeCalibrated)
        blinkCalib(false);

    return config->calibrated && FailsafeCalibrated;
}

