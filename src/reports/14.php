<?php
if(isset($_GET['download']))
{header('Content-type: application/text');header('Content-Disposition: attchment; filename="14.cpp"');readfile($_SERVER['DOCUMENT_ROOT']."/solutions/14.cpp");exit;}

$user="none";
require_once $_SERVER['DOCUMENT_ROOT']."/kit/main.php";

template_begin('Zgłoszenie 14',$user);

if(isset($_GET['source']))
{
echo '<div style="margin: 60px 50px">', shell_exec($_SERVER['DOCUMENT_ROOT']."/judge/CTH ".$_SERVER['DOCUMENT_ROOT']."/solutions/14.cpp"), '
</div>';
template_end();
exit;
}

echo '<div style="text-align: center">
<p style="padding: 15px 0 0 200px;font-size: 35px;text-align: left">Zgłoszenie 14</p>
<div class ="btn-toolbar">
<a class="btn-small" href="?source">View source</a>
<a class="btn-small" href="?download">Download</a>
</div>
<p style="font-size: 30px;">Zadanie 2</p>
<div class="submit_status">
<pre>Score: 0/100
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
<td>bar0</td>
<td class="wa">Wrong answer</td>
<td>0.00/1.00</td>
<td>0/0</td>
</tr>
<tr>
<td>bar1a</td>
<td class="wa">Wrong answer</td>
<td>0.00/1.00</td>
<td rowspan="5">0/10</td>
</tr>
<tr>
<td>bar1b</td>
<td class="wa">Wrong answer</td>
<td>0.00/1.00</td>
</tr>
<tr>
<td>bar1c</td>
<td class="wa">Wrong answer</td>
<td>0.00/1.00</td>
</tr>
<tr>
<td>bar1d</td>
<td class="wa">Wrong answer</td>
<td>0.00/1.00</td>
</tr>
<tr>
<td>bar1e</td>
<td class="wa">Wrong answer</td>
<td>0.00/1.00</td>
</tr>
<tr>
<td>bar2a</td>
<td class="wa">Wrong answer</td>
<td>0.00/1.00</td>
<td rowspan="3">0/10</td>
</tr>
<tr>
<td>bar2b</td>
<td class="tl_re">Time limit</td>
<td>1.00/1.00</td>
</tr>
<tr>
<td>bar2c</td>
<td class="wa">Wrong answer</td>
<td>0.00/1.00</td>
</tr>
<tr>
<td>bar3a</td>
<td class="tl_re">Time limit</td>
<td>1.00/1.00</td>
<td rowspan="4">0/10</td>
</tr>
<tr>
<td>bar3b</td>
<td class="tl_re">Time limit</td>
<td>1.00/1.00</td>
</tr>
<tr>
<td>bar3c</td>
<td class="wa">Wrong answer</td>
<td>0.00/1.00</td>
</tr>
<tr>
<td>bar3d</td>
<td class="tl_re">Time limit</td>
<td>1.00/1.00</td>
</tr>
<tr>
<td>bar4a</td>
<td class="tl_re">Time limit</td>
<td>1.00/1.00</td>
<td rowspan="3">0/10</td>
</tr>
<tr>
<td>bar4b</td>
<td class="tl_re">Time limit</td>
<td>1.00/1.00</td>
</tr>
<tr>
<td>bar4c</td>
<td class="tl_re">Time limit</td>
<td>1.00/1.00</td>
</tr>
<tr>
<td>bar5a</td>
<td class="tl_re">Time limit</td>
<td>1.00/1.00</td>
<td rowspan="2">0/10</td>
</tr>
<tr>
<td>bar5b</td>
<td class="tl_re">Time limit</td>
<td>1.00/1.00</td>
</tr>
<tr>
<td>bar6a</td>
<td class="tl_re">Time limit</td>
<td>1.00/1.00</td>
<td rowspan="3">0/10</td>
</tr>
<tr>
<td>bar6b</td>
<td class="tl_re">Time limit</td>
<td>1.00/1.00</td>
</tr>
<tr>
<td>bar6c</td>
<td class="wa">Wrong answer</td>
<td>0.03/1.00</td>
</tr>
<tr>
<td>bar7a</td>
<td class="tl_re">Time limit</td>
<td>1.00/1.00</td>
<td rowspan="4">0/10</td>
</tr>
<tr>
<td>bar7b</td>
<td class="tl_re">Time limit</td>
<td>1.00/1.00</td>
</tr>
<tr>
<td>bar7c</td>
<td class="wa">Wrong answer</td>
<td>0.07/1.00</td>
</tr>
<tr>
<td>bar7d</td>
<td class="tl_re">Time limit</td>
<td>1.00/1.00</td>
</tr>
<tr>
<td>bar8a</td>
<td class="tl_re">Time limit</td>
<td>1.00/1.00</td>
<td rowspan="3">0/10</td>
</tr>
<tr>
<td>bar8b</td>
<td class="tl_re">Time limit</td>
<td>1.00/1.00</td>
</tr>
<tr>
<td>bar8c</td>
<td class="tl_re">Time limit</td>
<td>1.00/1.00</td>
</tr>
<tr>
<td>bar9a</td>
<td class="tl_re">Time limit</td>
<td>1.00/1.00</td>
<td rowspan="4">0/10</td>
</tr>
<tr>
<td>bar9b</td>
<td class="tl_re">Time limit</td>
<td>1.00/1.00</td>
</tr>
<tr>
<td>bar9c</td>
<td class="tl_re">Time limit</td>
<td>1.00/1.00</td>
</tr>
<tr>
<td>bar9d</td>
<td class="wa">Wrong answer</td>
<td>0.26/1.00</td>
</tr>
<tr>
<td>bar10a</td>
<td class="tl_re">Time limit</td>
<td>1.00/1.00</td>
<td rowspan="3">0/10</td>
</tr>
<tr>
<td>bar10b</td>
<td class="tl_re">Time limit</td>
<td>1.00/1.00</td>
</tr>
<tr>
<td>bar10c</td>
<td class="tl_re">Time limit</td>
<td>1.00/1.00</td>
</tr>
</tbody>
</table>
</div>
</div>';

template_end();
?>