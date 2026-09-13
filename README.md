# ReactorNet

基于 epoll 的 C++17 Reactor 模式网络库，从零实现，不依赖任何网络框架。

## 已完成

- **EventLoop / Channel** — 单线程 Reactor 核心，epoll 水平触发（LT）
- **Acceptor** — 非阻塞 `accept4`，循环收取到 `EAGAIN`

## 开发中

- **TcpConnection / TcpServer** — 连接生命周期管理
- **echo server 示例 + 压测**

## 构建与测试

```bash
cmake -B build -S .
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## 环境

Linux / C++17 / CMake 3.16+ / g++ 13
