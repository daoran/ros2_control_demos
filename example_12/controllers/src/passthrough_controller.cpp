// Copyright (c) 2023, PAL Robotics
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "passthrough_controller/passthrough_controller.hpp"

#include <algorithm>

#include "controller_interface/helpers.hpp"
#include "pluginlib/class_list_macros.hpp"

namespace
{  // utility

// called from RT control loop
void reset_controller_reference_msg(passthrough_controller::DataType & msg)
{
  for (auto & data : msg.data)
  {
    data = std::numeric_limits<double>::quiet_NaN();
  }
}

}  // namespace

namespace passthrough_controller
{

controller_interface::CallbackReturn PassthroughController::on_init()
{
  try
  {
    param_listener_ = std::make_shared<ParamListener>(get_node());
    params_ = param_listener_->get_params();
  }
  catch (const std::exception & e)
  {
    fprintf(stderr, "Exception thrown during init stage with message: %s \n", e.what());
    return controller_interface::CallbackReturn::ERROR;
  }
  return controller_interface::CallbackReturn::SUCCESS;
}

controller_interface::InterfaceConfiguration
PassthroughController::command_interface_configuration() const
{
  controller_interface::InterfaceConfiguration command_interfaces_config;
  command_interfaces_config.type = controller_interface::interface_configuration_type::INDIVIDUAL;
  command_interfaces_config.names = command_interface_names_;

  return command_interfaces_config;
}

controller_interface::InterfaceConfiguration PassthroughController::state_interface_configuration()
  const
{
  return controller_interface::InterfaceConfiguration{
    controller_interface::interface_configuration_type::NONE};
}

controller_interface::CallbackReturn PassthroughController::on_configure(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  params_ = param_listener_->get_params();
  command_interface_names_ = params_.interfaces;

  joints_cmd_sub_ = this->get_node()->create_subscription<DataType>(
    "~/commands", rclcpp::SystemDefaultsQoS(),
    [this](const DataType::SharedPtr msg)
    {
      // check if message is correct size, if not ignore
      if (msg->data.size() == command_interface_names_.size())
      {
        rt_buffer_.set(*msg);
      }
      else
      {
        RCLCPP_ERROR(
          this->get_node()->get_logger(), "Invalid command received of %zu size, expected %zu size",
          msg->data.size(), command_interface_names_.size());
      }
    });

  // pre-reserve command interfaces
  command_interfaces_.reserve(command_interface_names_.size());

  RCLCPP_INFO(this->get_node()->get_logger(), "configure successful");

  // The names should be in the same order as for command interfaces for easier matching
  reference_interface_names_ = command_interface_names_;
  // for any case make reference interfaces size of command interfaces
  reference_interfaces_.resize(
    reference_interface_names_.size(), std::numeric_limits<double>::quiet_NaN());

  return controller_interface::CallbackReturn::SUCCESS;
}

controller_interface::CallbackReturn PassthroughController::on_activate(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  // reset command buffer if a command came through callback when controller was inactive
  reset_controller_reference_msg(command_);
  rt_buffer_.try_set(command_);

  RCLCPP_INFO(this->get_node()->get_logger(), "activate successful");

  std::fill(
    reference_interfaces_.begin(), reference_interfaces_.end(),
    std::numeric_limits<double>::quiet_NaN());

  return controller_interface::CallbackReturn::SUCCESS;
}

controller_interface::CallbackReturn PassthroughController::on_deactivate(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  // Try to set default value in command.
  reset_controller_reference_msg(command_);
  rt_buffer_.try_set(command_);

  return controller_interface::CallbackReturn::SUCCESS;
}

bool PassthroughController::on_set_chained_mode(bool /*chained_mode*/) { return true; }

controller_interface::return_type PassthroughController::update_and_write_commands(
  const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
  for (size_t i = 0; i < command_interfaces_.size(); ++i)
  {
    if (!std::isnan(reference_interfaces_[i]))
    {
      command_interfaces_[i].set_value(reference_interfaces_[i]);
    }
  }

  return controller_interface::return_type::OK;
}

std::vector<hardware_interface::CommandInterface>
PassthroughController::on_export_reference_interfaces()
{
  std::vector<hardware_interface::CommandInterface> reference_interfaces;

  for (size_t i = 0; i < reference_interface_names_.size(); ++i)
  {
    reference_interfaces.push_back(
      hardware_interface::CommandInterface(
        get_node()->get_name(), reference_interface_names_[i], &reference_interfaces_[i]));
  }

  return reference_interfaces;
}

controller_interface::return_type PassthroughController::update_reference_from_subscribers(
  const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
  auto command_op = rt_buffer_.try_get();
  if (command_op.has_value())
  {
    command_ = command_op.value();
  }
  // message is valid
  if (
    !command_.data.empty() && std::all_of(
                                command_.data.cbegin(), command_.data.cend(),
                                [](const auto & value) { return std::isfinite(value); }))
  {
    if (reference_interfaces_.size() != command_.data.size())
    {
      RCLCPP_ERROR_THROTTLE(
        get_node()->get_logger(), *(get_node()->get_clock()), 1000,
        "command size (%zu) does not match number of reference interfaces (%zu)",
        command_.data.size(), reference_interfaces_.size());
      return controller_interface::return_type::ERROR;
    }
    reference_interfaces_ = command_.data;
  }

  return controller_interface::return_type::OK;
}

}  // namespace passthrough_controller

PLUGINLIB_EXPORT_CLASS(
  passthrough_controller::PassthroughController, controller_interface::ChainableControllerInterface)
