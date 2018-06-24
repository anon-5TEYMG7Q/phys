#include <lama_jockeys/navigating_jockey.h>

namespace lama_jockeys
{

NavigatingJockey::NavigatingJockey(const std::string& name) :
  Jockey(name),
  server_(nh_, name, boost::bind(&NavigatingJockey::goalCallback, this, _1), false),
  goal_reached_(false)
{
  server_.registerPreemptCallback(boost::bind(&NavigatingJockey::preemptCallback, this));

  server_.start();
  ROS_DEBUG("Action server '%s' started for Navigation", jockey_name_.c_str());

  if (!private_nh_.getParamCached("max_goal_distance", max_goal_distance_))
    max_goal_distance_ = 10.0;
  if (!private_nh_.getParamCached("max_goal_dtheta", max_goal_dtheta_))
    max_goal_dtheta_ = 0.785;  // 45 deg
  if (!private_nh_.getParamCached("kp_v", kp_v_))
    kp_v_ = 0.05;
  if (!private_nh_.getParamCached("kp_w", kp_w_))
    kp_w_ = 0.2;
  if (!private_nh_.getParamCached("min_velocity", min_velocity_))
    min_velocity_ = 0.020;
  if (!private_nh_.getParamCached("reach_distance", reach_distance_))
    reach_distance_ = 0.050;
}

void NavigatingJockey::goalCallback(const lama_jockeys::NavigateGoalConstPtr& goal)
{
  goal_.action = goal->action;

  // Check that preempt has not been requested by the client.
  if (server_.isPreemptRequested() || !ros::ok())
  {
    ROS_INFO("%s: Preempted", jockey_name_.c_str());
    // set the action state to preempted
    server_.setPreempted();
    return;
  }

  switch (goal_.action)
  {
    case lama_jockeys::NavigateGoal::STOP:
      ROS_DEBUG("Received action STOP");
      initAction();
      // Reset the goal, just in case.
      goal_.edge = lama_msgs::LamaObject();
      goal_.descriptor_link = lama_msgs::DescriptorLink();
      goal_.relative_edge_start = geometry_msgs::Pose();
      onStop();
      break;
    case lama_jockeys::NavigateGoal::TRAVERSE:
      ROS_DEBUG("Received action TRAVERSE");
      initAction();
      goal_.edge = goal->edge;
      goal_.descriptor_link = goal->descriptor_link;
      goal_.relative_edge_start = goal->relative_edge_start;
      onTraverse();
      break;
    case lama_jockeys::NavigateGoal::INTERRUPT:
      ROS_DEBUG("Received action INTERRUPT");
      interrupt();
      onInterrupt();
      break;
    case lama_jockeys::NavigateGoal::CONTINUE:
      ROS_DEBUG("Received action CONTINUE");
      resume();
      onContinue();
      break;
  }
}

void NavigatingJockey::preemptCallback()
{
  ROS_INFO("%s: Preempted", jockey_name_.c_str());
  // set the action state to preempted
  server_.setPreempted();
}

void NavigatingJockey::initAction()
{
  Jockey::initAction();
  result_ = NavigateResult();
}

void NavigatingJockey::onInterrupt()
{
  ROS_DEBUG("%s: navigating goal %d interrupted", jockey_name_.c_str(), goal_.edge.id);
}

void NavigatingJockey::onContinue()
{
  ROS_DEBUG("%s: navigating goal %d resumed", jockey_name_.c_str(), goal_.edge.id);
}

/** Return the twist to reach the given goal pose
 * 
 * There is no cycle in this function, so it should be called periodically by class instances.
 *
 * goal[in] position of the goal relative to the robot
 */
geometry_msgs::Twist NavigatingJockey::goToGoal(const geometry_msgs::Point& goal)
{
  geometry_msgs::Twist twist;

  if (isGoalReached())
    return twist;

  double distance = std::sqrt(goal.x * goal.x + goal.y * goal.y);
  if (distance > max_goal_distance_)
  {
    ROS_DEBUG("%s: distance to goal (%f) is greater than max (%f)", jockey_name_.c_str(), distance, max_goal_distance_);
    return twist;
  }

  if (distance < reach_distance_)
  {
    setGoalReached();
    return twist;
  }

  double dtheta = std::atan2(goal.y, goal.x);
  ROS_DEBUG("%s: distance to goal: %f, dtheta to goal: %f", jockey_name_.c_str(), distance, dtheta);
  if (dtheta > max_goal_dtheta_) dtheta = max_goal_dtheta_;
  if (dtheta < -max_goal_dtheta_) dtheta = -max_goal_dtheta_;

  // Only move forward if the goal is in front of the robot (+/- max_goal_dtheta_).
  // The linear velocity is max if the goal is in front of the robot and 0 if at max_goal_dtheta_.
  double vx = kp_v_ * distance * (max_goal_dtheta_ - std::fabs(dtheta)) / max_goal_dtheta_; 
  double wz = kp_w_ * dtheta;
  if (vx < min_velocity_)
  {
    vx = min_velocity_;
  }

  twist.linear.x = vx;
  twist.angular.z = wz;
  return twist;
}

} // namespace lama_jockeys

