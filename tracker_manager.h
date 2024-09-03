#ifndef EYEDID_CPP_SAMPLE_TRACKER_MANAGER_H_
#define EYEDID_CPP_SAMPLE_TRACKER_MANAGER_H_

#include <atomic>
#include <future>
#include <memory>
#include <string>
#include <vector>

#include "eyedid/gaze_tracker.h"
#include "eyedid/util/display.h"

#include "opencv2/opencv.hpp"

#include "simple_signal.h"

namespace sample {

class TrackerManager :
  public eyedid::ITrackingCallback,
  public eyedid::ICalibrationCallback {
 public:
  TrackerManager() = default;

  bool initialize(const std::string &license_key, const EyedidTrackerOptions& options);

  void setDefaultCameraToDisplayConverter(const eyedid::DisplayInfo& display_info);

  bool addFrame(std::int64_t timestamp, const cv::Mat& frame);

  void startFullWindowCalibration(EyedidCalibrationPointNum target_num, EyedidCalibrationAccuracy accuracy);

  void setWholeScreenToAttentionRegion(const eyedid::DisplayInfo& display_info);

  // message senders
  signal<void(int, int, bool)> on_gaze_;
  signal<void(float)> on_calib_progress_;
  signal<void(int, int)> on_calib_next_point_;
  signal<void()> on_calib_start_;
  signal<void(const std::vector<float>&)> on_calib_finish_;

  std::string window_name_;

 private:
  void OnMetrics(uint64_t timestamp, const EyedidGazeData& gaze_data, const EyedidFaceData& face_data,
                const EyedidBlinkData& blink_data, const EyedidUserStatusData& user_status_data) override;
  void OnGaze(uint64_t timestamp, float x, float y, float fixation_x, float fixation_y,
              EyedidTrackingState tracking_state, EyedidEyeMovementState eye_movement_state);
  void OnFace(uint64_t timestamp, float score, float left, float top, float right, float bottom,
              float pitch, float yaw, float roll, float center_x, float center_y, float center_z);
  void OnAttention(float score);
  void OnBlink(uint64_t timestamp, bool isBlinkLeft, bool isBlinkRight, bool isBlink,
               float leftOpenness, float rightOpenness);

  void OnDrowsiness(uint64_t timestamp, bool isDrowsiness, float intensity);

  void OnCalibrationProgress(float progress) override;
  void OnCalibrationNextPoint(float next_point_x, float next_point_y) override;
  void OnCalibrationFinish(const std::vector<float> &calib_data) override;

  eyedid::GazeTracker gaze_tracker_;
  std::future<void> delayed_calibration_;
  std::atomic_bool calibrating_{false};
};

} // namespace sample

#endif // EYEDID_CPP_SAMPLE_TRACKER_MANAGER_H_
