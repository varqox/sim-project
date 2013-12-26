<?php
$path="./";
$user="guest";

require_once $_SERVER['DOCUMENT_ROOT']."/kit/main.php";

template_begin('Wysyłanie rozwiązań',$user);

echo '<div style="text-align: center">
<img src="/kit/img/SIM-logo.png" width="20%" height=20%"/>
<p style="font-size: 30px">W trakcie tworzenia...</p>
<hr/>
<p>Witam na stronie głównej serwisu SIM używanego do przeprowadzania konkursów w programowaniu</p>
</div>';

template_end();
?>
