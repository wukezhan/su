<?php
use su\timer;

$t = time();
define('ST', $t);

$t1 = new timer();
$t1->every(1000, function () use ($t1) {
    $tn = time() - ST;
    echo "every 1: ", $tn, "\n";
    if ($tn >= 4) {
        $t1->close();
    }
});

$t2 = new timer();
$t2->every(1000, function () use ($t2) {
    $tn = time() - ST;
    echo "every 2: ", $tn, "\n";
    if ($tn >= 4) {
        $t2->close();
    }
});