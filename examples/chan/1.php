<?php
use su\chan;

$chan = new chan();
var_dump($chan->is_full());
$chan->send(1);
var_dump($chan->is_full());