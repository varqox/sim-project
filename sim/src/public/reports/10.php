<?php
if(isset($_GET['download']))
{header('Content-type: application/text');header('Content-Disposition: attchment; filename="10.cpp"');readfile($_SERVER['DOCUMENT_ROOT']."/../solutions/10.cpp");exit;}

$user="none";
require_once $_SERVER['DOCUMENT_ROOT']."/kit/main.php";

template_begin('Zgłoszenie 10',$user);

if(isset($_GET['source']))
{
echo '<div style="margin: 60px 50px">', shell_exec($_SERVER['DOCUMENT_ROOT']."/../judge/CTH ".$_SERVER['DOCUMENT_ROOT']."/../solutions/10.cpp"), '
</div>';
template_end();
exit;
}

echo '<div style="text-align: center">
<p style="padding: 15px 0 0 200px;font-size: 35px;text-align: left">Zgłoszenie 10</p>
<div class ="btn-toolbar">
<a class="btn-small" href="?source">View source</a>
<a class="btn-small" href="?download">Download</a>
</div>
<p style="font-size: 30px;">Zadanie 2</p>
<div class="submit_status">
<pre>Status: Compilation failed</pre>
<pre>Points: 0<pre>
<pre>10.cpp: In function ‘int main()’:
10.cpp:20:16: error: ‘system’ was not declared in this scope
</pre>
</div>
</div>';

template_end();
?>