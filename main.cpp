#include "RtmpClient.h"
#include <iostream>

int main(int argc, char* argv[]) {
    // SRS 服务器地址、端口、应用名和流名
    std::string server = "127.0.0.1"; // 替换为你的 SRS 服务器地址
    int port = 1935; // SRS 默认 RTMP 端口
    std::string app = "live";
    std::string stream = "mystream";

    // 创建 RTMP 客户端
    RtmpClient client(server, port, app, stream);

    // 连接到服务器
    if (!client.connect()) {
        std::cerr << "Failed to connect to RTMP server" << std::endl;
        return 1;
    }

    // 推送 FLV 文件
    if (!client.pushFlvFile("demo.flv")) {
        std::cerr << "Failed to push FLV file" << std::endl;
        return 1;
    }

    // 关闭连接
    client.close();
    std::cout << "Push completed" << std::endl;
    return 0;
}