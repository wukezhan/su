# Su Framework

## Overview

Su framework is a high performance & asynchronous & concurrent networking framework written as c extension for PHP language.

## Features
- Asynchronous & non-blocking IO
    - TCP
    - File
    - UDP
- Coroutine
    - Coroutine
    - Channel
- Master-worker processes
- High resolution timer

## examples
```php
<?php
use su\co;

co::run(function(){
    $n = 10;
    while ($n>0) {
        echo "co 1: ", $n, "\n";
        co::sched();
        $n = $n - 2;
    }
});

co::run(function(){
    $n = 9;
    while ($n>0) {
        echo "co 2: ", $n, "\n";
        co::sched();
        $n = $n - 2;
    }
});
```
