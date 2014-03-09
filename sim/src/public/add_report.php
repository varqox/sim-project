<?php
$user="none";

require_once $_SERVER['DOCUMENT_ROOT']."/kit/main.php";

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

template_begin('Submit a solution',$user);

echo '
<div style="text-align: center">
<p style="font-size: 35px">Submit a solution<p>
<form enctype="multipart/form-data"  action="" method="POST">
Report ID: <input type="text" name="report_id"><br/>
Task ID: <input type="text" name="task_id"><br/>
<input type="file" name="solution"><br/>
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
	else if($_FILES['solution']['error']>0 || !file_put_contents($_SERVER['DOCUMENT_ROOT']."/reports/".$_POST['report_id'].".php", "<?php\nif(isset(\$_GET['download']))\n{header('Content-type: application/text');header('Content-Disposition: attchment; filename=\"".$_POST['report_id'].".cpp\"');readfile(\$_SERVER['DOCUMENT_ROOT'].\"/../solutions/".$_POST['report_id'].".cpp\");exit;}\n\n\$user=\"".$user."\";\nrequire_once \$_SERVER['DOCUMENT_ROOT'].\"/kit/main.php\";\n\ntemplate_begin('Zgłoszenie ".$_POST['report_id']."',\$user);\n\nif(isset(\$_GET['source']))\n{\necho '<div style=\"margin: 60px 50px\">', shell_exec(\$_SERVER['DOCUMENT_ROOT'].\"/../judge/CTH \".\$_SERVER['DOCUMENT_ROOT'].\"/../solutions/".$_POST['report_id'].".cpp\"), '\n</div>';\ntemplate_end();\nexit;\n}\n\necho '<div style=\"text-align: center\">\n<p style=\"padding: 15px 0 0 200px;font-size: 35px;text-align: left\">Zgłoszenie ".$_POST['report_id']."</p>\n<div class =\"btn-toolbar\">\n<a class=\"btn-small\" href=\"?source\">View source</a>\n<a class=\"btn-small\" href=\"?download\">Download</a>\n</div>\n<p style=\"font-size: 30px;\">Zadanie ".$_POST['task_id']."</p>\n<div class=\"submit_status\">\n<pre>Status: Waiting for judge...</pre>\n</div>\n</div>';\n\ntemplate_end();\n?>"))
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