#include <laser_odometry_core/laser_odometry_core.h>

#include <laser_odometry_core/laser_odometry_utils.h>

#include <boost/assign/list_of.hpp>

namespace laser_odometry
{

tf::Transform LaserOdometryBase::getEstimatedPose() const noexcept
{
  return world_origin_to_base_;
}

void LaserOdometryBase::reset()
{
  /// @note to be implemented in the derived class.
}

bool LaserOdometryBase::configure()
{
  private_nh_.param("laser_frame", laser_frame_, std::string("base_laser_link"));
  private_nh_.param("base_frame",  base_frame_,  std::string("base_link"));
  private_nh_.param("world_frame", world_frame_, std::string("world"));
  private_nh_.param("laser_odom_frame", laser_odom_frame_, std::string("odom"));

  // Init all tf to I
  base_to_laser_     = tf::Transform::getIdentity();
  laser_to_base_     = tf::Transform::getIdentity();
  relative_tf_       = tf::Transform::getIdentity();
  world_origin_      = tf::Transform::getIdentity();
  world_to_base_     = tf::Transform::getIdentity();
  guess_relative_tf_ = tf::Transform::getIdentity();

  // Default covariance diag :
  std::vector<double> default_covariance;
  private_nh_.param("covariance_diag", default_covariance, default_covariance);

  if (default_covariance.size() == 6)
    default_covariance_ = default_covariance;
  else
  {
    ROS_WARN_STREAM("Retrieved " << default_covariance_.size()
                    << " covariance coeff. Should be 6. Setting default.");

    default_covariance_.resize(6);
    std::fill_n(default_covariance_.begin(), 6, 1e-9);
  }

  // Configure derived class
  configured_ = configureImpl();

  return configured_;
}

tf::Transform& LaserOdometryBase::getOrigin()
{
  return world_origin_;
}

const tf::Transform& LaserOdometryBase::getOrigin() const
{
  return world_origin_;
}

void LaserOdometryBase::setOrigin(const tf::Transform& origin)
{
  world_origin_ = origin;
}

tf::Transform& LaserOdometryBase::getInitialGuess()
{
  return guess_relative_tf_;
}

const tf::Transform& LaserOdometryBase::getInitialGuess() const
{
  return guess_relative_tf_;
}

void LaserOdometryBase::setInitialGuess(const tf::Transform& guess)
{
  guess_relative_tf_ = guess;
}

tf::Transform& LaserOdometryBase::getLaserPose()
{
  return base_to_laser_;
}

const tf::Transform& LaserOdometryBase::getLaserPose() const
{
  return base_to_laser_;
}

void LaserOdometryBase::setLaserPose(const tf::Transform& base_to_laser)
{
  base_to_laser_ = base_to_laser;
  laser_to_base_ = base_to_laser.inverse();
}

bool LaserOdometryBase::process(const sensor_msgs::LaserScanPtr scan_ptr,
                                nav_msgs::OdometryPtr odom_ptr,
                                nav_msgs::OdometryPtr /*relative_odom_ptr*/)
{
  //if (scan_ptr == nullptr || pose_ptr == nullptr) return false;
  assert(scan_ptr != nullptr);
  assert(odom_ptr != nullptr);

  geometry_msgs::Pose2DPtr pose_2d_ptr = boost::make_shared<geometry_msgs::Pose2D>();

  bool processed = process(scan_ptr, pose_2d_ptr);

  odom_ptr->header.stamp = scan_ptr->header.stamp;
  odom_ptr->header.frame_id = laser_odom_frame_;

  odom_ptr->pose.pose.position.x  = pose_2d_ptr->x;
  odom_ptr->pose.pose.position.y  = pose_2d_ptr->y;
  odom_ptr->pose.pose.position.z  = 0;
  odom_ptr->pose.pose.orientation = tf::createQuaternionMsgFromYaw(pose_2d_ptr->theta);

  fillCovariance(odom_ptr->pose.covariance);

  return processed;
}

bool LaserOdometryBase::configured() const noexcept
{
  return configured_;
}

tf::Transform LaserOdometryBase::predict(const tf::Transform& /*tf*/)
{
  return tf::Transform::getIdentity();
}

void LaserOdometryBase::fillOdomMsg(const sensor_msgs::LaserScanPtr current_scan_ptr,
                                    nav_msgs::OdometryPtr odom_ptr)
{
  odom_ptr->header.stamp    = current_scan_ptr->header.stamp;
  odom_ptr->header.frame_id = laser_odom_frame_;

  odom_ptr->pose.pose.position.x = world_origin_to_base_.getOrigin().getX();
  odom_ptr->pose.pose.position.y = world_origin_to_base_.getOrigin().getY();
  odom_ptr->pose.pose.position.z = 0;

  tf::quaternionTFToMsg(world_origin_to_base_.getRotation(),
                        odom_ptr->pose.pose.orientation);
}

void LaserOdometryBase::fillPose2DMsg(geometry_msgs::Pose2DPtr pose_ptr)
{
  pose_ptr->x = world_origin_to_base_.getOrigin().getX();
  pose_ptr->y = world_origin_to_base_.getOrigin().getY();
  pose_ptr->theta = tf::getYaw(world_origin_to_base_.getRotation());
}

void LaserOdometryBase::fillCovariance(Covariance& covariance)
{
  covariance = boost::assign::list_of
               (static_cast<double>(default_covariance_[0]))  (0)  (0)  (0)  (0) (0)
               (0)  (static_cast<double>(default_covariance_[1]))  (0)  (0)  (0) (0)
               (0)  (0)  (static_cast<double>(default_covariance_[2]))  (0)  (0) (0)
               (0)  (0)  (0)  (static_cast<double>(default_covariance_[3]))  (0) (0)
               (0)  (0)  (0)  (0)  (static_cast<double>(default_covariance_[4])) (0)
               (0)  (0)  (0)  (0)  (0)  (static_cast<double>(default_covariance_[5]));
}

std::string LaserOdometryBase::getFrameBase()  const noexcept
{
  return base_frame_;
}
std::string LaserOdometryBase::getFrameLaser() const noexcept
{
  return laser_frame_;
}
std::string LaserOdometryBase::getFrameWorld() const noexcept
{
  return world_frame_;
}
std::string LaserOdometryBase::getFrameOdom()  const noexcept
{
  return laser_odom_frame_;
}
void LaserOdometryBase::setFrameBase(const std::string& frame)
{
  base_frame_ = frame;
}
void LaserOdometryBase::setFrameLaser(const std::string& frame)
{
  laser_frame_ = frame;
}
void LaserOdometryBase::setFrameWorld(const std::string& frame)
{
  world_frame_ = frame;
}
void LaserOdometryBase::setFrameOdom(const std::string& frame)
{
  laser_odom_frame_ = frame;
}
ros::Time LaserOdometryBase::getCurrentTime() const noexcept
{
  return current_time_;
}

} /* namespace laser_odometry */
