<?php
require_once $_SERVER['DOCUMENT_ROOT']."/../php/main.php";

if(!isset($_GET['round']) || !is_numeric($_GET['round']))
	header("Location: /");

if(!check_logged_in())
{
	header("Location: /login.php");
	exit;
}

$stmt = DB::pdo()->prepare("SELECT type FROM users WHERE id=?");
$stmt->bindValue(1, $_SESSION['id'], PDO::PARAM_STR);
$stmt->execute();
if(!($row = $stmt->fetch()))
	E_403();
$user_privileges = privileges($row[0]);
$stmt->closeCursor();

$stmt = DB::pdo()->prepare("SELECT r.parent,r.task_id,r.name,r.privileges,r.begin_time,r.end_time,r.author,t.name FROM rounds r, tasks t WHERE r.id=? AND r.task_id=t.id");
$stmt->bindValue(1, $_GET['round'], PDO::PARAM_INT);
$stmt->execute();
if(!($row = $stmt->fetch()) || !isset($row[1]))
	E_404();
$time = time();
$task = array('id' => $row[1], 'name' => $row[7]);
$rounds = array(0 => $_GET['round']);
$parent = $row[0];
$path = "<a href=\"/round.php?id=".$_GET['round']."\">$row[2]</a>";
if(privileges($row[3]) < $user_privileges)
	E_403();
if($_SESSION['id'] != $row[6] && privileges($row[3]) == $user_privileges && ((isset($row[4]) && strtotime($row[4]) > $time) || (isset($row[5]) && $time > strtotime($row[5]))))
	E_403();
$stmt->closeCursor();
$stmt = DB::pdo()->prepare("SELECT parent,privileges,name FROM rounds WHERE id=?");
while($parent >= 1)
{
	$stmt->bindValue(1, $parent, PDO::PARAM_INT);
	$stmt->execute();
	if(!($row = $stmt->fetch()))
		E_404();
	if($parent > 1)
		$path = "<a href=\"/round.php?id=$parent\">$row[2]</a> / $path";
	$rounds[] = $parent;
	$parent = $row[0];
	if(privileges($row[1]) < $user_privileges)
		E_403();
	$stmt->closeCursor();
}

template_begin('Submit a solution','','.body{margin-left:150px}');

echo '<ul class="menu"><li><a href="/round.php?id=',$_GET['round'],'">View round</a></li><li><a href="/submissions/?id=',$_GET['round'],'">Submissions</a></li></ul>';

global $info, $lII;

if($_SERVER['REQUEST_METHOD'] == 'POST' && isset($_FILES['solution']))
{
	if($_FILES['solution']['size'] > 102400)
		$info.="<p>File can't be larger than 100 kB.</p>";
	else if($_FILES['solution']['error'] > 0)
		$info.="<p>Error occurred.</p>";
	else
	{
		$stmt = DB::pdo()->prepare("INSERT INTO reports (user_id,round_id,task_id,time) VALUES(?,?,?,FROM_UNIXTIME(?))");
		$stmt->bindValue(1,$_SESSION['id'], PDO::PARAM_STR);
		$stmt->bindValue(2,$_GET['round'], PDO::PARAM_INT);
		$stmt->bindValue(3,$task['id'], PDO::PARAM_INT);
		$stmt->bindValue(4,$time, PDO::PARAM_STR);
		$stmt->execute();
		if(1 > $stmt->rowCount() || !copy($_FILES['solution']['tmp_name'], $_SERVER['DOCUMENT_ROOT'].'/../solutions/'.($lII=DB::pdo()->lastInsertID()).'.cpp') || !file_put_contents($_SERVER['DOCUMENT_ROOT']."/submissions/".$lII.".php", "<?php\nif(isset(\$_GET['download']))\n{header('Content-type: application/text');header('Content-Disposition: attchment; filename=\"".$lII.".cpp\"');readfile(\$_SERVER['DOCUMENT_ROOT'].\"/../solutions/".$lII.".cpp\");exit;}\n\nrequire_once \$_SERVER['DOCUMENT_ROOT'].\"/../php/main.php\";\ntemplate_begin('Zgłoszenie ".$lII."');\nif(isset(\$_GET['source']))\n{echo '<div style=\"margin: 60px 0 0 50px\">', shell_exec(\$_SERVER['DOCUMENT_ROOT'].\"/../judge/CTH \".\$_SERVER['DOCUMENT_ROOT'].\"/../solutions/".$lII.".cpp\"), '</div>';template_end();exit;}\n\necho '<div style=\"text-align: center\">\n<div class=\"report-info\">\n<h1>Zgłoszenie ".$lII."</h1>\n<div class=\"btn-toolbar\">\n<a class=\"btn-small\" href=\"?source\">View source</a>\n<a class=\"btn-small\" href=\"?download\">Download</a>\n</div>\n<h2>Zadanie ".$task['id'].": ".$task['name']."</h2>\n</div>\n<div class=\"submit-status\">\n<pre>Status: Waiting for judge...</pre>\n</div>\n</div>';\ntemplate_end();\n?>"))
			$info.="<p>Error occurred.</p>";
		else
		{
			$stmt->closeCursor();
			$stmt = DB::pdo()->prepare("UPDATE reports SET status='waiting' WHERE id=?");
			$stmt->bindValue(1,$lII, PDO::PARAM_INT);
			$stmt->execute();
			$stmt = DB::pdo()->prepare("INSERT INTO reports_to_rounds (round_id,report_id,user_id,time) VALUES(?,?,?,FROM_UNIXTIME(?))");
			foreach($rounds as $i)
			{
				$stmt->bindValue(1,$i,PDO::PARAM_STR);
				$stmt->bindValue(2,$lII,PDO::PARAM_STR);
				$stmt->bindValue(3,$_SESSION['id'],PDO::PARAM_STR);
				$stmt->bindValue(4,$time,PDO::PARAM_STR);
				$stmt->execute();
			}
			shell_exec("(cd ".$_SERVER['DOCUMENT_ROOT']."../judge ; sudo ./judge_machine) > /dev/null 2> /dev/null &");
			header("Location: /submissions/".$lII.".php");
		}
	}
}

echo '<div class="path">',$path,"</div>",$info,'<div class="form-container">
<h1>Submit a solution</h1>
<h4>Problem: <a href="/round.php?id=',$_GET['round'],'">',$task['name'],'</a></h4>
<form enctype="multipart/form-data" action method="POST">
<label>File</label>
<input class="input-block" type="file" name="solution">
<input type="submit" name="submit" value="Submit" >
</form>
</div>';

template_end();
?>