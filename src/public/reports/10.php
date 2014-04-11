<?php
if(isset($_GET['download']))
{header('Content-type: application/text');header('Content-Disposition: attchment; filename="10.cpp"');readfile($_SERVER['DOCUMENT_ROOT']."/../solutions/10.cpp");exit;}

$user="none";
require_once $_SERVER['DOCUMENT_ROOT']."/kit/main.php";

template_begin('Zgłoszenie 10',$user);

if(isset($_GET['source']))
{
echo '<div style="margin: 60px 0 0 50px">', shell_exec($_SERVER['DOCUMENT_ROOT']."/../judge/CTH ".$_SERVER['DOCUMENT_ROOT']."/../solutions/10.cpp"), '
</div>';
template_end();
exit;
}

echo '<div style="text-align: center">
<div class="report-info">
<h1>Zgłoszenie 10</h1>
<div class="btn-toolbar">
<a class="btn-small" href="?source">View source</a>
<a class="btn-small" href="?download">Download</a>
</div>
<h2>Zadanie 2</h2>
</div>
<div class="submit-status">
<pre>Status: Compilation failed</pre>
<pre>Points: 0<pre>
<pre>10.cpp: In function \'int main()\':
10.cpp:20:16: error: \'system\' was not declared in this scope
 system ("pause");
                ^
</pre>
</div>
</div>';

template_end();
?>