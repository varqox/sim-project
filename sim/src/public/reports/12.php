<?php
if(isset($_GET['download']))
{header('Content-type: application/text');header('Content-Disposition: attchment; filename="12.cpp"');readfile($_SERVER['DOCUMENT_ROOT']."/../solutions/12.cpp");exit;}


require_once $_SERVER['DOCUMENT_ROOT']."/../php/main.php";

template_begin('Zgłoszenie 12');

if(isset($_GET['source']))
{
echo '<div style="margin: 60px 0 0 50px">', shell_exec($_SERVER['DOCUMENT_ROOT']."/../judge/CTH ".$_SERVER['DOCUMENT_ROOT']."/../solutions/12.cpp"), '
</div>';
template_end();
exit;
}

echo '<div style="text-align: center">
<div class="report-info">
<h1>Zgłoszenie 12</h1>
<div class="btn-toolbar">
<a class="btn-small" href="?source">View source</a>
<a class="btn-small" href="?download">Download</a>
</div>
<h2>Zadanie 2</h2>
</div>
<div class="submit-status">
<pre>Score: 50/100
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
<td>bar0</td>
<td class="ok">OK</td>
<td>0.00/1.00</td>
<td class="groupscore">0/0</td>
</tr>
<tr>
<td>bar1a</td>
<td class="ok">OK</td>
<td>0.00/1.00</td>
<td class="groupscore" rowspan="5">10/10</td>
</tr>
<tr>
<td>bar1b</td>
<td class="ok">OK</td>
<td>0.00/1.00</td>
</tr>
<tr>
<td>bar1c</td>
<td class="ok">OK</td>
<td>0.00/1.00</td>
</tr>
<tr>
<td>bar1d</td>
<td class="ok">OK</td>
<td>0.00/1.00</td>
</tr>
<tr>
<td>bar1e</td>
<td class="ok">OK</td>
<td>0.00/1.00</td>
</tr>
<tr>
<td>bar2a</td>
<td class="ok">OK</td>
<td>0.00/1.00</td>
<td class="groupscore" rowspan="3">10/10</td>
</tr>
<tr>
<td>bar2b</td>
<td class="ok">OK</td>
<td>0.00/1.00</td>
</tr>
<tr>
<td>bar2c</td>
<td class="ok">OK</td>
<td>0.00/1.00</td>
</tr>
<tr>
<td>bar3a</td>
<td class="ok">OK</td>
<td>0.07/1.00</td>
<td class="groupscore" rowspan="4">10/10</td>
</tr>
<tr>
<td>bar3b</td>
<td class="ok">OK</td>
<td>0.09/1.00</td>
</tr>
<tr>
<td>bar3c</td>
<td class="ok">OK</td>
<td>0.10/1.00</td>
</tr>
<tr>
<td>bar3d</td>
<td class="ok">OK</td>
<td>0.05/1.00</td>
</tr>
<tr>
<td>bar4a</td>
<td class="ok">OK</td>
<td>0.17/1.00</td>
<td class="groupscore" rowspan="3">10/10</td>
</tr>
<tr>
<td>bar4b</td>
<td class="ok">OK</td>
<td>0.14/1.00</td>
</tr>
<tr>
<td>bar4c</td>
<td class="ok">OK</td>
<td>0.17/1.00</td>
</tr>
<tr>
<td>bar5a</td>
<td class="ok">OK</td>
<td>0.21/1.00</td>
<td class="groupscore" rowspan="2">10/10</td>
</tr>
<tr>
<td>bar5b</td>
<td class="ok">OK</td>
<td>0.18/1.00</td>
</tr>
<tr>
<td>bar6a</td>
<td class="tl-re">Time limit</td>
<td>1.00/1.00</td>
<td class="groupscore" rowspan="3">0/10</td>
</tr>
<tr>
<td>bar6b</td>
<td class="tl-re">Time limit</td>
<td>1.00/1.00</td>
</tr>
<tr>
<td>bar6c</td>
<td class="tl-re">Time limit</td>
<td>1.00/1.00</td>
</tr>
<tr>
<td>bar7a</td>
<td class="tl-re">Time limit</td>
<td>1.00/1.00</td>
<td class="groupscore" rowspan="4">0/10</td>
</tr>
<tr>
<td>bar7b</td>
<td class="tl-re">Time limit</td>
<td>1.00/1.00</td>
</tr>
<tr>
<td>bar7c</td>
<td class="tl-re">Time limit</td>
<td>1.00/1.00</td>
</tr>
<tr>
<td>bar7d</td>
<td class="tl-re">Time limit</td>
<td>1.00/1.00</td>
</tr>
<tr>
<td>bar8a</td>
<td class="tl-re">Time limit</td>
<td>1.50/1.50</td>
<td class="groupscore" rowspan="3">0/10</td>
</tr>
<tr>
<td>bar8b</td>
<td class="tl-re">Time limit</td>
<td>1.50/1.50</td>
</tr>
<tr>
<td>bar8c</td>
<td class="tl-re">Time limit</td>
<td>1.50/1.50</td>
</tr>
<tr>
<td>bar9a</td>
<td class="tl-re">Time limit</td>
<td>3.00/3.00</td>
<td class="groupscore" rowspan="4">0/10</td>
</tr>
<tr>
<td>bar9b</td>
<td class="tl-re">Time limit</td>
<td>3.00/3.00</td>
</tr>
<tr>
<td>bar9c</td>
<td class="tl-re">Time limit</td>
<td>3.00/3.00</td>
</tr>
<tr>
<td>bar9d</td>
<td class="tl-re">Time limit</td>
<td>3.00/3.00</td>
</tr>
<tr>
<td>bar10a</td>
<td class="tl-re">Time limit</td>
<td>3.00/3.00</td>
<td class="groupscore" rowspan="3">0/10</td>
</tr>
<tr>
<td>bar10b</td>
<td class="tl-re">Time limit</td>
<td>3.00/3.00</td>
</tr>
<tr>
<td>bar10c</td>
<td class="tl-re">Time limit</td>
<td>3.00/3.00</td>
</tr>
</tbody>
</table>
</div>
</div>';

template_end();
?>