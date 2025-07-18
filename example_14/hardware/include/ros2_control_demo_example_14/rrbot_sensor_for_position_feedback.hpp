// Copyright (c) 2021, Stogl Robotics Consulting UG (haftungsbeschränkt)
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

//
// Authors: Denis Stogl
//

#ifndef ROS2_CONTROL_DEMO_EXAMPLE_14__RRBOT_SENSOR_FOR_POSITION_FEEDBACK_HPP_
#define ROS2_CONTROL_DEMO_EXAMPLE_14__RRBOT_SENSOR_FOR_POSITION_FEEDBACK_HPP_

#include <netinet/in.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "hardware_interface/handle.hpp"
#include "hardware_interface/hardware_info.hpp"
#include "hardware_interface/sensor_interface.hpp"
#include "hardware_interface/system_interface.hpp"
#include "hardware_interface/types/hardware_interface_return_values.hpp"
#include "rclcpp/clock.hpp"
#include "rclcpp/macros.hpp"
#include "rclcpp/time.hpp"

namespace ros2_control_demo_example_14
{
class RRBotSensorPositionFeedback : public hardware_interface::SensorInterface
{
public:
  RCLCPP_SHARED_PTR_DEFINITIONS(RRBotSensorPositionFeedback)

  hardware_interface::CallbackReturn on_init(
    const hardware_interface::HardwareInfo & info) override;

  hardware_interface::CallbackReturn on_configure(
    const rclcpp_lifecycle::State & previous_state) override;

  hardware_interface::CallbackReturn on_activate(
    const rclcpp_lifecycle::State & previous_state) override;

  hardware_interface::CallbackReturn on_deactivate(
    const rclcpp_lifecycle::State & previous_state) override;

  hardware_interface::CallbackReturn on_cleanup(
    const rclcpp_lifecycle::State & previous_state) override;

  hardware_interface::CallbackReturn on_shutdown(
    const rclcpp_lifecycle::State & previous_state) override;

  hardware_interface::return_type read(
    const rclcpp::Time & time, const rclcpp::Duration & period) override;

private:
  // Parameters for the RRBot simulation
  double hw_start_sec_;
  double hw_stop_sec_;
  double hw_slowdown_;

  // Store the command for the simulated robot
  double measured_velocity_;  // Local variable, but avoid initialization on each read
  double last_measured_velocity_;

  // Timestamps to calculate position for velocity
  rclcpp::Clock clock_;
  rclcpp::Time last_timestamp_;
  rclcpp::Time current_timestamp;  // Local variable, but avoid initialization on each read

  // Sync incoming commands between threads
  std::atomic<double> rt_incoming_data_;
  std::atomic<bool> receive_data_;

  // Create timer to checking incoming data on socket
  std::thread incoming_data_thread_;
  std::mutex mtx;
  std::condition_variable cv;

  // Fake "mechanical connection" between actuator and sensor using sockets
  struct sockaddr_in address_;
  uint16_t socket_port_;
  int address_length_;
  int obj_socket_;
  int sockoptval_ = 1;
  int sock_;
};

}  // namespace ros2_control_demo_example_14

#endif  // ROS2_CONTROL_DEMO_EXAMPLE_14__RRBOT_SENSOR_FOR_POSITION_FEEDBACK_HPP_
