<?php


require_once $_SERVER['DOCUMENT_ROOT']."/../php/main.php";

template_begin('Trololo');

echo '
<center>
<table class="table">
<thead>
<tr>
	<th>Test</th>
	<th>Limit czasu</th>
</tr>
</thead>
<tbody>
<tr>
	<td>trol0</td>
	<td>0.10</td>
</tr>
<tr>
	<td>trol1</td>
	<td>3.10</td>
</tr>
<tr>
	<td>trol2</td>
	<td>1.00</td>
</tr>
</tbody>
</table>
<table class="table">
<thead>
<tr>
	<th>Testy</th>
	<th>Punkty</th>
</tr>
</thead>
<tbody>
<tr>
  <td>trol0</td>
  <td rowspan="2">99/99</td>
<tr>
  <td>trol1</td>
</tr>
<tr>
  <td colspan="2"><b>Add test here</b></td>
</tr>
<tr>
  <td>trol2</td>
  <td rowspan="1">1/1</td>
</tr>
<tr>
  <td colspan="2"><b>Add test here</b></td>
</tr>
<tr>
  <td>trol0</td>
  <td rowspan="2">99/99</td>
</tr>
<tr>
  <td>trol1</td>
</tr>
<tr>
  <td colspan="2"><b>Add test here</b></td>
</tr>
<tr>
  <td>trol2</td>
  <td rowspan="1">1/1</td>
</tr>
<tr>
  <td>trol0</td>
  <td rowspan="2">99/99</td>
</tr>
<tr>
  <td>trol1</td>
</tr>
<tr>
  <td colspan="2"><b>Add test here</b></td>
</tr>
<tr>
  <td>trol2</td>
  <td rowspan="1">1/1</td>
</tr>
<tr>
  <td colspan="2"><b>Add test here</b></td>
</tr>
</tbody>
</table>
</center>';

template_end();
?>