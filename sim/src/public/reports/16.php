<?php
if(isset($_GET['download']))
{header('Content-type: application/text');header('Content-Disposition: attchment; filename="16.cpp"');readfile($_SERVER['DOCUMENT_ROOT']."/../solutions/16.cpp");exit;}

$user="none";
require_once $_SERVER['DOCUMENT_ROOT']."/kit/main.php";

template_begin('Zgłoszenie 16',$user);

if(isset($_GET['source']))
{
echo '<div style="margin: 60px 0 0 50px">', shell_exec($_SERVER['DOCUMENT_ROOT']."/../judge/CTH ".$_SERVER['DOCUMENT_ROOT']."/../solutions/16.cpp"), '
</div>';
template_end();
exit;
}

echo '<div style="text-align: center">
<div class="report-info">
<h1>Zgłoszenie 16</h1>
<div class="btn-toolbar">
<a class="btn-small" href="?source">View source</a>
<a class="btn-small" href="?download">Download</a>
</div>
<h2>Zadanie 3</h2>
</div>
<div class="submit-status">
<pre>Score: 90/100
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
<td>hot0</td>
<td class="ok">OK</td>
<td>0.00/1.00</td>
<td class="groupscore">0/0</td>
</tr>
<tr>
<td>hot1a</td>
<td class="ok">OK</td>
<td>0.00/1.00</td>
<td class="groupscore" rowspan="5">10/10</td>
</tr>
<tr>
<td>hot1b</td>
<td class="ok">OK</td>
<td>0.01/1.00</td>
</tr>
<tr>
<td>hot1c</td>
<td class="ok">OK</td>
<td>0.00/1.00</td>
</tr>
<tr>
<td>hot1d</td>
<td class="ok">OK</td>
<td>0.00/1.00</td>
</tr>
<tr>
<td>hot1e</td>
<td class="ok">OK</td>
<td>0.00/1.00</td>
</tr>
<tr>
<td>hot2a</td>
<td class="ok">OK</td>
<td>0.00/1.00</td>
<td class="groupscore" rowspan="2">10/10</td>
</tr>
<tr>
<td>hot2b</td>
<td class="ok">OK</td>
<td>0.00/1.00</td>
</tr>
<tr>
<td>hot3a</td>
<td class="ok">OK</td>
<td>0.00/1.00</td>
<td class="groupscore" rowspan="2">10/10</td>
</tr>
<tr>
<td>hot3b</td>
<td class="ok">OK</td>
<td>0.00/1.00</td>
</tr>
<tr>
<td>hot4a</td>
<td class="ok">OK</td>
<td>0.00/1.00</td>
<td class="groupscore" rowspan="2">10/10</td>
</tr>
<tr>
<td>hot4b</td>
<td class="ok">OK</td>
<td>0.00/1.00</td>
</tr>
<tr>
<td>hot5a</td>
<td class="ok">OK</td>
<td>0.01/1.00</td>
<td class="groupscore" rowspan="2">10/10</td>
</tr>
<tr>
<td>hot5b</td>
<td class="ok">OK</td>
<td>0.00/1.00</td>
</tr>
<tr>
<td>hot6a</td>
<td class="ok">OK</td>
<td>0.03/1.00</td>
<td class="groupscore" rowspan="2">10/10</td>
</tr>
<tr>
<td>hot6b</td>
<td class="ok">OK</td>
<td>0.01/1.00</td>
</tr>
<tr>
<td>hot7a</td>
<td class="ok">OK</td>
<td>0.03/1.00</td>
<td class="groupscore" rowspan="2">10/10</td>
</tr>
<tr>
<td>hot7b</td>
<td class="ok">OK</td>
<td>0.02/1.00</td>
</tr>
<tr>
<td>hot8a</td>
<td class="ok">OK</td>
<td>0.10/3.00</td>
<td class="groupscore" rowspan="2">10/10</td>
</tr>
<tr>
<td>hot8b</td>
<td class="ok">OK</td>
<td>0.03/3.00</td>
</tr>
<tr>
<td>hot9a</td>
<td class="ok">OK</td>
<td>0.18/6.00</td>
<td class="groupscore" rowspan="2">10/10</td>
</tr>
<tr>
<td>hot9b</td>
<td class="ok">OK</td>
<td>0.02/6.00</td>
</tr>
<tr>
<td>hot10a</td>
<td class="ok">OK</td>
<td>0.19/6.00</td>
<td class="groupscore" rowspan="2"></tbody>
</table>
</div>
</div>';

template_end();
?>