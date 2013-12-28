<?php
$user="guest";

include $_SERVER['DOCUMENT_ROOT']."/kit/main.php";

function PrintError($value)
{
	template_begin('...',$user);
	echo $value;
	template_end();
}

if(isset($_GET['id']))
{
	if(is_numeric($_GET['id']))
	{
		header('Content-type: application/pdf');
		header('Content-Disposition: filename="'.substr(file($_SERVER['DOCUMENT_ROOT']."/../tasks/".$_GET['id']."/conf.cfg")[0], 0, -1).'.pdf"');
		readfile($_SERVER['DOCUMENT_ROOT']."/../tasks/".$_GET['id']."/content.pdf");
	}
	else
		PrintError('<p>Task id have to be a number</p>');
}
else
	PrintError('<p>You have to specify the task id</p>');
?>