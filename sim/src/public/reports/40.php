<?php
if(isset($_GET['download']))
{header('Content-type: application/text');header('Content-Disposition: attchment; filename="40.cpp"');readfile($_SERVER['DOCUMENT_ROOT']."/../solutions/40.cpp");exit;}

$user="none";
require_once $_SERVER['DOCUMENT_ROOT']."/kit/main.php";

template_begin('Zgłoszenie 40',$user);

if(isset($_GET['source']))
{
echo '<div style="margin: 60px 50px">', shell_exec($_SERVER['DOCUMENT_ROOT']."/../judge/CTH ".$_SERVER['DOCUMENT_ROOT']."/../solutions/40.cpp"), '
</div>';
template_end();
exit;
}

echo '<div style="text-align: center">
<div class="report-info">
<h1>Zgłoszenie 40</h1>
<div class="btn-toolbar">
<a class="btn-small" href="?source">View source</a>
<a class="btn-small" href="?download">Download</a>
</div>
<h2>Zadanie 6</h2>
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
<td>prz0</td>
<td class="ok">OK</td>
<td>0.02/0.50</td>
<td class="groupscore" rowspan="5">0/0</td>
</tr>
<tr>
<td>prz1ocen</td>
<td class="ok">OK</td>
<td>0.01/0.50</td>
</tr>
<tr>
<td>prz2ocen</td>
<td class="ok">OK</td>
<td>0.00/0.50</td>
</tr>
<tr>
<td>prz3ocen</td>
<td class="ok">OK</td>
<td>0.00/0.50</td>
</tr>
<tr>
<td>prz4ocen</td>
<td class="ok">OK</td>
<td>0.12/0.50</td>
</tr>
<tr>
<td>prz1a</td>
<td class="ok">OK</td>
<td>0.00/0.50</td>
<td class="groupscore" rowspan="4">5/5</td>
</tr>
<tr>
<td>prz1b</td>
<td class="ok">OK</td>
<td>0.02/0.50</td>
</tr>
<tr>
<td>prz1c</td>
<td class="ok">OK</td>
<td>0.00/0.50</td>
</tr>
<tr>
<td>prz1d</td>
<td class="ok">OK</td>
<td>0.00/0.50</td>
</tr>
<tr>
<td>prz2a</td>
<td class="ok">OK</td>
<td>0.00/0.50</td>
<td class="groupscore" rowspan="5">12/12</td>
</tr>
<tr>
<td>prz2b</td>
<td class="ok">OK</td>
<td>0.00/0.50</td>
</tr>
<tr>
<td>prz2c</td>
<td class="ok">OK</td>
<td>0.00/0.50</td>
</tr>
<tr>
<td>prz2d</td>
<td class="ok">OK</td>
<td>0.00/0.50</td>
</tr>
<tr>
<td>prz2e</td>
<td class="ok">OK</td>
<td>0.00/0.50</td>
</tr>
<tr>
<td>prz3a</td>
<td class="ok">OK</td>
<td>0.00/0.50</td>
<td class="groupscore" rowspan="3">7/7</td>
</tr>
<tr>
<td>prz3b</td>
<td class="ok">OK</td>
<td>0.00/0.50</td>
</tr>
<tr>
<td>prz3c</td>
<td class="ok">OK</td>
<td>0.00/0.50</td>
</tr>
<tr>
<td>prz4a</td>
<td class="ok">OK</td>
<td>0.00/0.50</td>
<td class="groupscore" rowspan="3">7/7</td>
</tr>
<tr>
<td>prz4b</td>
<td class="ok">OK</td>
<td>0.01/0.50</td>
</tr>
<tr>
<td>prz4c</td>
<td class="ok">OK</td>
<td>0.00/0.50</td>
</tr>
<tr>
<td>prz5a</td>
<td class="ok">OK</td>
<td>0.01/0.50</td>
<td class="groupscore" rowspan="3">7/7</td>
</tr>
<tr>
<td>prz5b</td>
<td class="ok">OK</td>
<td>0.00/0.50</td>
</tr>
<tr>
<td>prz5c</td>
<td class="ok">OK</td>
<td>0.01/0.50</td>
</tr>
<tr>
<td>prz6a</td>
<td class="ok">OK</td>
<td>0.04/1.00</td>
<td class="groupscore" rowspan="4">7/7</td>
</tr>
<tr>
<td>prz6b</td>
<td class="ok">OK</td>
<td>0.01/1.00</td>
</tr>
<tr>
<td>prz6c</td>
<td class="ok">OK</td>
<td>0.02/1.00</td>
</tr>
<tr>
<td>prz6d</td>
<td class="ok">OK</td>
<td>0.04/1.00</td>
</tr>
<tr>
<td>prz7a</td>
<td class="ok">OK</td>
<td>0.05/1.50</td>
<td class="groupscore" rowspan="5">11/11</td>
</tr>
<tr>
<td>prz7b</td>
<td class="ok">OK</td>
<td>0.05/1.50</td>
</tr>
<tr>
<td>prz7c</td>
<td class="ok">OK</td>
<td>0.04/1.50</td>
</tr>
<tr>
<td>prz7d</td>
<td class="ok">OK</td>
<td>0.09/1.50</td>
</tr>
<tr>
<td>prz7e</td>
<td class="ok">OK</td>
<td>0.07/1.50</td>
</tr>
<tr>
<td>prz8a</td>
<td class="ok">OK</td>
<td>0.14/2.50</td>
<td class="groupscore" rowspan="5">11/11</td>
</tr>
<tr>
<td>prz8b</td>
<td class="ok">OK</td>
<td>0.05/2.50</td>
</tr>
<tr>
<td>prz8c</td>
<td class="ok">OK</td>
<td>0.08/2.50</td>
</tr>
<tr>
<td>prz8d</td>
<td class="ok">OK</td>
<td>0.17/2.50</td>
</tr>
<tr>
<td>prz8e</td>
<td class="ok">OK</td>
<td>0.14/2.50</td>
</tr>
<tr>
<td>prz9a</td>
<td class="ok">OK</td>
<td>0.18/3.00</td>
<td class="groupscore" rowspan="5">11/11</td>
</tr>
<tr>
<td>prz9b</td>
<td class="ok">OK</td>
<td>0.09/3.00</td>
</tr>
<tr>
<td>prz9c</td>
<td class="ok">OK</td>
<td>0.10/3.00</td>
</tr>
<tr>
<td>prz9d</td>
<td class="ok">OK</td>
<td>0.26/3.00</td>
</tr>
<tr>
<td>prz9e</td>
<td class="ok">OK</td>
<td>0.20/3.00</td>
</tr>
<tr>
<td>prz10a</td>
<td class="ok">OK</td>
<td>0.25/4.00</td>
<td class="groupscore" rowspan="5">11/11</td>
</tr>
<tr>
<td>prz10b</td>
<td class="ok">OK</td>
<td>0.11/4.00</td>
</tr>
<tr>
<td>prz10c</td>
<td class="ok">OK</td>
<td>0.11/4.00</td>
</tr>
<tr>
<td>prz10d</td>
<td class="ok">OK</td>
<td>0.35/4.00</td>
</tr>
<tr>
<td>prz10e</td>
<td class="ok">OK</td>
<td>0.28/4.00</td>
</tr>
<tr>
<td>prz11a</td>
<td class="ok">OK</td>
<td>0.35/8.00</td>
<td class="groupscore" rowspan="8">11/11</td>
</tr>
<tr>
<td>prz11b</td>
<td class="ok">OK</td>
<td>0.14/8.00</td>
</tr>
<tr>
<td>prz11c</td>
<td class="ok">OK</td>
<td>0.13/8.00</td>
</tr>
<tr>
<td>prz11d</td>
<td class="ok">OK</td>
<td>0.63/8.00</td>
</tr>
<tr>
<td>prz11e</td>
<td class="ok">OK</td>
<td>0.78/8.00</td>
</tr>
<tr>
<td>prz11f</td>
<td class="ok">OK</td>
<td>0.64/8.00</td>
</tr>
<tr>
<td>prz11g</td>
<td class="ok">OK</td>
<td>0.74/8.00</td>
</tr>
<tr>
<td>prz11h</td>
<td class="ok">OK</td>
<td>0.39/8.00</td>
</tr>
</tbody>
</table>
</div>
</div>';

template_end();
?>