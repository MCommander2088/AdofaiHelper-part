#include <iostream>
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <conio.h> // 监听按键所需
#include <regex>
#include <string>
#include <vector>
#include <map>

#if (1 == 2)
#include <F:\codes\CProjects\adofaiHelper\json.hpp>
#else
#include "json.hpp"
#endif

#include <cmath>
#include <algorithm>

/*
为了防止出现
(.text+0x1c6): undefined reference to `__imp_GetOpenFileNameA'错误，
请在编译参数添加：
"-static-libgcc",
"-lgdi32",
"-lcomdlg32"
*/


#include <chrono>

class NanoTimer
{
private:
    using Clock = std::chrono::steady_clock;
    Clock::time_point m_start;
    Clock::time_point m_end;
    bool m_running = false;

public:
    // 开始计时
    void start()
    {
        m_start = Clock::now();
        m_running = true;
    }

    // 停止计时
    void stop()
    {
        m_end = Clock::now();
        m_running = false;
    }

    // 获取耗时，单位：纳秒
    long long get_time() const
    {
        Clock::time_point end_point;
        if (m_running)
            end_point = Clock::now();
        else
            end_point = m_end;

        return std::chrono::duration_cast<std::chrono::nanoseconds>(end_point - m_start).count();
    }

    // 辅助：获取毫秒(浮点，方便查看)
    double get_ms() const
    {
        return static_cast<double>(get_time()) / 1000000.0;
    }
};

using namespace std;
using json = nlohmann::json;
using namespace nlohmann::literals;
namespace fs = filesystem;
#define _USE_MATH_DEFINES
#ifndef M_PI
#define M_PI 3.14159265358979323846  // π 的近似值
#endif

// 轮子区，包含所有的轮子

// 文件选择窗口
string OpenFileDialog(const string& filter) {
    // 初始化文件对话框结构体
    OPENFILENAMEA ofn;          // ANSI 版本（避免宽字符问题）
    char szFile[MAX_PATH] = {0}; // 存储文件路径的缓冲区（MAX_PATH=260）

    // 清空结构体并设置必要参数
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);       // 结构体大小
    ofn.hwndOwner = NULL;                // 父窗口句柄（无父窗口设为 NULL）
    ofn.lpstrFile = szFile;              // 存储路径的缓冲区
    ofn.nMaxFile = sizeof(szFile);       // 缓冲区大小
    ofn.lpstrFilter = filter.c_str();    // 文件过滤器（如 "文本文件|*.txt|所有文件|*.*"）
    ofn.nFilterIndex = 1;                // 默认选中第一个过滤器
    ofn.lpstrTitle = "Choose .adofai File";         // 对话框标题
    ofn.Flags = OFN_PATHMUSTEXIST |      // 路径必须存在
                OFN_FILEMUSTEXIST;      // 文件必须存在

    // 显示文件打开对话框（返回 TRUE 表示用户选择了文件）
    if (GetOpenFileNameA(&ofn) == TRUE) {
        return ofn.lpstrFile;  // 返回文件路径
    } else {
        return "";  // 用户取消或出错，返回空字符串
    }
}

// 打印一个vector的所有内容
void print_vector(vector<double> vec){
    if (vec.size() == 0){
        cout << "vector is empty" << endl;//获取vec的名字
        return;
    }

    cout << "[";
    for (int i = 0;i <= vec.size()-1;i++){
        try{
            cout << vec[i];
        }catch(exception e){
            break;
        }
        if (i != vec.size()-1){
            cout << ", ";
        }
    }
    cout << "]" << endl;
}

// 按下按键
void press(int vk)
{
    keybd_event(vk,0,0,0);
    keybd_event(vk,0,KEYEVENTF_KEYUP,0);
}

double mod360(double ang)
{
    ang = std::fmod(ang, 360.0);
    if (ang < 0) ang += 360.0;
    return ang;
}

double caculate_angle(double now,double next,bool is_twirl,bool is_mid_spin){
    /* BLOCKED */
}

// Utilities 功能区
class Analyzer
{
    public:
        string file_path;
        string file_content;

        vector<double> angleData;
        vector<map<string,double>> events;
        vector<double> delays;
        vector<double> angles;

        double offset;
        double bpm;
        double pitch;
        double countdownTicks;
        
        json j;

        void init();

        void open_file();
        void get_events();
        void get_angle_data();

        void process();

        map<string,double> get_event(int floor);
        bool has_floor_with_value(string target_key, int target_value, int target_floor);
};

// 移除对象、数组末尾多余逗号
std::string fix_json_trailing_comma(std::string text)
{
    // 匹配：逗号 + 任意空白 + }
    static std::regex reg_obj(R"(,\s*})");
    // 匹配：逗号 + 任意空白 + ]
    static std::regex reg_arr(R"(,\s*\])");

    text = std::regex_replace(text, reg_obj, "}");
    text = std::regex_replace(text, reg_arr, "]");
    return text;
}

void Analyzer::init()
{
    open_file();


    file_content = fix_json_trailing_comma(file_content);
    if (file_content == ""){
        cout << "File is Empty" << endl;
        return ;
    }
    try
    {
        // json反序列化
        j = json::parse(file_content);
    }
    catch (json::parse_error& e)
    {
        std::cerr << "解析错误：" << e.what() << "\n偏移位置：" << e.byte << std::endl;
    }
    catch (...)
    {
        std::cerr << "未知错误" << std::endl;
    }
    
    // get offset
    offset = j["settings"].value("offset",0);

    // get bpm
    bpm = j["settings"].value("bpm",0);

    // get 音高
    pitch = j["settings"].value("pitch",100);

    // get 倒数计数
    countdownTicks = j["settings"].value("countdownTicks",0);


    // 注意顺序
    get_angle_data();
    get_events();

    process();

}

void Analyzer::get_angle_data()
{
    //Get angleData
    vector<double> fangleData = j["angleData"];
    //将fangleData赋值到类中public的angleData
    angleData = fangleData;
    if (angleData.size() == 0) {
        cout << "can't get angle data" << endl;
    }

    //cout << "ordinary angleData size:" << angleData.size() << endl;
    //print_vector(angleData);
}

void Analyzer::get_events()
{
    /* BLOCKED */
}

std::vector<std::string> split(const std::string& src, const std::string& sep)
{
    std::vector<std::string> vec;
    size_t start = 0;
    size_t pos;
    while ((pos = src.find(sep, start)) != std::string::npos)
    {
        vec.push_back(src.substr(start, pos - start));
        start = pos + sep.size();
    }
    vec.push_back(src.substr(start));
    return vec;
}

void Analyzer::open_file()
{

    file_path = OpenFileDialog("Adofai Level Files(*.adofai)\0*.adofai\0\0");

    /*
    if (!file_path.empty()) {
        std::cout << "file path:" << file_path << std::endl;
    } else {
        std::cout << "user canceled selection" << std::endl;
        return ;
    }*/
   if (file_path.empty()) {
       return ;
   }

    // 以二进制模式打开文件（必须保留 BOM 字节）
    ifstream file(file_path, ios::binary);
    if (!file.is_open()) {
        cerr << "cannot open the file! " << file_path << endl;
        return ;
    }

    // 读取全部内容到 vector（避免手动计算大小）
    vector<char> buffer((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
    if (buffer.empty()) {
        cerr << "file is empty! " << file_path << endl;
        return ;
    }

    // 检测并移除 UTF-8 BOM（前 3 字节：0xEF 0xBB 0xBF）
    const auto* data = buffer.data();
    if (buffer.size() >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
        // 使用迭代器构造字符串（跳过前 3 字节）
        file_content = string(buffer.begin() + 3, buffer.end());
        return ;
    }

    // 无 BOM，直接用迭代器构造字符串
    file_content = string(buffer.begin(), buffer.end());

    file_content = split(file_content,"decorations")[0];
    file_content.erase(file_content.length()-1);
    file_content += "}";
    return ;
}


void Analyzer::process(){
    /* BLOCKED */
}


// 全局变量，用于控制主循环
volatile bool g_bExit = false;
volatile bool k_left = false;
volatile bool k_right = false;

// 低级键盘钩子过程函数
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    // 通常约定，当nCode小于0时，钩子过程必须将消息传递给CallNextHookEx，且不应处理该消息。
    if (nCode < 0) {
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    // 检查是否为按键按下消息 (WM_KEYDOWN 或 WM_SYSKEYDOWN)
    if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
        // 从lParam中获取键盘事件详细信息
        KBDLLHOOKSTRUCT* pKbStruct = (KBDLLHOOKSTRUCT*)lParam;
        
        // 判断按下的键是否为Insert键 (VK_INSERT)
        if (pKbStruct->vkCode == VK_INSERT) {
            g_bExit = true; // 设置退出标志
            // 通常，即使捕获了目标按键，也建议继续传递消息，除非你想屏蔽它。
            // return 1; // 如果不想让Insert消息继续传递，可以返回1而不是CallNextHookEx
        }

        if (pKbStruct->vkCode == VK_LEFT) {
            k_left = true;
            
            // 拦截
            return 1;
        }
        if (pKbStruct->vkCode == VK_RIGHT) {
            k_right = true;

            // 拦截
            return 1;
        }
    }

    // 将消息传递给钩子链中的下一个钩子
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void player(vector<double> delays ,double offset)
{
    double t;
    vector<double> times;
    for(int i;i<=delays.size()-1;i++){
        t += delays[i];
        times.push_back(t);
    }
    

    cout << endl << "press [insert] at the first floor, keyboard listener running." << endl;
    
    // 设置低级键盘钩子 (WH_KEYBOARD_LL)
    HHOOK hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandle(NULL), 0);
    
    if (hKeyboardHook == NULL) {
        std::cerr << "安装键盘钩子失败！" << std::endl;
        return ;
    }

    // 消息循环：钩子正常工作所必需
    MSG msg;
    while (!g_bExit) {
        // PeekMessage 非阻塞地检查消息队列，避免主循环卡住
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        Sleep(10); // 短暂睡眠以减少CPU占用
    }

    NanoTimer timer;

    cout << "play starts." << endl;
    //cout << "while loop will continue " << delays.size() << " times" << endl;

    // https://blog.csdn.net/caoshangpa/article/details/78552505
    int i = 0;
    timer.start();
    long long now;
    long long time_stamp;
    int offset_ms = 0;
    while (true){
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (k_left){
            offset_ms +=1;
            k_left = false;
        }
        if (k_right){
            offset_ms -=1;
            k_right = false;
        }

        if (i >= times.size()){
            break;
        }
        now = timer.get_time() + offset_ms*1000000;
        time_stamp = times[i]*1000000;
        if(now >= time_stamp){
            press(32);
            i++;
            //if (i != times.size()) cout << "\r" << "progress: " << i << "/" << times.size() << " time deviation:" << now - time_stamp - offset_ms*1000000 << " offset: " << offset_ms <<"                  ";
        }
    }
    timer.stop();

    // 卸载钩子
    UnhookWindowsHookEx(hKeyboardHook);
}


int main()
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    const std::string VERSION = "a06-20260728-0019";
    /*
        版本命名规范：
        u-未完成
        a-alpha测试
        b-beta测试
        yyyymmdd日期
        -0001编号，更新一次++
    */

    cout << "     _          _            __           _     _   _          _                       " << endl;           
    cout << "    / \\      __| |   ___    / _|   __ _  (_)   | | | |   ___  | |  _ __     ___   _ __ " << endl;
    cout << "   / _ \\    / _` |  / _ \\  | |_   / _` | | |   | |_| |  / _ \\ | | | '_ \\   / _ \\ | '__|" << endl;
    cout << "  / ___ \\  | (_| | | (_) | |  _| | (_| | | |   |  _  | |  __/ | | | |_) | |  __/ | |   " << endl;
    cout << " /_/   \\_\\  \\__,_|  \\___/  |_|    \\__,_| |_|   |_| |_|  \\___| |_| | .__/   \\___| |_|   " << endl;
    cout << "                                                                  |_|                  " << endl;

    cout << "Version: " << VERSION << endl << endl;

    //初始化铺面分析
    Analyzer analyzer;
    
    analyzer.init();

    if (analyzer.delays.size() == 0){
        system("pause");
        return 0;
    }
    player(analyzer.delays,analyzer.offset);
    cout << endl << "Play Finished." << endl;
    system("pause");

    return 0;
}
