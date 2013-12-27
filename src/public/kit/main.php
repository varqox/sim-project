<?php

function template_begin($title, $user, $scripts='', $styles='')
{
echo '<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>',$title,'</title>
<link rel="stylesheet" href="/kit/styles.css">
<script src="/kit/scripts.js"></script>
<script> var start_time=', round(microtime(true) * 1000), ', load=-1, time_difference;</script>
<link rel="shortcut icon" href="/kit/img/favicon.png">',(strlen($scripts)==0 ? '':"\n<script>".$scripts.'</script>'),(strlen($styles)==0 ? '':"\n<style>".$styles.'</style>'),'
</head>
<body onload="updateClock()">
<div class="navbar">
<div class="navbar-body">
<a href="/" class="brand">SIM</a>
<a href="/contests.php">Konkursy</a>
<a href="/admin.php">Admin</a>
<a href="/files/">Files</a>
<a href="/reports/">Reports</a>
<div class="dropdown">
<span id="clock"></span>
<a onclick="f(this);" class="user"><strong>',$user,'</strong><b class="caret"></b></a>
<ul>
<li><a href="/index.php">logout</a></li>
<li><a href="/bodzio.php">bodzio</a></li>
<li><a href="/add_report.php">Submit a solution</a></li>
<li><a href="/test.php">test</a></li>
<li><a href="/trol.php">troll</a></li>
</ul>
</div>
</div>
</div>
<div class="body-main">

';
}

function template_end()
{
echo '

</div>
</body>
</html>';
}
?>