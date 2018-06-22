<?php
use su\co as co;
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
