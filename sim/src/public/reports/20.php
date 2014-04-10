<?php
if(isset($_GET['download']))
{header('Content-type: application/text');header('Content-Disposition: attchment; filename="20.cpp"');readfile($_SERVER['DOCUMENT_ROOT']."/../solutions/20.cpp");exit;}

$user="none";
require_once $_SERVER['DOCUMENT_ROOT']."/kit/main.php";

template_begin('Zgłoszenie 20',$user);

if(isset($_GET['source']))
{
echo '<div style="margin: 60px 50px">', shell_exec($_SERVER['DOCUMENT_ROOT']."/../judge/CTH ".$_SERVER['DOCUMENT_ROOT']."/../solutions/20.cpp"), '
</div>';
template_end();
exit;
}

echo '<div style="text-align: center">
<div class="report-info">
<h1>Zgłoszenie 20</h1>
<div class="btn-toolbar">
<a class="btn-small" href="?source">View source</a>
<a class="btn-small" href="?download">Download</a>
</div>
<h2>Zadanie 4</h2>
</div>
<div class="submit-status">
<pre>Score: 100/100
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
<td>roz0</td>
<td class="ok">OK</td>
<td>0.00/1.00</td>
<td class="groupscore">0/0</td>
</tr>
<tr>
<td>roz1ocen</td>
<td class="ok">OK</td>
<td>0.00/1.00</td>
<td class="groupscore">0/0</td>
</tr>
<tr>
<td>roz2ocen</td>
<td class="ok">OK</td>
<td>0.00/1.00</td>
<td class="groupscore">0/0</td>
</tr>
<tr>
<td>roz3ocen</td>
<td class="ok">OK</td>
<td>0.02/1.00</td>
<td class="groupscore">0/0</td>
</tr>
<tr>
<td>roz4ocen</td>
<td class="ok">OK</td>
<td>0.00/5.00</td>
<td class="groupscore">0/0</td>
</tr>
<tr>
<td>roz1</td>
<td class="ok">OK</td>
<td>0.01/1.00</td>
<td class="groupscore">12/12</td>
</tr>
<tr>
<td>roz2</td>
<td class="ok">OK</td>
<td>0.00/1.00</td>
<td class="groupscore">12/12</td>
</tr>
<tr>
<td>roz3</td>
<td class="ok">OK</td>
<td>0.00/3.00</td>
<td class="groupscore">12/12</td>
</tr>
<tr>
<td>roz4</td>
<td class="ok">OK</td>
<td>0.00/3.00</td>
<td class="groupscore">12/12</td>
</tr>
<tr>
<td>roz5</td>
<td class="ok">OK</td>
<td>0.00/3.00</td>
<td class="groupscore">13/13</td>
</tr>
<tr>
<td>roz6</td>
<td class="ok">OK</td>
<td>0.00/3.00</td>
<td class="groupscore">13/13</td>
</tr>
<tr>
<td>roz7</td>
<td class="ok">OK</td>
<td>0.00/3.00</td>
<td class="groupscore">13/13</td>
</tr>
<tr>
<td>roz8</td>
<td class="ok">OK</td>
<td>0.00/3.00</td>
<td class="groupscore">13/13</td>
</tr>
</tbody>
</table>
</div>
</div>';

template_end();
?>