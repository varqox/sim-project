<?php
if(isset($_GET['download']))
{header('Content-type: application/text');header('Content-Disposition: attchment; filename="3.cpp"');readfile($_SERVER['DOCUMENT_ROOT']."/../solutions/3.cpp");exit;}

$user="none";
require_once $_SERVER['DOCUMENT_ROOT']."/kit/main.php";

template_begin('Zgłoszenie 3',$user);

if(isset($_GET['source']))
{
echo '<div style="margin: 60px 0 0 50px">', shell_exec($_SERVER['DOCUMENT_ROOT']."/../judge/CTH ".$_SERVER['DOCUMENT_ROOT']."/../solutions/3.cpp"), '
</div>';
template_end();
exit;
}

echo '<div style="text-align: center">
<div class="report-info">
<h1>Zgłoszenie 3</h1>
<div class="btn-toolbar">
<a class="btn-small" href="?source">View source</a>
<a class="btn-small" href="?download">Download</a>
</div>
<h2>Zadanie 1</h2>
</div>
<div class="submit-status">
<pre>Status: Compilation failed</pre>
<pre>Points: 0<pre>
<pre>3.cpp:1:1: error: \'Pin\' does not name a type
 Pin 2.13 kit 61147
 ^
</pre>
</div>
</div>';

template_end();
?>