/*
 * jacolib.h
 *
 *  Created on: Feb 16, 2013
 *  Modified on: June 25, 2013
 *      Author: mdedonato, Clearpath Robotics
 */

#ifndef JACO_DRIVER_JACO_API_H
#define JACO_DRIVER_JACO_API_H

#include <dlfcn.h>
#include <ros/ros.h>

#include <iostream>

#include "kinova/Kinova.API.UsbCommandLayerUbuntu.h"
#include "kinova/KinovaTypes.h"


namespace kinova
{

#define JACO_USB_LIBRARY  "Kinova.API.USBCommandLayerUbuntu.so"
#define JACO_COMM_LIBRARY "Kinova.API.CommLayerUbuntu.so"

class JacoAPI
{
 public:
    JacoAPI(void);

    int (*initAPI)(void);
    int (*closeAPI)(void);
    int (*getAPIVersion)(int[API_VERSION_COUNT]);
    int (*getDeviceCount)(int &);
    int (*getDevices)(KinovaDevice[MAX_KINOVA_DEVICE], int &);
    int (*setActiveDevice)(KinovaDevice);

    int (*getGeneralInformations)(GeneralInformations &);
    int (*getQuickStatus)(QuickStatus &);
    // int (*GetForcesInfo)(ForcesInfo &);

    int (*getCodeVersion)(int[CODE_VERSION_COUNT]);
    int (*startControlAPI)();
    int (*stopControlAPI)();
    int (*initFingers)();

    int (*moveHome)();

    int (*getCartesianPosition)(CartesianPosition &);
    int (*getAngularPosition)(AngularPosition &);
    int (*getAngularCommand)(AngularPosition &);
    int (*getAngularVelocity)(AngularPosition &);
    int (*getCartesianForce)(CartesianPosition &);
    int (*setCartesianForceMinMax)(CartesianInfo, CartesianInfo);
    int (*setCartesianInertiaDamping)(CartesianInfo, CartesianInfo);
    int (*startForceControl)();
    int (*stopForceControl)();
    int (*getAngularForce)(AngularPosition &);
    int (*getAngularCurrent)(AngularPosition &);
    int (*getControlType)(int &);
    int (*getActualTrajectoryInfo)(TrajectoryPoint &);
    int (*getGlobalTrajectoryInfo)(TrajectoryFIFO &);
    int (*getSensorsInfo)(SensorsInfo &);
    int (*setAngularControl)();
    int (*setCartesianControl)();
    int (*restoreFactoryDefault)();
    int (*sendJoystickCommand)(JoystickCommand);
    int (*getJoystickValue)(JoystickCommand &);
    int (*sendAdvanceTrajectory)(TrajectoryPoint);
    int (*sendBasicTrajectory)(TrajectoryPoint);
    int (*getClientConfigurations)(ClientConfigurations &);
    int (*setClientConfigurations)(ClientConfigurations);
    int (*eraseAllTrajectories)();
    int (*getPositionCurrentActuators)(float[POSITION_CURRENT_COUNT]);
    int (*setActuatorPID)(unsigned int, float, float, float);

    int (*getEndEffectorOffset)(unsigned int&, float&, float&, float&);
    int (*setEndEffectorOffset)(unsigned int, float, float, float);
};

}  // namespace kinova
#endif  // JACO_DRIVER_JACO_API_H
