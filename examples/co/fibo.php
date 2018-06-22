<?php
use su\co;
use su\chan;

$ca = new chan();
$cb = new chan();

co::run(function() use($ca, $cb) {
    // co fibo
    $current = 1;
    $previous = 0;
    for ($i=0;;$i++) {
        $ca->send($current);
        if ($cb->recv() == 0) {
            break;
        }
        $tmp = $current;
        $current = $previous + $current;
        $previous = $tmp;
    }
});

for($i=1; $i<=12; $i++) {
    $a = $ca->recv();
    echo "fibo {$i}: {$a}\n";
    $cb->send(1);
}
$cb->send(0);
echo "all is ok\n";
