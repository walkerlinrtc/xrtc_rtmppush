#ifndef RTMP_CLIENT_H
#define RTMP_CLIENT_H

#include <string>
#include <vector>
#include <cstdint>
#include <map>
#include <chrono>
#include "Vnsp_WriteLog.h"

class RtmpClient {
public:
    RtmpClient(const std::string& server, int port, const std::string& app, const std::string& stream);
    ~RtmpClient();

    // 初始化并连接到 RTMP 服务器
    bool connect();
    // 推送 FLV 文件
    bool pushFlvFile(const std::string& filePath);
    // 关闭连接
    void close();

private:
    // RTMP 握手
    bool handshake();
    // 发送 RTMP 命令
    bool sendConnect();
    bool sendCreateStream();
    bool sendPublish();
    // 发送 RTMP 数据消息
    bool sendData(const std::vector<uint8_t>& data, uint32_t timestamp, uint8_t type);
    // 读取 FLV 文件并推送
    bool readAndSendFlv(const std::string& filePath);
    // 网络操作
    bool sendPacket(const std::vector<uint8_t>& packet, int retryCount = 3);
    bool receivePacket(std::vector<uint8_t>& buffer, size_t size, int retryCount = 3);
    // AMF0 编码
    std::vector<uint8_t> encodeAmf0Connect();
    std::vector<uint8_t> encodeAmf0CreateStream();
    std::vector<uint8_t> encodeAmf0Publish();
    // AMF0 解析
    struct Amf0Value {
        enum Type { NUMBER, BOOLEAN, STRING, OBJECT, NULL_TYPE, ARRAY };
        Type type;
        double number;
        bool boolean;
        std::string string;
        std::map<std::string, Amf0Value> object;
        std::vector<Amf0Value> array;
    };
    bool parseAmf0Response(const std::vector<uint8_t>& response, Amf0Value& result);
    bool parseAmf0Value(const std::vector<uint8_t>& data, size_t& pos, Amf0Value& value);
    // 分片机制
    bool sendChunkedData(const std::vector<uint8_t>& data, uint32_t timestamp, uint8_t type, uint32_t streamId, size_t chunkSize = 128);
    // 重试连接
    bool reconnect();

    std::string server_; // RTMP 服务器地址
    int port_; // 服务器端口
    std::string app_; // RTMP 应用名
    std::string stream_; // 流名称
    int socket_; // TCP 套接字
    uint32_t streamId_; // 流 ID
    uint64_t fileOffset_; // 文件偏移，用于重连恢复
    uint32_t baseTimestamp_; // 第一个 Tag 的时间戳
    std::chrono::steady_clock::time_point startTime_; // 推流开始时间
    size_t chunkSize_; // Chunk 大小
};

#endif // RTMP_CLIENT_H