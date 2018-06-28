<?php
use su\timer;

$t = time();
define('ST', $t);

$t1 = new timer();
$t1->after(1000, function () {
    echo "time 1: ",(time() - ST), "\n";
});

$t2 = new timer();
$t2->after(2000, function () {
    echo "time 2: ",(time() - ST), "\n";
});