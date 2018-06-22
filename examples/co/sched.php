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
