#include "face_demo.h"

#include "biometric/face/face_preprocessor.h"
#include "biometric/face/face_recognition_engine.h"
#include "business/face_capture_worker.h"
#include "business/face_punch_worker.h"
#include "db_storage.h"
#include "services/punch_service.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace cv;
using std::cout; using std::endl;

// ==================== 全局静态变量 ======================

static std::vector<Mat> face_samples;// 存储所有人脸训练样本（预处理后的灰度图像）
static std::vector<int> labels;// 对应的标签ID（与face_samples一一对应）
static std::vector<std::string> names = {"Unknown"}; // ID 0 为 Unknown，其余从数据库加载
static int current_id = 0;// 当前选中的用户ID（用于采集样本）
static std::atomic<bool> show_recognition{true};// 控制是否显示识别结果
static std::mutex g_names_mutex;// 保护 names 变量的互斥锁

constexpr std::size_t kPunchQueueCapacity = 10;
static smart_attendance::business::FacePunchWorker g_punch_worker{
    kPunchQueueCapacity};
static smart_attendance::biometric::face::IFaceRecognitionEngine*
    g_recognition_engine = nullptr;
static std::unique_ptr<smart_attendance::business::FaceCaptureWorker>
    g_capture_worker;

static Mat current_frame;// [Eoic4新增] 用于在函数间共享最新一帧画面
// [Epic 4.4 新增] 保护 current_frame 的互斥锁
static std::mutex g_data_mutex;
// [Epic 3.3] 用户列表缓存，用于给 C 语言 UI 提供数据
static std::vector<UserData> g_user_cache;
// [Epic 3.4] 考勤记录缓存
static std::vector<AttendanceRecord> g_record_cache;
static PreprocessConfig preprocess_config; // 全局预处理配置
static std::mutex g_preprocess_mutex;

static cv::Mat g_display_frame_buffer; // 专门给 UI 显示用的帧缓存
static std::mutex g_display_mutex;     // 保护 g_display_frame_buffer 的锁

const std::string MODEL_FILE = "face_model.xml"; // 模型文件名

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

static smart_attendance::biometric::face::FacePreprocessConfig
toFacePreprocessConfig(const PreprocessConfig& config) {
    using smart_attendance::biometric::face::FacePreprocessConfig;
    using smart_attendance::biometric::face::HistogramEqualization;
    return FacePreprocessConfig{
        config.enable_crop,
        config.crop_margin_percent,
        config.enable_resize_eq,
        config.enablez_resize,
        config.resize_size,
        static_cast<HistogramEqualization>(config.hist_eq_method),
        config.clahe_clip_limit,
        config.clahe_tile_grid_size,
        config.enable_roi_enhance,
        config.roi_contrast,
        config.roi_brightness};
}

static smart_attendance::business::FaceCaptureWorkerCallbacks
makeCaptureCallbacks() {
    smart_attendance::business::FaceCaptureWorkerCallbacks callbacks;
    callbacks.recognitionEnabled = []() {
        return show_recognition.load();
    };
    callbacks.preprocessConfig = []() {
        std::lock_guard<std::mutex> lock(g_preprocess_mutex);
        return toFacePreprocessConfig(preprocess_config);
    };
    callbacks.resolveUserName = [](int userId) {
        std::lock_guard<std::mutex> lock(g_names_mutex);
        return userId >= 0 && static_cast<std::size_t>(userId) < names.size()
            ? names[static_cast<std::size_t>(userId)]
            : std::string("Unknown");
    };
    callbacks.submitPunch = [](
        int userId,
        std::time_t timestamp,
        const cv::Mat& snapshot,
        std::string userName,
        const std::atomic<bool>& stopRequested) {
        return g_punch_worker.submit(
            userId,
            timestamp,
            snapshot,
            std::move(userName),
            stopRequested);
    };
    callbacks.publishCurrentFrame = [](const cv::Mat& frame) {
        std::lock_guard<std::mutex> lock(g_data_mutex);
        frame.copyTo(current_frame);
    };
    callbacks.publishDisplayFrame = [](const cv::Mat& frame) {
        std::lock_guard<std::mutex> lock(g_display_mutex);
        frame.copyTo(g_display_frame_buffer);
    };
    return callbacks;
}

void business_run_database_writer_task(
    const std::atomic<bool>& stopRequested) {
    g_punch_worker.run(stopRequested);
}

void business_wake_database_writer_task() {
    std::cout << ">>> [Business] Stopping DB Writer thread..." << std::endl;
    g_punch_worker.wake();
}

void business_wake_capture_task() {
    // 采集 Worker 可能正阻塞于已满的打卡队列，必须唤醒以观察停止标志。
    // 当前 VideoCapture 无可移植唤醒 API，UDP 探测仍每 200ms 检查停止标志。
    g_punch_worker.wake();
}

void business_run_capture_task(
    const std::atomic<bool>& stopRequested) {
    if (!g_capture_worker) {
        throw std::runtime_error("FaceCaptureWorker is not initialized");
    }
    g_capture_worker->run(stopRequested);
}

/**
 * @brief 业务模块初始化函数
 * @return true-初始化成功，false-失败
 * @note 包括加载人脸检测器、初始化人脸识别器和设置预处理配置；采集由 Worker 启动。
 */

bool business_init() {
    if (!g_punch_worker.isConfigured() || g_recognition_engine == nullptr) {
        std::cerr << "[Business] Face engine and PunchService must be configured "
                     "before initialization."
                  << std::endl;
        return false;
    }

    // 加载人脸检测器（Haar级联分类器）
    std::string cascade_path = find_cascade();
    if (cascade_path.empty() ||
        !g_recognition_engine->initializeDetector(cascade_path)) {
        std::cerr << "找不到/加载失败: haarcascade_frontalface_default.xml\n";
        return false;
    }

    std::cout << ">>> [Business] 摄像头初始化已移交至后台线程，主界面立即启动。" << std::endl;

    g_capture_worker =
        std::make_unique<smart_attendance::business::FaceCaptureWorker>(
            *g_recognition_engine, makeCaptureCallbacks());
    
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
        std::cout << ">>> [Business] 发现本地模型 " << MODEL_FILE
                  << "，正在快速加载..." << std::endl;
        if (g_recognition_engine->loadModel(MODEL_FILE)) {
            std::vector<UserData> users_info = db_get_all_users_light();
            for (const auto& u : users_info) {
                if (names.size() <= u.id) {
                    names.resize(u.id + 1, "Unknown");
                }
                names[u.id] = u.name;
            }

            model_loaded = true;
            std::cout << ">>> [Business] 模型加载成功！无需重新训练。" << std::endl;
        } else {
            std::cerr << "[Business] 模型文件可能已损坏，将回退到全量训练。"
                      << std::endl;
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
                if (!g_recognition_engine->train(face_samples, labels)) {
                    std::cerr << ">>> [Business] 全量训练失败。" << std::endl;
                    return false;
                }
                std::cout << ">>> [Business] 训练完成。" << std::endl;

                if (!g_recognition_engine->saveModel(MODEL_FILE)) {
                    std::cerr << ">>> [Error] 新模型保存失败: "
                              << MODEL_FILE << std::endl;
                    return false;
                } else {
                    std::cout << ">>> [Business] 新模型已保存至: "
                              << MODEL_FILE << std::endl;
                }
            } else {
                std::cout << ">>> [Business] 未找到有效的本地头像文件，无法完成训练。" << std::endl;
            }
        } else {
             std::cout << ">>> [Business] 数据库无用户，跳过训练。" << std::endl;
        }
    }

    return true;
}

void business_enter_home_screen() {
    show_recognition = true;
    std::cout << "[Business] 进入主页，开启人脸识别打卡" << std::endl;
}

void business_leave_home_screen() {
    show_recognition = false;
    std::cout << "[Business] 离开主页，关闭人脸识别打卡" << std::endl;
}

void business_configure_punch_service(
    smart_attendance::services::PunchService& punchService) noexcept {
    g_punch_worker.configure(punchService);
}

void business_configure_face_recognition_engine(
    smart_attendance::biometric::face::IFaceRecognitionEngine&
        recognitionEngine) noexcept {
    g_recognition_engine = &recognitionEngine;
}

void business_shutdown() {
    std::cout << ">>> [Business] 正在释放识别模型和业务缓存..." << std::endl;

    // 采集 Worker 已经 join；再次 close 可覆盖初始化失败留下的部分句柄。
    if (g_capture_worker) {
        g_capture_worker->close();
        g_capture_worker.reset();
    }
    if (g_recognition_engine != nullptr) {
        g_recognition_engine->reset();
    }

    std::vector<cv::Mat>().swap(face_samples);
    std::vector<int>().swap(labels);
    {
        std::lock_guard<std::mutex> lock(g_names_mutex);
        std::vector<std::string>().swap(names);
    }

    {
        std::lock_guard<std::mutex> lock(g_data_mutex);
        current_frame.release();
        std::vector<UserData>().swap(g_user_cache);
        std::vector<AttendanceRecord>().swap(g_record_cache);
    }
    {
        std::lock_guard<std::mutex> lock(g_display_mutex);
        g_display_frame_buffer.release();
    }
    g_punch_worker.reset();

    current_id = 0;
    show_recognition.store(false);
    {
        std::lock_guard<std::mutex> lock(g_preprocess_mutex);
        preprocess_config = PreprocessConfig{};
    }
    g_recognition_engine = nullptr;
    std::cout << ">>> [Business] 识别模型和业务缓存已释放。" << std::endl;
}

/**
 * @brief 将BGR图像转换为灰度图像
 * @param inputImage 输入图像（BGR或BGRA格式）
 * @return 灰度图像
 * @note Epic 3要求实现的独立函数
 */

cv::Mat convertToGrayscale(const cv::Mat& inputImage) {
    smart_attendance::biometric::face::FacePreprocessor preprocessor;
    auto result = preprocessor.toGrayscale(inputImage);
    if (!result) {
        std::cerr << "[Business] 不支持的图像通道数: " << inputImage.channels() << std::endl;
        return cv::Mat();
    }
    return std::move(result).value();
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
 * @note 如果样本少于 2 个则不进行训练；训练状态由人脸引擎维护。
 */
void business_start_training() {
    if (face_samples.size() < 2) { 
        cout << "[Business] 样本过少 (<2)，无法训练。\n"; 
    } else if (g_recognition_engine != nullptr &&
               g_recognition_engine->train(face_samples, labels)) {
        cout << "[Business] 模型训练完成。\n";
    } else {
        std::cerr << "[Business] 模型训练失败。\n";
    }
} 

/**
 * @brief 切换识别显示开关（开 / 关）
 * @note 若尚未训练则无法开启识别，函数会在日志中说明原因
 */
void business_toggle_recognition() {
    if (g_recognition_engine == nullptr ||
        !g_recognition_engine->isTrained()) {
        cout << "[Business] 尚未训练，无法开启识别。\n";
        show_recognition.store(false);
    } else {
        const bool enabled = !show_recognition.load();
        show_recognition.store(enabled);
        cout << "[Business] 识别功能: " << (enabled ? "开启" : "关闭") << endl;
    }
}

/**
 * @brief 设置人脸识别开关状态
 * @param enable true开启识别，false关闭识别
 */
void business_set_recognition_enabled(bool enable) {
    if ((g_recognition_engine == nullptr ||
         !g_recognition_engine->isTrained()) && enable) {
        cout << "[Business] 尚未训练，无法开启识别。\n";
        show_recognition.store(false);
    } else {
        show_recognition.store(enable);
        cout << "[Business] 识别功能: " << (enable ? "开启" : "关闭") << endl;
    }
}

/**
 * @brief 获取当前人脸识别开关状态
 * @return true识别已开启，false识别已关闭
 */
bool business_get_recognition_enabled(void) {
    return show_recognition.load();
}

// ==========================================
// Epic 4: 改造后的运行函数
// ==========================================

/**
 * @brief 获取采集 Worker 最近发布的处理后画面。
 * @return 当前帧的独立副本；尚无画面时返回空 Mat。
 * @note 线程安全，不读取摄像头、不执行识别或打卡。
 */
cv::Mat business_get_frame() {
    std::lock_guard<std::mutex> lock(g_data_mutex);
    if (current_frame.empty()) {
        return cv::Mat();
    }
    return current_frame.clone();
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
    //cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
    frame.copyTo(rgb);
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
        if (g_recognition_engine == nullptr) {
            std::cerr << ">>> [Error] 人脸识别引擎未配置。" << std::endl;
            return false;
        }
        if (!g_recognition_engine->isTrained()) {
            if (!g_recognition_engine->train(new_imgs, new_labels)) {
                std::cerr << ">>> [Error] 模型初始化训练失败。" << std::endl;
                return false;
            }
            std::cout << ">>> [Business] 模型初始化训练完成。" << std::endl;
        } else {
            if (!g_recognition_engine->update(new_imgs, new_labels)) {
                std::cerr << ">>> [Error] 模型增量更新失败。" << std::endl;
                return false;
            }
            std::cout << ">>> [Business] 模型增量更新完成。" << std::endl;
        }

        // D. 立即保存到磁盘 (Model Persistence)
        // 这样下次启动时，business_init 就能直接读取这个 xml 文件
        if (g_recognition_engine->saveModel(MODEL_FILE)) {
            std::cout << ">>> [Business] 模型已成功保存至: " << MODEL_FILE << std::endl;
        } else {
            std::cerr << ">>> [Error] 模型文件保存失败。" << std::endl;
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
        if (g_recognition_engine == nullptr) {
            std::cerr << ">>> [Error] 人脸识别引擎未配置。" << std::endl;
            return false;
        }
        if (!g_recognition_engine->isTrained()) {
            if (!g_recognition_engine->train(new_imgs, new_labels)) {
                std::cerr << ">>> [Error] 模型初始化训练失败。" << std::endl;
                return false;
            }
        } else {
            if (!g_recognition_engine->update(new_imgs, new_labels)) {
                std::cerr << ">>> [Error] 模型增量更新失败。" << std::endl;
                return false;
            }
        }

        // 保存模型到磁盘
        if (g_recognition_engine->saveModel(MODEL_FILE)) {
            std::cout << ">>> [Business] 模型增量更新完成，已保存至: " << MODEL_FILE << std::endl;
        } else {
            std::cerr << ">>> [Error] 模型文件保存失败。" << std::endl;
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
    if (config == nullptr) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(g_preprocess_mutex);
        preprocess_config = *config;
    }
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
    std::lock_guard<std::mutex> lock(g_preprocess_mutex);
    return preprocess_config;
}

/**
 * @brief 设置直方图均衡化选项 (供 UI 按钮调用)
 * @param enable 是否启用直方图均衡化
 * @param method 直方图均衡化方法（0-无, 1-全局, 2-CLAHE）
 * @note 动态调整直方图均衡化选项
 */
void business_set_histogram_equalization(bool enable, int method) {
    {
        std::lock_guard<std::mutex> lock(g_preprocess_mutex);
        preprocess_config.enable_resize_eq = enable;
        preprocess_config.hist_eq_method = method;
    }
    
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
    const float boundedClipLimit = std::max(1.0f, clip_limit);
    const cv::Size boundedGridSize(
        std::max(1, grid_width), std::max(1, grid_height));
    {
        std::lock_guard<std::mutex> lock(g_preprocess_mutex);
        preprocess_config.clahe_clip_limit = boundedClipLimit;
        preprocess_config.clahe_tile_grid_size = boundedGridSize;
    }
    
    std::cout << "[Business] CLAHE 参数已更新。剪切限制: "
              << boundedClipLimit << ", 网格大小: "
              << boundedGridSize.width << "x" << boundedGridSize.height
              << std::endl;
}

/**
 * @brief 设置ROI增强参数 (供 UI 按钮调用)
 * @param enable 是否启用ROI增强
 * @param contrast 对比度增强因子
 * @param brightness 亮度增强偏移量
 * @note 动态调整ROI增强选项
 */
void business_set_roi_enhance(bool enable, float contrast, float brightness){
    {
        std::lock_guard<std::mutex> lock(g_preprocess_mutex);
        preprocess_config.enable_roi_enhance = enable;
        preprocess_config.roi_contrast = contrast;
        preprocess_config.roi_brightness = brightness;
    }

    std::cout << "[Business] ROI增强: " << (enable ? "启用" : "禁用")
              << ", 对比度: " << contrast
              << ", 亮度: " << brightness << std::endl;
}

/**
 * @brief 保留给旧 UI 调用方的配置刷新兼容入口。
 * @note PunchService 每次打卡都会读取当前规则，因此不再维护业务层规则缓存。
 */
void business_reload_config() {
    std::cout << ">>> [Business] 打卡规则由 PunchService 按请求读取，无需刷新缓存。"
              << std::endl;
}
