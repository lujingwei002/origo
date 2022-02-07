# origo


# 下载依赖
```bash
cd dep
make fetch
make
make install
```
# 编译
```bash
mkdir build
cd build
cmake ..
make
```
#启动
```bash
#指定配置文件
gate -c gate.conf 
#后台启动
gate -c gate.conf -d
```
#停止
```bash
gate -c gate.conf -s stop
```

#重载配置文件
```bash
gate -c gate.conf -s reload
```

