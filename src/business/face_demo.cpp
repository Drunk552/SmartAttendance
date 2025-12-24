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
#include "face_demo.h" // 人脸识别演示模块的头文件
#include "lvgl.h" // 嵌入式图形库头文件（预留接口，当前未使用）
#include "db_storage.h"//数据层头文件
#include <mutex>
#include "attendance_rule.h"

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
static std::vector<std::string> names = {"user1","user2","user3","user4","user5"};// 用户名映射
static int current_id = 0;// 当前选中的用户ID（用于采集样本）
static bool trained = false;// 标识是否已完成训练
static bool show_recognition = false;// 控制是否显示识别结果

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
 * @brief 通过GStreamer打开SDP推流
 * @param sdp_path SDP文件路径
 * @return 已打开的VideoCapture对象，如果失败则返回未打开的对象
 * @note 用于接收网络视频流，配置为低延迟模式
 */

static VideoCapture open_sdp_stream(const std::string& sdp_path) {
    // GStreamer管道配置：
    // filesrc: 从文件读取SDP描述
    // sdpdemux: 解析SDP，提取媒体流
    // rtpjitterbuffer: 抖动缓冲，latency=0表示最低延迟
    // rtpvrawdepay: RTP载荷解析
    // videoconvert: 格式转换
    // appsink: 输出到OpenCV，配置为异步、丢弃旧帧、只保留最新一帧
    std::string pipe =
        "filesrc location=" + sdp_path + " ! sdpdemux "
        "! rtpjitterbuffer latency=0 "
        "! rtpvrawdepay ! videoconvert "
        "! video/x-raw,format=BGR "
        "! appsink sync=false drop=true max-buffers=1";

    VideoCapture cap(pipe, cv::CAP_GSTREAMER);// 使用GStreamer后端
    return cap;
}

/**
 * @brief 业务模块初始化函数
 * @return true-初始化成功，false-失败
 * @note 包括加载人脸检测器、打开视频源、初始化人脸识别器、设置预处理配置
 */

bool business_init() {
    //加载人脸检测器（Haar级联分类器）
    std::string cascade_path = find_cascade();
    if (cascade_path.empty() || !face_cas.load(cascade_path)) {
        std::cerr << "找不到/加载失败: haarcascade_frontalface_default.xml\n";
        return false;
    }

    //打开视频输入 (默认尝试打开 SDP)
    std::string sdp = "/tmp/yuyv.sdp"; // 硬编码默认路径，或从配置读取
    cap = open_sdp_stream(sdp);// 尝试打开GStreamer流
    
    if (!cap.isOpened()) {
        std::cerr << "[WARN] 打不开 SDP 推流，回退到摄像头 0。\n";
        cap.open(0);
        
        // 【新增代码】强制设置低分辨率和缓冲区大小
        if (cap.isOpened()) {
            // 设置为 320x240，匹配我们的屏幕尺寸，大幅提升处理速度
            cap.set(cv::CAP_PROP_FRAME_WIDTH, 320);
            cap.set(cv::CAP_PROP_FRAME_HEIGHT, 240);
            
            // 尝试设置缓冲区为1 (部分驱动支持)，只取最新一帧
            cap.set(cv::CAP_PROP_BUFFERSIZE, 1);
            
            std::cout << "[Business] Camera set to 320x240 Low Latency Mode.\n";
        }
    }
    
    if (!cap.isOpened()) {
        std::cerr << "[ERR] 无法打开任何视频源。\n";
        return false;
    }

    //初始化LBPH人脸识别器
    recog = LBPHFaceRecognizer::create(1,8,8,8, 500.0);// 参数：半径=1, 邻域=8, 网格X=8, 网格Y=8, 阈值=500.0
    
    // 确保数据层已初始化 (连接数据库)
    if (!data_init()) {
        std::cerr << "[Business] 数据层初始化失败！" << std::endl;
        return false;
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
        f.close(); // 文件存在，关闭流
        try {
            std::cout << ">>> [Business] 发现本地模型 " << MODEL_FILE << "，正在快速加载..." << std::endl;
            recog->read(MODEL_FILE); // 直接读取 XML
            
            // 虽然不用读图片，但必须从数据库加载 ID->姓名的映射关系
            // 使用 lightweight 接口 (不读 BLOB，速度快)
            std::vector<UserData> users_info = db_get_all_users();
            
            for (const auto& u : users_info) {
                // 确保 names 向量够长
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

    // B. 如果模型加载失败（或文件不存在），执行全量训练并保存
    if (!model_loaded) {
        std::cout << ">>> [Business] 开始执行全量训练 (读取数据库 BLOB)..." << std::endl;
        
        // 使用 heavyweight 接口 (读取 BLOB 图片)
        std::vector<UserData> users = db_get_all_users(); // 注意：这里调用的是原有的全量接口
        
        if (!users.empty()) {
            for (const auto& u : users) {
                // 解码图片
                cv::Mat sample = cv::imdecode(u.face_feature, cv::IMREAD_GRAYSCALE);
                if (!sample.empty()) {
                    face_samples.push_back(sample);
                    labels.push_back(u.id);
                    
                    // 维护名字映射
                    if (names.size() <= u.id) {
                        names.resize(u.id + 1, "Unknown");
                    }
                    names[u.id] = u.name;
                }
            }
            
            // 开始训练
            if (!face_samples.empty()) {
                recog->train(face_samples, labels);
                trained = true;
                std::cout << ">>> [Business] 训练完成。" << std::endl;
                
                // 【保存】训练完成后立即保存模型，下次启动就快了
                recog->write(MODEL_FILE);
                std::cout << ">>> [Business] 新模型已保存至: " << MODEL_FILE << std::endl;
            }
        } else {
             std::cout << ">>> [Business] 数据库无用户，跳过训练。" << std::endl;
        }
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

/**
 * @brief 处理并保存人脸图像
 * @param inputImage 输入图像（BGR格式）
 * @return true-处理并保存成功，false-失败
 * @note 包括人脸检测、预处理、样本添加和保存
*/

bool business_processAndSaveImage(const cv::Mat& inputImage) {
    if (inputImage.empty()) {
        std::cerr << "[Business] 输入图像为空，无法处理和保存。" << std::endl;
        return false;
    }//校验输入图像有效性

    cv::Rect face_roi;  // 存储检测到的人脸区域
    
    bool face_detected = detect_face(inputImage, face_roi, face_cas);
    
    if (!face_detected) {
        std::cerr << "[Business] 未检测到人脸，无法保存图像。" << std::endl;
        return false;
    }//检测人脸

    //输出人脸位置
    std::cout << "[Business] 检测到人脸，位置: ("
              << face_roi.x << "," << face_roi.y << ") "
              << face_roi.width << "x" << face_roi.height << std::endl;
    
     cv::Mat preprocessed_face;// 存储预处理后的人脸图像

    if(preprocess_config.debug_show_steps){
        preprocessed_face = preprocess_face_complete(inputImage, face_roi, preprocess_config);
        std::cout << "[Business] 预处理完成，调试模式已启用。" << std::endl;
    } // 调试模式下显示所有预处理步骤

    // 修改为调用新的注册接口 (BLOB存储)
    // A.准备用户名
    // (逻辑：尝试使用 names 列表中现有的名字，或者生成默认名字)
    std::string reg_name;
    if (current_id < names.size() && names[current_id] != "Unknown") {
        reg_name = names[current_id];
    } else {
        reg_name = "User_" + std::to_string(current_id);
    }

    // B. [关键修改] 调用数据层的新接口：注册用户 (存入 DB users 表)
    // 注意：这里不再调用 data_saveImage，而是 data_registerUser
    int new_uid = data_registerUser(reg_name, preprocessed_face);

    // C. 判断结果并更新内存状态
    if (new_uid != -1) {
        // 1. 更新内存中的训练集 (这样不需要重启程序就能训练)
        face_samples.push_back(preprocessed_face);
        labels.push_back(new_uid);

        // 2. 更新 names 映射表 (确保向量足够长)
        if (names.size() <= new_uid) {
            names.resize(new_uid + 1, "Unknown");
        }
        names[new_uid] = reg_name;

        // 3. 实时更新模型 (增量训练，可选)
        // 这样采集完马上就能识别，不需要按 't' 重新训练所有数据
        std::vector<cv::Mat> new_samples = {preprocessed_face};
        std::vector<int> new_labels = {new_uid};
        recog->update(new_samples, new_labels);
        trained = true;

        std::cout << "[Business] 用户注册成功! Name: " << reg_name 
                  << " | DB_ID: " << new_uid << std::endl;
        return true;
    } else {
        std::cerr << "[Business] 数据库注册失败！" << std::endl;
        return false;
    }//保存图像结果反馈
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

// ==========================================
// Epic 4: 改造后的运行函数
// ==========================================

/**
 * @brief 业务单次运行函数（Epic 4 修改版）
 * @return cv::Mat 处理后的图像（带有人脸框和文字），用于 UI 显示
 * @note 移除了 imshow 和 waitKey，不再阻塞，不再直接处理键盘
 */
cv::Mat business_get_frame() {// 函数名建议修改，原名 business_run_once 也可以保留但返回值要改
    if (!cap.isOpened()) return Mat();

    // [Epic 4.4 新增] 加锁，防止写入时被其他线程读取
    std::lock_guard<std::mutex> lock(g_data_mutex);

    // 【可选优化】如果在 Linux/V4L2 下延迟依然存在，可以取消下面这行的注释
    // 它的作用是每次读取前先抓取一次丢弃，确保拿到的是最新的
    cap.grab();

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
                    
                    // 调用数据库接口 (传入动态计算的 shift_id)
                    bool logged = db_log_attendance(pred_label, shift_id_for_log, current_frame, db_status);
                    
                    if (logged) {
                        std::cout << "[Business] 打卡成功: " << text 
                                  << " 状态: " << db_status 
                                  << " 差异: " << result.minutes_diff << "分" << std::endl;
                        text += " [OK]";
                    }
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

// [Epic4新增] 实现获取显示帧接口
/**
 * @brief LVGL 专用的显示接口
 * @param buffer LVGL 的 Framebuffer 或 Canvas 缓冲区（预分配，大小至少 w*h*3）
 * @param w 目标宽度
 * @param h 目标高度
 * @return true 成功填充 buffer 并可供显示；false 失败（无帧或转换失败）
 * @note 目前按 RGB888 (3 bytes/pixel) 拷贝，如需支持其他 LVGL color depth 请调整转换逻辑
 */
bool business_get_display_frame(void* buffer, int w, int h) {

    cv::Mat frame = business_get_frame();

    if (frame.empty()) return false;

    Mat resized, rgb;
    // 1. 缩放到 UI 指定的大小
    cv::resize(frame, resized, Size(w, h));

    // 2. 颜色转换: OpenCV 默认是 BGR，LVGL 需要 RGB
    // 注意：根据你的 LV_COLOR_DEPTH，如果是 32位可能需要转 BGRA/RGBA
    // 这里假设 LVGL 配置为 RGB888 (24位) 或 ARGB8888 (32位)
    // 简单起见，我们转成 BGR -> RGB (24位)
    cv::cvtColor(resized, rgb, COLOR_BGR2RGB);

    // 3. 内存拷贝
    // 确保 buffer 足够大 (w * h * 3)
    memcpy(buffer, rgb.data, w * h * 3);
    
    return true;
}

/**
 * @brief 获取用户总数并刷新缓存（供 UI 列表使用）
 * @return 当前用户数量
 * @note 会调用 data_getAllUsers() 刷新 g_user_cache
 */
int business_get_user_count(void) {
    // 每次进入列表页时，从数据库重新拉取一次数据
    g_user_cache = data_getAllUsers();
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

 /* @brief 触发一次采集 (供 UI 按钮调用)
 * @return bool - 采集是否成功
 * @note 使用 current_frame 进行采集，确保所见即所得
 */
bool business_capture_snapshot(){// 触发拍照函数

    // [Epic 4.4 新增] 加锁保护读取
    std::lock_guard<std::mutex> lock(g_data_mutex);

    //检查是否有画面
    if (!current_frame.empty()) {
        std::cout <<">>> [Business]触发采集..." << endl;

        bool success = business_processAndSaveImage(current_frame);// 使用 current_frame 进行采集
        
        if(success){
            long long last_id = data_getLastImageID();
            std::cout << "[Business] 采集成功,图像ID: " << last_id << std::endl;   
        }
        return success;// 返回采集结果
    } 
        else
        {
            std::cerr << "[Business] 采集失败：当前没有画面帧" << std::endl;
            return false;
        }//采集失败
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

    std::cout << "[Business] ROI增强： " << (enable ? "启用" : "禁用")
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