<?php
if(isset($_GET['download']))
{header('Content-type: application/text');header('Content-Disposition: attchment; filename="50.cpp"');readfile($_SERVER['DOCUMENT_ROOT']."/../solutions/50.cpp");exit;}

$user="none";
require_once $_SERVER['DOCUMENT_ROOT']."/kit/main.php";

template_begin('Zgłoszenie 50',$user);

if(isset($_GET['source']))
{
echo '<div style="margin: 60px 0 0 50px">', shell_exec($_SERVER['DOCUMENT_ROOT']."/../judge/CTH ".$_SERVER['DOCUMENT_ROOT']."/../solutions/50.cpp"), '
</div>';
template_end();
exit;
}

echo '<div style="text-align: center">
<div class="report-info">
<h1>Zgłoszenie 50</h1>
<div class="btn-toolbar">
<a class="btn-small" href="?source">View source</a>
<a class="btn-small" href="?download">Download</a>
</div>
<h2>Zadanie 5</h2>
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
<td>pal0</td>
<td class="ok">OK</td>
<td>0.00/2.00</td>
<td class="groupscore">0/0</td>
</tr>
<tr>
<td>pal1a</td>
<td class="ok">OK</td>
<td>0.00/2.00</td>
<td class="groupscore" rowspan="4">10/10</td>
</tr>
<tr>
<td>pal1b</td>
<td class="ok">OK</td>
<td>0.00/2.00</td>
</tr>
<tr>
<td>pal1c</td>
<td class="ok">OK</td>
<td>0.00/2.00</td>
</tr>
<tr>
<td>pal1d</td>
<td class="ok">OK</td>
<td>0.00/2.00</td>
</tr>
<tr>
<td>pal2a</td>
<td class="ok">OK</td>
<td>0.00/2.00</td>
<td class="groupscore" rowspan="4">10/10</td>
</tr>
<tr>
<td>pal2b</td>
<td class="ok">OK</td>
<td>0.00/2.00</td>
</tr>
<tr>
<td>pal2c</td>
<td class="ok">OK</td>
<td>0.00/2.00</td>
</tr>
<tr>
<td>pal2d</td>
<td class="ok">OK</td>
<td>0.00/2.00</td>
</tr>
<tr>
<td>pal3a</td>
<td class="ok">OK</td>
<td>0.07/2.00</td>
<td class="groupscore" rowspan="4">10/10</td>
</tr>
<tr>
<td>pal3b</td>
<td class="ok">OK</td>
<td>0.07/2.00</td>
</tr>
<tr>
<td>pal3c</td>
<td class="ok">OK</td>
<td>0.07/2.00</td>
</tr>
<tr>
<td>pal3d</td>
<td class="ok">OK</td>
<td>0.10/2.00</td>
</tr>
<tr>
<td>pal4a</td>
<td class="ok">OK</td>
<td>0.12/2.00</td>
<td class="groupscore" rowspan="4">10/10</td>
</tr>
<tr>
<td>pal4b</td>
<td class="ok">OK</td>
<td>0.13/2.00</td>
</tr>
<tr>
<td>pal4c</td>
<td class="ok">OK</td>
<td>0.07/2.00</td>
</tr>
<tr>
<td>pal4d</td>
<td class="ok">OK</td>
<td>0.07/2.00</td>
</tr>
<tr>
<td>pal5a</td>
<td class="ok">OK</td>
<td>0.07/2.00</td>
<td class="groupscore" rowspan="4">10/10</td>
</tr>
<tr>
<td>pal5b</td>
<td class="ok">OK</td>
<td>0.08/2.00</td>
</tr>
<tr>
<td>pal5c</td>
<td class="ok">OK</td>
<td>0.07/2.00</td>
</tr>
<tr>
<td>pal5d</td>
<td class="ok">OK</td>
<td>0.07/2.00</td>
</tr>
<tr>
<td>pal6a</td>
<td class="ok">OK</td>
<td>0.07/2.00</td>
<td class="groupscore" rowspan="4">10/10</td>
</tr>
<tr>
<td>pal6b</td>
<td class="ok">OK</td>
<td>0.08/2.00</td>
</tr>
<tr>
<td>pal6c</td>
<td class="ok">OK</td>
<td>0.07/2.00</td>
</tr>
<tr>
<td>pal6d</td>
<td class="ok">OK</td>
<td>0.08/2.00</td>
</tr>
<tr>
<td>pal7a</td>
<td class="ok">OK</td>
<td>0.07/2.00</td>
<td class="groupscore" rowspan="4">10/10</td>
</tr>
<tr>
<td>pal7b</td>
<td class="ok">OK</td>
<td>0.07/2.00</td>
</tr>
<tr>
<td>pal7c</td>
<td class="ok">OK</td>
<td>0.08/2.00</td>
</tr>
<tr>
<td>pal7d</td>
<td class="ok">OK</td>
<td>0.09/2.00</td>
</tr>
<tr>
<td>pal8a</td>
<td class="ok">OK</td>
<td>0.07/2.00</td>
<td class="groupscore" rowspan="4">10/10</td>
</tr>
<tr>
<td>pal8b</td>
<td class="ok">OK</td>
<td>0.07/2.00</td>
</tr>
<tr>
<td>pal8c</td>
<td class="ok">OK</td>
<td>0.08/2.00</td>
</tr>
<tr>
<td>pal8d</td>
<td class="ok">OK</td>
<td>0.07/2.00</td>
</tr>
<tr>
<td>pal9a</td>
<td class="ok">OK</td>
<td>0.07/2.00</td>
<td class="groupscore" rowspan="4">10/10</td>
</tr>
<tr>
<td>pal9b</td>
<td class="ok">OK</td>
<td>0.07/2.00</td>
</tr>
<tr>
<td>pal9c</td>
<td class="ok">OK</td>
<td>0.07/2.00</td>
</tr>
<tr>
<td>pal9d</td>
<td class="ok">OK</td>
<td>0.06/2.00</td>
</tr>
<tr>
<td>pal10a</td>
<td class="ok">OK</td>
<td>0.07/2.00</td>
<td class="groupscore" rowspan="4">10/10</td>
</tr>
<tr>
<td>pal10b</td>
<td class="ok">OK</td>
<td>0.06/2.00</td>
</tr>
<tr>
<td>pal10c</td>
<td class="ok">OK</td>
<td>0.08/2.00</td>
</tr>
<tr>
<td>pal10d</td>
<td class="ok">OK</td>
<td>0.07/2.00</td>
</tr>
</tbody>
</table>
</div>
</div>';

template_end();
?>