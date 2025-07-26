#include "RtmpClient.h"
#include <iostream>
#include <Vnsp_WriteLog.h>

int main(int argc, char* argv[]) {
    Vnsp_WriteLog* m_pWriteLog;
    m_pWriteLog = Vnsp_WriteLog::GetInstance();

    // SRS 服务器地址、端口、应用名和流名
    std::string server = "127.0.0.1"; // 替换为你的 SRS 服务器地址
    int port = 1935; // SRS 默认 RTMP 端口
    std::string app = "live";
    std::string stream = "mystream";
    VNSP_LOG(LOG_INFO, "main", "Starting RTMP push to %s:%d/%s/%s", server.c_str(), port, app.c_str(), stream.c_str());
    // 创建 RTMP 客户端
    RtmpClient client(server, port, app, stream);

    // 连接到服务器
    if (!client.connect()) {
        VNSP_LOG(LOG_ERROR, "main", "Failed to connect to RTMP server %s:%d", server.c_str(), port);
        return 1;
    }

    // 推送 FLV 文件
    if (!client.pushFlvFile("demo.flv")) {
        VNSP_LOG(LOG_ERROR, "main", "Failed to push FLV file demo.flv");
        return 1;
    }

    // 关闭连接
    client.close();
    VNSP_LOG(LOG_INFO, "main", "Push completed successfully");
    return 0;
}