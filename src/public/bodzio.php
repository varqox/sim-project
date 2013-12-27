<?php
$user="guest";

include $_SERVER['DOCUMENT_ROOT']."/kit/main.php";

template_begin('Trololo',$user,'','.separate{border-bottom: 1px solid #a2a2a2;}');

echo '
<center style="font-size: 50px">
Zostałeś zbodziony! :P
</center>';

template_end();
?>