#include "crow_all.h"
#include <opencv2/opencv.hpp>
#include <fstream>
#include <sstream>
#include <cmath>
#include <vector>
#include <numeric>
#include <regex>

double luminance_to_brightness(double Y) {
    return 413.435 * pow((0.002745 * Y + 0.0189623), 2.22);
}

std::vector<std::pair<double, std::string>> detect_harmful_flashes(const std::vector<std::tuple<double, int, double>>& events, double fps, double area_threshold, int flash_intensity_threshold, int flash_frequency_threshold) {
    std::vector<std::pair<double, std::string>> dangerous_sections;
    for (size_t i = 0; i < events.size() - 1; ++i) {
        std::vector<std::tuple<double, int, double>> window_events(events.begin() + i, events.begin() + i + 2);
        int window_frames = std::accumulate(window_events.begin(), window_events.end(), 0, [](int sum, const std::tuple<double, int, double>& event) { return sum + std::get<1>(event); });
        double window_time = window_frames / fps;
        
        if (window_time < 1 && std::all_of(window_events.begin(), window_events.end(), [&](const std::tuple<double, int, double>& event) { return std::get<0>(event) >= flash_intensity_threshold; })) {
            double flash_frequency = 1 / window_time;
            if (flash_frequency > flash_frequency_threshold && std::any_of(window_events.begin(), window_events.end(), [](const std::tuple<double, int, double>& event) { return std::get<2>(event) < 160; })) {
                dangerous_sections.emplace_back(std::get<1>(events[i]) / fps, "Harmful flash detected: " + std::to_string(flash_frequency) + " Hz");
            }
        }
    }
    return dangerous_sections;
}

std::vector<std::pair<double, std::string>> analyze_video(const std::string& video_path, double area_threshold=0.35, int flash_intensity_threshold=10, int flash_frequency_threshold=3, double interval=0.04) {
    cv::VideoCapture cap(video_path);
    double fps = cap.get(cv::CAP_PROP_FPS);
    int frame_interval = std::max(1, static_cast<int>(fps * interval));
    
    int frame_count = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
    std::vector<std::tuple<double, int, double>> events;
    double prev_brightness = -1;
    double acc_brightness_diff = 0;
    int frames_since_last_extreme = 0;
    
    for (int frame_number = 0; frame_number < frame_count; frame_number += frame_interval) {
        cap.set(cv::CAP_PROP_POS_FRAMES, frame_number);
        cv::Mat frame;
        cap.read(frame);
        
        cv::Mat gray;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        double brightness = luminance_to_brightness(cv::mean(gray)[0]);
        
        if (prev_brightness != -1) {
            double brightness_diff = brightness - prev_brightness;
            if (std::signbit(brightness_diff) != std::signbit(acc_brightness_diff) && acc_brightness_diff != 0) {
                events.emplace_back(std::abs(acc_brightness_diff), frames_since_last_extreme, std::min(brightness, prev_brightness));
                acc_brightness_diff = 0;
                frames_since_last_extreme = 0;
            } else {
                acc_brightness_diff += brightness_diff;
                frames_since_last_extreme += frame_interval;
            }
        }
        
        prev_brightness = brightness;
    }
    
    cap.release();
    return detect_harmful_flashes(events, fps, area_threshold, flash_intensity_threshold, flash_frequency_threshold);
}

std::vector<std::pair<double, std::string>> detect_saturated_red(const std::string& video_path, int red_threshold=200, int other_threshold=90, double area_threshold=0.25) {
    cv::VideoCapture cap(video_path);
    std::vector<std::pair<double, std::string>> dangerous_sections;
    int frame_number = 0;
    
    while (cap.isOpened()) {
        cv::Mat frame;
        if (!cap.read(frame)) break;
        
        cv::Mat ycbcr;
        cv::cvtColor(frame, ycbcr, cv::COLOR_BGR2YCrCb);
        std::vector<cv::Mat> channels;
        cv::split(ycbcr, channels);
        
        cv::Mat red_mask = (channels[0] >= 66) & (channels[0] <= 104) & (channels[2] >= 72) & (channels[2] <= 110) & (channels[1] >= 218) & (channels[1] <= 255);
        double red_area = static_cast<double>(cv::countNonZero(red_mask)) / (frame.rows * frame.cols);
        
        if (red_area >= area_threshold) {
            dangerous_sections.emplace_back(frame_number / cap.get(cv::CAP_PROP_FPS), "Saturated red transition");
        }
        
        frame_number++;
    }
    
    cap.release();
    return dangerous_sections;
}

std::string categorize_risk(int num_violations) {
    if (num_violations < 50) return "Low";
    else if (num_violations < 150) return "Medium";
    else return "High";
}

std::string parse_multipart(const std::string& body, const std::string& boundary, const std::string& field_name, std::string& filename) {
    std::string pattern = "--" + boundary + "\\r\\nContent-Disposition: form-data; name=\"" + field_name + "\"; filename=\"([^\"]+)\"\\r\\nContent-Type: ([^\\r\\n]+)\\r\\n\\r\\n(.*)\\r\\n--" + boundary + "--";
    std::regex re(pattern, std::regex::extended);
    std::smatch match;
    if (std::regex_search(body, match, re)) {
        filename = match[1].str();
        std::string content = match[3].str();
        std::ofstream ofs("./" + filename, std::ios::binary);
        ofs.write(content.data(), content.size());
        ofs.close();
        return content;
    }
    return "";
}

bool save_video(const std::string &file_content, const std::string &file_path) {
    std::ofstream ofs(file_path, std::ios::binary);
    if (ofs.is_open()) {
        ofs.write(file_content.c_str(), file_content.size());
        ofs.close();
        return true;
    }
    return false;
}

int main() {
    crow::SimpleApp app;

    CROW_ROUTE(app, "/analyze_video").methods("POST"_method)
    ([](const crow::request& req) {
        std::string content_type = req.get_header_value("Content-Type");
        std::string boundary = content_type.substr(content_type.find("boundary=") + 9);
        std::string video_filename;
        std::string file_content = parse_multipart(req.body, boundary, "file", video_filename);

        if (video_filename.empty()) {
            return crow::response(400, "File not found in request");
        }
        
        auto dangerous_flashes = analyze_video("./" + video_filename);
        auto dangerous_reds = detect_saturated_red("./" + video_filename);
        
        auto dangerous_sections = dangerous_flashes;
        dangerous_sections.insert(dangerous_sections.end(), dangerous_reds.begin(), dangerous_reds.end());
        
        int num_violations = dangerous_sections.size();
        std::string risk_level = categorize_risk(num_violations);
        
        std::stringstream result_html;
        result_html << "<html><head><title>Analysis Result</title><style>"
                    << ".bar { width: 100%; background-color: #ddd; }"
                    << ".low { width: 33.3%; height: 30px; background-color: green; }"
                    << ".medium { width: 33.3%; height: 30px; background-color: yellow; }"
                    << ".high { width: 33.3%; height: 30px; background-color: red; }"
                    << "</style></head><body><h1>Analysis Result</h1>"
                    << "<p>Number of Violations: " << num_violations << "</p>"
                    << "<div class=\"bar\"><div class=\"" << risk_level << "\"></div></div>"
                    << "<p>Risk Level: " << risk_level << "</p>"
                    << "<a href=\"/\">Upload Another Video</a></body></html>";
        
        return crow::response(result_html.str());
    });

    CROW_ROUTE(app, "/").methods("GET"_method)
    ([]() {
        crow::json::wvalue content;
        content["message"] = "Hello, World!";
        content["status"] = "success";
        content["version"] = "1.0.0";
        return crow::response(content);
    });

    CROW_ROUTE(app, "/upload-video").methods("POST"_method)
    ([](const crow::request& req) {

        crow::multipart::message msg(req);

        crow::multipart::part Part = msg.get_part_by_name("file");
        std::string file_path = "./uploaded_video.mp4";
        save_video(Part.body, file_path);
        auto dangerous_flashes = analyze_video(file_path);
        auto dangerous_reds = detect_saturated_red(file_path);
        
        auto dangerous_sections = dangerous_flashes;
        dangerous_sections.insert(dangerous_sections.end(), dangerous_reds.begin(), dangerous_reds.end());
        
        int num_violations = dangerous_sections.size();
        std::string risk_level = categorize_risk(num_violations);
        
        crow::json::wvalue result;
        result["Number of Violations"] = num_violations;
        result["Risk Level"] = risk_level;
        
        return crow::response(result);
    });

    app.port(8002).multithreaded().run();
}
