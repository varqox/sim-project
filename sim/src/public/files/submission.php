<?php
if(isset($_GET['download']))
{header('Content-type: application/text');header('Content-Disposition: attchment; filename="15.cpp"');readfile($_SERVER['DOCUMENT_ROOT']."/../solutions/15.cpp");exit;}

require_once $_SERVER['DOCUMENT_ROOT']."/../php/submission.php";
template_begin('Zgłoszenie 15');
if(isset($_GET['source']))
{echo '<div style="margin: 60px 0 0 10px">', shell_exec($_SERVER['DOCUMENT_ROOT']."/../judge/CTH ".$_SERVER['DOCUMENT_ROOT']."/../solutions/15.cpp"), '</div>';template_end();exit;}

echo '<div style="text-align: center">
<div class="submission-info">
<h1>Zgłoszenie 15</h1>
<div class="btn-toolbar">
<a class="btn-small" href="?source">View source</a>
<a class="btn-small" href="?download">Download</a>
</div>
<h2>Zadanie 7: Temperatura</h2>
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
<td>tem0</td><td class="ok">OK</td><td>0.00/1.00</td><td class="groupscore" rowspan="5">0/0</td>
</tr>
<tr>
<td>tem1ocen</td><td class="ok">OK</td><td>0.00/1.00</td></tr>
<tr>
<td>tem2ocen</td><td class="ok">OK</td><td>0.00/1.00</td></tr>
<tr>
<td>tem3ocen</td><td class="ok">OK</td><td>0.00/1.00</td></tr>
<tr>
<td>tem4ocen</td><td class="ok">OK</td><td>0.14/5.00</td></tr>
<tr>
<td>tem1a</td><td class="ok">OK</td><td>0.00/1.00</td><td class="groupscore" rowspan="3">8/8</td>
</tr>
<tr>
<td>tem1b</td><td class="ok">OK</td><td>0.00/1.00</td></tr>
<tr>
<td>tem1c</td><td class="ok">OK</td><td>0.00/1.00</td></tr>
<tr>
<td>tem2a</td><td class="ok">OK</td><td>0.00/1.00</td><td class="groupscore" rowspan="4">8/8</td>
</tr>
<tr>
<td>tem2b</td><td class="ok">OK</td><td>0.00/1.00</td></tr>
<tr>
<td>tem2c</td><td class="ok">OK</td><td>0.00/1.00</td></tr>
<tr>
<td>tem2d</td><td class="ok">OK</td><td>0.00/1.00</td></tr>
<tr>
<td>tem3a</td><td class="ok">OK</td><td>0.00/1.00</td><td class="groupscore" rowspan="3">8/8</td>
</tr>
<tr>
<td>tem3b</td><td class="ok">OK</td><td>0.00/1.00</td></tr>
<tr>
<td>tem3c</td><td class="ok">OK</td><td>0.00/1.00</td></tr>
<tr>
<td>tem4a</td><td class="ok">OK</td><td>0.08/2.50</td><td class="groupscore" rowspan="3">8/8</td>
</tr>
<tr>
<td>tem4b</td><td class="ok">OK</td><td>0.08/2.50</td></tr>
<tr>
<td>tem4c</td><td class="ok">OK</td><td>0.09/2.50</td></tr>
<tr>
<td>tem5a</td><td class="ok">OK</td><td>0.15/4.00</td><td class="groupscore" rowspan="3">8/8</td>
</tr>
<tr>
<td>tem5b</td><td class="ok">OK</td><td>0.17/4.00</td></tr>
<tr>
<td>tem5c</td><td class="ok">OK</td><td>0.17/4.00</td></tr>
<tr>
<td>tem6a</td><td class="ok">OK</td><td>0.19/5.00</td><td class="groupscore" rowspan="3">8/8</td>
</tr>
<tr>
<td>tem6b</td><td class="ok">OK</td><td>0.17/5.00</td></tr>
<tr>
<td>tem6c</td><td class="ok">OK</td><td>0.20/5.00</td></tr>
<tr>
<td>tem7a</td><td class="ok">OK</td><td>0.24/5.00</td><td class="groupscore" rowspan="3">8/8</td>
</tr>
<tr>
<td>tem7b</td><td class="ok">OK</td><td>0.18/5.00</td></tr>
<tr>
<td>tem7c</td><td class="ok">OK</td><td>0.25/5.00</td></tr>
<tr>
<td>tem8a</td><td class="ok">OK</td><td>0.23/5.00</td><td class="groupscore" rowspan="3">8/8</td>
</tr>
<tr>
<td>tem8b</td><td class="ok">OK</td><td>0.16/5.00</td></tr>
<tr>
<td>tem8c</td><td class="ok">OK</td><td>0.18/5.00</td></tr>
<tr>
<td>tem9a</td><td class="ok">OK</td><td>0.12/5.00</td><td class="groupscore" rowspan="3">9/9</td>
</tr>
<tr>
<td>tem9b</td><td class="ok">OK</td><td>0.13/5.00</td></tr>
<tr>
<td>tem9c</td><td class="ok">OK</td><td>0.13/5.00</td></tr>
<tr>
<td>tem10a</td><td class="ok">OK</td><td>0.13/5.00</td><td class="groupscore" rowspan="2">9/9</td>
</tr>
<tr>
<td>tem10b</td><td class="ok">OK</td><td>0.13/5.00</td></tr>
<tr>
<td>tem11a</td><td class="ok">OK</td><td>0.18/5.00</td><td class="groupscore" rowspan="2">9/9</td>
</tr>
<tr>
<td>tem11b</td><td class="ok">OK</td><td>0.25/5.00</td></tr>
<tr>
<td>tem12a</td><td class="ok">OK</td><td>0.17/5.00</td><td class="groupscore" rowspan="2">9/9</td>
</tr>
<tr>
<td>tem12b</td><td class="ok">OK</td><td>0.23/5.00</td></tr>
</tbody>
</table>
</div>
</div>';
template_end();
?>