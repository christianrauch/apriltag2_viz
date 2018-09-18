// ros
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/compressed_image.hpp>
#include <apriltag_msgs/msg/april_tag_detection.hpp>
#include <apriltag_msgs/msg/april_tag_detection_array.hpp>

#include <opencv/cv.hpp>
#include <opencv/highgui.h>

class AprilVizNode : public rclcpp::Node {
public:
    AprilVizNode() : Node("apriltag_viz", "apriltag") {
        sub_img = this->create_subscription<sensor_msgs::msg::CompressedImage>(
            "image/compressed",
            std::bind(&AprilVizNode::onImage, this, std::placeholders::_1),
            rmw_qos_profile_sensor_data);
        sub_tag = this->create_subscription<apriltag_msgs::msg::AprilTagDetectionArray>(
            "detections",
            std::bind(&AprilVizNode::onTags, this, std::placeholders::_1));

        get_parameter_or<std::string>("overlay_mode", overlay_mode, "axes");
    }

private:
    rclcpp::Subscription<sensor_msgs::msg::CompressedImage>::SharedPtr sub_img;
    rclcpp::Subscription<apriltag_msgs::msg::AprilTagDetectionArray>::SharedPtr sub_tag;

    cv::Mat img;
    cv::Mat merged;
    cv::Mat overlay;
    std::string overlay_mode;

    static const std::array<cv::Scalar, 4> colours;

    static std::array<double, 2> project(const std::array<double,9> H, // homography matrix
                                         const std::array<double,2> pc) // point in camera
    {
        std::array<double,2> pi;    // point in image
        const auto z = H[3*2+0] * pc[0] + H[3*2+1] * pc[1] + H[3*2+2];
        for(uint i(0); i<2; i++) {
            pi[i] = (H[3*i+0] * pc[0] + H[3*i+1] * pc[1] + H[3*i+2]) / z;
        }
        return pi;
    }

    void onImage(const sensor_msgs::msg::CompressedImage::SharedPtr msg_img) {
        img = cv::imdecode(cv::Mat(msg_img->data), CV_LOAD_IMAGE_COLOR);

        if(overlay.empty()) {
            merged = img;
        }
        else {
            // blend overlay and image
            double alpha;
            get_parameter_or("alpha", alpha, 0.5);
            cv::addWeighted(img, 1, overlay, alpha, 0, merged, -1);
        }

        cv::imshow("tag", merged);
        cv::waitKey(1);
    }

    void onTags(const apriltag_msgs::msg::AprilTagDetectionArray::SharedPtr msg_tag) {
        if(img.empty())
            return;

        // overlay with transparent background
        overlay = cv::Mat(img.size(), CV_8UC3, cv::Scalar(0,0,0,0));

        for(const auto& d : msg_tag->detections) {
            if(overlay_mode=="axes") {
                // axes
                const auto c = project(d.homography, {{0,0}});
                const auto x = project(d.homography, {{1,0}});
                const auto y = project(d.homography, {{0,1}});
                cv::line(overlay, cv::Point2d(c[0], c[1]), cv::Point2d(x[0],x[1]), cv::Scalar(0,0,255,255), 3);
                cv::line(overlay, cv::Point2d(c[0], c[1]), cv::Point2d(y[0],y[1]), cv::Scalar(0,255,0,255), 3);
            }
            else if(overlay_mode=="tri") {
                // triangle patches
                std::array<cv::Point,3> points;
                points[0].x = d.centre.x;
                points[0].y = d.centre.y;

                for(uint i(0); i<4; i++) {
                    points[1].x = d.corners[i%4].x;
                    points[1].y = d.corners[i%4].y;
                    points[2].x = d.corners[(i+1)%4].x;
                    points[2].y = d.corners[(i+1)%4].y;

                    cv::fillConvexPoly(overlay, points.data(), 3, colours[i]);
                }
            }
            else {
                throw std::runtime_error("unknown overlay mode");
            }

            for(uint i(0); i<4; i++) {
                cv::circle(overlay, cv::Point(d.corners[i].x, d.corners[i].y), 5, colours[i], 2);
            }
        }
    }
};

const std::array<cv::Scalar, 4> AprilVizNode::colours = {{
    cv::Scalar(0,0,255,255),    // red
    cv::Scalar(0,255,0,255),    // green
    cv::Scalar(255,0,0,255),    // blue
    cv::Scalar(0,255,255,255)   // yellow
}};

int main(int argc, char **argv) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<AprilVizNode>());
    rclcpp::shutdown();
    return 0;
}
