<?php


require_once $_SERVER['DOCUMENT_ROOT']."/../php/main.php";

template_begin('Zgłoszenie');

echo '<div style="text-align: center">
<div class="submit_status">
<p style="font-size: 35px;">Zgłoszenie 1</p>
<pre>Status: Sprawdzono</pre>
<pre>Wynik: 0/100</pre>
<table style="margin-top: 5px" class="table">
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
  <td>1</td>
  <td class="wa">Zła odpowiedź</td>
  <td>0.00/3.00</td>
  <td rowspan="1">0/50</td>
</tr>
<tr>
  <td>2</td>
  <td class="wa">Zła odpowiedź</td>
  <td>0.01/3.00</td>
  <td rowspan="1">0/50</td>
</tr>
</tbody>
</table>
</div>
</div>';

template_end();
?>