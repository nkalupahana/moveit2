/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2014 Fetch Robotics Inc.
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
 *   * Neither the name of Fetch Robotics nor the names of its
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

#include <moveit/collision_plugin_loader/collision_plugin_loader.h>
#include <pluginlib/class_loader.hpp>
#include <memory>

static const std::string LOGNAME = "collision_detection";
namespace collision_detection
{
static const rclcpp::Logger LOGGER = rclcpp::get_logger("collision_plugin_loader");

class CollisionPluginLoader::CollisionPluginLoaderImpl
{
public:
  CollisionPluginLoaderImpl()
  {
    try
    {
      loader_ = std::make_shared<pluginlib::ClassLoader<CollisionPlugin>>("moveit_core",
                                                                          "collision_detection::CollisionPlugin");
    }
    catch (pluginlib::PluginlibException& e)
    {
      RCLCPP_ERROR(LOGGER, "Unable to construct colllision plugin loader. Error: %s", e.what());
    }
  }

  CollisionPluginPtr load(const std::string& name)
  {
    CollisionPluginPtr plugin;
    try
    {
      plugin = loader_->createUniqueInstance(name);
      plugins_[name] = plugin;
    }
    catch (pluginlib::PluginlibException& ex)
    {
      RCLCPP_ERROR(LOGGER, "Exception while loading %s : %s", name.c_str(), ex.what());
    }
    return plugin;
  }

  bool activate(const std::string& name, const planning_scene::PlanningScenePtr& scene)
  {
    std::map<std::string, CollisionPluginPtr>::iterator it = plugins_.find(name);
    if (it == plugins_.end())
    {
      CollisionPluginPtr plugin = load(name);
      if (plugin)
      {
        return plugin->initialize(scene);
      }
      return false;
    }
    if (it->second)
    {
      return it->second->initialize(scene);
    }
    return false;
  }

private:
  std::shared_ptr<pluginlib::ClassLoader<CollisionPlugin>> loader_;
  std::map<std::string, CollisionPluginPtr> plugins_;
};

CollisionPluginLoader::CollisionPluginLoader()
{
  loader_ = std::make_shared<CollisionPluginLoaderImpl>();
}

CollisionPluginLoader::~CollisionPluginLoader() = default;

bool CollisionPluginLoader::activate(const std::string& name, const planning_scene::PlanningScenePtr& scene)
{
  return loader_->activate(name, scene);
}

void CollisionPluginLoader::setupScene(const rclcpp::Node::SharedPtr& node,
                                       const planning_scene::PlanningScenePtr& scene)
{
  if (!scene)
  {
    RCLCPP_WARN(LOGGER, "Cannot setup scene, PlanningScenePtr is null.");
    return;
  }

  std::string param_name;
  std::string collision_detector_name;

  if (node->has_parameter("collision_detector"))
  {
    node->get_parameter("collision_detector", collision_detector_name);
  }
  else if (node->has_parameter(("/move_group/collision_detector")))
  {
    // Check for existence in move_group namespace
    // mainly for rviz plugins to get same collision detector.
    node->get_parameter("/move_group/collision_detector", collision_detector_name);
  }
  else
  {
    return;
  }

  if (collision_detector_name.empty())
  {
    // This is not a valid name for a collision detector plugin
    return;
  }

  activate(collision_detector_name, scene);
  RCLCPP_INFO(LOGGER, "Using collision detector: %s", scene->getCollisionDetectorName().c_str());
}

}  // namespace collision_detection
