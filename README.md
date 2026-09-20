# ReactorNet

基于 epoll 的 C++17 Reactor 模式网络库，从零实现，不依赖任何网络框架。

单线程 Reactor：`EventLoop` 是唯一接触 `epoll` 的人，其余组件通过 `Channel` 向它注册自己关心的事件与回调。

## 已完成

- **EventLoop / Channel** — 单线程 Reactor 核心，epoll 水平触发（LT）
- **Acceptor** — 非阻塞 `accept4`，循环 `accept` 到 `EAGAIN`
- **TcpConnection** — 单连接的读 / 写 / 关闭：一直读到 `EAGAIN`；写不完的部分进输出缓冲，并临时挂上 `EPOLLOUT`
- **TcpServer** — 组装 Acceptor 与连接表，用 `shared_ptr` 管理连接生命周期
- **echo_server 示例** — 业务逻辑十几行的 echo 服务

## 快速开始

```bash
./build.sh                      # 一键配置 + 构建
./build/echo_server             # 起 echo server（端口 8888）
echo hi | nc -N 127.0.0.1 8888  # 另开一个终端：原样打回 hi（-N：收到 EOF 后关闭写端，nc 才会退出）
ctest --test-dir build          # 跑全部单测
```

`echo_server` 的全部业务逻辑就是 `conn->send(msg)` 这一行 —— epoll 的细节都藏在库里面。

## 测试

| 测试 | 覆盖 |
|---|---|
| `test_channel` | Channel 的注册与事件回调 |
| `test_acceptor` | 非阻塞 accept（端口传 0 → 让内核分配，避免端口冲突） |
| `test_tcpconn` | TcpConnection 读写回环 |
| `test_echo` | TcpServer 全链路 echo + 连接无 fd 泄漏 |

## 实现要点

- **水平触发（LT）**：不设 `EPOLLET`。读要循环到 `EAGAIN`（LT 下不会丢数据，最坏只是多跑一轮 `epoll_wait`）；写完之后立即 `disableWriting()` —— 让 `EPOLLOUT` 常开会使 `epoll_wait` 空转、白烧 CPU。
- **非阻塞 accept**：用 `accept4(SOCK_NONBLOCK | SOCK_CLOEXEC)`。省一次 `fcntl`，没有「accept 返回到设成非阻塞之间」的竞态窗口，新 fd 也不会泄漏给子进程。
- **延迟删除**：连接不能在自己的回调栈上析构自己。关闭回调只把删除任务排进 `queueInLoop`，等本次事件派发全部结束、栈安全退出后再执行，避免 use-after-free。
- **打破 `shared_ptr` 自引用环**：连接的回调按值捕获了 `shared_ptr<自身>`，而这两个回调又存在连接自己的成员里 —— 引用计数永远归不了零，`~TcpConnection` 永不执行，`::close(fd)` 永不发生，于是**每条关闭的连接泄漏一个 fd**。解法是断开时先把回调置空。这个 bug 功能性断言抓不到（泄漏不影响 echo 的正确性），`test_echo` 里用 `countFds()` 数 `/proc/self/fd` 的条目数来断言。

## 本项目不做的事

- 多线程 / 线程池（只做单线程 Reactor 内核）
- 协议解析（库只搬运字节流、不分帧，粘包由应用层自理）
- 压测 / 性能基准

## 环境

Linux / C++17 / CMake 3.16+ / g++ 13
