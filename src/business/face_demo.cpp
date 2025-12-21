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
#include "face_demo.h" // 人脸识别演示模块的头文件
#include "lvgl.h" // 嵌入式图形库头文件（预留接口，当前未使用）
#include "db_storage.h"//数据层头文件
#include <mutex>

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

static Mat current_frame;// [Eoic4新增] 用于在函数间共享最新一帧画面
// [Epic 4.4 新增] 保护 current_frame 的互斥锁
static std::mutex g_data_mutex;
// [Epic 3.3] 用户列表缓存，用于给 C 语言 UI 提供数据
static std::vector<UserData> g_user_cache;
// [Epic 3.4] 考勤记录缓存
static std::vector<AttendanceRecord> g_record_cache;
static PreprocessConfig preprocess_config; // 全局预处理配置

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
    recog = LBPHFaceRecognizer::create(1,8,8,8, 80.0);// 参数：半径=1, 邻域=8, 网格X=8, 网格Y=8, 阈值=80.0
    
    // 确保数据层已初始化 (连接数据库)
    if (!data_init()) {
        std::cerr << "[Business] 数据层初始化失败！" << std::endl;
        return false;
    }

    // [修改] 从数据库加载用所有户数据并训练（BLOB）
    std::vector<UserData> users = data_getAllUsers();
    
    face_samples.clear();
    labels.clear();
    names.clear(); 
    names.push_back("Unknown"); // 0号 ID 预留给未知用户

    if (!users.empty()) {
        std::cout << ">>> [Business] 正在从数据库加载用户数据..." << std::endl;
        
        for (const auto& u : users) {
            // 核心步骤：将二进制 BLOB 解码为 OpenCV Mat (灰度图)
            // 注意：必须使用 IMREAD_GRAYSCALE，因为 LBPH 只接受灰度图
            cv::Mat sample = cv::imdecode(u.face_feature, cv::IMREAD_GRAYSCALE);
            
            if (!sample.empty()) {
                face_samples.push_back(sample); // 添加到训练样本集
                labels.push_back(u.id);         // 添加对应的 ID 标签
                
                // 维护 ID 到 名字 的映射 (names 向量)
                // 确保 names 向量够长，能存下当前的 u.id
                if (names.size() <= u.id) {
                    names.resize(u.id + 1, "Unknown");
                }
                names[u.id] = u.name;
            }
        }
    }

    //  如果加载到了数据，立即进行训练
    if (!face_samples.empty()) {
        recog->train(face_samples, labels);
        trained = true; // 标记为已训练状态，允许进行识别
        std::cout << "[Business] 模型已恢复，加载了 " << face_samples.size() << " 个样本。" << std::endl;
    } else {
        std::cout << "[Business] 数据库为空，等待新用户注册。" << std::endl;
        trained = false;
    }

    // 初始化变量状态
    current_id = 0;// 默认选择第一个用户
    trained = false;// 未训练状态
    show_recognition = false;// 不显示识别结果
    face_samples.clear();// 清空样本
    labels.clear();// 清空标签

    // 初始化预处理配置
    preprocess_config.enable_crop = true;
    preprocess_config.crop_margin_percent = 5;
    preprocess_config.enable_resize_eq = true;
    preprocess_config.hist_eq_method = HIST_EQ_CLAHE;
    preprocess_config.clahe_clip_limit = 2.0f;
    preprocess_config.clahe_tile_grid_size = cv::Size(8, 8);
    preprocess_config.enable_roi_enhance = false;
    preprocess_config.roi_contrast = 1.2f;
    preprocess_config.roi_brightness = 10.0f;
    preprocess_config.enablez_resize = true;
    preprocess_config.resize_size = cv::Size(128, 128);
    preprocess_config.debug_show_steps = false;
    
    cout << "业务模块初始化成功。预处理配置已设为默认值。\n";
    cout << "操作说明：\n"
         << "  [1~5] 切换ID, [c] 采集, [t] 训练, [r] 识别开关, [q] 退出程序\n"
         << "  新增预处理控制接口可用\n";
         
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

    // 3. 识别逻辑 (如果开启)
    if (show_recognition && trained && has) {
        Mat f = preprocess_face(current_frame, face);
        int pred_label = -1; double conf = 0.0;
        recog->predict(f, pred_label, conf);

        std::string text;
        if (pred_label >= 0 && pred_label < names.size()) {
            // 识别成功 (置信度阈值可调，越低越匹配)
            if (conf <= 80.0) {
                text = names[pred_label];
                
                // 核心打卡逻辑：识别成功 -> 保存考勤记录
                static long long last_save_time = 0;
                long long now = std::time(nullptr);
                
                // 简单的防抖动：3秒内同一人不重复打卡
                if (now - last_save_time > 3) {
                    // 调用数据层接口：保存原图(彩色)到磁盘，路径存DB
                    // 注意：这里传入的是 current_frame (当前帧)，不是裁剪后的人脸
                    bool logged = data_saveAttendance(pred_label, current_frame);
                    
                    if (logged) {
                        std::cout << "[Business] >>> 打卡成功: " << text 
                                  << " (ID: " << pred_label << ")" << std::endl;
                        last_save_time = now;
                        // 可选：在界面上显示“打卡成功”提示
                        text += " [Logged]";
                    }
                }
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
bool business_register_user(const char* name) {

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
    u.dept_id = 0;   // 默认无部门 (或设为1)
    u.password = ""; 
    u.card_id = "";

    // 3. 调用数据层接口 (自动处理图片编码和存储)
    int new_id = db_add_user(u, current_frame);
    
    if (new_id > 0) {
        std::cout << "[Business] Registration Success! ID: " << new_id << "\n";
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