<?php
use su\co as co;
define('ST', time());
function timeline($msg) {
    echo "[", (time()-ST), "] ", $msg, "\n";
}
co::run(function() {
    echo "2 start\n";
    co::sleep(2000);
    echo "2 ok\n";
});

echo "1 start\n";
co::sleep(1);
echo "1 ok\n";