# Su Framework

## 1. 概览

`Su Framework`（`速框架`），是一个全方位支持协程，且同时提供异步非阻塞IO的、使用C扩展实现的高性能 PHP 框架。

从底层技术及功能特性上，可以用如下的等式来概括 `Su` 框架：

> Su Framework = PHP（syntax） + Go（goroutine & channel） + Node.js（asynchronous & event-driven）

## 2. 特性

- 协程支持
    - 协程：su\co (su\coroutine)
    - 通道：su\chan (su\channel)
- 异步非阻塞I/O
    - 网络
        - TCP server & TCP client
        - UDP server & UDP client
        - HTTP server & HTTP client
        - WebSocket server
    - 文件
- 进程管理
    - master-worker 进程模式
    - 进程间通信管道
    - 完全平滑重启
- 高分辨率定时器
    - 毫秒级定时器
- ...

其中上述有链接部分为抢先预览版功能，更多重大功能将在正式版本中正式发布，敬请期待！

### 参与贡献

如果你对 `Su Framework` 感兴趣，欢迎通过以下的方式进行支持与贡献：

- 通过创建 [Issues](https://github.com/wukezhan/su/issues/new) 提交你遇到的问题
- 通过发起 [Pull Requests](https://github.com/wukezhan/su/pulls) 贡献代码
- 通过发起 [Pull Requests](https://github.com/wukezhan/su/pulls) 完善和优化文档（中/英文，尤其是英文文档支持）
- 贡献示例 & 教程

## 3. 示例

### 3.1 协程特性

#### 3.1.1 协程

```php
<?php
use su\co;
co::run(function() {
    $a = 1;
    while ($a <= 10) {
        echo "co a: {$a}\n";
        $a += 2;
        co::sched();
    }
});

co::run(function() {
    $b = 2;
    while ($b <= 10) {
        echo "co b: {$b}\n";
        co::sched();
        $b += 2;
    }
});
```
[猛击此处围观运行过程](https://asciinema.org/a/141601)

#### 3.1.2 协程 & 通道

```php
<?php
use su\co;
use su\chan;

$ca = new chan();
$cb = new chan();

co::run(function() use($ca, $cb) {
    // co fibo
    $current = 1;
    $previous = 0;
    for ($i=0; ; $i++) {
        $ca->send($current);
        if ($cb->recv() == 0) {
            break;
        }
        $tmp = $current;
        $current = $previous + $current;
        $previous = $tmp;
    }
});

co::run(function() use($ca, $cb) {
    // co caller
    for($i=1; $i<10; $i++) {
        $a = $ca->recv();
        echo "fibo {$i}: {$a}\n";
        $cb->send(1);
    }
    $cb->send(0);
    echo "all is ok\n";
});
```
[猛击此处围观运行过程](https://asciinema.org/a/141602)

#### 3.1.3 协程 & Sleep

```php
<?php
use su\co;
define('ST', time());
function timeline($msg) {
    echo "[", (time()-ST), "] ", $msg, "\n";
}

co::run(function() {
    $a = 1;
    while ($a <= 10) {
        timeline("co a: {$a}");
        $a += 2;
        co::sleep(1000);
    }
});

co::run(function() {
    $b = 2;
    while ($b <= 10) {
        timeline("co b: {$b}");
        co::sleep(1000);
        $b += 2;
    }
});
```
[猛击此处围观运行过程](https://asciinema.org/a/141598)

#### 3.1.4 定时器 & 协程 & 通道

```php
<?php
use su\co;
use su\chan;
use su\timer;

$chan = new chan();
$t1 = new timer();
$t1->after(1000, function() use($chan) {
    $msg = "timer is ok";
    echo "timer: {$msg}\n";
    $chan->send($msg);
});

co::run(function() use($chan) {
    $msg = $chan->recv();
    echo "co: {$msg}\n";
});
```
[猛击此处围观运行过程](https://asciinema.org/a/141604)


