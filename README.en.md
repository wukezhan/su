# Su Framework

## 1. Overview

`Su Framework` is a concurrent & high-performance PHP Framework with full-featured coroutines & asynchronous I/O, delivered as C extension. That we may say:

> Su Framework = PHP（syntax） + Go（goroutine & channel） + Node.js（asynchronous & event-driven）

## 2. Features

- Coroutine
    - Coroutine: su\co (su\coroutine)
    - Channel: su\chan (su\channel)
- Asynchronous I/O
    - Networking
        - TCP server & TCP client
        - UDP server & UDP client
        - HTTP server & HTTP client
        - WebSocket server
    - File
- Process manager
    - Master-worker processes
    - Ipc
    - Gracefully restart & upgrade
- High resolution clock
    - Clock with milliseconds
- ...

More features coming in the version 1.0.0

## 3. Examples

### 3.1 Coroutines

#### 3.1.1 Coroutine

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
[Click to check the result](https://asciinema.org/a/141601)

#### 3.1.2 Coroutine & Channel

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
[Click to check the result](https://asciinema.org/a/141602)

#### 3.1.3 Coroutine & Sleep

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
[Click to check the result](https://asciinema.org/a/141598)

#### 3.1.4 Timer & Coroutine & Channel

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
[Click to check the result](https://asciinema.org/a/141604)


