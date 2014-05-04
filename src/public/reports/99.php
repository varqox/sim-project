<?php
if(isset($_GET['download']))
{header('Content-type: application/text');header('Content-Disposition: attchment; filename="99.cpp"');readfile($_SERVER['DOCUMENT_ROOT']."/../solutions/99.cpp");exit;}


require_once $_SERVER['DOCUMENT_ROOT']."/../php/main.php";

template_begin('Zgłoszenie 99');

if(isset($_GET['source']))
{
echo '<div style="margin: 60px 0 0 50px">', shell_exec($_SERVER['DOCUMENT_ROOT']."/../judge/CTH ".$_SERVER['DOCUMENT_ROOT']."/../solutions/99.cpp"), '
</div>';
template_end();
exit;
}

echo '<div style="text-align: center">
<div class="report-info">
<h1>Zgłoszenie 99</h1>
<div class="btn-toolbar">
<a class="btn-small" href="?source">View source</a>
<a class="btn-small" href="?download">Download</a>
</div>
<h2>Zadanie 1</h2>
</div>
<div class="submit-status">
<pre>Score: 0/100
Status: Judged</pre>
<table style="margin-top: 5px" class="table results">
<thead>
<tr>
<th style="min-width: 80px">Test</th>
<th style="min-width: 190px">Result</th>
<th style="min-width: 100px">Time</th>
<th style="min-width: 70px">Result</th>
</tr>
</thead>
<tbody>
<tr>
<td>1</td>
<td class="wa">Wrong answer</td>
<td>0.00/3.00</td>
<td class="groupscore">0/50</td>
</tr>
<tr>
<td>2</td>
<td class="wa">Wrong answer</td>
<td>0.00/3.00</td>
<td class="groupscore">0/50</td>
</tr>
</tbody>
</table>
</div>
</div>';

template_end();
?>