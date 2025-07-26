#include "RtmpClient.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <chrono>
#include <thread>

RtmpClient::RtmpClient(const std::string& server, int port, const std::string& app, const std::string& stream)
    : server_(server), port_(port), app_(app), stream_(stream), socket_(-1), streamId_(1), fileOffset_(0), baseTimestamp_(0), chunkSize_(128) {}

RtmpClient::~RtmpClient() {
    close();
}

bool RtmpClient::connect() {
    // 创建 TCP 套接字
    socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_ < 0) {
        std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
        return false;
    }

    // 设置非阻塞
    int flags = fcntl(socket_, F_GETFL, 0);
    fcntl(socket_, F_SETFL, flags | O_NONBLOCK);

    // 连接到服务器
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port_);
    inet_pton(AF_INET, server_.c_str(), &serverAddr.sin_addr);

    if (::connect(socket_, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0 && errno != EINPROGRESS) {
        std::cerr << "Connect failed: " << strerror(errno) << std::endl;
        close();
        return false;
    }

    // 等待连接完成
    fd_set writeFds;
    FD_ZERO(&writeFds);
    FD_SET(socket_, &writeFds);
    struct timeval tv = {5, 0}; // 5 秒超时
    if (select(socket_ + 1, nullptr, &writeFds, nullptr, &tv) <= 0) {
        std::cerr << "Connect timeout or error" << std::endl;
        close();
        return false;
    }

    // 执行 RTMP 握手
    if (!handshake()) {
        std::cerr << "Handshake failed" << std::endl;
        close();
        return false;
    }

    // 发送 connect 命令
    if (!sendConnect()) {
        std::cerr << "Send connect failed" << std::endl;
        close();
        return false;
    }

    // 发送 createStream 命令
    if (!sendCreateStream()) {
        std::cerr << "Send createStream failed" << std::endl;
        close();
        return false;
    }

    // 发送 publish 命令
    if (!sendPublish()) {
        std::cerr << "Send publish failed" << std::endl;
        close();
        return false;
    }

    return true;
}

bool RtmpClient::reconnect() {
    close();
    std::cerr << "Attempting to reconnect..." << std::endl;
    for (int i = 0; i < 3; ++i) {
        if (connect()) {
            std::cerr << "Reconnect successful" << std::endl;
            return true;
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    std::cerr << "Reconnect failed after 3 attempts" << std::endl;
    return false;
}

bool RtmpClient::handshake() {
    // C0: 版本号
    uint8_t c0 = 0x03; // RTMP 版本 3
    if (!sendPacket(std::vector<uint8_t>{c0})) return false;

    // C1: 时间戳 + 零填充
    std::vector<uint8_t> c1(1536, 0);
    uint32_t timestamp = static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count() / 1000);
    c1[0] = (timestamp >> 24) & 0xFF;
    c1[1] = (timestamp >> 16) & 0xFF;
    c1[2] = (timestamp >> 8) & 0xFF;
    c1[3] = timestamp & 0xFF;
    if (!sendPacket(c1)) return false;

    // 接收 S0 + S1
    std::vector<uint8_t> s0s1(1537);
    if (!receivePacket(s0s1, 1537)) return false;
    if (s0s1[0] != 0x03) {
        std::cerr << "Invalid S0 version" << std::endl;
        return false;
    }

    // 发送 C2（回送 S1）
    if (!sendPacket(std::vector<uint8_t>(s0s1.begin() + 1, s0s1.end()))) return false;

    // 接收 S2
    std::vector<uint8_t> s2(1536);
    if (!receivePacket(s2, 1536)) return false;

    return true;
}

bool RtmpClient::sendConnect() {
    std::vector<uint8_t> connectPacket = encodeAmf0Connect();
    if (!sendPacket(connectPacket)) return false;

    std::vector<uint8_t> response(4096);
    if (!receivePacket(response, 4096)) return false;
    Amf0Value result;
    if (!parseAmf0Response(response, result)) return false;
    if (result.array.size() >= 2 && result.array[0].string == "_result" && result.array[1].number == 1.0) {
        return true;
    }
    std::cerr << "Connect response invalid" << std::endl;
    return false;
}

bool RtmpClient::sendCreateStream() {
    std::vector<uint8_t> createStreamPacket = encodeAmf0CreateStream();
    if (!sendPacket(createStreamPacket)) return false;

    std::vector<uint8_t> response(4096);
    if (!receivePacket(response, 4096)) return false;
    Amf0Value result;
    if (!parseAmf0Response(response, result)) return false;
    if (result.array.size() >= 4 && result.array[0].string == "_result" && result.array[1].number == 2.0) {
        streamId_ = static_cast<uint32_t>(result.array[3].number);
        return true;
    }
    std::cerr << "CreateStream response invalid" << std::endl;
    return false;
}

bool RtmpClient::sendPublish() {
    std::vector<uint8_t> publishPacket = encodeAmf0Publish();
    if (!sendPacket(publishPacket)) return false;

    std::vector<uint8_t> response(4096);
    if (!receivePacket(response, 4096)) return false;
    Amf0Value result;
    if (!parseAmf0Response(response, result)) return false;
    if (result.array.size() >= 4 && result.array[0].string == "onStatus" && result.array[3].object.count("code")) {
        std::string code = result.array[3].object["code"].string;
        if (code == "NetStream.Publish.Start") {
            return true;
        }
        std::cerr << "Publish failed: " << code << std::endl;
        return false;
    }
    std::cerr << "Publish response invalid" << std::endl;
    return false;
}

bool RtmpClient::pushFlvFile(const std::string& filePath) {
    fileOffset_ = 0;
    baseTimestamp_ = 0;
    startTime_ = std::chrono::steady_clock::now();
    return readAndSendFlv(filePath);
}

void RtmpClient::close() {
    if (socket_ >= 0) {
        ::close(socket_);
        socket_ = -1;
    }
}

bool RtmpClient::sendPacket(const std::vector<uint8_t>& packet, int retryCount) {
    for (int i = 0; i < retryCount; ++i) {
        size_t sent = 0;
        while (sent < packet.size()) {
            ssize_t n = send(socket_, packet.data() + sent, packet.size() - sent, 0);
            if (n <= 0) {
                if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
                    std::cerr << "Send failed: " << strerror(errno) << std::endl;
                    if (reconnect()) {
                        return sendPacket(packet, retryCount - i - 1);
                    }
                    return false;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            sent += n;
        }
        return true;
    }
    std::cerr << "Send failed after " << retryCount << " retries" << std::endl;
    return false;
}

bool RtmpClient::receivePacket(std::vector<uint8_t>& buffer, size_t size, int retryCount) {
    for (int i = 0; i < retryCount; ++i) {
        size_t received = 0;
        while (received < size) {
            ssize_t n = recv(socket_, buffer.data() + received, size - received, 0);
            if (n <= 0) {
                if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
                    std::cerr << "Recv failed: " << strerror(errno) << std::endl;
                    if (reconnect()) {
                        return receivePacket(buffer, size, retryCount - i - 1);
                    }
                    return false;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            received += n;
        }
        return true;
    }
    std::cerr << "Receive failed after " << retryCount << " retries" << std::endl;
    return false;
}

std::vector<uint8_t> RtmpClient::encodeAmf0Connect() {
    std::vector<uint8_t> packet;
    packet.push_back(0x03); // Chunk Stream ID = 3
    packet.push_back(0x00); packet.push_back(0x00); packet.push_back(0x00); // Timestamp = 0
    packet.push_back(0x00); packet.push_back(0x00); packet.push_back(0x00); // Message Length
    packet.push_back(0x14); // Message Type ID = 20 (AMF0 Command)
    packet.push_back(0x00); packet.push_back(0x00); packet.push_back(0x00); packet.push_back(0x00); // Message Stream ID = 0

    packet.push_back(0x02); // String type
    packet.push_back(0x00); packet.push_back(0x07); // Length = 7
    packet.insert(packet.end(), {'c', 'o', 'n', 'n', 'e', 'c', 't'});
    packet.push_back(0x00); // Number type
    packet.push_back(0x3F); packet.push_back(0xF0); packet.push_back(0x00); packet.push_back(0x00);
    packet.push_back(0x00); packet.push_back(0x00); packet.push_back(0x00); packet.push_back(0x00); // 1.0
    packet.push_back(0x03); // Object type
    packet.push_back(0x00); packet.push_back(0x03); // Length = 3
    packet.insert(packet.end(), {'a', 'p', 'p'});
    packet.push_back(0x02); // String type
    packet.push_back(0x00); packet.push_back(app_.size());
    packet.insert(packet.end(), app_.begin(), app_.end());
    packet.push_back(0x00); packet.push_back(0x05); // Length = 5
    packet.insert(packet.end(), {'t', 'c', 'U', 'r', 'l'});
    packet.push_back(0x02); // String type
    std::string tcUrl = "rtmp://" + server_ + ":" + std::to_string(port_) + "/" + app_;
    packet.push_back(0x00); packet.push_back(tcUrl.size());
    packet.insert(packet.end(), tcUrl.begin(), tcUrl.end());
    packet.push_back(0x00); packet.push_back(0x00); packet.push_back(0x09); // Object end

    size_t length = packet.size() - 12;
    packet[4] = (length >> 16) & 0xFF;
    packet[5] = (length >> 8) & 0xFF;
    packet[6] = length & 0xFF;

    return packet;
}

std::vector<uint8_t> RtmpClient::encodeAmf0CreateStream() {
    std::vector<uint8_t> packet;
    packet.push_back(0x03); // Chunk Stream ID = 3
    packet.push_back(0x00); packet.push_back(0x00); packet.push_back(0x00); // Timestamp = 0
    packet.push_back(0x00); packet.push_back(0x00); packet.push_back(0x00); // Message Length
    packet.push_back(0x14); // Message Type ID = 20
    packet.push_back(0x00); packet.push_back(0x00); packet.push_back(0x00); packet.push_back(0x00);

    packet.push_back(0x02); // String type
    packet.push_back(0x00); packet.push_back(0x0C); // Length = 12
    packet.insert(packet.end(), {'c', 'r', 'e', 'a', 't', 'e', 'S', 't', 'r', 'e', 'a', 'm'});
    packet.push_back(0x00); // Number type
    packet.push_back(0x40); packet.push_back(0x00); packet.push_back(0x00); packet.push_back(0x00);
    packet.push_back(0x00); packet.push_back(0x00); packet.push_back(0x00); packet.push_back(0x00); // 2.0
    packet.push_back(0x05); // Null type

    size_t length = packet.size() - 12;
    packet[4] = (length >> 16) & 0xFF;
    packet[5] = (length >> 8) & 0xFF;
    packet[6] = length & 0xFF;

    return packet;
}

std::vector<uint8_t> RtmpClient::encodeAmf0Publish() {
    std::vector<uint8_t> packet;
    packet.push_back(0x03); // Chunk Stream ID = 3
    packet.push_back(0x00); packet.push_back(0x00); packet.push_back(0x00); // Timestamp = 0
    packet.push_back(0x00); packet.push_back(0x00); packet.push_back(0x00); // Message Length
    packet.push_back(0x14); // Message Type ID = 20
    packet.push_back(0x00); packet.push_back(0x00); packet.push_back(0x00); packet.push_back(0x01); // Message Stream ID = 1

    packet.push_back(0x02); // String type
    packet.push_back(0x00); packet.push_back(0x07); // Length = 7
    packet.insert(packet.end(), {'p', 'u', 'b', 'l', 'i', 's', 'h'});
    packet.push_back(0x00); // Number type
    packet.push_back(0x40); packet.push_back(0x08); packet.push_back(0x00); packet.push_back(0x00);
    packet.push_back(0x00); packet.push_back(0x00); packet.push_back(0x00); packet.push_back(0x00); // 3.0
    packet.push_back(0x05); // Null type
    packet.push_back(0x02); // String type
    packet.push_back(0x00); packet.push_back(stream_.size());
    packet.insert(packet.end(), stream_.begin(), stream_.end());
    packet.push_back(0x02); // String type
    packet.push_back(0x00); packet.push_back(0x04); // Length = 4
    packet.insert(packet.end(), {'l', 'i', 'v', 'e'});

    size_t length = packet.size() - 12;
    packet[4] = (length >> 16) & 0xFF;
    packet[5] = (length >> 8) & 0xFF;
    packet[6] = length & 0xFF;

    return packet;
}

bool RtmpClient::parseAmf0Value(const std::vector<uint8_t>& data, size_t& pos, Amf0Value& value) {
    if (pos >= data.size()) return false;
    uint8_t type = data[pos++];

    switch (type) {
        case 0x00: // Number
            if (pos + 8 > data.size()) return false;
            value.type = Amf0Value::NUMBER;
            value.number = 0;
            for (int i = 0; i < 8; ++i) {
                value.number = (value.number << 8) | data[pos++];
            }
            return true;
        case 0x01: // Boolean
            if (pos >= data.size()) return false;
            value.type = Amf0Value::BOOLEAN;
            value.boolean = data[pos++] != 0;
            return true;
        case 0x02: // String
            if (pos + 2 > data.size()) return false;
            value.type = Amf0Value::STRING;
            uint16_t len = (data[pos] << 8) | data[pos + 1];
            pos += 2;
            if (pos + len > data.size()) return false;
            value.string = std::string(data.begin() + pos, data.begin() + pos + len);
            pos += len;
            return true;
        case 0x03: // Object
            value.type = Amf0Value::OBJECT;
            while (pos + 3 <= data.size()) {
                uint16_t keyLen = (data[pos] << 8) | data[pos + 1];
                pos += 2;
                if (keyLen == 0 && pos + 1 <= data.size() && data[pos] == 0x09) {
                    pos++; // Object end
                    break;
                }
                if (pos + keyLen > data.size()) return false;
                std::string key(data.begin() + pos, data.begin() + pos + keyLen);
                pos += keyLen;
                Amf0Value subValue;
                if (!parseAmf0Value(data, pos, subValue)) return false;
                value.object[key] = subValue;
            }
            return true;
        case 0x05: // Null
            value.type = Amf0Value::NULL_TYPE;
            return true;
        case 0x08: // Array
            if (pos + 4 > data.size()) return false;
            value.type = Amf0Value::ARRAY;
            uint32_t arrayLen = (data[pos] << 24) | (data[pos + 1] << 16) | (data[pos + 2] << 8) | data[pos + 3];
            pos += 4;
            for (uint32_t i = 0; i < arrayLen; ++i) {
                Amf0Value subValue;
                if (!parseAmf0Value(data, pos, subValue)) return false;
                value.array.push_back(subValue);
            }
            return true;
        default:
            std::cerr << "Unsupported AMF0 type: " << (int)type << std::endl;
            return false;
    }
}

bool RtmpClient::parseAmf0Response(const std::vector<uint8_t>& response, Amf0Value& result) {
    size_t pos = 12; // 跳过 RTMP 头部
    result.type = Amf0Value::ARRAY;
    while (pos < response.size()) {
        Amf0Value value;
        if (!parseAmf0Value(response, pos, value)) return false;
        result.array.push_back(value);
    }
    return true;
}

bool RtmpClient::sendChunkedData(const std::vector<uint8_t>& data, uint32_t timestamp, uint8_t type, uint32_t streamId, size_t chunkSize) {
    size_t sent = 0;
    bool firstChunk = true;
    while (sent < data.size()) {
        std::vector<uint8_t> packet;
        size_t chunkDataSize = std::min(chunkSize, data.size() - sent);

        // RTMP Chunk 头部
        if (firstChunk) {
            // Type 0: 完整头部
            packet.push_back(0x00 | 0x03); // Chunk Stream ID = 3
            packet.push_back((timestamp >> 16) & 0xFF);
            packet.push_back((timestamp >> 8) & 0xFF);
            packet.push_back(timestamp & 0xFF);
            size_t length = data.size();
            packet.push_back((length >> 16) & 0xFF);
            packet.push_back((length >> 8) & 0xFF);
            packet.push_back(length & 0xFF);
            packet.push_back(type); // Message Type ID (8: Audio, 9: Video)
            packet.push_back(streamId & 0xFF);
            packet.push_back((streamId >> 8) & 0xFF);
            packet.push_back((streamId >> 16) & 0xFF);
            packet.push_back((streamId >> 24) & 0xFF);
            firstChunk = false;
        } else {
            // Type 3: 续传头部
            packet.push_back(0xC0 | 0x03); // Chunk Stream ID = 3, Type 3
        }

        // 数据
        packet.insert(packet.end(), data.begin() + sent, data.begin() + sent + chunkDataSize);
        sent += chunkDataSize;

        if (!sendPacket(packet)) return false;
    }
    return true;
}

bool RtmpClient::readAndSendFlv(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open FLV file: " << filePath << std::endl;
        return false;
    }

    // 跳到上次推流的位置
    file.seekg(fileOffset_);

    // 跳过 FLV 头部 (9 字节)
    if (fileOffset_ == 0) {
        char header[9];
        file.read(header, 9);
        if (!file || std::string(header, 3) != "FLV") {
            std::cerr << "Invalid FLV file" << std::endl;
            return false;
        }
        fileOffset_ = 9;
    }

    // 读取第一个 PreviousTagSize
    uint32_t prevTagSize;
    file.read(reinterpret_cast<char*>(&prevTagSize), 4);
    fileOffset_ += 4;
    if (!file) return true; // 文件结束

    bool firstTag = true;
    while (file) {
        // 读取 Tag 头部 (11 字节)
        char tagHeader[11];
        file.read(tagHeader, 11);
        if (!file) break;
        fileOffset_ += 11;

        uint8_t tagType = tagHeader[0];
        uint32_t dataSize = (tagHeader[1] << 16) | (tagHeader[2] << 8) | tagHeader[3];
        uint32_t timestamp = (tagHeader[4] << 16) | (tagHeader[5] << 8) | tagHeader[6];
        timestamp |= (tagHeader[7] << 24); // Timestamp Extended

        // 读取 Tag 数据
        std::vector<uint8_t> tagData(dataSize);
        file.read(reinterpret_cast<char*>(tagData.data()), dataSize);
        if (!file) break;
        fileOffset_ += dataSize;

        // 读取 PreviousTagSize
        file.read(reinterpret_cast<char*>(&prevTagSize), 4);
        if (!file) break;
        fileOffset_ += 4;

        // 设置基准时间戳
        if (firstTag) {
            baseTimestamp_ = timestamp;
            startTime_ = std::chrono::steady_clock::now();
            firstTag = false;
        }

        // 计算相对时间戳
        uint32_t relativeTimestamp = timestamp - baseTimestamp_;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime_).count();
        if (relativeTimestamp > elapsed) {
            std::this_thread::sleep_for(std::chrono::milliseconds(relativeTimestamp - elapsed));
        }

        // 发送 Tag 数据（分片）
        if (!sendChunkedData(tagData, timestamp, tagType, streamId_)) {
            std::cerr << "Failed to send Tag data, retrying..." << std::endl;
            if (reconnect()) {
                file.seekg(fileOffset_ - (11 + dataSize + 4)); // 回退到当前 Tag
                continue;
            }
            return false;
        }
    }

    file.close();
    return true;
}