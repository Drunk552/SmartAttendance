#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/face.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <thread>// 多线程支持
#include <atomic>// 原子变量支持
#include <mutex>// 互斥锁支持
#include <chrono>//time.h>
#include <map>// 字典支持
#include <queue>// 队列支持
#include <condition_variable>// 条件变量支持
#include "face_demo.h" // 人脸识别演示模块的头文件
#include "lvgl.h" // 嵌入式图形库头文件（预留接口，当前未使用）
#include "db_storage.h"//数据层头文件
#include "attendance_rule.h"
#include "event_bus.h"// 事件总线头文件
#include "../ui/ui_controller.h"

using namespace cv;
using namespace cv::face;
using std::cout; using std::endl;

// ==========================================
//全局静态变量 原本在 main 中的局部变量)
// ==========================================

static VideoCapture cap;// 视频捕获对象，用于读取摄像头或视频流
static CascadeClassifier face_cas;// Haar级联分类器，用于人脸检测
static Ptr<LBPHFaceRecognizer> recog; // LBPH人脸识别器对象
static std::vector<Mat> face_samples;// 存储所有人脸训练样本（预处理后的灰度图像）
static std::vector<int> labels;// 对应的标签ID（与face_samples一一对应）
static std::vector<std::string> names = {"Unknown"}; // ID 0 为 Unknown，其余从数据库加载
static int current_id = 0;// 当前选中的用户ID（用于采集样本）
static bool trained = false;// 标识是否已完成训练
static bool show_recognition = true;// 控制是否显示识别结果
static std::mutex g_names_mutex;// 保护 names 变量的互斥锁

// 1. 定义打卡任务包
struct PunchTask {
    int user_id;
    int shift_id;
    cv::Mat snapshot; // 抓拍照片
    int status;       // 考勤状态
    std::string user_name; // 仅用于日志打印
    int minutes_diff;      // 仅用于日志打印
};
// 2. 队列与同步原语
static std::queue<PunchTask> g_punch_queue;      // 任务队列
static std::mutex g_queue_mutex;                 // 保护队列的锁
static std::condition_variable g_queue_cv;       // 信号量
static std::atomic<bool> g_db_writer_running{false}; // 写库线程运行标志
static std::thread g_db_writer_thread;           // 写库线程对象

// ============== [新增：考勤配置全局缓存] ==============
static RuleConfig g_rule_cfg;              // 全局规则配置
static std::vector<ShiftInfo> g_shifts;    // 全局班次列表
static bool g_is_config_loaded = false;    // 是否已加载标志位

static Mat current_frame;// [Eoic4新增] 用于在函数间共享最新一帧画面
// [Epic 4.4 新增] 保护 current_frame 的互斥锁
static std::mutex g_data_mutex;
// [Epic 3.3] 用户列表缓存，用于给 C 语言 UI 提供数据
static std::vector<UserData> g_user_cache;
// [Epic 3.4] 考勤记录缓存
static std::vector<AttendanceRecord> g_record_cache;
static PreprocessConfig preprocess_config; // 全局预处理配置

static cv::Mat g_display_frame_buffer; // 专门给 UI 显示用的帧缓存
static std::mutex g_display_mutex;     // 保护 g_display_frame_buffer 的锁
static std::atomic<bool> g_is_running{false}; // 线程运行标志
static std::thread g_worker_thread;    // 后台采集线程对象

const std::string MODEL_FILE = "face_model.xml"; // 模型文件名

/**
 * @brief 应用直方图均衡化
 * @param img 输入灰度图像
 * @param method 直方图均衡化方法（0-无, 1-全局, 2-CLAHE）
 * @return 处理后的图像
 * @note 根据选择的方法应用不同的均衡化技术
 */
static cv::Mat apply_histogram_equalization(const cv::Mat& img, int method) {
    cv::Mat result;

    if (method == HIST_EQ_GLOBAL) {
        cv::equalizeHist(img, result);
    } 
    
    else if (method == HIST_EQ_GLOBAL) {
        cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();// 创建CLAHE对象
        clahe->setClipLimit(preprocess_config.clahe_clip_limit);// 设置对比度限制
        clahe->setTilesGridSize(preprocess_config.clahe_tile_grid_size);// 设置网格大小
        clahe->apply(img, result);// 应用CLAHE均衡化
    } 
    
    else {
        result = img.clone();// 无均衡化，直接克隆原图
    }
    return result;// 返回处理后的图像
}

/**
 * @brief 增强人脸ROI的对比度和亮度
 * @param img 输入图像
 * @param config 预处理配置
 * @return 增强后的图像
 * @note 通过调整对比度和亮度来增强人脸区域
 */
static cv::Mat enhance_roi(const cv::Mat& img,const PreprocessConfig& config) {
    if(!config.enable_roi_enhance){
        return img.clone();
        }// 未启用增强，返回原图

    cv::Mat enhanced;
    img.convertTo(enhanced, -1, config.roi_contrast, config.roi_brightness);// 调整对比度和亮度

    cv::threshold(enhanced, enhanced, 255, 255, cv::THRESH_TRUNC);// 限制像素值上限为255
    cv::threshold(enhanced, enhanced, 0, 0, cv::THRESH_TOZERO);// 限制像素值在0-255范围内

    return enhanced;// 返回增强后的图像
}

/**
 * @brief 完整的人脸预处理流程
 * @param input_face 输入原始图像
 * @param face_roi 人脸区域矩形
 * @param config 预处理配置
 * @return 预处理后的人脸图像
 * @note 包括裁剪边界、尺寸归一化、直方图均衡化和ROI增强
 */

cv::Mat preprocess_face_complete(const cv::Mat& input_face, const cv::Rect& face_roi, const PreprocessConfig& config) {
    cv::Mat gray, crop = input_face(face_roi).clone();
    cvtColor(crop, gray, COLOR_BGR2GRAY);


    if (config.enable_crop) {
        const int w = crop.cols, h = crop.rows;
        const int mx = std::max(0, w * config.crop_margin_percent / 100);
        const int my = std::max(0, h * config.crop_margin_percent / 100);
        const int x = mx, y = my;
        const int ww = std::max(1, w - 2 * mx);
        const int hh = std::max(1, h - 2 * my);
        Rect tight = (Rect(x, y, ww, hh) & Rect(0, 0, w, h));
        gray = gray(tight);
    }// 裁剪边界

    if (config.enable_resize_eq) {
        if (config.enablez_resize) {
            resize(gray, gray, config.resize_size);// 尺寸归一化
        }

        gray = apply_histogram_equalization(gray, config.hist_eq_method);// 直方图均衡化
    }

    gray = enhance_roi(gray, config);//增强ROI对比度和亮度

    return gray;
}

/**
 * @brief 查找Haar级联分类器XML文件的路径
 * @return 找到的文件路径，如果找不到则返回空字符串
 * @note 在多个预设路径中查找人脸检测器文件
 */

static std::string find_cascade() {
    namespace fs = std::filesystem;
    std::vector<std::string> paths = {
        "./haarcascade_frontalface_default.xml",// 当前目录
        "/usr/share/opencv4/haarcascades/haarcascade_frontalface_default.xml",// OpenCV4路径
        "/usr/share/opencv/haarcascades/haarcascade_frontalface_default.xml",//OpenCV路径
    };
    for (auto &p : paths) if (fs::exists(p)) return p;
    return "";
}

/**
 * @brief 在图像中检测人脸
 * @param current_frame 输入图像
 * @param face 输出参数，检测到的人脸矩形区域
 * @param cas 人脸检测器（Haar级联分类器）
 * @return true-检测到人脸，false-未检测到人脸
 * @note 检测最大的人脸区域，适合单人脸场景
 */

static bool detect_face(const Mat& current_frame, Rect& face, CascadeClassifier& cas) {
    std::vector<Rect> faces;// 存储所有检测到的人脸
    Mat gray; cvtColor(current_frame, gray, COLOR_BGR2GRAY);// 转为灰度图（Haar检测需要灰度图）
    equalizeHist(gray, gray);// 直方图均衡化，增强对比度
    cas.detectMultiScale(gray, faces, 1.1, 3, 0, Size(80,80));// 检测人脸，参数：1.1-缩放因子，3-最小邻居数，0-标志，Size(80,80)-最小人脸尺寸
    if (faces.empty()) return false;// 未检测到人脸
    face = *std::max_element(faces.begin(), faces.end(),
    [](const Rect& a, const Rect& b){return a.area()<b.area();}); // 选择面积最大的人脸（假设场景中只有一个人）
    return true;
}

/**
 * @brief 预处理人脸图像
 * @param current_frame 原始图像
 * @param roi 人脸区域矩形
 * @return 预处理后的人脸图像（128x128灰度图，直方图均衡化）
 * @note 包括裁剪边界、尺寸归一化、直方图均衡化
 */

static Mat preprocess_face(const Mat& current_frame, const Rect& roi) {
    return preprocess_face_complete(current_frame, roi, preprocess_config);// 使用全局预处理配置
}


/**
 * @brief 使用硬编码参数打开 SDP 视频流
 * @param unused 未使用的文件路径参数
 * @return 打开的 VideoCapture 对象
 * @note 忽略传入的路径，直接使用预设的 GStreamer 管道参数
 */
static VideoCapture open_sdp_stream(const std::string& /*unused*/) {
    // 我们忽略传入的文件路径，直接使用更稳定的硬编码参数
    // 这串参数完全对应你生成的 sdp 文件内容
    std::string pipe = 
        "udpsrc port=5004 timeout=2000000000 ! "
        "application/x-rtp, media=(string)video, clock-rate=(int)90000, "
        "encoding-name=(string)RAW, sampling=(string)YCbCr-4:2:2, "
        "depth=(string)8, width=(string)640, height=(string)480, "
        "colorimetry=(string)BT601-5, payload=(int)96 ! "
        "rtpjitterbuffer latency=0 ! "
        "rtpvrawdepay ! videoconvert ! "
        "video/x-raw,format=BGR ! "
        "appsink sync=false drop=true max-buffers=1";

    std::cout << "[Stream] 使用硬编码管道连接..." << std::endl;
    VideoCapture cap(pipe, cv::CAP_GSTREAMER);
    return cap;
}

/**
 * @brief 数据库写入专用线程 (消费者)
 * @note 从队列取任务并串行写入，彻底解决 SQLite 多线程竞争问题
 */
static void attendance_writer_thread() {
    while (g_db_writer_running) {
        std::unique_lock<std::mutex> lock(g_queue_mutex);
        
        // 等待条件：队列不为空 OR 收到停止信号
        g_queue_cv.wait(lock, []{ 
            return !g_punch_queue.empty() || !g_db_writer_running; 
        });

        // 如果停止了且队列处理完了，就退出
        if (!g_db_writer_running && g_punch_queue.empty()) break;

        // 取出一个任务
        if (!g_punch_queue.empty()) {
            PunchTask task = g_punch_queue.front();
            g_punch_queue.pop();
            
            // 关键：取完数据立刻解锁，让生产者(识别线程)能继续塞数据，不用等我写完库
            lock.unlock(); 

            // 添加 try-catch 防止因单次写入失败导致整个程序崩溃退出
            try {
                bool logged = db_log_attendance(task.user_id, task.shift_id, task.snapshot, task.status);
                if(logged) {
                    std::cout << "[Async] Save OK -> User: " << task.user_name 
                               << " | Status: " << task.status 
                               << " | Diff: " << task.minutes_diff << "m" << std::endl;
                }else {
                    std::cerr << "[Async] Save Failed -> User: " << task.user_name << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "[Error] DB Write Exception: " << e.what() << std::endl;
                // 捕获错误，不要让线程退出！
            } catch (...) {
                std::cerr << "[Error] DB Write Unknown Error!" << std::endl;
            }

        }
    }
}

/**
 * @brief 后台采集与处理线程函数
 * @note 持续采集视频帧，进行人脸检测与识别，并更新全局显示缓存
 */
static void background_capture_loop() {
    // === 优化参数配置 ===
    const int SKIP_FRAMES = 4;           // 每 5 帧才做一次人脸检测 (0, 1, 2, 3, 4)
    const int RECOG_COOLDOWN_MS = 2000;  // 识别冷却时间 2000ms (2秒)

    // === 状态变量 ===
    int frame_counter = 0;               // 帧计数器
    cv::Rect last_face_rect;             // 上一次检测到的人脸区域
    bool is_tracking = false;            // 当前是否处于“跟踪”状态
    
    // 识别冷却时间控制
    std::map<int, std::chrono::steady_clock::time_point> user_cooldowns; 

    // 业务防抖缓存 (防止重复写入数据库)
    // Key: UserID, Value: 上次打卡的时间戳 (秒)
    std::map<int, time_t> last_punch_cache;

    auto last_ui_update_time = std::chrono::steady_clock::now();// 上次UI更新的时间点
    
    int fail_consecutive_count = 0;// 连续失败计数器

    while (g_is_running) {
        try {
            // 1. 检查连接状态 (SDP 重连逻辑)
            if (!cap.isOpened()) {
                static int retry_cnt = 0;
                if (++retry_cnt % 10 == 0) { // 每2秒重试
                    std::cout << "[Stream] 尝试重连 SDP..." << std::endl;
                    cap.release();
                    cap = open_sdp_stream("/tmp/yuyv.sdp");
                    if (cap.isOpened()) fail_consecutive_count = 0; // 重置计数
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                continue;
            }

            cv::Mat frame;
            //  尝试读取 (非阻塞尝试)
            bool success = cap.read(frame);
            
            if (!success || frame.empty()) {
                fail_consecutive_count++;
                
                // 如果连续 60 帧 (约2秒) 都读不到数据，说明流断了
                // 必须强制释放 cap，否则它可能会一直卡死
                if (fail_consecutive_count > 60) {
                    std::cerr << "[Stream] 严重错误：流已中断，强制重启连接！" << std::endl;
                    cap.release(); // 强制关闭，触发上面的 !isOpened 重连逻辑
                    fail_consecutive_count = 0;
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }// 锁在这里立即释放，UI 线程随时可以拿 current_frame 去渲染！

            // 2. 核心业务处理 (缩小锁范围，防止阻塞 UI)
            cv::Mat process_frame;
            {
                std::lock_guard<std::mutex> lock(g_data_mutex);
                frame.copyTo(current_frame);// 更新共享的当前帧 
                process_frame = current_frame.clone(); // 克隆一份交给本线程进行耗时计算
            }

            // -------------------- [ 跳帧检测] ----------------
            bool perform_detection = (frame_counter % (SKIP_FRAMES + 1) == 0);
            frame_counter++;

            bool has_face = false;
            cv::Rect face;

            if (perform_detection) {
                // 执行真正的耗时检测
                has_face = detect_face(process_frame, face, face_cas);
                    
                if (has_face) {
                    last_face_rect = face; // 更新缓存
                    is_tracking = true;
                } else {
                    is_tracking = false;   // 丢失目标
                }
            } else {
                // 跳帧期间：直接沿用上一帧的结果 (假定人脸移动不快)
                // 只有当之前处于跟踪状态时，才认为有脸
                if (is_tracking) {
                    face = last_face_rect;
                    has_face = true;
                }
            }
            // ------------------------------------------------------

            if (has_face) {
                // 绘制人脸框 (视觉反馈)
                // 如果是跳帧期间画的框，可以用不同颜色(例如黄色)来区分调试，或者统一用绿色
                cv::Scalar color = perform_detection ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 255, 200);
                cv::rectangle(process_frame, face, color, 2);

                // -------------------- [ 识别冷却] ----------------
                if (perform_detection && show_recognition && trained) {
                        
                    cv::Mat f = preprocess_face(process_frame, face);
                    int label = -1; double conf = 0.0;
                    recog->predict(f, label, conf);
                        
                    if (label != -1 && conf < 100.0) { // 阈值
                            
                        std::string name;
                        {
                        // 加锁读取，防止崩坏
                        std::lock_guard<std::mutex> lock(g_names_mutex);
                        name = (label < names.size()) ? names[label] : "Unknown";
                        }
                            
                        //  先定义 text 变量，用于后续追加状态
                        std::string text = name; 
                            
                        // 检查该用户的独立冷却时间
                        auto now = std::chrono::steady_clock::now();
                        bool in_cooldown = false;
                            
                        if (user_cooldowns.find(label) != user_cooldowns.end()) {
                            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - user_cooldowns[label]).count();
                            if (elapsed < RECOG_COOLDOWN_MS) {
                                in_cooldown = true;
                            }
                        }

                        // 只有不在冷却中，才执行打卡逻辑
                        if (!in_cooldown) {
                                
                            // =====================  1. 业务防抖检查 =================
                            // 获取当前系统时间 (秒)
                            time_t now_sec = std::time(nullptr);
                            time_t last_p = 0;

                            // 先查内存缓存 (最快)
                            if (last_punch_cache.find(label) != last_punch_cache.end()) {
                                last_p = last_punch_cache[label];
                            } 
                            // 内存没有查数据库 (兜底)
                            else {
                                // 确保 db_storage.h 中声明了此函数，如果没有请添加
                                last_p = db_getLastPunchTime(label);
                                last_punch_cache[label] = last_p;
                            }

                            // 判断时间间隔 (例如 60秒 内禁止重复打卡)
                            if (now_sec - last_p < 60) {
                                // --- 情况 A: 重复打卡 ---
                                // 仅在界面显示提示，不执行任何写库操作
                                text += " [Repeat]"; 
                            }
                            else {
                                // --- 情况 B: 有效打卡  ---
                                    
                                // ===== 考勤规则计算 =====
                                    
                                // 1. 构建班次配置
                                ShiftConfig default_shift;
                                default_shift.start_time = "09:00";
                                default_shift.end_time = "18:00";
                                default_shift.late_threshold_min = g_rule_cfg.late_threshold; 
                                    
                                // 2. 计算状态
                                PunchResult result = AttendanceRule::calculatePunchStatus(now_sec, default_shift, true); 
                                    
                                // 3. 准备数据
                                cv::Mat snapshot = process_frame.clone();
                                int uid = label;
                                int sid = 0; 
                                int sts = 0; 
                                if (result.status == PunchStatus::LATE) sts = 1;
                                else if (result.status == PunchStatus::EARLY) sts = 2;
                                else if (result.status == PunchStatus::ABSENT) sts = 4;
                                    
                                int diff = result.minutes_diff;
                                std::string user_n = name;

                                // 4. 异步队列推送
                                {
                                    std::lock_guard<std::mutex> lock(g_queue_mutex);
                                    // 控制队列长度，防止内存占用过高
                                    if (g_punch_queue.size() > 10) { 
                                        std::cerr << "[Warn] DB Queue Full! Drop." << std::endl;
                                    } else {
                                        g_punch_queue.push({uid, sid, snapshot, sts, user_n, diff});
                                        g_queue_cv.notify_one(); 
                                    }
                                }

                                // 5. UI 反馈
                                text += " [OK]";

                                //  打卡成功后，立即更新内存缓存
                                last_punch_cache[label] = now_sec;
                            }

                            // 更新视觉冷却时间 (保证提示语显示 2秒)
                            user_cooldowns[label] = now;
                        }
                        else {
                            // 可选：显示冷却中，比如 text += " (Wait)";
                        }

                        // 统一在最后绘制文字，这样才能显示出 "User [OK]"
                        cv::putText(process_frame, text, cv::Point(face.x, face.y - 10), 
                                    cv::FONT_HERSHEY_SIMPLEX, 0.9, cv::Scalar(0, 255, 0), 2);
                    }
                }
                // ------------------------------------------------------
            }

            // 耗时计算和画框全部结束后，短暂加锁将画面同步回共享区
            {
                std::lock_guard<std::mutex> lock(g_data_mutex);
                process_frame.copyTo(current_frame);
            }

            // 3. 更新 UI 显示缓存(带限流保护)
            auto now = std::chrono::steady_clock::now();
            // 计算时间差 (毫秒)
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_ui_update_time).count();

            //  只有距离上次刷新超过 40ms (约 25 FPS) 才通知 UI
            // 这样既保证了识别是 60 FPS (高精度)，又防止了 UI 队列爆炸
            // 将限制从 40ms 改为 16ms (约 60 FPS)，让预览画面丝滑顺畅
            if (elapsed_ms >= 16) {
                {
                    std::lock_guard<std::mutex> lock(g_display_mutex);
                    current_frame.copyTo(g_display_frame_buffer);
                }

                // 发送刷新信号
                EventBus::getInstance().publish(EventType::CAMERA_FRAME_READY, nullptr);
                
                // 更新最后刷新时间
                last_ui_update_time = now;
            }

            // 4. 线程休眠与内存清理
            frame.release();// 释放当前帧内存
            // [优化] 为了达到 60FPS，理论间隔应为 16ms。
            // 但考虑到 cap.read() 本身可能是阻塞的（等待硬件数据），这里只休眠极短时间释放 CPU 即可。
            // 如果你的 CPU 占用率过高，可以改为 10ms-15ms；如果追求极致流畅，改为 1ms。
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
        //   捕获所有异常，防止程序崩溃
        catch (const cv::Exception& e) {
            std::cerr << "[Error] OpenCV Exception in capture loop: " << e.what() << std::endl;
            // 遇到 OpenCV 错误，暂停一下，避免刷屏
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        } 
        catch (const std::exception& e) {
            std::cerr << "[Error] Std Exception in capture loop: " << e.what() << std::endl;
        } 
        catch (...) {
            std::cerr << "[Error] Unknown crash in capture loop!" << std::endl;
        }
    }
}

/**
 * @brief 业务模块初始化函数
 * @return true-初始化成功，false-失败
 * @note 包括加载人脸检测器、打开视频源、初始化人脸识别器、设置预处理配置
 */

bool business_init() {
    // 订阅屏幕切换事件，控制识别状态
    auto& bus = EventBus::getInstance();
    bus.subscribe(EventType::ENTER_HOME_SCREEN, [](void*){
        show_recognition = true;
        std::cout << "[Business] 进入主页，开启人脸识别打卡" << std::endl;
    });
    
    bus.subscribe(EventType::LEAVE_HOME_SCREEN, [](void*){
        show_recognition = false;
        std::cout << "[Business] 离开主页，关闭人脸识别打卡" << std::endl;
    });

    // 加载人脸检测器（Haar级联分类器）
    std::string cascade_path = find_cascade();
    if (cascade_path.empty() || !face_cas.load(cascade_path)) {
        std::cerr << "找不到/加载失败: haarcascade_frontalface_default.xml\n";
        return false;
    }

    std::cout << ">>> [Business] 摄像头初始化已移交至后台线程，主界面立即启动。" << std::endl;

    // 初始化LBPH人脸识别器
    recog = LBPHFaceRecognizer::create(1, 8, 8, 8, 500.0);
    
    // 确保数据层已初始化 (连接数据库)
    if (!data_init()) {
        std::cerr << "[Business] 数据层初始化失败！" << std::endl;
        return false;
    }

    // 系统启动时，静默清理 30 天前的旧打卡抓拍图，释放磁盘空间
    std::cout << ">>> [Business] 正在检查磁盘空间与过期打卡抓拍图..." << std::endl;
    int cleaned_images = db_cleanup_old_attendance_images(30); // 30天
    if (cleaned_images > 0) {
        std::cout << ">>> [Business] 自动清理完毕！共删除 " << cleaned_images << " 张过期图片。" << std::endl;
    } else {
        std::cout << ">>> [Business] 磁盘状态良好，暂无过期打卡图片需清理。" << std::endl;
    }

    // 准备全局变量
    face_samples.clear();
    labels.clear();
    names.clear(); 
    names.push_back("Unknown"); // ID=0 预留

    bool model_loaded = false;

    // A. 尝试加载本地模型文件
    std::ifstream f(MODEL_FILE);
    if (f.good()) {
        f.close(); 
        try {
            std::cout << ">>> [Business] 发现本地模型 " << MODEL_FILE << "，正在快速加载..." << std::endl;
            recog->read(MODEL_FILE); 
            
            std::vector<UserData> users_info = db_get_all_users_light();
            for (const auto& u : users_info) {
                if (names.size() <= u.id) {
                    names.resize(u.id + 1, "Unknown");
                }
                names[u.id] = u.name;
            }

            model_loaded = true;
            trained = true;
            std::cout << ">>> [Business] 模型加载成功！无需重新训练。" << std::endl;

        } catch (const cv::Exception& e) {
            std::cerr << "[Business] 模型文件可能已损坏，将回退到全量训练。错误: " << e.what() << std::endl;
            model_loaded = false;
        }
    }

    // B. 如果模型加载失败，执行全量训练并保存
    if (!model_loaded) {
        std::cout << ">>> [Business] 开始执行全量训练 (读取本地头像文件)..." << std::endl;
        
        std::vector<UserData> users = db_get_all_users(); 
        
        if (!users.empty()) {
            for (const auto& u : users) {
                cv::Mat sample;
                
                // 判断路径是否为空，并用 imread 读取本地图片
                if (!u.avatar_path.empty()) {
                    sample = cv::imread(u.avatar_path, cv::IMREAD_GRAYSCALE);
                }

                // 安全校验：如果图片成功读取，才加入训练集
                if (!sample.empty()) {
                    face_samples.push_back(sample);
                    labels.push_back(u.id);
                    
                    // 维护名字映射
                    if (names.size() <= u.id) {
                        names.resize(u.id + 1, "Unknown");
                    }
                    names[u.id] = u.name;
                } else {
                    // 打印警告，避免某个人没头像导致整个训练卡死
                    std::cerr << "[Warn] 无法加载用户头像，已跳过。用户ID: " << u.id 
                              << " 路径: " << (u.avatar_path.empty() ? "空" : u.avatar_path) << std::endl;
                }
            }
            
            // 开始训练
            if (!face_samples.empty()) {
                recog->train(face_samples, labels);
                trained = true;
                std::cout << ">>> [Business] 训练完成。" << std::endl;
                
                recog->write(MODEL_FILE);
                std::cout << ">>> [Business] 新模型已保存至: " << MODEL_FILE << std::endl;
            } else {
                std::cout << ">>> [Business] 未找到有效的本地头像文件，无法完成训练。" << std::endl;
            }
        } else {
             std::cout << ">>> [Business] 数据库无用户，跳过训练。" << std::endl;
        }
    }

    // 启动采集线程 
    if (!g_is_running) {
        g_is_running = true;
        g_worker_thread = std::thread(background_capture_loop);
        std::cout << ">>> [Business] Background capture thread started." << std::endl;
    }

    // 启动数据库写入线程
    if (!g_db_writer_running) {
        g_db_writer_running = true;
        g_db_writer_thread = std::thread(attendance_writer_thread);
        std::cout << ">>> [Business] DB Writer thread started." << std::endl;
    }

    return true;
}

/**
 * @brief 将BGR图像转换为灰度图像
 * @param inputImage 输入图像（BGR或BGRA格式）
 * @return 灰度图像
 * @note Epic 3要求实现的独立函数
 */

cv::Mat convertToGrayscale(const cv::Mat& inputImage) {
    if (inputImage.empty()) {
        std::cerr << "[Business] 输入图像为空，无法转换灰度" << std::endl;
        return cv::Mat();// 返回空矩阵
    }
    
    cv::Mat gray;
    if (inputImage.channels() == 3) {
        cvtColor(inputImage, gray, cv::COLOR_BGR2GRAY);
    }
     else if (inputImage.channels() == 4) {
        cvtColor(inputImage, gray, cv::COLOR_BGRA2GRAY);
    } 
    else if (inputImage.channels() == 1) {
        inputImage.copyTo(gray);// 已经是灰度图，直接复制
    } 
    else {
        std::cerr << "[Business] 不支持的图像通道数: " << inputImage.channels() << std::endl;
        return cv::Mat();
    }
    
    return gray;
}

// ==========================================
// Epic 4: 新增控制接口 (供 UI 按钮调用)
// ==========================================
/**
 * @brief 切换当前用户 ID（供 UI 按钮或外部调用）
 * @param id 要切换到的用户 ID
 * @note 会验证范围；若越界会打印错误信息并保持当前 ID 不变
 */
void business_set_current_id(int id) {
    if (id >= 0 && id < names.size()) {
        current_id = id;
        cout << "[Business] 当前用户切换为: " << names[current_id] << endl;
    } else {
        std::cerr << "[Business] ID 超出范围" << endl;
    }
}  

/**
 * @brief 基于当前内存样本训练识别模型
 * @note 如果样本少于 2 个则不进行训练；训练成功后会将 `trained` 置为 true
 */
void business_start_training() {
    if (face_samples.size() < 2) { 
        cout << "[Business] 样本过少 (<2)，无法训练。\n"; 
    } else {
        recog->train(face_samples, labels);
        trained = true;
        cout << "[Business] 模型训练完成。\n";
    }
} 

/**
 * @brief 切换识别显示开关（开 / 关）
 * @note 若尚未训练则无法开启识别，函数会在日志中说明原因
 */
void business_toggle_recognition() {
    if (!trained) {
        cout << "[Business] 尚未训练，无法开启识别。\n";
        show_recognition = false;
    } else {
        show_recognition = !show_recognition;
        cout << "[Business] 识别功能: " << (show_recognition ? "开启" : "关闭") << endl;
    }
}

/**
 * @brief 设置人脸识别开关状态
 * @param enable true开启识别，false关闭识别
 */
void business_set_recognition_enabled(bool enable) {
    if (!trained && enable) {
        cout << "[Business] 尚未训练，无法开启识别。\n";
        show_recognition = false;
    } else {
        show_recognition = enable;
        cout << "[Business] 识别功能: " << (show_recognition ? "开启" : "关闭") << endl;
    }
}

/**
 * @brief 获取当前人脸识别开关状态
 * @return true识别已开启，false识别已关闭
 */
bool business_get_recognition_enabled(void) {
    return show_recognition;
}

// ==========================================
// Epic 4: 改造后的运行函数
// ==========================================

/**
 * @brief 业务单次运行函数（Epic 4 修改版）
 * @return cv::Mat 处理后的图像（带有人脸框和文字），用于 UI 显示
 * @note 移除了 imshow 和 waitKey，不再阻塞，不再直接处理键盘
 */
cv::Mat business_get_frame() {// 函数名建议修改，原名 business_run_once 也可以保留但返回值要改

    //  加锁，防止写入时被其他线程读取
    std::lock_guard<std::mutex> lock(g_data_mutex);
    if (current_frame.empty()) return cv::Mat();
    return current_frame.clone();

    // 1. 读取一帧
    if (!cap.read(current_frame) || current_frame.empty()) {
        return Mat(); 
    }

    // 2. 检测人脸
    Rect face;
    bool has = detect_face(current_frame, face, face_cas);
    if (has) rectangle(current_frame, face, Scalar(0,255,0), 2);

    // ================== 【新增调试日志】 ==================
    // 每 100 帧打印一次当前状态，帮助定位问题
    // 观察终端输出的 status 字段
    static int debug_counter = 0;
    if (debug_counter++ % 100 == 0) {
        std::cout << "[Debug] 状态检查 -> "
                  << "识别开关: " << (show_recognition ? "ON" : "OFF")
                  << " | 模型已训练: " << (trained ? "YES" : "NO")
                  << " | 检测到人脸: " << (has ? "YES" : "NO") 
                  << std::endl;
    }

    // 3. 识别逻辑 (如果开启)
    if (show_recognition && trained && has) {
        Mat f = preprocess_face(current_frame, face);
        int pred_label = -1; double conf = 0.0;
        recog->predict(f, pred_label, conf);

        std::string text;
        if (pred_label >= 0 && pred_label < names.size()) {
            // 识别成功 (置信度阈值可调，越低越匹配)
            // 这里将原来 80.0 改为100.0，放宽一些识别条件
            if (conf <= 100.0) {
                text = names[pred_label];
                
                // === [Phase 05 修改] 考勤核心逻辑 ===
                
                // 1. 获取当前时间
                time_t now = std::time(nullptr);
                
                // 2. [Story 1.3] 检查重复打卡 (数据库校验)
                // 只有当距离上次打卡超过 5分钟 (300秒) 才允许入库
                time_t last_punch = db_getLastPunchTime(pred_label);
                
                if (now - last_punch < 300) {
                    // --- 重复打卡分支 ---
                    text += " [Repeat]"; // 在人脸上方显示提示
                    // 可选：在这里调用 UI 弹窗接口，例如 ui_show_toast("5分钟内已打卡");
                } 
                else {
                    // --- 有效打卡分支 ---
                    
                    // 调用考勤规则引擎计算状态
                    // --- 步骤 A: 准备全局配置 (懒加载) ---
                    static RuleConfig rule_cfg;
                    static std::vector<ShiftInfo> all_shifts;
                    
                    if (!g_is_config_loaded || g_shifts.empty()) {
                        g_rule_cfg = db_get_global_rules();
                        g_shifts = db_get_shifts(); // 加载所有班次供智能匹配使用
                        g_is_config_loaded = true;
                    }

                    // --- 步骤 B: 确定目标班次 ---
                    ShiftConfig target_config;
                    bool is_check_in = true; // 判定是上班卡还是下班卡
                    int shift_id_for_log = 0; // 用于记录到数据库的班次ID

                    // [新增逻辑] 1. 优先检查用户是否绑定了“固定班次”
                    // 注意：需确保 db_storage.h 中已声明 db_get_user_shift
                    ShiftInfo user_fixed_shift = db_get_user_shift(pred_label);

                    if (user_fixed_shift.id > 0) {
                        // === 分支 1: 用户有固定排班 (如: 保安夜班) ===
                        std::cout << "[Rule] 用户 " << names[pred_label] << " 绑定班次: " << user_fixed_shift.name << std::endl;
                        
                        target_config.start_time = user_fixed_shift.s1_start; 
                        target_config.end_time   = user_fixed_shift.s1_end;
                        target_config.late_threshold_min = g_rule_cfg.late_threshold;
                        shift_id_for_log = user_fixed_shift.id;

                        // 简单的“距离判定法”决定是上班还是下班：
                        // 看当前时间离“上班点”近，还是离“下班点”近
                        int now_mins = (localtime(&now)->tm_hour * 60) + localtime(&now)->tm_min;
                        int start_mins = AttendanceRule::timeStringToMinutes(user_fixed_shift.s1_start);
                        int end_mins   = AttendanceRule::timeStringToMinutes(user_fixed_shift.s1_end);

                        // 简单的跨天处理 (如果跨天，下班时间的分钟数应+1440)
                        if (user_fixed_shift.cross_day) {
                             if (end_mins < start_mins) end_mins += 1440;
                             if (now_mins < start_mins) now_mins += 1440; // 假设打卡也是次日
                        }

                        int dist_start = std::abs(now_mins - start_mins);
                        int dist_end = std::abs(now_mins - end_mins);
                        
                        is_check_in = (dist_start <= dist_end);
                    } 
                    else {
                        // === 分支 2: 无固定排班，使用原来的“智能匹配” (AM/PM 通排) ===
                        ShiftConfig shift_am, shift_pm;
                        int id_am = 0, id_pm = 0;

                        // 默认取前两个班次作为早/晚班
                        if (g_shifts.size() >= 2) {
                            if (!g_shifts.empty()) {
                                // 上午规则：取 s1_start, s1_end
                                shift_am = {g_shifts[0].s1_start, g_shifts[0].s1_end, g_rule_cfg.late_threshold};
                                
                                // 下午规则：取 s2_start, s2_end
                                shift_pm = {g_shifts[0].s2_start, g_shifts[0].s2_end, g_rule_cfg.late_threshold};
                            }
                        } else {
                            // 兜底默认值
                            shift_am = {"09:00", "12:00", g_rule_cfg.late_threshold};
                            shift_pm = {"13:00", "18:00", g_rule_cfg.late_threshold};
                        }

                        // 使用折中原则判断归属
                        int shift_owner = AttendanceRule::determineShiftOwner(now, shift_am, shift_pm);
                        
                        if (shift_owner == 1) {
                            target_config = shift_am;
                            shift_id_for_log = id_am;
                            is_check_in = true; // 上午班 -> 算上班
                        } else {
                            target_config = shift_pm;
                            shift_id_for_log = id_pm;
                            is_check_in = false; // 下午班 -> 算下班
                        }
                    }

                    // --- 步骤 C: 计算最终状态 (迟到/早退/正常) ---
                    PunchResult result = AttendanceRule::calculatePunchStatus(now, target_config, is_check_in);
                    
                    // 4. 入库保存 (记录详细状态)
                    // 转换状态枚举到 int (0:Normal, 1:Late, 2:Early, 3:Absent)
                    int db_status = 0;
                    if (result.status == PunchStatus::LATE) db_status = 1;
                    else if (result.status == PunchStatus::EARLY) db_status = 2;
                    else if (result.status == PunchStatus::ABSENT) db_status = 4;
                    
                    // ===================== [异步打卡] =================

                    // 1. 关键：必须克隆当前帧！
                    // 因为 current_frame 是全局复用的，下一帧采集会覆盖它。
                    // 如果直接传 current_frame 给子线程，子线程保存的可能是下一帧的画面或损坏的数据。
                    cv::Mat snapshot = current_frame.clone();

                    // 2. 捕获必要的局部变量 (值拷贝)
                    // 这些变量在下一轮循环会变，所以必须拷贝一份传给 Lambda
                    int uid = pred_label;
                    int sid = shift_id_for_log;
                    int sts = db_status;
                    int diff = result.minutes_diff;
                    std::string user_n = names[pred_label]; // 用于日志

                    // 3. 推送任务到队列，而不是启动新线程
                    {
                        std::lock_guard<std::mutex> lock(g_queue_mutex);
                        
                        // [Epic 4.4 优化] 防护：如果积压超过 50 条，说明写入速度严重滞后
                        // 此时选择丢弃当前最新的打卡任务，优先保命（防止内存耗尽崩溃）
                        if (g_punch_queue.size() > 50) {
                            std::cerr << "[Warn] DB Writer Queue FULL (>50)! Dropping record for: " << user_n << std::endl;
                            
                            // 可选：你也可以在这里加一行代码，让界面显示一个红色的 "Busy" 图标提醒用户
                        } 
                        else {
                            // 队列未满，正常入队
                            g_punch_queue.push({uid, sid, snapshot, sts, user_n, diff});
                            g_queue_cv.notify_one(); // 唤醒后台写库线程
                        }
                    }

                    // 4. UI 立即反馈 (乐观更新)
                    // 不需要等数据库返回，直接告诉用户“识别成功”，体验最流畅
                    text += " [OK]";
                }
                // ======================================
            } else {
                text = "unknown"; // 置信度不够
            }
            text += " " + cv::format("%.0f", conf);
        } else {
            text = "unknown";
        }

        putText(current_frame, text, Point(face.x, std::max(0, face.y-10)),
                FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0,255,0), 2);
    }

    // 4. 在图像上绘制状态信息 (OSD)
    // 注意：在更高级的集成中，这些文字应该由 LVGL 绘制在 Label 控件上，而不是画在图里。
    // 但为了 Epic 4 的过渡，我们先保留在图上绘制。
    std::string status = "User: " + names[current_id];
    status += trained ? " [Trained]" : " [No Model]";
    status += show_recognition ? " [Recog ON]" : "";
    
    putText(current_frame, status, Point(10, 30), FONT_HERSHEY_SIMPLEX, 
            0.6, Scalar(0, 255, 255), 2);

    // 5. 返回图像给 UI 层
    return current_frame;
}

/**
 * @brief 获取用于显示的当前帧图像（RGB格式并缩放到指定大小）
 * @param buffer 输出缓冲区，需预先分配好空间 (w * h * 3 bytes)
 * @param w 目标宽度
 * @param h 目标高度
 * @return true 成功获取并填充图像；false 无可用图像
 * @note 该函数线程安全，会加锁保护读取最新帧
 */
bool business_get_display_frame(void* buffer, int w, int h) {
    cv::Mat frame;
    
    // 1. 快速取出最新的一帧 (加锁时间极短)
    {
        std::lock_guard<std::mutex> lock(g_display_mutex);
        if (g_display_frame_buffer.empty()) return false;
        // 拷贝引用或深拷贝均可，这里用深拷贝最安全
        g_display_frame_buffer.copyTo(frame);
    }

    // 2. 耗时的缩放和转换在锁外进行，不影响后台采集
    cv::Mat resized, rgb;

    // ==========================================
    // 解决 240x260 拉伸问题的裁剪逻辑
    // ==========================================
    
    // 目标是填满高度 (h>=260)，保持 4:3 比例
    // 计算等比缩放后的宽度： 260 / 480 * 640 ≈ 346
    int scaled_w = (frame.cols * h) / frame.rows; 
    // 先等比缩放 (此时图像是 346x260，不会变形)
    cv::resize(frame, frame, cv::Size(scaled_w, h));
    // 计算需要裁剪掉的左右两边宽度
    // (346 - 240) / 2 = 53
    int crop_x = (scaled_w - w) / 2;
    // 执行中心裁剪 (只保留中间的 width=240 部分)
    if (crop_x > 0) {
        // 确保裁剪区域不越界
        if (crop_x + w > scaled_w) crop_x = scaled_w - w;
        
        cv::Rect roi(crop_x, 0, w, h);
        // 使用 clone() 确保内存连续，防止显示花屏
        frame = frame(roi).clone(); 
    } else {
        // 兜底逻辑：如果计算出的宽度不够（极少情况），则强制缩放
        cv::resize(frame, frame, cv::Size(w, h));
    }
    cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);

    // 3. 填入 buffer
    memcpy(buffer, rgb.data, w * h * 3);
    
    return true;
}

/**
 * @brief 获取用户总数并刷新缓存（供 UI 列表使用）
 * @return 当前用户数量
 * @note 会调用 db_get_all_users() 刷新 g_user_cache
 */
int business_get_user_count(void) {
    // 每次进入列表页时，从数据库重新拉取一次数据
    g_user_cache = db_get_all_users();
    return (int)g_user_cache.size();
} 

/**
 * @brief 获取指定索引用户的信息（从缓存）
 * @param index 要获取的索引（0..count-1）
 * @param id_out 可选输出：写入用户 ID
 * @param name_buf 可选输出：写入用户名（需提供 len），会确保 null-terminated
 * @param len name_buf 缓冲区大小
 * @return true 成功；false 索引越界
 */
bool business_get_user_at(int index, int *id_out, char *name_buf, int len) {
    // 越界检查
    if (index < 0 || index >= (int)g_user_cache.size()) return false;
    
    const UserData& u = g_user_cache[index];
    
    if (id_out) *id_out = u.id;
    
    if (name_buf && len > 0) {
        // 安全拷贝字符串
        strncpy(name_buf, u.name.c_str(), len - 1);
        name_buf[len - 1] = '\0'; // 确保结尾符
    }
    return true;
} 

/**
 * @brief 使用当前帧注册新用户（将原图作为人脸数据保存）
 * @param name 新用户姓名
 * @return true 成功注册并写入数据库；false 失败（无帧或 DB 错误）
 * @note 此接口会调用 db_add_user(current_frame) 并刷新用户缓存
 */
bool business_register_user(const char* name, int dept_id) {

    // [Epic 4.4 新增] 加锁保护读取
    std::lock_guard<std::mutex> lock(g_data_mutex);

    // 1. 检查是否有画面
    if (current_frame.empty()) {
        std::cerr << "[Business] Error: No camera frame for registration!\n";
        return false;
    }

    std::cout << "[Business] Registering user: " << name << "...\n";

    // 2. 构造用户数据
    UserData u;
    u.name = name;
    u.role = 0;      // 默认为普通员工
    u.dept_id = dept_id;   // 默认无部门 (或设为1)
    u.default_shift_id = 0; // 暂时默认
    u.password = ""; 
    u.card_id = "";

    // 3. 调用数据层接口 (自动处理图片编码和存储)
    int new_id = db_add_user(u, current_frame);
    
    if (new_id > 0) {
        std::cout << "[Business] Registration Success! ID: " << new_id << "\n";
        
        // 注册后更新模型并保存到 XML
        // A. 图像预处理：LBPH 需要灰度图
        cv::Mat gray_frame;
        if (current_frame.channels() == 3) {
            cv::cvtColor(current_frame, gray_frame, cv::COLOR_BGR2GRAY);
        } else {
            gray_frame = current_frame.clone();
        }

        // B. 更新内存中的 ID->姓名 映射表
        // 确保 names 向量容量足够，避免越界
        std::lock_guard<std::mutex> lock(g_names_mutex);// 加锁保护 names
        if ((int)names.size() <= new_id) {
            names.resize(new_id + 1, "Unknown");
        }
        names[new_id] = name;

        // C. 更新识别模型 (增量更新)
        // 构造临时的 vector 传给 update/train 接口
        std::vector<cv::Mat> new_imgs = { gray_frame };
        std::vector<int> new_labels = { new_id };

        // 如果是系统第一个用户，必须用 train 初始化；否则用 update 追加
        if (!trained) {
            recog->train(new_imgs, new_labels);
            trained = true;
            std::cout << ">>> [Business] 模型初始化训练完成。" << std::endl;
        } else {
            recog->update(new_imgs, new_labels);
            std::cout << ">>> [Business] 模型增量更新完成。" << std::endl;
        }

        // D. 立即保存到磁盘 (Model Persistence)
        // 这样下次启动时，business_init 就能直接读取这个 xml 文件
        try {
            recog->write(MODEL_FILE);
            std::cout << ">>> [Business] 模型已成功保存至: " << MODEL_FILE << std::endl;
        } catch (const cv::Exception& e) {
            std::cerr << ">>> [Error] 模型文件保存失败: " << e.what() << std::endl;
        }

        // 刷新缓存，确保列表页能看到新用户
        business_get_user_count(); 
        return true;
    } else {
        std::cerr << "[Business] DB Add Failed!\n";
        return false;
    }
} 

/**
 * @brief 使用当前帧更新老用户的人脸
 * @param user_id 要更新的老用户 ID
 * @return true 成功；false 失败
 */
bool business_update_user_face(int user_id) {
    // 1. 加锁保护读取
    std::lock_guard<std::mutex> lock(g_data_mutex);

    // 2. 检查是否有画面
    if (current_frame.empty()) {
        std::cerr << "[Business] Error: No camera frame for updating face!\n";
        return false;
    }

    std::cout << "[Business] Updating face for user ID: " << user_id << "...\n";

    // 3. 调用数据层接口更新数据库 (调用我们上一轮加的函数)
    // 注意：确保该文件包含了 db_storage.h 并且能识别 db_update_user_face
    if (db_update_user_face(user_id, current_frame)) {
        std::cout << "[Business] DB Face Update Success! ID: " << user_id << "\n";
        
        // ========================================================
        // 核心：更新人脸识别模型 (LBPH 支持给同一个 ID 增量追加人脸)
        // ========================================================
        cv::Mat gray_frame;
        if (current_frame.channels() == 3) {
            cv::cvtColor(current_frame, gray_frame, cv::COLOR_BGR2GRAY);
        } else {
            gray_frame = current_frame.clone();
        }

        std::vector<cv::Mat> new_imgs = { gray_frame };
        std::vector<int> new_labels = { user_id };

        // 增量更新模型特征
        if (!trained) {
            recog->train(new_imgs, new_labels);
            trained = true;
        } else {
            recog->update(new_imgs, new_labels); // 追加特征
        }

        // 保存模型到磁盘
        try {
            recog->write(MODEL_FILE);
            std::cout << ">>> [Business] 模型增量更新完成，已保存至: " << MODEL_FILE << std::endl;
        } catch (const cv::Exception& e) {
            std::cerr << ">>> [Error] 模型文件保存失败: " << e.what() << std::endl;
        }

        return true;
    } else {
        std::cerr << "[Business] DB Face Update Failed!\n";
        return false;
    }
}

// ==========================================
// [Epic 3.4] 考勤记录接口实现
// ==========================================

/**
 * @brief 获取考勤记录数量并刷新缓存（查询最近所有记录并保留前 50 条）
 * @return 缓存中的记录数（最多 50）
 * @note 调用 db_get_records(0, 2100 年) 并将结果保存到 g_record_cache
 */
int business_get_record_count(void) {
    // 查询最近 100 条记录 (从 0 到 2099年)
    // 注意：db_get_records 已经在 db_storage.cpp 中按时间倒序排列了
    long long start = 0;
    long long end = 4102444800; // 2100年
    
    g_record_cache = db_get_records(start, end);
    
    // 如果记录太多，只取前 50 条显示，防止列表过长卡顿
    if (g_record_cache.size() > 50) {
        g_record_cache.resize(50);
    }
    
    return (int)g_record_cache.size();
} 

/**
 * @brief 获取指定索引的格式化考勤记录文本
 * @param index 记录索引（0..count-1）
 * @param buf 输出缓冲区
 * @param len 缓冲区大小
 * @return true 成功填充 buf；false 索引越界
 * @note 输出格式: "MM-DD HH:MM Username [Status]"
 */
bool business_get_record_at(int index, char *buf, int len) {
    if (index < 0 || index >= (int)g_record_cache.size()) return false;
    
    const AttendanceRecord& r = g_record_cache[index];
    
    // 1. 转换时间戳为 HH:MM 格式
    time_t raw = (time_t)r.timestamp;
    struct tm *info = localtime(&raw);
    char time_str[16];
    strftime(time_str, sizeof(time_str), "%m-%d %H:%M", info);
    
    // 2. 转换状态码
    const char* status_str = "OK";
    if (r.status == 1) status_str = "Late";
    else if (r.status == 2) status_str = "LeftEarly";
    else if (r.status == 3) status_str = "OT"; // Overtime
    
    // 3. 格式化输出: "12-17 09:00 Alice [OK]"
    if (buf && len > 0) {
        snprintf(buf, len, "%s %s [%s]", time_str, r.user_name.c_str(), status_str);
    }
    
    return true;
} 

/**
* @brief 设置人脸预处理配置 (供 UI 调用)
* @param config 预处理配置结构体
* note 更新全局预处理配置
 */
void business_set_preprocess_config(const PreprocessConfig* config) {
    preprocess_config = *config;
    std::cout << "[Business] 预处理配置已更新。" << std::endl;
    std::cout << "  裁剪边界: " << (config->enable_crop ? "启用" : "禁用") << std::endl;
    std::cout << "  尺寸归一化: " << (config->enable_resize_eq ? "启用" : "禁用") << std::endl;   
    std::cout << "  直方图均衡化方法: ";

    if (config->hist_eq_method == HIST_EQ_NONE) {
        std::cout << "禁用" << std::endl;
    } 

    else if (config->hist_eq_method == HIST_EQ_GLOBAL) {
        std::cout << "全局均衡化" << std::endl;
    } 

    else if (config->hist_eq_method == HIST_EQ_CLAHE) {
        std::cout << "CLAHE" << std::endl;
    } 
    
    else {
        std::cout << "未知(" << config->hist_eq_method << ")" << std::endl;
    }

    std::cout << "  ROI增强: " << (config->enable_roi_enhance ? "启用" : "禁用") << std::endl;// 输出ROI增强状态
    std::cout << "  目标尺寸: " << config->resize_size.width << "x" << config->resize_size.height << std::endl;  // 输出目标尺寸
}

/**
 * @brief 获取当前人脸预处理配置 (供 UI 调用)
 * @return 预处理配置结构体
 */
PreprocessConfig business_get_preprocess_config() {
    return preprocess_config;
}

/**
 * @brief 设置直方图均衡化选项 (供 UI 按钮调用)
 * @param enable 是否启用直方图均衡化
 * @param method 直方图均衡化方法（0-无, 1-全局, 2-CLAHE）
 * @note 动态调整直方图均衡化选项
 */
void business_set_histogram_equalization(bool enable, int method) {
    preprocess_config.enable_resize_eq = enable;
    preprocess_config.hist_eq_method = method;
    
    std::cout << "[Business] 直方图均衡化: " << (enable ? "启用" : "禁用");
    if (enable) {
        std::cout << "，方法: ";
        if (method == HIST_EQ_GLOBAL) {
            std::cout << "全局均衡化";
        } else if (method == HIST_EQ_CLAHE) {
            std::cout << "CLAHE";
        } else {
            std::cout << "未知(" << method << ")";
        }
    }
    std::cout << std::endl;
}

/**
 * @brief 设置CLAHE参数 (供 UI 按钮调用)
 * @param clip_limit CLAHE剪切限制
 * @param grid_width CLAHE网格宽度
 * @param grid_height CLAHE网格高度
 * @note 动态调整CLAHE参数
 */
void business_set_clahe_parameters(float clip_limit,int grid_width,int grid_height){
    preprocess_config.clahe_clip_limit = std::max(1.0f, clip_limit);
    preprocess_config.clahe_tile_grid_size = cv::Size(std::max(1, grid_width), std::max(1, grid_height));
    
    std::cout << "[Business] CLAHE 参数已更新。剪切限制: " << preprocess_config.clahe_clip_limit
              << ", 网格大小: " << preprocess_config.clahe_tile_grid_size.width
              << "x" << preprocess_config.clahe_tile_grid_size.height << std::endl;
}

/**
 * @brief 设置ROI增强参数 (供 UI 按钮调用)
 * @param enable 是否启用ROI增强
 * @param contrast 对比度增强因子
 * @param brightness 亮度增强偏移量
 * @note 动态调整ROI增强选项
 */
void business_set_roi_enhance(bool enable, float contrast, float brightness){
    preprocess_config.enable_roi_enhance = enable;
    preprocess_config.roi_contrast = contrast;
    preprocess_config.roi_brightness = brightness;

    std::cout << "[Business] ROI增强: " << (enable ? "启用" : "禁用")
              << ", 对比度: " << contrast
              << ", 亮度: " << brightness << std::endl;
}

/**
 * @brief 强制刷新考勤配置
 * @note 将加载标志位置为 false，下次 business_get_frame 运行时会自动从数据库重新拉取
 */
void business_reload_config() {
    // [Epic 4.4] 建议加锁 (如果你有多线程隐患)
    std::lock_guard<std::mutex> lock(g_data_mutex); 
    
    g_is_config_loaded = false;
    std::cout << ">>> [Business] 配置已过期，将在下一帧自动刷新。" << std::endl;
}

/**
 * @brief 业务退出函数
 * @note 停止采集线程和数据库写入线程
 */
void business_quit() {
    // 1. 停止采集线程
    if (g_is_running) {
        std::cout << ">>> [Business] Stopping capture thread..." << std::endl;// 停止采集线程
        g_is_running = false;
        if (g_worker_thread.joinable()) {
            g_worker_thread.join();
        }
        std::cout << ">>> [Business] Capture thread stopped." << std::endl;// 停止完成
    }

    // 2. 停止数据库写入线程
    if (g_db_writer_running) {
        std::cout << ">>> [Business] Stopping DB Writer thread..." << std::endl;// 停止DB写入线程
        g_db_writer_running = false;
        g_queue_cv.notify_all(); // 唤醒沉睡的线程让它退出
        
        if (g_db_writer_thread.joinable()) {
            g_db_writer_thread.join();
        }
        std::cout << ">>> [Business] DB Writer thread stopped." << std::endl;// 停止完成
    }
}