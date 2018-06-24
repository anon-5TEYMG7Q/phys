/*
 * Copyright 2015 Fadri Furrer, ASL, ETH Zurich, Switzerland
 * Copyright 2015 Michael Burri, ASL, ETH Zurich, Switzerland
 * Copyright 2015 Mina Kamel, ASL, ETH Zurich, Switzerland
 * Copyright 2015 Janosch Nikolic, ASL, ETH Zurich, Switzerland
 * Copyright 2015 Markus Achtelik, ASL, ETH Zurich, Switzerland
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "waypoint_publisher_node.h"

namespace quad_control {

std::vector<quad_control::WaypointWithTime> WaypointWithTime::Read_waypoints(std::vector<quad_control::WaypointWithTime> waypoints){

  //std::ifstream wp_file("/home/nishat/hectorQuad/src/quad_control/resource/kitchen_waypoints.txt"); 
  std::ifstream wp_file("/home/nishat/nishat/hectorQuad/src/quad_control/resource/kitchen_waypoints.txt"); 
  // /home/wil/ros/catkin_ws/src/arducopter_slam/quad_control/resource/
  //wg_waypoints.txt  kitchen_waypoints.txt

  if (wp_file.is_open()) {
    double t, x, y, z, yaw;
    // Only read complete waypoints.
    while (wp_file >> t >> x >> y >> z >> yaw) {
      waypoints.push_back(WaypointWithTime(t, x, y, z, yaw * DEG_2_RAD));
    }
    wp_file.close();
    printf("Read %d waypoints. \n", (int )waypoints.size());
    return waypoints;
  }

  else {
    ROS_ERROR_STREAM("Unable to open poses file: ");
    //return 0;
  }

}

WaypointPublisherNode::WaypointPublisherNode(){

  InitializeParams();

  ros::NodeHandle nh;

  // Subscribers
  cmd_pos_sub_ = nh.subscribe("command/trajectory", 10, &WaypointPublisherNode::CommandTrajectoryCallback, this);
  odometry_sub_ = nh.subscribe("ground_truth/odometry", 10, &WaypointPublisherNode::OdometryCallback, this);
  cmd_vel_sub_ = nh.subscribe("/cmd_vel", 10, &WaypointPublisherNode::CommandVelCallback, this);
  cmd_threednav_sub_ = nh.subscribe("/cmd_3dnav", 10, &WaypointPublisherNode::threedNavCallback, this);

  //Publisher
  trajectory_pub = nh.advertise<mav_msgs::CommandTrajectory>("command/waypoint", 10);

  ROS_INFO_ONCE("Started Waypoint Publisher.");

}


WaypointPublisherNode::~WaypointPublisherNode() {}

void WaypointPublisherNode::InitializeParams(){

  ros::NodeHandle pnh("~");   

  desired_wp.position.x = 0.0;
  desired_wp.position.y = 0.0;
  desired_wp.position.z = 0.0;
  desired_wp.yaw = 0.0;

  waypoints_read = 0;
  published = 0;
  current_time = ros::Time::now().toSec(); 
  start_time = ros::Time::now().toSec(); 
  i = 0;

  ROS_INFO_ONCE("Waypoint_publisher_node Paramters Initialized.");

}

void WaypointPublisherNode::CommandVelCallback(const geometry_msgs::TwistConstPtr& command_velocity_msg){

  ROS_INFO_ONCE("Position_controller_node got first Command Velocity message.");

  cmd_vel = *command_velocity_msg;

}

void WaypointPublisherNode::CommandTrajectoryCallback(const mav_msgs::CommandTrajectoryConstPtr& command_trajectory_msg){

  ROS_INFO_ONCE("Position_controller_node got first Trajectory message.");

  //Convert to Eigen
  mav_msgs::eigenCommandTrajectoryFromMsg(*command_trajectory_msg, &command_trajectory);

}

void WaypointPublisherNode::threedNavCallback(const mav_msgs::CommandTrajectoryConstPtr& threed_nav_msg){
  
  ROS_INFO_ONCE("Position_controller_node got first 3d Nav message.");

  //Convert to Eigen
  mav_msgs::eigenCommandTrajectoryFromMsg(*threed_nav_msg, &threedNav_trajectory);

}

void WaypointPublisherNode::OdometryCallback(const nav_msgs::OdometryConstPtr& odometry_msg){

  ROS_INFO_ONCE("Position_controller_node got first GPS message.");

  current_gps_ = *odometry_msg;

  //Convert quaternion to Euler angles
  tf:quaternionMsgToTF(current_gps_.pose.pose.orientation, q);
  tf::Matrix3x3(q).getRPY(gps_roll, gps_pitch, gps_yaw);
  ROS_DEBUG("RPY = (%lf, %lf, %lf)", gps_roll, gps_pitch, gps_yaw);

  // Calculate Deisred Position from Controller Input By Default
  desired_wp.position.x = current_gps_.pose.pose.position.x + command_trajectory.position(0);
  desired_wp.position.y = current_gps_.pose.pose.position.y + command_trajectory.position(1);
  desired_wp.yaw = gps_yaw + command_trajectory.yaw;

  //printf("x: %lf %lf yaw: %lf \n",current_gps_.pose.pose.position.x, command_trajectory.position(0),gps_yaw)

  //Maintain altitude while maneuvering
  if(fabs(command_trajectory.position(2)) >= .01){
    desired_wp.position.z = current_gps_.pose.pose.position.z + command_trajectory.position(2);
  }
  else{
    desired_wp.position.z = desired_wp.position.z;
  }

  //Launch mode
  if(command_trajectory.snap(0)){    

    desired_wp.position.x = current_gps_.pose.pose.position.x;
    desired_wp.position.y = current_gps_.pose.pose.position.y;
    desired_wp.position.z = 1.0;
    desired_wp.yaw = gps_yaw;
  }

  //Land mode
  if(command_trajectory.snap(1)){    

    desired_wp.position.x = current_gps_.pose.pose.position.x;
    desired_wp.position.y = current_gps_.pose.pose.position.y;
    desired_wp.position.z = 0.05;
    desired_wp.yaw = gps_yaw;
  }

  control_mode.UpdateSwitchValue(command_trajectory.jerk(1));
  auto_mode.UpdateSwitchValue(command_trajectory.jerk(2));
  threednav_mode.UpdateSwitchValue(command_trajectory.snap(2));

  //Mission Mode triggered
  if(control_mode.GetSwitchValue()){

    ROS_INFO("Waypoint Mission Mode triggered");

    if(!waypoints_read){
      waypoints = waypoint_utility.Read_waypoints(waypoints);

      int size = waypoints.size();
      printf("Start publishing #%d waypoints \n", size);

      waypoints_read = 1;
      }

      if(i < waypoints.size()){

	const WaypointWithTime& wp = waypoints[i];

	if(!published){	

	printf("Publishing #%d x=%f y=%f z=%f yaw=%f, and wait for %fs. \n", (int)i, wp.wp.position.x, wp.wp.position.y, wp.wp.position.z, wp.wp.yaw, wp.waiting_time);

	published = 1;
	start_time = ros::Time::now().toSec();
	}

	if((current_time-start_time) < wp.waiting_time){

	//Rotate into BF
        waypointBF = control_mode.rotateGFtoBF(wp.wp.position.x-current_gps_.pose.pose.position.x, wp.wp.position.y-current_gps_.pose.pose.position.y, wp.wp.position.z, 0, 0, gps_yaw);

	desired_wp.position.x = (current_gps_.pose.pose.position.x + waypointBF(0));
        desired_wp.position.y = (current_gps_.pose.pose.position.y + waypointBF(1));
        desired_wp.position.z = wp.wp.position.z;
        desired_wp.yaw = wp.wp.yaw;

        desired_wp.jerk.x = 1;

        desired_wp.header.stamp = ros::Time::now();
        desired_wp.header.frame_id = "desired_mission_frame";
        trajectory_pub.publish(desired_wp);

	current_time = ros::Time::now().toSec();
 
	}
	else{
	  
	  i = i + 1;
	  published = 0;

	}        

      }
  }
  //Autonomous Mode triggered
  else if(auto_mode.GetSwitchValue()){

    ROS_INFO("Autonomous Mode triggered");

    // Calculate Deisred Position from Vel Cmd
    desired_wp.position.x = current_gps_.pose.pose.position.x + cmd_vel.linear.x;
    desired_wp.position.y = current_gps_.pose.pose.position.y + cmd_vel.linear.y;
    desired_wp.position.z = desired_wp.position.z + cmd_vel.linear.z;
    desired_wp.yaw = gps_yaw + cmd_vel.angular.z;

    desired_wp.jerk.x = 1;	//Set flag for position controller

    desired_wp.header.stamp = ros::Time::now();
    desired_wp.header.frame_id = "desired_auto_frame";
    trajectory_pub.publish(desired_wp);
  }
  else if(threednav_mode.GetSwitchValue()){

    ROS_INFO("3d Navigation Mode triggered");

    //Rotate into BF
    waypointBF = control_mode.rotateGFtoBF(threedNav_trajectory.position(0)-current_gps_.pose.pose.position.x, threedNav_trajectory.position(1)-current_gps_.pose.pose.position.y, threedNav_trajectory.position(2), 0, 0, gps_yaw);

    desired_wp.position.x = (current_gps_.pose.pose.position.x + waypointBF(0));
    desired_wp.position.y = (current_gps_.pose.pose.position.y + waypointBF(1));
    desired_wp.position.z = threedNav_trajectory.position(2);    
    desired_wp.yaw = threedNav_trajectory.yaw;

    desired_wp.jerk.x = 1;	//Set flag for position controller

    desired_wp.header.stamp = ros::Time::now();
    desired_wp.header.frame_id = "3dnav_mission_frame";
    trajectory_pub.publish(desired_wp);
  }
  //Simple GPS Mode or Mission Mode disabled
  else{
   //clear waypoints variable
   ROS_INFO("RESET");
   waypoints.clear();
   i = 0;
   waypoints_read = 0;
   published = 0;

  //modes
  desired_wp.snap.x = command_trajectory.snap(0);	// takeoff
  desired_wp.snap.y = command_trajectory.snap(1);	// land
  desired_wp.jerk.x = command_trajectory.jerk(0);	//enable GPS
  desired_wp.jerk.y = 0.0;	// enable mission

  desired_wp.header.stamp = ros::Time::now();
  desired_wp.header.frame_id = "desired_waypoint_frame";
  trajectory_pub.publish(desired_wp);
  }

}

}

//Main
int main(int argc, char** argv) {

  ros::init(argc, argv, "waypoint_publisher_node");

  quad_control::WaypointPublisherNode waypoint_publisher_node;

  ros::spin();

  return 0;
}




