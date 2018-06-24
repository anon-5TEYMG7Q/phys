//============================================================================
// Name        : jaco_arm_driver.cpp
// Author      : WPI, Clearpath Robotics
// Version     : 0.5
// Copyright   : BSD
// Description : A ROS driver for controlling the Kinova Jaco robotic manipulator arm
//============================================================================

#include "kinova_driver/jaco_api.h"
#include "kinova_driver/jaco_arm.h"
#include "kinova_driver/jaco_pose_action.h"
#include "kinova_driver/jaco_angles_action.h"
#include "kinova_driver/jaco_fingers_action.h"
#include "kinova_driver/jaco_trajectory_action.h"
#include "kinova_driver/jaco_gripper_action.h"

int main(int argc, char **argv)
{
    ros::init(argc, argv, "jaco_arm_driver");
    ros::NodeHandle nh("~");
    boost::recursive_mutex api_mutex;

    bool is_first_init = true;
    while (ros::ok())
    {
        try
        {
            kinova::JacoComm comm(nh, api_mutex, is_first_init);
            kinova::JacoArm jaco(comm, nh);
            //kinova::JacoPoseActionServer pose_server(comm, nh);
            //kinova::JacoAnglesActionServer angles_server(comm, nh);
            //kinova::JacoFingersActionServer fingers_server(comm, nh);
            kinova::JacoTrajectoryActionServer trajectory_server(comm, nh);
            kinova::JacoGripperActionServer gripper_server(comm, nh);

            ros::spin();
        }
        catch(const std::exception& e)
        {
            ROS_ERROR_STREAM(e.what());
            kinova::JacoAPI api;
            boost::recursive_mutex::scoped_lock lock(api_mutex);
            api.closeAPI();
            ros::Duration(1.0).sleep();
        }

        is_first_init = false;
    }
    return 0;
}
