<?php
$path="./";


require_once $_SERVER['DOCUMENT_ROOT']."/../php/main.php";

template_begin('SIM - Platforma do trzaskania zadań');

echo '<div style="text-align: center">
<img src="/kit/img/SIM-logo.png" width="260" height="336" alt=""/>
<p style="font-size: 30px">Welcome to SIM</p>
<hr/>
<p>SIM is open source platform for carrying out algorithmic contests</p>
</div>';

template_end();
?>
