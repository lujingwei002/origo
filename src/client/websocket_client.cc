#include "client/websocket_client.h"
#include "server.h"
#include "client.h"
#include "gate.h"
#include "encrypt/sha1.h"
#include "encrypt/base64.h"
#include <cstdio>
#include <cstring>
#include <iostream>
#include <locale> // std::locale std::tolower
#include <algorithm>    // std::transform
#include<netinet/in.h> // ntohs

/*

 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-------+-+-------------+-------------------------------+
|F|R|R|R| opcode|M| Payload len |    Extended payload length    |
|I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
|N|V|V|V|       |S|             |   (if payload len==126/127)   |
| |1|2|3|       |K|             |                               |
+-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
|     Extended payload length continued, if payload len == 127  |
+ - - - - - - - - - - - - - - - +-------------------------------+
|                               |Masking-key, if MASK set to 1  |
+-------------------------------+-------------------------------+
| Masking-key (continued)       |          Payload Data         |
+-------------------------------- - - - - - - - - - - - - - - - +
:                     Payload Data continued ...                :
+ - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
|                     Payload Data continued ...                |
+---------------------------------------------------------------+

*/
class Server;
class Agent;

enum websocket_frame_type {
    websocket_frame_type_close = 8,
    websocket_frame_type_text = 1,
    websocket_frame_type_binary = 2,
    websocket_frame_type_ping = 9,
    websocket_frame_type_pong = 10,
};

struct websocket_frame_header {
    uint8_t opcode:4;
    uint8_t rsv:3;
    uint8_t fin:1;
    uint8_t payloadLen:7;
    uint8_t mask:1;
};
const int websocket_frame_header_max_size = 16;

int frame_pack(char* frame, int opcode, const char* data, size_t len) {
    websocket_frame_header header;
    header.fin = 1;//结束帧
    header.rsv = 0;
    header.opcode = opcode;
    header.mask = 0;//没有掩码
    if (len >= 0xffff) {
        gate->logError("send frame too large");
        return 0;
    } else if(len >= 126) {
        header.payloadLen = 126;
        char* offset = (char*)frame;
        *(websocket_frame_header*)offset = header; offset += sizeof(header);
        *((uint8_t*)offset) = (len>>8)&0xff; offset += sizeof(uint8_t);
        *((uint8_t*)offset) = (len   )&0xff; offset += sizeof(uint8_t);
        memcpy(offset, data, len); offset += len;
        return sizeof(header) + sizeof(uint16_t) + len;
    } else {
        header.payloadLen = len;
        char* offset = (char*)frame;
        *(websocket_frame_header*)offset = header; offset += sizeof(header);
        memcpy(offset, data, len); offset += len;
        return sizeof(header) + len;
    }
    return 0;
}

static int on_message_begin(http_parser* parser){
    //this->client->logDebug("on_message_begin");
    return 0;
}

static int on_url(http_parser* parser, const char *at, size_t length){
    //gate->logDebug("on_url length:%ld", length);
    WebsocketClient* handler = (WebsocketClient*)parser->data;
    handler->url.assign(at, length);
    http_parser_url urlParser;
    http_parser_url_init(&urlParser);
    http_parser_parse_url(at, length, 1, &urlParser);
    if(urlParser.field_set & (1 << UF_PATH)) {
        handler->path.assign(at + urlParser.field_data[UF_PATH].off, urlParser.field_data[UF_PATH].len);
    }
   /* HttpRequest* request = (HttpRequest*)parser->data;*/
    //if(urlParser.field_set & (1 << UF_PATH)) {
        //request->Path.assign(at + urlParser.field_data[UF_PATH].off, urlParser.field_data[UF_PATH].len);
    //}
    //if(urlParser.field_set & (1 << UF_QUERY)) {
        //request->Query.assign(at + urlParser.field_data[UF_QUERY].off, urlParser.field_data[UF_QUERY].len);
    //}
    //if(urlParser.field_set & (1 << UF_SCHEMA)) {
        //request->Schema.assign(at + urlParser.field_data[UF_SCHEMA].off, urlParser.field_data[UF_SCHEMA].len);
    //}
    //if(urlParser.field_set & (1 << UF_HOST)) {
        //request->Host.assign(at + urlParser.field_data[UF_HOST].off, urlParser.field_data[UF_HOST].len);
   /* }*/
    return 0;
}

static int on_status(http_parser* parser, const char *at, size_t length){
    //gate->logDebug("on_status length:%d", length);
    return 0;
}

static int on_header_field(http_parser* parser, const char *at, size_t length){
    //gate->logDebug("on_header_field length:%d", length);
    WebsocketClient* handler = (WebsocketClient*)parser->data;
    handler->headerParsing.assign(at, length);
    return 0;
}

static int on_header_value(http_parser* parser, const char *at, size_t length){
    WebsocketClient* handler = (WebsocketClient*)parser->data;
    //HttpRequest* request = (HttpRequest*)parser->data;
    std::string value(at, length);
    std::string field = handler->headerParsing;
    std::transform(field.begin(), field.end(), field.begin(), ::tolower);
    handler->onHeaderValue(field, value);
    //std::transform(value.begin(), value.end(), value.begin(), ::tolower);
    //request->headerDict[field] = value;
    //this->client->logDebug("on_header_value %s:%s", field.c_str(), value.c_str());
    return 0;
}

static int on_headers_complete(http_parser* parser){
    //gate->logDebug("on_headers_complete");
    //HttpRequest* request = (HttpRequest*)parser->data;
    //request->Method.assign(http_method_str((http_method)parser->method));
    //std::transform(request->Method.begin(), request->Method.end(), request->Method.begin(), ::toupper);
    //request->isUpgrade = parser->upgrade == 1 ? true : false;
    return 0;
}

static int on_body(http_parser* parser, const char *at, size_t length){
    //HttpRequest* request = (HttpRequest*)parser->data;
    //request->Body.append(at, length);
    //coord::Append(request->payload, at, length);
    //coord::Append(request->payload, 0);
    //gate->logDebug("on_body length:%ld", length);
    return 0;
}

static int on_message_complete(http_parser* parser){
    //gate->logDebug("on_message_complete");
    WebsocketClient* handler = (WebsocketClient*)parser->data;
    handler->onMessageComplete();
    //HttpRequest* request = (HttpRequest*)parser->data;
    //request->isComplete = true;
    return 0;
}

static int on_chunk_header(http_parser* parser){
    //gate->logDebug("on_chunk_header");
    return 0;
}

static int on_chunk_complete(http_parser* parser){
    //gate->logDebug("on_chunk_complete");    
    return 0;
}

WebsocketClient* NewWebsocketClient(Gate* gate, Server* server, Client* client) {
    WebsocketClient* self = new WebsocketClient(gate, server, client);
    return self;
}

WebsocketClient::WebsocketClient(Gate* gate, Server* server, Client* client) {
    this->gate = gate;
    this->server = server;
    this->client = client;
    this->wantUpgrade = false;
    this->isUpgrade = false;

    http_parser_init(&this->parser, HTTP_REQUEST);
    this->parser.data = this;
    http_parser_settings_init(&this->setting);
    this->setting.on_message_begin = on_message_begin;
    this->setting.on_url = on_url;
    this->setting.on_status = on_status;
    this->setting.on_header_field = on_header_field;
    this->setting.on_header_value = on_header_value;
    this->setting.on_headers_complete = on_headers_complete;
    this->setting.on_body = on_body;
    this->setting.on_message_complete = on_message_complete;
    this->setting.on_chunk_header = on_chunk_header;
    this->setting.on_chunk_complete = on_chunk_complete;
    if (server->config.net == "wss") {
        this->ssl = SSL_new(this->server->sslCtx);
        this->read_bio = BIO_new(BIO_s_mem());
        this->write_bio = BIO_new(BIO_s_mem());
        SSL_set_bio(this->ssl, this->read_bio, this->write_bio);
        SSL_set_accept_state(this->ssl);
    } else {
        this->ssl = nullptr;
        this->read_bio = nullptr;
        this->write_bio = nullptr;
    }
}

WebsocketClient::~WebsocketClient() {
    if (nullptr != this->ssl) {
        SSL_free(this->ssl);
    }
}

void WebsocketClient::onMessageComplete() {
    static thread_local char secret[20];
    static thread_local char acceptKey[32]; // ceil(20/3)*4
    static thread_local char dateStr[128];
    static thread_local char buffer[1024];
    if (!this->wantUpgrade) {
        this->client->Close();
        return;
    }
    size_t acceptKeyLen;
    std::string randomString = this->secWebsocketKey;
    randomString.append("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    Sha1Encode(secret, randomString.c_str(), randomString.length());
    int err = Base64Encode(secret, sizeof(secret), acceptKey, &acceptKeyLen);
    if (err) {
        this->client->Close();
        return;
    }
    acceptKey[acceptKeyLen] = 0;
    // 计算日期 
    time_t t = time(NULL);
    struct tm* tmp = localtime(&t);
    strftime(dateStr, sizeof(dateStr), "%a, %d %b %Y %H:%M:%S GMT", tmp);
    
    byte_array* response = this->client->WillSend(1024);
    if (nullptr == response) {
        this->client->Close();
        return;
    }
    int r = snprintf(buffer, sizeof(buffer), "HTTP/1.1 101 Switching Protocol\r\n\
Connection: Upgrade\r\n\
Upgrade: WebSocket\r\n\
Access-Control-Allow-Credentials: true\r\n\
Access-Control-Allow-Origin: *\r\n\
Access-Control-Allow-Headers: Content-Type\r\n\
Sec-WebSocket-Accept: %s\r\n\
Date:%s\r\n\r\n", acceptKey, dateStr);
    if (r < 0) {
        this->client->Close();
        return;
    }
    this->isUpgrade = true;
    this->sendResponse(buffer, r);
}

void WebsocketClient::onHeaderValue(std::string& field, std::string& value) {
    if (field == "connection") {
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        if(value == "upgrade") {
            this->wantUpgrade = true;
        }
    } else if (field == "sec-websocket-key") {
        this->secWebsocketKey = value;
    }
}

int WebsocketClient::Unpack(const char* data, size_t len) {
    if (nullptr != this->ssl) {
        return this->recvEncryptData(data, len);
    } else {
        return this->recvData(data, len);
    }
}

int WebsocketClient::recvEncryptData(const char* data, size_t len) {
    //printf("recvEncryptData %ld\n", len);
    BIO_write(this->read_bio, data, len);
    if (!SSL_is_init_finished(this->ssl)) {
        int ret = SSL_accept(this->ssl);
        if (ret != 1) {
            int err = SSL_get_error(this->ssl, ret);
            if (err == SSL_ERROR_WANT_READ) {
            } else if(err == SSL_ERROR_WANT_WRITE) {
            }
        } else {
            this->readDecryptData();
        }
        this->writeBioToSocket();
    } else {
        this->readDecryptData();
    }
    return len;
}

void WebsocketClient::readDecryptData() {
    static thread_local char data[10240];
    for(;;) {
        int byteRead = SSL_read(this->ssl, data, sizeof(data));
        //printf("readDecryptData %d\n", byteRead);
        if (byteRead < 0) {
            int err = SSL_get_error(this->ssl, byteRead);
            if (err == SSL_ERROR_WANT_READ) {
            } else if(err == SSL_ERROR_WANT_WRITE) {
                this->writeBioToSocket();
            }
            break;
        } else if(byteRead == 0) {
            break;
        } else {
            this->recvData(data, byteRead);
            break;
        }
    }
}

int WebsocketClient::writeBioToSocket() {
    static thread_local char data[10240];
    for(;;) {
        int byteRead = BIO_read(this->write_bio, data, sizeof(data));
        //printf("writeBioToSocket %d/%d\n", byteRead, sizeof(data));
        if (byteRead <= 0) {
            break;
        }
        int err = this->send(data, byteRead);
        if (err) {
            return err;
        }
    }
    return 0;
}

int WebsocketClient::recvData(const char* data, size_t len) {
    if (!this->isUpgrade) {
        this->client->logDebug("[websocket] recv http request\n%s", data);
        const char* header = strstr(data, "\r\n\r\n");
        if(header == NULL){
            return 0;
        }
        int ir = http_parser_execute(&this->parser, &this->setting, data, len);
        return ir;
    } else {
        websocket_frame_header* header = (websocket_frame_header*)data;
        uint64_t realHeaderLen = sizeof(websocket_frame_header);
        uint64_t realPayloadLen = header->payloadLen;
        uint64_t realFrameLen = 0;
        //掩码
        unsigned char *mask = 0;
        //负载
        char *payloadData = 0;
        if (header->payloadLen == 126){
            realHeaderLen = sizeof(websocket_frame_header) + 2;
        }
        else if (header->payloadLen == 127){
            realHeaderLen = sizeof(websocket_frame_header) + 8;
        }
        if (header->mask == 1){
            mask = (unsigned char *)data + realHeaderLen;
            realHeaderLen += 4;
        }
        //负载
        payloadData = (char *)data + realHeaderLen;
        //测试数据长度
        if (len < realHeaderLen){
            return 0;
        }
        //解释负载长度
        if (header->payloadLen == 126){
            //2个字节的长度
            realPayloadLen = ntohs(*((uint16_t*)(data + sizeof(websocket_frame_header))));
        }
        else if (header->payloadLen == 127){
            realPayloadLen = ntohl(*((uint64_t*)(data + sizeof(websocket_frame_header))));
        }
        realFrameLen = realHeaderLen + realPayloadLen;
        //测试数据长度
        if (len < realFrameLen){
            return 0;
        }
        //用掩码修改数据
        if (header->mask == 1){
            for (uint64_t i = 0; i < realPayloadLen; i++) {
                payloadData[i] = payloadData[i] ^ mask[i % 4];
            }
        }
        this->client->logDebug("[websocket] recv a frame, payload len %ld", realPayloadLen);
        switch(header->opcode) {
            case websocket_frame_type_text:
                {
                    this->recvTextFrame(payloadData, realPayloadLen);
                }
                break;
            case websocket_frame_type_close:
                {
                    this->recvCloseFrame();
                }
                break;
            case websocket_frame_type_ping:
                {
                    this->recvPingFrame();
                }
                break;
            case websocket_frame_type_binary:
                {
                    this->recvBinaryFrame(payloadData, realPayloadLen);
                }
                break;
        }
        return realFrameLen;
    }
}

void WebsocketClient::recvTextFrame(const char* data, uint64_t len) {
    this->client->logDebug("[websocket] recvTextFrame");
    this->client->Recv(data, len);
}

void WebsocketClient::recvBinaryFrame(const char* data, uint64_t len) {
    this->client->logDebug("[websocket] recvBinaryFrame");
    this->client->Recv(data, len);
}

void WebsocketClient::recvCloseFrame() {
    this->client->logDebug("[websocket] recvCloseFrame");
}

void WebsocketClient::recvPingFrame() {
    this->client->logDebug("[websocket] recvPingFrame");
}

int WebsocketClient::send(const char* data, size_t len) {
    //printf("send %ld\n", len);
    byte_array* buffer = this->client->WillSend(len);
    if (nullptr == buffer) {
        return -1;
    }
    memcpy(buffer->back(), data, len);
    buffer->write(len);
    return 0;
}

int WebsocketClient::sendResponse(const char* data, size_t len) {
    //printf("sendResponse %ld\n", len);
    if (nullptr != this->ssl) {
        int byteWrite = SSL_write(this->ssl, data, len);
        return this->writeBioToSocket();
    } else {
        byte_array* response = this->client->WillSend(len);
        if (nullptr == response) {
            return -1;
        }
        memcpy(response->back(), data, len);
        response->write(len);
        return 0;
    }
}

int WebsocketClient::Pack(const char* data, size_t len) {
    if (len >= 0xffff) {
        gate->logError("pack frame too large");
        return -1;
    }
    if (nullptr != this->ssl) {
        static thread_local char buffer[0xffff];
        int frameLen = frame_pack(buffer, websocket_frame_type_binary, data, len);
        if (frameLen <= 0) {
            return -1;
        }
        this->client->logDebug("[websocket] pack, len:%d", frameLen);
        int byteWrite = SSL_write(this->ssl, buffer, frameLen);
        return this->writeBioToSocket();
    } else {
        //printf("sendFrame %ld\n", len);
        byte_array* frame = this->client->WillSend(websocket_frame_header_max_size + len);
        if (nullptr == frame) {
            return -1;
        }
        int frameLen = frame_pack(frame->back(), websocket_frame_type_binary, data, len);
        if (frameLen <= 0) {
            return -1;
        }
        this->client->logDebug("[websocket] pack, len:%d", frameLen);
        frame->write(frameLen);
        return 0;
    }
}


