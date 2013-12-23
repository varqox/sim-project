<?php
if(isset($_GET['download']))
{header('Content-type: application/text');header('Content-Disposition: attchment; filename="2.cpp"');readfile($_SERVER['DOCUMENT_ROOT']."/solutions/2.cpp");exit;}

$user="none";
require_once $_SERVER['DOCUMENT_ROOT']."/kit/main.php";

template_begin('Zgłoszenie 2',$user);

if(isset($_GET['source']))
{
echo '<div style="margin: 60px 50px">', shell_exec($_SERVER['DOCUMENT_ROOT']."/judge/CTH ".$_SERVER['DOCUMENT_ROOT']."/solutions/2.cpp"), '
</div>';
template_end();
exit;
}

echo '<div style="text-align: center">
<p style="padding: 15px 0 0 200px;font-size: 35px;text-align: left">Zgłoszenie 2</p>
<div class ="btn-toolbar">
<a class="btn-small" href="?source">View source</a>
<a class="btn-small" href="?download">Download</a>
</div>
<p style="font-size: 30px;">Zadanie 1</p>
<div class="submit_status">
<pre>Status: Sprawdzono</pre>
<pre>Wynik: 0/100</pre>
<table style="margin-top: 5px" class="table results">
<thead>
<tr>
  <th style="min-width: 70px">Test</th>
  <th style="min-width: 180px">Wynik</th>
  <th style="min-width: 90px">Czas/Limit</th>
  <th style="min-width: 60px">Punkty</th>
</tr>
</thead>
<tbody>
<tr>
  <td>1</td>
  <td class="wa">Zła odpowiedź</td>
  <td>0.00/3.00</td>
  <td rowspan="1" class="groupscore">0/50</td>
</tr>
<tr>
  <td>2</td>
  <td class="wa">Zła odpowiedź</td>
  <td>0.00/3.00</td>
  <td rowspan="1" class="groupscore">0/50</td>
</tr>
</tbody>
</table>
</div>
</div>';

template_end();
?>