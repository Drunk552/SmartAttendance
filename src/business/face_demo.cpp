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
#include "face_demo.h" // 记得引入头文件
#include "lvgl.h" // 如果将来要把图像转给 LVGL，这里以后需要加逻辑

using namespace cv;
using namespace cv::face;
using std::cout; using std::endl;

// ==========================================
// 1. 全局静态变量 (原本在 main 中的局部变量)
// ==========================================
// 这些变量必须提出来，否则每次函数调用结束它们就消失了
static VideoCapture cap;
static CascadeClassifier face_cas;
static Ptr<LBPHFaceRecognizer> recog;
// [修改前]
// static std::vector<Mat> samples;
//修改后
static std::vector<Mat> face_samples;
static std::vector<int> labels;
static std::vector<std::string> names = {"user1","user2","user3","user4","user5"};
static int current_id = 0;
static bool trained = false;
static bool show_recognition = false;

// ==========================================
// 2. 辅助函数 (保持不变)
// ==========================================
// 级联文件查找
static std::string find_cascade() {
    namespace fs = std::filesystem;
    std::vector<std::string> paths = {
        "./haarcascade_frontalface_default.xml",
        "/usr/share/opencv4/haarcascades/haarcascade_frontalface_default.xml",
        "/usr/share/opencv/haarcascades/haarcascade_frontalface_default.xml"
    };
    for (auto &p : paths) if (fs::exists(p)) return p;
    return "";
}

static bool detect_face(const Mat& frame, Rect& face, CascadeClassifier& cas) {
    std::vector<Rect> faces;
    Mat gray; cvtColor(frame, gray, COLOR_BGR2GRAY);
    equalizeHist(gray, gray);
    cas.detectMultiScale(gray, faces, 1.1, 3, 0, Size(80,80));
    if (faces.empty()) return false;
    face = *std::max_element(faces.begin(), faces.end(),
                             [](const Rect& a, const Rect& b){return a.area()<b.area();});
    return true;
}

static Mat preprocess_face(const Mat& frame, const Rect& roi) {
    Mat gray, crop = frame(roi).clone();
    cvtColor(crop, gray, COLOR_BGR2GRAY);
    const int w = crop.cols, h = crop.rows;
    const int mx = std::max(0, w / 20);   // 5%
    const int my = std::max(0, h / 20);   // 5%
    const int x  = mx, y = my;
    const int ww = std::max(1, w - 2 * mx);
    const int hh = std::max(1, h - 2 * my);
    Rect tight = (Rect(x, y, ww, hh) & Rect(0, 0, w, h));
    Mat face = gray(tight);
    resize(face, face, Size(128, 128));
    equalizeHist(face, face);
    return face;
}

// —— 新增：通过 GStreamer 打开 SDP 推流 ——
// 返回已打开的 VideoCapture；失败则返回未打开的对象
static VideoCapture open_sdp_stream(const std::string& sdp_path) {
    // 低延迟、只取最新帧
    std::string pipe =
        "filesrc location=" + sdp_path + " ! sdpdemux "
        "! rtpjitterbuffer latency=0 "
        "! rtpvrawdepay ! videoconvert "
        "! video/x-raw,format=BGR "
        "! appsink sync=false drop=true max-buffers=1";

    VideoCapture cap(pipe, cv::CAP_GSTREAMER);
    return cap;
}

// ==========================================
// 3. 业务初始化函数 (替代 main 的前半部分)
// ==========================================
bool business_init() {
    // 1) 加载人脸检测器
    std::string cascade_path = find_cascade();
    if (cascade_path.empty() || !face_cas.load(cascade_path)) {
        std::cerr << "找不到/加载失败: haarcascade_frontalface_default.xml\n";
        return false;
    }

    // 2) 打开视频输入 (默认尝试打开 SDP)
    std::string sdp = "/tmp/yuyv.sdp"; // 硬编码默认路径，或从配置读取
    cap = open_sdp_stream(sdp);
    
    if (!cap.isOpened()) {
        std::cerr << "[WARN] 打不开 SDP 推流，回退到摄像头 0。\n";
        cap.open(0);
    }
    
    if (!cap.isOpened()) {
        std::cerr << "[ERR] 无法打开任何视频源。\n";
        return false;
    }

    // 3) 初始化识别器
    recog = LBPHFaceRecognizer::create(1,8,8,8, 80.0);
    
    // 初始化变量状态
    current_id = 0;
    trained = false;
    show_recognition = false;
    face_samples.clear();
    labels.clear();

    cout << "业务模块初始化成功。操作说明：\n"
         << "  [1~5] 切换ID, [c] 采集, [t] 训练, [r] 识别开关, [q] 退出程序\n";
         
    return true;
}

// ==========================================
// 4. 业务单次运行函数 (替代 main 的 for(;;) 循环体)
// ==========================================
void business_run_once() {
    Mat frame;
    if (!cap.isOpened()) return;

    // 读取一帧
    if (!cap.read(frame) || frame.empty()) {
        return;
    }

    // 检测人脸
    Rect face;
    bool has = detect_face(frame, face, face_cas);
    if (has) rectangle(frame, face, Scalar(0,255,0), 2);

    // 识别逻辑
    if (show_recognition && trained && has) {
        Mat f = preprocess_face(frame, face);
        int pred_label = -1; double conf = 0.0;
        recog->predict(f, pred_label, conf);

        std::string text;
        if (pred_label >= 0) {
            text = (conf <= 80.0 ? names[pred_label] : "unknown");
            text += "  dist=" + cv::format("%.1f", conf);
        } else text = "unknown";

        putText(frame, text, Point(face.x, std::max(0, face.y-10)),
                FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0,255,0), 2);
    }

    // 显示提示信息
    std::string hint = "ID=" + names[current_id] + " (1-5, c, t, r)";
    putText(frame, hint, Point(10, 30), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(255,255,255), 2);

    // 显示窗口 (注意：在最终产品中，这里应该把 frame 转给 LVGL 显示)
    imshow("LBPH Face Demo (Integration)", frame);

    // 处理键盘输入 (仅用于 Phase01 调试)
    char key = (char)waitKey(1); // 等待 1ms
    
    if (key == 'q') {
        exit(0); // 简单的退出方式
    }
    if (key >= '1' && key <= '5') { 
        current_id = key - '1'; 
        cout << "当前ID -> " << names[current_id] << endl; 
    }
    else if (key == 'c') {
        if (!has) { cout << "未检测到人脸，无法采集。\n"; }
        else {
            Mat f = preprocess_face(frame, face);
            face_samples.push_back(f); labels.push_back(current_id);
            cout << "采集: " << names[current_id] << "（总样本=" << face_samples.size() << "）\n";
        }
    } else if (key == 't') {
        if (face_samples.size() < 2) { cout << "样本过少，至少多采几张。\n"; }
        else {
            recog->train(face_samples, labels); trained = true;
            cout << "训练完成。按 [r] 开启识别显示。\n";
        }
    } else if (key == 'r') {
        if (!trained) cout << "尚未训练，先按 [t]。\n";
        else {
            show_recognition = !show_recognition;
            cout << "识别显示 = " << (show_recognition ? "ON" : "OFF") << endl;
        }
    }
}