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
        cap.open(0);// 回退到默认摄像头（索引0）
    }
    
    if (!cap.isOpened()) {
        std::cerr << "[ERR] 无法打开任何视频源。\n";
        return false;
    }

    //初始化LBPH人脸识别器
    recog = LBPHFaceRecognizer::create(1,8,8,8, 80.0);// 参数：半径=1, 邻域=8, 网格X=8, 网格Y=8, 阈值=80.0
    
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

    else {
        preprocessed_face = preprocess_face_complete(inputImage, face_roi, preprocess_config);// 预处理人脸图像
    }

      if (preprocessed_face.empty()) {
        std::cerr << "[Business] 预处理人脸图像失败，图像为空。" << std::endl;
        return false;
    }//校验预处理结果
 
    std::cout << "[Business] 预处理成功，尺寸: "
              << preprocessed_face.cols << "x" << preprocessed_face.rows
              << "，通道数: " << preprocessed_face.channels() << std::endl;

    face_samples.push_back(preprocessed_face);// 添加样本
    labels.push_back(current_id);// 添加样本和标签

    if (current_id >= names.size()) {
        std::cerr << "[Business] 警告：当前ID(" << current_id << ")超出用户名映射范围！" << std::endl;
    }// 校验ID范围
    
    else {
        std::cout << "[Business] 人脸处理完成，用户: " << names[current_id]
                  << "，总样本数: " << face_samples.size() << std::endl;
    }// 输出处理结果

    bool save_success = data_saveImage(preprocessed_face);// 保存图像到数据层

    if (save_success) {
        std::cout << "[Business] 图像保存成功。" << std::endl;
        return true;
    }//保存图像结果反馈 

    else {
        std::cerr << "[Business] 图像保存失败。" << std::endl;
        return false;
    }//保存图像结果反馈
}

// ==========================================
// Epic 4: 新增控制接口 (供 UI 按钮调用)
// ==========================================
// 切换当前用户 ID
void business_set_current_id(int id) {
    if (id >= 0 && id < names.size()) {
        current_id = id;
        cout << "[Business] 当前用户切换为: " << names[current_id] << endl;
    } else {
        std::cerr << "[Business] ID 超出范围" << endl;
    }
}

// 触发训练 (替代键盘 't')
void business_start_training() {
    if (face_samples.size() < 2) { 
        cout << "[Business] 样本过少 (<2)，无法训练。\n"; 
    } else {
        recog->train(face_samples, labels);
        trained = true;
        cout << "[Business] 模型训练完成。\n";
    }
}

// 切换识别开关 (替代键盘 'r')
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
cv::Mat business_get_frame() { // 函数名建议修改，原名 business_run_once 也可以保留但返回值要改
    if (!cap.isOpened()) return Mat();

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
            text = (conf <= 80.0 ? names[pred_label] : "unknown");
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
 * @param buffer LVGL 的 Framebuffer 或 Canvas 缓冲区
 * @param w 目标宽度
 * @param h 目标高度
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
 * @brief 触发一次采集 (供 UI 按钮调用)
 * @return bool - 采集是否成功
 * @note 使用 current_frame 进行采集，确保所见即所得
 */
bool business_capture_snapshot(){// 触发拍照函数
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