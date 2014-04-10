<?php
if(isset($_GET['download']))
{header('Content-type: application/text');header('Content-Disposition: attchment; filename="7.cpp"');readfile($_SERVER['DOCUMENT_ROOT']."/../solutions/7.cpp");exit;}

$user="none";
require_once $_SERVER['DOCUMENT_ROOT']."/kit/main.php";

template_begin('Zgłoszenie 7',$user);

if(isset($_GET['source']))
{
echo '<div style="margin: 60px 50px">', shell_exec($_SERVER['DOCUMENT_ROOT']."/../judge/CTH ".$_SERVER['DOCUMENT_ROOT']."/../solutions/7.cpp"), '
</div>';
template_end();
exit;
}

echo '<div style="text-align: center">
<div class="report-info">
<h1>Zgłoszenie 7</h1>
<div class="btn-toolbar">
<a class="btn-small" href="?source">View source</a>
<a class="btn-small" href="?download">Download</a>
</div>
<h2>Zadanie 2</h2>
</div>
<div class="submit-status">
<pre>Status: Compilation failed</pre>
<pre>Points: 0<pre>
<pre>7.cpp:1:1: error: \'Pin\' does not name a type
 Pin 2.13 kit 61147
 ^
</pre>
</div>
</div>';

template_end();
?>