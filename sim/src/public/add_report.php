<?php


require_once $_SERVER['DOCUMENT_ROOT']."/../php/main.php";

function make_safe_string($in)
{
	$out="";
	for($i=0; $i<strlen($in); ++$i)
	{
		if($in[$i]=='"') $out.='\\"';
		else if($in[$i]=='\\') $out.='\\\\';
		else $out.=$in[$i];
		// $out.="\n";
	}
return $out;
}

template_begin('Submit a solution');

echo '
<div class="form-container">
<h1>Submit a solution</h1>
<form enctype="multipart/form-data" action method="POST">
<label>Report ID</label>
<input class="input-block" type="text" name="report_id">
<label>Task ID</label>
<input class="input-block" type="text" name="task_id">
<label>File</label>
<input class="input-block" type="file" name="solution">
<input type="submit" name="submit" value="Submit" >
</form>
</div>';

// echo "<pre>", make_safe_string("???SAF?asfAS/s?FA?sa/FS?Af?S'sfsfsa?FSA?Fsa\\fs'/'sf?Afs/a'f?''??''"), "</pre>";
if(isset($_POST['submit']))
{
	// print_r($_SERVER);
	/*	$f=file_get_contents($_FILES['solution']['tmp_name']);
	echo $f, "</pre>\n";*/
	if(!isset($_POST['report_id'])) echo "<p>You must specify report ID<p>\n";
	else if(!isset($_POST['task_id']) || !copy($_FILES['solution']['tmp_name'], $_SERVER['DOCUMENT_ROOT'].'/../solutions/'.$_POST['report_id'].'.cpp')) echo "<p>You must specify task ID<p>\n";
	else if($_FILES['solution']['error']>0 || !file_put_contents($_SERVER['DOCUMENT_ROOT']."/reports/".$_POST['report_id'].".php", "<?php\nif(isset(\$_GET['download']))\n{header('Content-type: application/text');header('Content-Disposition: attchment; filename=\"".$_POST['report_id'].".cpp\"');readfile(\$_SERVER['DOCUMENT_ROOT'].\"/../solutions/".$_POST['report_id'].".cpp\");exit;}\n\nrequire_once \$_SERVER['DOCUMENT_ROOT'].\"/kit/main.php\";\n\ntemplate_begin('Zgłoszenie ".$_POST['report_id'].");\n\nif(isset(\$_GET['source']))\n{\necho '<div style=\"margin: 60px 0 0 50px\">', shell_exec(\$_SERVER['DOCUMENT_ROOT'].\"/../judge/CTH \".\$_SERVER['DOCUMENT_ROOT'].\"/../solutions/".$_POST['report_id'].".cpp\"), '\n</div>';\ntemplate_end();\nexit;\n}\n\necho '<div style=\"text-align: center\">\n<div class=\"report-info\">\n<h1>Zgłoszenie ".$_POST['report_id']."</h1>\n<div class=\"btn-toolbar\">\n<a class=\"btn-small\" href=\"?source\">View source</a>\n<a class=\"btn-small\" href=\"?download\">Download</a>\n</div>\n<h2>Zadanie ".$_POST['task_id']."</h2>\n</div>\n<div class=\"submit-status\">\n<pre>Status: Waiting for judge...</pre>\n</div>\n</div>';\n\ntemplate_end();\n?>"))

	{
		echo "<pre>";
		print_r($_POST);
		print_r($_FILES);
		echo "<p>ERROR!</p>\n</pre>\n";
	}
	else
	{
		file_put_contents($_SERVER['DOCUMENT_ROOT']."../judge/queue/.".$_POST['report_id'], $_POST['report_id']."\n".$_POST['task_id']);
		shell_exec("mv ".$_SERVER['DOCUMENT_ROOT']."../judge/queue/.".$_POST['report_id']." ".$_SERVER['DOCUMENT_ROOT']."../judge/queue/".$_POST['report_id']);
		shell_exec("(cd ".$_SERVER['DOCUMENT_ROOT']."../judge ; sudo ./judge_machine) > /dev/null 2> /dev/null &");
		header("Location: ".$_SERVER['HTTP_ORIGIN']."/reports/".$_POST['report_id'].".php");
	}
}

template_end();
?>