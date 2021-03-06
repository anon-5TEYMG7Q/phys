/*
 * experiment.cpp
 *
 *  Created on: Apr 2, 2011
 *      Author: titus
 */

#include "ros/ros.h"
#include "nav_msgs/Odometry.h"
#include "ml_light_pioneer/actions.h"
#include "ml_light_pioneer/qlearner.h"
#include "ml_light_pioneer/states_stage.h"
#include "ml_light_pioneer/learning_curve.h"
#include "std_msgs/Bool.h"
#include "geometry_msgs/Pose.h"

class Experiment
{
public:
	Experiment(ros::NodeHandle n);
private:
	ros::NodeHandle n_;
	nav_msgs::Odometry odom_msg_;

	bool move_stopped_, learn_;
	int num_reps_, cnt_rep_, state_, state_p_, action_, mode_, cnt_timesteps_;
	double freq_, goal_radius_, start_radius_, reward_, goalx_, goaly_;
	double bounds_[4];

  enum {MODE_REP_START, MODE_REP, MODE_RETURN, MODE_DONE}; 

	States* states_;
	Actions* actions_;
	QLearner* qobj_;
	LearningCurve* lc_;
	
  ros::Publisher move_pub_;
	ros::Subscriber bool_sub_, odom_sub_;
	ros::Timer timer_;

	void odom_cb(const nav_msgs::Odometry msg);
	void bool_cb(const std_msgs::Bool msg);
	void timer_cb(const ros::TimerEvent& event);
	double getDistance(void);
	bool outOfBounds(void);
};

Experiment::Experiment(ros::NodeHandle n):n_(n)
{
	ros::NodeHandle n_private("~");
	n_private.param("num_reps", num_reps_, 100);
	n_private.param("freq", freq_, 2.0);
	n_private.param("goal_radius", goal_radius_, 0.5);
  n_private.param("start_radius", start_radius_, 5.0);
  n_private.param("goalx", goalx_, 0.0);
  n_private.param("goaly", goaly_, 0.0);
  n_private.param("bounds/tlx", bounds_[0], -8.0);
  n_private.param("bounds/tly", bounds_[1], 8.0);
  n_private.param("bounds/brx", bounds_[2], 8.0);
  n_private.param("bounds/bry", bounds_[3], -8.0);
  
  if (n_private.hasParam("qarray"))
    learn_ = false;
  else
    learn_ = true;

  srand ( time(NULL) );
	cnt_rep_ = 0;
	cnt_timesteps_ = 0;
	mode_ = MODE_REP_START;

	states_ = new States(n);
	actions_ = new Actions(n);
	qobj_ = new QLearner(n);
	lc_ = new LearningCurve();

  move_pub_ = n_.advertise<geometry_msgs::Pose>("move_cmd", 1);
  bool_sub_ = n_.subscribe("move_done", 1, &Experiment::bool_cb, this);
	odom_sub_ = n.subscribe("base_pose_ground_truth", 10, &Experiment::odom_cb, this);
	timer_ = n.createTimer(ros::Duration(1/freq_), &Experiment::timer_cb, this);
}

void Experiment::odom_cb(const nav_msgs::Odometry msg)
{
  odom_msg_ = msg;
}

void Experiment::bool_cb(const std_msgs::Bool msg)
{
  if (move_stopped_ == false)
    move_stopped_ = msg.data;  
}

void Experiment::timer_cb(const ros::TimerEvent& event)
{  
	switch(mode_)
	{
	case MODE_REP_START:
	  actions_->Start();
		state_ = states_->GetState();
		action_ = qobj_->GetAction(state_);
		actions_->Move(action_);
		ROS_INFO("Starting rep: %d", cnt_rep_);
		mode_ = MODE_REP;
		cnt_timesteps_++;
		break;
	case MODE_REP:
    state_p_ = (int)states_->GetState();

		if (learn_)
		{
			reward_ = states_->GetReward();
			qobj_->Update(reward_, state_, state_p_, action_);
			ROS_INFO("Action: %d, produced state: %d with reward: %f", action_, state_p_, reward_);
			ROS_INFO("Table: \n%s", qobj_->PrintTable().c_str());
		}

		state_ = state_p_;

    action_ = qobj_->GetAction(state_);
		actions_->Move(action_);
		
		cnt_timesteps_++;

		if (getDistance() < goal_radius_ || outOfBounds())
		{
      double rand_ang, rand_orientation, start_x, start_y;

			mode_ = MODE_RETURN;
			ROS_INFO("Completed rep: %d, returning to start location", cnt_rep_); 
			actions_->Stop();
			lc_->UpdateSteps(cnt_timesteps_);
			cnt_timesteps_ = 0;

      // Calculate next position
      rand_ang = 2.0 * M_PI * (rand() / ((double)RAND_MAX + 1));
      rand_orientation = 2.0 * M_PI * (rand() / ((double)RAND_MAX + 1));
      ROS_INFO("Rand_Ang: %f, Rand orient: %f", rand_ang, rand_orientation);
      start_x = goalx_ + start_radius_ * cos(rand_ang);
      start_y = goaly_ + start_radius_ * sin(rand_ang);
      
      // Send the next start position and wait for move_stopped flag
      geometry_msgs::Pose start_msg;
      start_msg.position.x = start_x;
      start_msg.position.y = start_y;
      start_msg.orientation = tf::createQuaternionMsgFromYaw(rand_orientation);
      move_pub_.publish(start_msg);
		}
		break;
	case MODE_RETURN:
  	if (move_stopped_ == true)
		{
			move_stopped_ = false;
			mode_ = MODE_REP_START;
      qobj_->DecreaseTemp();
			cnt_rep_++;
		}

		if (cnt_rep_ > num_reps_)
			mode_ = MODE_DONE;
		break;
	case MODE_DONE:
	  lc_->ShowImage();
	  exit(1);
		break;
	}
}

double Experiment::getDistance(void)
{
  return sqrt(pow(goalx_ - odom_msg_.pose.pose.position.x, 2) + 
              pow(goaly_ - odom_msg_.pose.pose.position.y, 2));
}

bool Experiment::outOfBounds(void)
{
  double x = odom_msg_.pose.pose.position.x, y = odom_msg_.pose.pose.position.y;
  if (x < bounds_[0] || x > bounds_[2] || y > bounds_[1] || y < bounds_[3])
    return true;
  else
    return false;
}

int main(int argc, char **argv)
{
	ros::init(argc, argv, "experiment");
	ros::NodeHandle n;

	Experiment* e = new Experiment(n);
	ros::spin();

	return 0;
}
