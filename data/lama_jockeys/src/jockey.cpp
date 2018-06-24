/* Base class for jockeys
 *
 */

#include <lama_jockeys/jockey.h>

namespace lama_jockeys
{

Jockey::Jockey(const std::string& name) :
  private_nh_("~"),
  jockey_name_(name)
{
  map_agent_ = nh_.serviceClient<lama_interfaces::ActOnMap>("/lama_map_agent");
  map_agent_.waitForExistence();
}

void Jockey::initAction()
{
  start_time_ = ros::Time::now();
  interrupted_ = false;
  interruption_time_ = ros::Time(0);
  resume_time_ = ros::Time(0);
  interruptions_duration_ = ros::Duration(0);
}

void Jockey::interrupt()
{
  if (!interrupted_)
  {
    interrupted_ = true;
    interruption_time_ = ros::Time::now();
  }
}

void Jockey::resume()
{
  if (interrupted_)
  {
    interrupted_ = false;
    resume_time_ = ros::Time::now();
    interruptions_duration_ += resume_time_ - interruption_time_;
  }
}

void Jockey::onInterrupt()
{
  ROS_DEBUG("Action interrupted");
}

void Jockey::onContinue()
{
  ROS_DEBUG("Action resumed");
}

ros::Duration Jockey::getCompletionDuration() const
{
  return ros::Time::now() - start_time_ - interruptions_duration_;
}

} // namespace lama_jockeys

