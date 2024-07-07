#include <opencv2/opencv.hpp>
#include "crow.h"
#include <fstream>
#include <vector>
#include <tuple>
#include <cmath>
#include <sstream>

double luminance_to_brightness(double Y)
{
    return 413.435 * std::pow(0.002745 * Y + 0.0189623, 2.22);
}

std::vector<std::tuple<double, double, double>> analyze_video(const std::string &video_path, double area_threshold = 0.35,
                                                              double flash_intensity_threshold = 10, double flash_frequency_threshold = 3, double interval = 0.04)
{

    cv::VideoCapture cap(video_path);
    if (!cap.isOpened())
    {
        std::cerr << "Error: Could not open video file " << video_path << std::endl;
        return {};
    }

    double fps = cap.get(cv::CAP_PROP_FPS);
    int frame_interval = std::max(1, static_cast<int>(fps * interval));
    int frame_count = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));

    std::vector<std::tuple<double, double, double>> events;
    double prev_brightness = -1;
    double acc_brightness_diff = 0;
    int frames_since_last_extreme = 0;

    for (int frame_number = 0; frame_number < frame_count; frame_number += frame_interval)
    {
        cap.set(cv::CAP_PROP_POS_FRAMES, frame_number);
        cv::Mat frame;
        cap.read(frame);
        if (frame.empty())
            break;

        cv::Mat gray;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        double brightness = luminance_to_brightness(cv::mean(gray)[0]);

        if (prev_brightness >= 0)
        {
            double brightness_diff = brightness - prev_brightness;
            if ((brightness_diff * acc_brightness_diff <= 0) && (acc_brightness_diff != 0))
            {
                events.emplace_back(std::abs(acc_brightness_diff), frames_since_last_extreme, std::min(brightness, prev_brightness));
                acc_brightness_diff = 0;
                frames_since_last_extreme = 0;
            }
            else
            {
                acc_brightness_diff += brightness_diff;
                frames_since_last_extreme += frame_interval;
            }
        }
        prev_brightness = brightness;
    }
    cap.release();
    return events;
}

std::vector<std::pair<double, std::string>> detect_harmful_flashes(const std::vector<std::tuple<double, double, double>> &events,
                                                                   double fps, double area_threshold, double flash_intensity_threshold, double flash_frequency_threshold)
{

    std::vector<std::pair<double, std::string>> dangerous_sections;

    for (size_t i = 0; i < events.size() - 1; ++i)
    {
        auto window_events = std::vector<std::tuple<double, double, double>>(events.begin() + i, events.begin() + i + 2);
        int window_frames = 0;
        for (const auto &event : window_events)
        {
            window_frames += std::get<1>(event);
        }
        double window_time = window_frames / fps;

        if (window_time < 1)
        {
            bool all_above_intensity = true;
            for (const auto &event : window_events)
            {
                if (std::get<0>(event) < flash_intensity_threshold)
                {
                    all_above_intensity = false;
                    break;
                }
            }
            if (all_above_intensity)
            {
                double flash_frequency = 1 / window_time;
                for (const auto &event : window_events)
                {
                    if (flash_frequency > flash_frequency_threshold && std::get<2>(event) < 160)
                    {
                        dangerous_sections.emplace_back(std::get<1>(events[i]) / fps, "Harmful flash detected: " + std::to_string(flash_frequency) + " Hz");
                        break;
                    }
                }
            }
        }
    }
    return dangerous_sections;
}

std::vector<std::pair<double, std::string>> detect_saturated_red(const std::string &video_path, int red_threshold = 200,
                                                                 int other_threshold = 90, double area_threshold = 0.25)
{

    cv::VideoCapture cap(video_path);
    if (!cap.isOpened())
    {
        std::cerr << "Error: Could not open video file " << video_path << std::endl;
        return {};
    }

    std::vector<std::pair<double, std::string>> dangerous_sections;
    int frame_number = 0;

    while (cap.isOpened())
    {
        cv::Mat frame;
        cap.read(frame);
        if (frame.empty())
            break;

        cv::Mat ycbcr;
        cv::cvtColor(frame, ycbcr, cv::COLOR_BGR2YCrCb);
        std::vector<cv::Mat> channels;
        cv::split(ycbcr, channels);
        cv::Mat red_mask = (channels[0] >= 66) & (channels[0] <= 104) &
                           (channels[2] >= 72) & (channels[2] <= 110) &
                           (channels[1] >= 218) & (channels[1] <= 255);
        double red_area = cv::countNonZero(red_mask) / static_cast<double>(frame.total());

        if (red_area >= area_threshold)
        {
            dangerous_sections.emplace_back(frame_number / cap.get(cv::CAP_PROP_FPS), "Saturated red transition");
        }
        ++frame_number;
    }
    cap.release();
    return dangerous_sections;
}

std::string categorize_risk(int num_violations)
{
    if (num_violations < 50)
        return "Low";
    else if (num_violations < 150)
        return "Medium";
    else
        return "High";
}

int main()
{

        auto dangerous_flashes = analyze_video(file_name);
        auto dangerous_reds = detect_saturated_red(file_name);
        auto dangerous_sections = detect_harmful_flashes(dangerous_flashes, 30.0, 0.35, 10, 3); // Assuming fps = 30 for simplicity

        dangerous_sections.insert(dangerous_sections.end(), dangerous_reds.begin(), dangerous_reds.end());
        int num_violations = dangerous_sections.size();
        std::string risk_level = categorize_risk(num_violations);

    return 0;
}
