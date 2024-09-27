#include "tracker_manager.h"

#include <iostream>
#include <utility>
#include <vector>

#include "eyedid/util/display.h"

namespace sample {

static std::vector<float> getWindowRectWithPadding(const char* window_name, int padding = 30) {
  const auto window_rect = eyedid::getWindowRect(window_name);
  return {
    static_cast<float>(window_rect.x + padding),
    static_cast<float>(window_rect.y + padding),
    static_cast<float>(window_rect.x + window_rect.width - padding),
    static_cast<float>(window_rect.y + window_rect.height - padding)};
}

void TrackerManager::OnMetrics(uint64_t timestamp,
                              const EyedidGazeData &gaze_data,
                              const EyedidFaceData &face_data,
                              const EyedidBlinkData &blink_data,
                              const EyedidUserStatusData &user_status_data) {
  this->OnGaze(timestamp,
               gaze_data.x,
               gaze_data.y,
               gaze_data.fixation_x,
               gaze_data.fixation_y,
               gaze_data.tracking_state,
               gaze_data.movement_state);
  this->OnFace(timestamp,
               face_data.score,
               face_data.left,
               face_data.top,
               face_data.right,
               face_data.bottom,
               face_data.pitch,
               face_data.yaw,
               face_data.roll,
               face_data.center_x,
               face_data.center_y,
               face_data.center_z);
  this->OnBlink(timestamp,
                blink_data.is_blink_left,
                blink_data.is_blink_right,
                blink_data.is_blink,
                blink_data.left_openness,
                blink_data.right_openness);
  this->OnAttention(user_status_data.attention_score);
  this->OnDrowsiness(timestamp, user_status_data.is_drowsy, user_status_data.drowsiness_intensity);
}

void TrackerManager::OnGaze(uint64_t timestamp,
                            float x, float y,
                            float fixation_x, float fixation_y,
                            EyedidTrackingState tracking_state,
                            EyedidEyeMovementState eye_movement_state) {
  if (tracking_state != kEyedidTrackingSuccess) {
    on_gaze_(0, 0, false);
    return;
  }

  // Convert the gaze point(in display pixels) to the pixels of the OpenCV window
  // (because top left of the window may not be [0, 0])
  // This can be omitted if the top left of the window is always [0, 0], such as fullscreen-guaranteed applications
  auto winPos = eyedid::getWindowPosition(window_name_);
  x -= static_cast<float>(winPos.x);
  y -= static_cast<float>(winPos.y);

  on_gaze_(static_cast<int>(x), static_cast<int>(y), true);
}

void TrackerManager::OnFace(uint64_t timestamp,
                            float score,
                            float left,
                            float top,
                            float right,
                            float bottom,
                            float pitch,
                            float yaw,
                            float roll,
                            float center_x,
                            float center_y,
                            float center_z) {
  std::cout << "Face Score: " << timestamp << ", " << score << '\n';
}

void TrackerManager::OnAttention(float score) {
  std::cout << "Attention: " << score << '\n';
}

void TrackerManager::OnBlink(uint64_t timestamp, bool isBlinkLeft, bool isBlinkRight, bool isBlink,
                             float leftOpenness, float rightOpenness) {
  std::cout << "Blink: " << leftOpenness << ", " << rightOpenness << ", " << isBlinkLeft <<  ", " << isBlinkRight  <<'\n';
}

void TrackerManager::OnDrowsiness(uint64_t timestamp, bool isDrowsiness, float intensity) {
  std::cout << "Drowsiness: " << isDrowsiness << '\n';
}

void TrackerManager::OnCalibrationProgress(float progress) {
  on_calib_progress_(progress);
}

void TrackerManager::OnCalibrationNextPoint(float next_point_x, float next_point_y) {
  const auto winPos = eyedid::getWindowPosition(window_name_);
  const auto x = static_cast<int>(next_point_x - static_cast<float>(winPos.x));
  const auto y = static_cast<int>(next_point_y - static_cast<float>(winPos.y));
  on_calib_next_point_(x, y);
  gaze_tracker_.startCollectSamples();
}

void TrackerManager::OnCalibrationFinish(const std::vector<float> &calib_data) {
  on_calib_finish_(calib_data);
  calibrating_.store(false, std::memory_order_release);
}

bool TrackerManager::initialize(const std::string &license_key, const EyedidTrackerOptions& options) {
  const auto code = gaze_tracker_.initialize(license_key, options);
  if (code != 0) {
    std::cerr << "Failed to authenticate (code: " << code << " )\n";
    return false;
  }

  // Set approximate distance between the face and the camera.
  gaze_tracker_.setFaceDistance(60);

  // Attach callbacks to eyedid::GazeTracker
  gaze_tracker_.setTrackingCallback(this);
  gaze_tracker_.setCalibrationCallback(this);

  return true;
}

void TrackerManager::setDefaultCameraToDisplayConverter(const eyedid::DisplayInfo &display_info) {
  gaze_tracker_.converter() = eyedid::makeDefaultCameraToDisplayConverter<float>(
    static_cast<float>(display_info.widthPx), static_cast<float>(display_info.heightPx),
    display_info.widthMm, display_info.heightMm);
}

bool TrackerManager::addFrame(std::int64_t timestamp, const cv::Mat &frame) {
  return gaze_tracker_.addFrame(timestamp, frame.data, frame.cols, frame.rows);
}

void TrackerManager::startFullWindowCalibration(EyedidCalibrationPointNum target_num, EyedidCalibrationAccuracy accuracy) {
  bool expected = false;
  if (!calibrating_.compare_exchange_strong(expected, true))
    return;

  on_calib_start_();

  // Delay start to show description message
  delayed_calibration_ = std::async(std::launch::async, [=]() {
    // TODO(Tony): Handle force exit
    std::this_thread::sleep_for(std::chrono::seconds(3));
    const auto window_rect = getWindowRectWithPadding(window_name_.c_str());
    gaze_tracker_.startCalibration(target_num, accuracy,
                                   window_rect[0], window_rect[1], window_rect[2], window_rect[3]);
  });
}

void TrackerManager::setWholeScreenToAttentionRegion(const eyedid::DisplayInfo& display_info) {
  gaze_tracker_.setAttentionRegion(0, 0,
                             static_cast<float>(display_info.widthPx), static_cast<float>(display_info.heightPx));
}

} // namespace sample
