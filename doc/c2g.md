
# 一.流程图
~~~ sequence
前端->网关: handshake(1)
网关-->前端: handshake(1)
Note right of 网关:握手
前端-->网关: handshake_ack(2)

前端->网关: data(3)
网关->前端: data(3)
前端->网关: heartbeat(4)
网关->前端: heartbeat(4)
网关->前端: kick(5)
网关->前端: down(6)
网关->前端: maintain(7)
~~~

# 二.协议结构
```bash
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-------+---------------+-------------------------------+
|F|R|R|R| Opcode|   Reserve     |    Payload Data               |
|I|S|S|S|  (4)  |     (8)       |                               |
|N|V|V|V|       |               |                               |
| |1|2|3|       |               |                               |
+-+-+-+-+-------+---------------+ - - - - - - - - - - - - - - - +
|                     Payload Data                              | 
+-------------------------------- - - - - - - - - - - - - - - - +
:                     Payload Data continued ...                :
+ - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
|                     Payload Data continued ...                |
+---------------------------------------------------------------+
```

如果是tcp的话，前面还有2字节的消息包长度，不包括自身

如果是websocket的话，用websocket本身的消息头

* FIN：帧结束位
* RSV：保留位
* Opcode：类型

  1. 握手
  2. 握手确认
  3. 数据帧
  4. 心跳
  5. 踢下线
  6. 服务器关闭
  7. 服务器维护
* Reserve：保留位
* Payload Data：数据


# Payload

## 1.握手

```json
//格式是JSON串
request {
    "path":"/server1",
    "password":"xxx",
    "secret": "随机生成的字符串"
}
/*
path: 路径,比如用来选择服务器
password: 密码
secret: 随机生成的字符串,使用rsa加密
*/
response {
    "code":200,
    "heartbeat":60
}
/*
code: 	200 OK
		400 BAD_REQUEST
		401 Unauthorized
		402	Payment Required
		403 Forbidden
		404 NOT FOUND 
*/
```

## 2.握手确认

```json
无
```

## 3.数据帧

```json
sproto
```

## 4.心跳

```json
发送间隔是握手时返回的
```

## 5.踢下线

```json
字符串，描述原因
```

## 6.服务器关闭

```json
字符串，描述原因
```

## 7.服务器维护

```json
字符串，描述原因
```

