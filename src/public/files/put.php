<?php
$user="guest";

require_once $_SERVER['DOCUMENT_ROOT']."/kit/main.php";

template_begin('Zgłoszenie',$user);

echo '<div style="text-align: center">
<div class="submit_status">
<pre>Status: Sprawdzono</pre>
<pre>Wynik: 300/300</pre>
</div>
<table class="table">
<thead>
<tr>
  <th style="min-width: 70px">Test</th>
  <th style="min-width: 100px">Wynik</th>
  <th style="min-width: 80px">Czas/Limit</th>
  <th style="min-width: 60px">Punkty</th>
</tr>
</thead>
<tbody>
<tr>
  <td>trol0</td>
  <td class="ok">OK</td>
  <td>0.00/0.10</td>
  <td rowspan="2">99/99</td>
</tr>
<tr>
  <td>trol1</td>
  <td class="ok">OK</td>
  <td>0.00/3.10</td>
</tr>
<tr>
  <td>trol2</td>
  <td class="ok">OK</td>
  <td>0.00/1.00</td>
  <td rowspan="1">1/1</td>
</tr>
<tr>
  <td>trol0</td>
  <td class="ok">OK</td>
  <td>0.00/0.10</td>
  <td rowspan="2">99/99</td>
</tr>
<tr>
  <td>trol1</td>
  <td class="ok">OK</td>
  <td>0.00/3.10</td>
</tr>
<tr>
  <td>trol2</td>
  <td class="ok">OK</td>
  <td>0.00/1.00</td>
  <td rowspan="1">1/1</td>
</tr>
<tr>
  <td>trol0</td>
  <td class="ok">OK</td>
  <td>0.00/0.10</td>
  <td rowspan="2">99/99</td>
</tr>
<tr>
  <td>trol1</td>
  <td class="ok">OK</td>
  <td>0.00/3.10</td>
</tr>
<tr>
  <td>trol2</td>
  <td class="ok">OK</td>
  <td>0.00/1.00</td>
  <td rowspan="1">1/1</td>
</tr>
</tbody>
</table>
</div>';

template_end();
?>