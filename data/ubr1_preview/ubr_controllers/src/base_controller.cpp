/*********************************************************************
 *  Software License Agreement (BSD License)
 *
 *  Copyright (c) 2013-2014, Unbounded Robotics Inc.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Unbounded Robotics nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

/* Author: Michael Ferguson */

#include <pluginlib/class_list_macros.h>
#include <ubr_controllers/base_controller.h>

PLUGINLIB_EXPORT_CLASS(ubr_controllers::BaseController, ubr_controllers::Controller)

namespace ubr_controllers
{

bool BaseController::init(ros::NodeHandle& nh, ControllerManager* manager)
{
  /* We absolutely need access to the controller manager */
  if (!manager)
  {
    ROS_ERROR_NAMED("BaseController", "No controller manager available.");
    initialized_ = false;
    return false;
  }

  Controller::init(nh, manager);

  /* Initialize joints */
  left_ = manager_->getJointHandle("base_l_wheel_joint");
  right_ = manager_->getJointHandle("base_r_wheel_joint");
  if (left_ == NULL || right_ == NULL)
  {
    ROS_ERROR_NAMED("BaseController", "Cannot get wheel joints.");
    initialized_ = false;
    return false;
  }
  left_last_position_ = left_->getPosition();
  right_last_position_ = right_->getPosition();
  last_update_ = ros::Time::now();

  /* Get base parameters */
  nh.param<double>("track_width", track_width_, 0.33665);
  nh.param<double>("radians_per_meter", radians_per_meter_, 17.4978147374);
  nh.param<bool>("publish_tf", publish_tf_, true);
  nh.param<std::string>("odometry_frame", odometry_frame_, "odom");
  nh.param<std::string>("base_frame", base_frame_, "base_link");
  nh.param<double>("moving_threshold", moving_threshold_, 0.0001);

  double t;
  nh.param<double>("/timeout", t, 0.25);
  timeout_ = ros::Duration(t);

  /* Get limits of base controller */
  nh.param<double>("max_velocity_x", max_velocity_x_, 1.0);
  nh.param<double>("max_velocity_r", max_velocity_r_, 4.5);
  nh.param<double>("max_acceleration_x", max_acceleration_x_, 0.75);
  nh.param<double>("max_acceleration_r", max_acceleration_r_, 3.0);

  /* Subscribe to base commands */
  cmd_sub_ = nh.subscribe<geometry_msgs::Twist>("command", 1,
                boost::bind(&BaseController::command, this, _1));

  /* Publish odometry & tf */
  ros::NodeHandle n;
  odom_pub_ = n.advertise<nav_msgs::Odometry>("odom", 10);
  if (publish_tf_)
    broadcaster_.reset(new tf::TransformBroadcaster());

  initialized_ = true;
  return initialized_;
}

void BaseController::command(const geometry_msgs::TwistConstPtr& msg)
{
  if (!initialized_)
  {
    ROS_ERROR_NAMED("BaseController", "Unable to accept command, not initialized.");
    return;
  }

  last_command_ = ros::Time::now();
  desired_x_ = msg->linear.x;
  desired_r_ = msg->angular.z;
  manager_->requestStart(name_);
}

bool  BaseController::start()
{
  if (!initialized_)
  {
    ROS_ERROR_NAMED("BaseController", "Unable to start, not initialized.");
    return false;
  }

  if (ros::Time::now() - last_command_ >= timeout_)
  {
    ROS_ERROR_NAMED("BaseController", "Unable to start, command has timed out.");
    return false;
  }

  return true;
}

bool BaseController::preempt(bool force)
{
  /* If we have timed out, certainly preempt */
  if (last_update_ - last_command_ >= timeout_)
    return true;

  /* If not moving, preempt */
  if (last_sent_x_ == 0.0 && last_sent_r_ == 0.0)
    return true;

  /* If forced, preempt */
  if (force)
    return true;

  return false;
}

bool BaseController::update(const ros::Time now, const ros::Duration dt)
{
  if (!initialized_)
    return false;  // should never really hit this

  /* See if we have timed out and need to stop */
  if (now - last_command_ >= timeout_)
  {
    ROS_DEBUG_THROTTLE_NAMED(5, "BaseController", "Command timed out.");
    desired_x_ = desired_r_ = 0.0;
  }

  /* Do velocity acceleration/limiting */
  if (desired_x_ > last_sent_x_)
  {
    last_sent_x_ += max_acceleration_x_ * (now - last_update_).toSec();
    if (last_sent_x_ > desired_x_)
      last_sent_x_ = desired_x_;
  }
  else
  {
    last_sent_x_ -= max_acceleration_x_ * (now - last_update_).toSec();
    if (last_sent_x_ < desired_x_)
      last_sent_x_ = desired_x_;
  }
  if (desired_r_ > last_sent_r_)
  {
    last_sent_r_ += max_acceleration_r_ * (now - last_update_).toSec();
    if (last_sent_r_ > desired_r_)
      last_sent_r_ = desired_r_;
  }
  else
  {
    last_sent_r_ -= max_acceleration_r_ * (now - last_update_).toSec();
    if (last_sent_r_ < desired_r_)
      last_sent_r_ = desired_r_;
  }

  double dx = 0.0;
  double dr = 0.0;

  double left_dx = static_cast<double>((left_->getPosition() - left_last_position_)/radians_per_meter_);
  double right_dx = static_cast<double>((right_->getPosition() - right_last_position_)/radians_per_meter_);
  double left_vel = static_cast<double>(left_->getVelocity()/radians_per_meter_);
  double right_vel = static_cast<double>(right_->getVelocity()/radians_per_meter_);
  left_last_position_ = left_->getPosition();
  right_last_position_ = right_->getPosition();

  /* Calculate forward and angular differences */
  double d = (left_dx+right_dx)/2.0;
  double th = (right_dx-left_dx)/track_width_;

  /* Calculate forward and angular velocities */
  dx = (left_vel + right_vel)/2.0;
  dr = (right_vel - left_vel)/track_width_;

  /* Update store odometry */
  odom_.pose.pose.position.x += d*cos(theta_);
  odom_.pose.pose.position.y += d*sin(theta_);
  theta_ += th;

  /* Actually set command */
  if ((last_sent_x_ != 0.0) || (last_sent_r_ != 0.0) ||
      (fabs(dx) > 0.05) || (fabs(dr) > 0.05))
  {
    setCommand(last_sent_x_ - (last_sent_r_/2.0 * track_width_),
              last_sent_x_ + (last_sent_r_/2.0 * track_width_));
  }

  /* Update odometry information. */
  odom_.pose.pose.orientation.z = sin(theta_/2.0);
  odom_.pose.pose.orientation.w = cos(theta_/2.0);
  odom_.twist.twist.linear.x = dx;
  odom_.twist.twist.angular.z = dr;

  last_update_ = now;
  return true;
}

bool BaseController::publish(ros::Time time)
{
  /* publish or perish */
  odom_.header.stamp = time;
  odom_pub_.publish(odom_);

  if (publish_tf_)
  {
    tf::Transform transform;
    transform.setOrigin(tf::Vector3(odom_.pose.pose.position.x, odom_.pose.pose.position.y, 0.0));
    transform.setRotation(tf::Quaternion(odom_.pose.pose.orientation.x,
                                         odom_.pose.pose.orientation.y,
                                         odom_.pose.pose.orientation.z,
                                         odom_.pose.pose.orientation.w) );
    /*
     * REP105 (http://ros.org/reps/rep-0105.html)
     *   says: map -> odom -> base_link
     */
    broadcaster_->sendTransform(tf::StampedTransform(transform, time, odometry_frame_, base_frame_));
  }

  return true;
}

void BaseController::setCommand(float left, float right)
{
  /* convert meters/sec into radians/sec */
  left_->setVelocityCommand(left * radians_per_meter_, 0.0);
  right_->setVelocityCommand(right * radians_per_meter_, 0.0);
}

}  // namespace ubr_controllers
