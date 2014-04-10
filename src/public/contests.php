<?php
$user="guest";

include $_SERVER['DOCUMENT_ROOT']."/kit/main.php";

template_begin('Konkursy',$user,'',
'.tasks{
	line-height: 30px;
	margin-left: auto;
	margin-right: auto;
	borderspacing: 0;
}
.tasks td{
	border-top: 1px solid #a2a2a2;
	padding: 5px 10px 5px 10px;
}
.tasks thead th{
	padding: 3px 10px 3px 10px;
}
.tasks tr td:first-child{
	text-align: right;
}');


echo '<table class="tasks">
<thead>
<tr>
<th style="min-width: 30px">ID</th>
<th style="min-width: 50px">Skrót</th>
<th style="min-width: 150px">Nazwa zadania</th>
</tr>
</thead>
<tbody>
<tr>
<td>1</td>
<td>';
$file_content=file($_SERVER['DOCUMENT_ROOT']."/../tasks/1/conf.cfg");
echo substr($file_content[0], 0, -1),'</td>
<td><a href="/task_content.php?id=1">',substr($file_content[1], 0, -1),'</a></td>
</tr>
<tr>
<td>2</td>
<td>';
$file_content=file($_SERVER['DOCUMENT_ROOT']."/../tasks/2/conf.cfg");
echo substr($file_content[0], 0, -1),'</td>
<td><a href="/task_content.php?id=2">',substr($file_content[1], 0, -1),'</a></td>
</tr>
</tbody>
</table>';
template_end();
?>