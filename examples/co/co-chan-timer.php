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
