<?php
if(isset($_GET['download']))
{header('Content-type: application/text');header('Content-Disposition: attchment; filename="16.cpp"');readfile($_SERVER['DOCUMENT_ROOT']."/../solutions/16.cpp");exit;}

$user="none";
require_once $_SERVER['DOCUMENT_ROOT']."/kit/main.php";

template_begin('Zgłoszenie 16',$user);

if(isset($_GET['source']))
{
echo '<div style="margin: 60px 50px">', shell_exec($_SERVER['DOCUMENT_ROOT']."/../judge/CTH ".$_SERVER['DOCUMENT_ROOT']."/../solutions/16.cpp"), '
</div>';
template_end();
exit;
}

echo '<div style="text-align: center">
<p style="padding: 15px 0 0 200px;font-size: 35px;text-align: left">Zgłoszenie 16</p>
<div class ="btn-toolbar">
<a class="btn-small" href="?source">View source</a>
<a class="btn-small" href="?download">Download</a>
</div>
<p style="font-size: 30px;">Zadanie 3</p>
<div class="submit_status">
<pre>Score: 100/100
Status: Judged</pre>
<table style="margin-top: 5px" class="table results">
<thead>
<tr>
<th style="min-width: 70px">Test</th>
<th style="min-width: 180px">Result</th>
<th style="min-width: 90px">Time</th>
<th style="min-width: 60px">Result</th>
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
<td>0.00/1.00</td>
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
<td>0.00/1.00</td>
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
<td>0.01/1.00</td>
<td class="groupscore" rowspan="2">10/10</td>
</tr>
<tr>
<td>hot6b</td>
<td class="ok">OK</td>
<td>0.00/1.00</td>
</tr>
<tr>
<td>hot7a</td>
<td class="ok">OK</td>
<td>0.02/1.00</td>
<td class="groupscore" rowspan="2">10/10</td>
</tr>
<tr>
<td>hot7b</td>
<td class="ok">OK</td>
<td>0.01/1.00</td>
</tr>
<tr>
<td>hot8a</td>
<td class="ok">OK</td>
<td>0.08/3.00</td>
<td class="groupscore" rowspan="2">10/10</td>
</tr>
<tr>
<td>hot8b</td>
<td class="ok">OK</td>
<td>0.02/3.00</td>
</tr>
<tr>
<td>hot9a</td>
<td class="ok">OK</td>
<td>0.21/6.00</td>
<td class="groupscore" rowspan="2">10/10</td>
</tr>
<tr>
<td>hot9b</td>
<td class="ok">OK</td>
<td>0.00/6.00</td>
</tr>
<tr>
<td>hot10a</td>
<td class="ok">OK</td>
<td>0.18/6.00</td>
<td class="groupscore" rowspan="2">10/10</td>
</tr>
<tr>
<td>hot10b</td>
<td class="ok">OK</td>
<td>0.00/6.00</td>
</tr>
</tbody>
</table>
</div>
</div>';

template_end();
?>