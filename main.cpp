#include <iostream>
#include <stdint.h>
#include <thread>
#include "rtc_base/thread.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "main_thread.hpp"
#include "rtc_base/logging.h"


// #include <utility>
// #include <stdexcept>
// #include <cmath>
// #include <iostream>

// /**
//  * @brief Determines the target resolution based on input resolution and desired quality
//  *
//  * @param inputWidth Input video width (must be positive)
//  * @param inputHeight Input video height (must be positive)
//  * @param targetQuality Target quality (must be 360, 720, or 1080)
//  * @return std::pair<int, int> Target resolution (width, height)
//  * @throw std::invalid_argument if input parameters are invalid
//  */
// std::pair<int, int> transformResolution(int inputWidth, int inputHeight, int targetQuality) {
//     // Input validation
//     if (inputWidth <= 0 || inputHeight <= 0) {
//         throw std::invalid_argument("Input width and height must be positive");
//     }

//     if (targetQuality != 360 && targetQuality != 720 && targetQuality != 1080) {
//         throw std::invalid_argument("Target quality must be 360, 720, or 1080");
//     }

//     // Define target resolutions
//     const std::pair<int, int> RES_360P = {640, 360};
//     const std::pair<int, int> RES_720P = {1280, 720};
//     const std::pair<int, int> RES_1080P = {1920, 1080};

//     // Define resolution thresholds
//     const int LOW_RES_WIDTH = 640;
//     const int LOW_RES_HEIGHT = 480;
//     const int HD_WIDTH = 1280;
//     const int HD_HEIGHT = 720;
//     const int FHD_WIDTH = 1920;
//     const int FHD_HEIGHT = 1080;

//     // For 360p (640x360)
//     if (targetQuality == 360) {
//         // If input resolution is less than or equal to 640x480
//         if (inputHeight <= LOW_RES_HEIGHT) {
//             return RES_360P;  // Scale up to 640x360
//         }
//         // For any other resolution
//         return RES_360P;  // Scale down to 640x360
//     }
//     // For 720p (1280x720)
//     else if (targetQuality == 720) {
//         // If input resolution is less than or equal to 640x480
//         if (inputHeight <= LOW_RES_HEIGHT) {
//             return RES_360P;  // Scale up to 640x360
//         }
//         // If input resolution is between 640x480 and 1280x720
//         else if (inputHeight <= HD_HEIGHT && inputHeight > LOW_RES_HEIGHT) {
//             return RES_720P;  // Scale up to 1280x720
//         }
//         // For any other resolution
//         return RES_720P;  // Scale down to 1280x720
//     }
//     // For 1080p (1920x1080)
//     else if (targetQuality == 1080) {
//         // If input resolution is less than or equal to 640x480
//         if (inputHeight <= LOW_RES_HEIGHT) {
//             return RES_360P;  // Scale up to 640x360
//         }
//         // If input resolution is between 640x480 and 1280x720
//         else if (inputHeight <= HD_HEIGHT && inputHeight > LOW_RES_HEIGHT) {
//             return RES_720P;  // Scale up to 1280x720
//         }
//         // If input resolution is between 1280x720 and 1920x1080
//         else if (inputHeight <= FHD_HEIGHT && inputHeight > HD_HEIGHT) {
//             return RES_1080P;  // Scale up to 1920x1080
//         }
//         // For any other resolution
//         return RES_1080P;  // Scale down to 1920x1080
//     }

//     // This line should never be reached due to input validation
//     return {inputWidth, inputHeight};
// }

// // Example usage:
// int main() {
//     try {
//         // Test cases for various resolutions
//         const std::pair<int, int> resolutions[] = {
//             {3840, 2160},
//             {1920, 1080},   // 1080p
//             {1536,  864},   // 864p
//             {1440,  810},   // 810p
//             {1280,  720},   // 720p
//             {1152,  648},   // 648p
//             {1096,  616},   // 616p
//             {1024,  576},
//             {960,   540},   // 540p
//             {852,   480},   // 480p
//             {728,   410},
//             {768,   432},   // 432p
//             {696,   392},   // 392p
//             {640,   480},   // 360p
//             {640,   360},   // 360p
//             {568,   320},
//             {512,   288},
//             {464,   260},
//             {424,   240}
//         };

//         std::cout << "Testing 360p target quality:\n";
//         for (const auto& res : resolutions) {
//             auto result = transformResolution(res.first, res.second, 360);
//             std::cout << res.first << "x" << res.second << " -> "
//                       << result.first << "x" << result.second << "\n";
//         }

//         std::cout << "\nTesting 720p target quality:\n";
//         for (const auto& res : resolutions) {
//             auto result = transformResolution(res.first, res.second, 720);
//             std::cout << res.first << "x" << res.second << " -> "
//                       << result.first << "x" << result.second << "\n";
//         }

//         std::cout << "\nTesting 1080p target quality:\n";
//         for (const auto& res : resolutions) {
//             auto result = transformResolution(res.first, res.second, 1080);
//             std::cout << res.first << "x" << res.second << " -> "
//                       << result.first << "x" << result.second << "\n";
//         }

//     } catch (const std::invalid_argument& e) {
//         std::cerr << "Error: " << e.what() << std::endl;
//         return 1;
//     }

//     return 0;
// }





// 一个简单的消息处理类
class MessageHandler {
public:
    void OnMessage(const std::string& message) {
        std::cout << "Received message on thread " << std::this_thread::get_id() << ": " << message << std::endl;
    }

    // 添加一个同步方法
    int CalculateOnMainThread(int value) {
        std::cout << "Calculating on thread: " << std::this_thread::get_id() << std::endl;
        // 模拟一些计算
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return value * 2;
    }
};

int main() {
    try {
        std::cout << "Main thread ID: " << std::this_thread::get_id() << std::endl;

        // 初始化主线程
        if (!MainThread::Instance()->Initialize()) {
            RTC_LOG(LS_ERROR) << "Failed to initialize main thread";
            return 1;
        }

        // 创建消息处理器
        MessageHandler handler;

        // 创建一个工作线程，用于发送消息到主线程
        std::thread worker([&handler]() {
            std::cout << "Worker thread started: " << std::this_thread::get_id() << std::endl;
            
            // 发送一些消息到主线程
            for (int i = 0; i < 5; ++i) {
                std::cout << "Posting task " << i << std::endl;
                
                // 使用 PostTask 异步发送消息
                MainThread::Instance()->PostTask([&handler, i]() {
                        handler.OnMessage("Async message " + std::to_string(i));
                        std::cout << "Task " << i << " executed on thread: " << std::this_thread::get_id() << std::endl;
                    }
                );

                // 等待一小段时间
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            // 演示延迟任务
            std::cout << "Posting delayed task" << std::endl;
            MainThread::Instance()->PostDelayedTask([&handler]() {
                    handler.OnMessage("Delayed message");
                    std::cout << "Delayed task executed on thread: " << std::this_thread::get_id() << std::endl;
                }, webrtc::TimeDelta::Seconds(1)
            );

            // 演示 BlockingCall
            std::cout << "\nDemonstrating BlockingCall..." << std::endl;
            for (int i = 1; i <= 3; ++i) {
                std::cout << "Calling CalculateOnMainThread(" << i << ") from worker thread" << std::endl;
                
                int result = 0;
                MainThread::Instance()->BlockingCall([&handler, i, &result]() {
                        result = handler.CalculateOnMainThread(i);
                    }
                );
                
                std::cout << "Got result from main thread: " << result << std::endl;
            }

            std::cout << "Worker thread finished posting all tasks" << std::endl;
        });

        // 将工作线程设置为分离状态
        worker.detach();

        std::cout << "Starting message loop..." << std::endl;
        
        MainThread::Instance()->Loop(100);

        return 0;
    } catch (const std::exception& e) {
        RTC_LOG(LS_ERROR) << "Error in main: " << e.what();
        return 1;
    }
}
