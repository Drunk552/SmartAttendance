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
 * @param frame 输入图像
 * @param face 输出参数，检测到的人脸矩形区域
 * @param cas 人脸检测器（Haar级联分类器）
 * @return true-检测到人脸，false-未检测到人脸
 * @note 检测最大的人脸区域，适合单人脸场景
 */

static bool detect_face(const Mat& frame, Rect& face, CascadeClassifier& cas) {
    std::vector<Rect> faces;// 存储所有检测到的人脸
    Mat gray; cvtColor(frame, gray, COLOR_BGR2GRAY);// 转为灰度图（Haar检测需要灰度图）
    equalizeHist(gray, gray);// 直方图均衡化，增强对比度
    cas.detectMultiScale(gray, faces, 1.1, 3, 0, Size(80,80));// 检测人脸，参数：1.1-缩放因子，3-最小邻居数，0-标志，Size(80,80)-最小人脸尺寸
    if (faces.empty()) return false;// 未检测到人脸
    face = *std::max_element(faces.begin(), faces.end(),
    [](const Rect& a, const Rect& b){return a.area()<b.area();}); // 选择面积最大的人脸（假设场景中只有一个人）
    return true;
}

/**
 * @brief 预处理人脸图像
 * @param frame 原始图像
 * @param roi 人脸区域矩形
 * @return 预处理后的人脸图像（128x128灰度图，直方图均衡化）
 * @note 包括裁剪边界、尺寸归一化、直方图均衡化
 */

static Mat preprocess_face(const Mat& frame, const Rect& roi) {
    Mat gray, crop = frame(roi).clone();// 克隆人脸区域，避免修改原图
    cvtColor(crop, gray, COLOR_BGR2GRAY);// 转为灰度图
    const int w = crop.cols, h = crop.rows;
    const int mx = std::max(0, w / 20);   // 计算5%的边界（左右）
    const int my = std::max(0, h / 20);   // 计算5%的边界（上下）
    const int x  = mx, y = my;
    const int ww = std::max(1, w - 2 * mx);//裁剪后的宽度
    const int hh = std::max(1, h - 2 * my);//裁剪后的高度
    Rect tight = (Rect(x, y, ww, hh) & Rect(0, 0, w, h));// 确保不越界
    Mat face = gray(tight);
    resize(face, face, Size(128, 128)); // 统一尺寸为128x128
    equalizeHist(face, face);// 直方图均衡化，增强对比度
    return face;
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
 * @return true-初始化成功，false-初始化失败
 * @note 初始化所有组件：人脸检测器、视频源、识别器
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

    cout << "业务模块初始化成功。操作说明：\n"
         << "  [1~5] 切换ID, [c] 采集, [t] 训练, [r] 识别开关, [q] 退出程序\n";
         
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
 * @brief 请求业务层处理并保存图像
 * @param inputImage - 从摄像头捕获的原始图像（BGR格式，与OpenCV一致）
 * @return bool - 处理及保存是否成功
 * @note 严格遵循接口定义 2.2.1：UI层 -> 业务层
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

    cv::Mat preprocessed_face = preprocess_face(inputImage, face_roi);
    if (preprocessed_face.empty()) { 
        std::cerr << "[Business] 人脸预处理失败，图像为空。" << std::endl;
        return false;
    }//预处理人脸图像

    //添加到样本集合和标签
    face_samples.push_back(preprocessed_face);
    labels.push_back(current_id);

    //输出样本信息
    if (current_id >= names.size()) {
        std::cerr << "[Business] 警告：当前ID(" << current_id << ")超出用户名映射范围！" << std::endl;
    } else {
        std::cout << "[Business] 人脸处理完成，用户: " << names[current_id]
                  << "，总样本数: " << face_samples.size() << std::endl;
    }

    //保存图像到数据层
    bool save_success = data_saveImage(preprocessed_face);
    if (save_success) {
        std::cout << "[Business] 图像保存成功。" << std::endl;
        return true;
    } else {
        std::cerr << "[Business] 图像保存失败。" << std::endl;
        return false;
    }
}

/**
 * @brief 测试business_processAndSaveImage接口
 * @param imagePath 测试图像路径
 * @return 测试是否通过
 */
bool test_business_processAndSaveImage(const std::string& imagePath) {
    std::cout << "=== 开始测试business_processAndSaveImage接口 ===" << std::endl;
        
    cv::Mat testImage = cv::imread(imagePath);
    if (testImage.empty()) {
        std::cerr << "测试失败：无法加载测试图像 " << imagePath << std::endl;
        return false;
    }//读取本地图片

    std::cout << "测试图像加载成功，尺寸: " << testImage.cols << "x" << testImage.rows << std::endl;
    
    std::cout << "正在调用business_processAndSaveImage接口..." << std::endl;
    bool result = business_processAndSaveImage(testImage);//调用业务层接口
    
    if (result) {
        std::cout << " 业务层接口测试通过" << std::endl;
    } 
    else {
        std::cerr << "业务层接口测试失败" << std::endl;
    }// 验证结果
    
    std::cout << "=== 测试完成 ===" << std::endl;
    return result;
}

/**
 * @brief 测试convertToGrayscale函数
 * @param imagePath 测试图像路径
 * @return 测试是否通过
 */
bool test_convertToGrayscale(const std::string& imagePath) {
    std::cout << "=== 开始测试convertToGrayscale函数 ===" << std::endl;
    
    cv::Mat testImage = cv::imread(imagePath);
    if (testImage.empty()) {
        std::cerr << "测试失败：无法加载测试图像" << std::endl;
        return false;
    }

    cv::Mat grayImage = convertToGrayscale(testImage);
    if (grayImage.empty()) {
        std::cerr << "BGR转灰度测试失败" << std::endl;
        return false;
    } // 测试BGR图像转换

    std::cout << "BGR转灰度测试成功，灰度图尺寸: " << grayImage.cols << "x" << grayImage.rows << "，通道数: " << grayImage.channels() << std::endl;

    cv::Mat grayCopy = convertToGrayscale(grayImage);
    if (grayCopy.empty()) {
        std::cerr << "灰度图转灰度测试失败" << std::endl;
        return false;
    }// 测试灰度图像转换

    std::cout << "灰度图转灰度测试成功" << std::endl;
  
    std::cout << "=== 测试完成 ===" << std::endl;
    return true;
}

/**
 * @brief 业务单次运行函数（处理一帧图像）
 * @note 每调用一次处理一帧，包含：读取、检测、识别、显示、键盘响应
 */
void business_run_once() {
    Mat frame;//存储当前帧
    if (!cap.isOpened()) return;// 安全检查：确保视频捕获已打开

    // 读取一帧
    if (!cap.read(frame) || frame.empty()) {
        return;// 读取失败或空帧，直接返回
    }

    // 检测人脸
    Rect face;// 存储检测到的人脸区域
    bool has = detect_face(frame, face, face_cas);
    if (has) rectangle(frame, face, Scalar(0,255,0), 2);// 在图像上绘制绿色矩形框

    // 识别逻辑
    if (show_recognition && trained && has) {
        Mat f = preprocess_face(frame, face);// 预处理人脸图像
        int pred_label = -1; double conf = 0.0;// 预测的标签ID
        recog->predict(f, pred_label, conf);// 使用LBPH识别器进行预测

        std::string text;// 显示文本
        if (pred_label >= 0) {
            text = (conf <= 80.0 ? names[pred_label] : "unknown"); // 置信度阈值80.0：小于等于80认为是可信识别，大于80认为是unknown
            text += "  dist=" + cv::format("%.1f", conf);
        } else text = "unknown";// 预测失败

        // 在人脸上方显示识别结果
        putText(frame, text, Point(face.x, std::max(0, face.y-10)),
                FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0,255,0), 2);
    }

    // 显示提示信息
    std::string hint = "ID=" + names[current_id] + " (1-5, c, t, r)";
    putText(frame, hint, Point(10, 30), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(255,255,255), 2);

    // 显示窗口 （注意：在最终产品中，这里应该把frame转给LVGL显示，目前使用OpenCV窗口用于调试）
    imshow("LBPH Face Demo (Integration)", frame);

    // 处理键盘输入 (仅用于 Phase01 调试)
    char key = (char)waitKey(1); // 等待 1ms
    
    if (key == 'q') {
        exit(0); // 简单的退出方式
    }
    if (key >= '1' && key <= '5') { 
        current_id = key - '1'; 
        cout << "当前ID -> " << names[current_id] << endl; 
    } // 切换当前用户ID（1对应user1，5对应user5）
    else if (key == 'c') {
        if (!has) { cout << "未检测到人脸，无法采集。\n"; }// 采集样本
        else {
            Mat f = preprocess_face(frame, face);// 预处理人脸并添加到训练集
            face_samples.push_back(f);// 添加样本图像
            labels.push_back(current_id);// 添加对应的标签
            cout << "采集: " << names[current_id] << "（总样本=" << face_samples.size() << "）\n";
        }
    } else if (key == 't') {
        if (face_samples.size() < 2) { cout << "样本过少，至少多采几张。\n"; }
        else {
            recog->train(face_samples, labels); // 训练LBPH识别器
            trained = true;// 标记为已训练
            cout << "训练完成。按 [r] 开启识别显示。\n";
        }// 训练模型
    } else if (key == 'r') {
        if (!trained) cout << "尚未训练，先按 [t]。\n";
        else {
            show_recognition = !show_recognition;// 切换状态
            cout << "识别显示 = " << (show_recognition ? "ON" : "OFF") << endl;
        }
    }// 切换识别显示
}

